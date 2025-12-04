import subprocess
import re

# ==== CONFIGURABLE PARAMETERS ====
bits = 16
M = bits+2   # number of rows
N = bits   # number of columns

row_in1 = 1    # first input row (read)
row_out = M    # destination row (write result)

# row_in2 will vary from 2 to 5 (inclusive) in the RCS loop

instances_file = "sram_8t_helper.cir"
template_file = "sram_8t_template.cir"
top_file = "sram_8t_run.cir"
pattern_file = "data_in.txt"        # text file with M lines, each line N bits (0/1)
data_out_file = "data_out.txt"      # output file for digital results

# Time at which to sample Q nodes (SPICE time string with units)
SAMPLE_TIME = "800p"                # <-- change this as you like

# Voltage threshold for digital 1/0 decision
DIGITAL_THRESHOLD = 0.9

# Time window for energy measurement (must match your .tran range)
ENERGY_TSTART = "200p"
ENERGY_TSTOP  = "1400p"                # <-- change to your tran stop time
# =================================


def read_pattern(filename, M, N):
    """
    Read an MxN pattern of '0'/'1' from a text file.
    - Each non-empty line = one row.
    - Extra spaces are ignored.
    - If a line has fewer than N bits, it's padded with '0'.
    - If it has more, it's truncated to N.
    - Bits are then inverted (as you had in your code).
    """
    rows = []
    with open(filename, "r") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            # remove spaces if any
            line = line.replace(" ", "")
            bits = [ch if ch in ("0", "1") else "0" for ch in line]
            # invert bits (keep your original behaviour)
            bits = ["1" if b == "0" else "0" for b in bits]
            print(f"Read line: {line} -> bits (inverted): {bits}")
            if len(bits) < N:
                bits += ["0"] * (N - len(bits))
            elif len(bits) > N:
                bits = bits[:N]
            rows.append(bits)
            if len(rows) >= M:
                break

    # If fewer than M useful lines, pad with all-zero rows
    while len(rows) < M:
        rows.append(["0"] * N)

    return rows  # list of M rows, each row is list of N chars '0'/'1'

def generate_instances(M, N, row_in1, row_in2, row_out, out_file, pattern, sample_time):
    """
    Generate the helper netlist (instances + sources + ICs) and
    return a string with analysis commands (plot + measures)
    to inject into the .control block.
    """
    with open(out_file, "w") as f:
        f.write("* Auto-generated SRAM array instances\n")
        f.write(
            f"* M={M}, N={N}, row_in1={row_in1}, row_in2={row_in2}, "
            f"row_out={row_out}\n\n"
        )

        # --------------------------------------------------
        # 1) Instantiate all 8T cells
        # Node naming:
        #   q_r{r}_c{c}, q_bar_r{r}_c{c}
        #   wwl_{r}, rwl_{r}
        #   wbl_{c}, wbl_bar_{c}, rbl_{c}
        # --------------------------------------------------
        for r in range(1, M + 1):
            for c in range(1, N + 1):
                f.write(
                    "Xcell_r{r}_c{c} wbl_{c} wbl_bar_{c} "
                    "q_r{r}_c{c} q_bar_r{r}_c{c} "
                    "wwl_{r} rwl_{r} rbl_{c} "
                    "8t_sram "
                    "width_N={{1.5*width_N}} width_N_acc={{width_N}} width_P={{width_N}} "
                    "width_N_read_1={{20*LAMBDA}} width_N_read_2={{20*LAMBDA}}\n"
                    .format(r=r, c=c)
                )
        f.write("\n")

        # --------------------------------------------------
        # 2) Per-column circuits:
        #    - precharge rbl_c with pre_charge_single (node=rbl_c, pc_en=pch)
        #    - bitline capacitance C_rbl_c
        #    - write_driver for each column
        # --------------------------------------------------
        for c in range(1, N + 1):
            # Precharge read bitline
            f.write(
                "Xpc_c{c} rbl_{c} pch pre_charge_single width_P={{width_pc}}\n"
                .format(c=c)
            )

            # Bitline capacitance on rbl_c
            f.write("C_rbl_{c} rbl_{c} gnd 50fF\n".format(c=c))

            # Write driver for this column
            f.write(
                "Xwd_c{c} rbl_{c} wwl_{row_out} wbl_{c} wbl_bar_{c} "
                "write_driver width_N={{width_N}} width_P={{width_P}}\n"
                .format(c=c, row_out=row_out)
            )

        f.write("\n")

        # --------------------------------------------------
        # 2b) Global precharge control for all columns
        # --------------------------------------------------
        f.write(
            "V_pc pch gnd "
            "pulse 0 1.8 init_time taper_time taper_time wl_dur wl_period\n\n"
        )

        # --------------------------------------------------
        # 3) Wordline drivers for all rows
        # --------------------------------------------------
        for r in range(1, M + 1):
            if r == row_in1 or r == row_in2:
                # Read rows
                f.write(
                    "V_rwl_{r} rwl_{r} gnd "
                    "pulse 0 1.8 init_time taper_time taper_time wl_dur wl_period\n"
                    .format(r=r)
                )
                f.write("V_wwl_{r} wwl_{r} gnd dc 0\n".format(r=r))
            elif r == row_out:
                # Write row
                f.write("V_rwl_{r} rwl_{r} gnd dc 0\n".format(r=r))
                f.write(
                    "V_wwl_{r} wwl_{r} gnd "
                    "pulse 0 1.8 init_time taper_time taper_time wl_dur wl_period\n"
                    .format(r=r)
                )
            else:
                # Idle rows
                f.write("V_rwl_{r} rwl_{r} gnd dc 0\n".format(r=r))
                f.write("V_wwl_{r} wwl_{r} gnd dc 0\n".format(r=r))

        f.write("\n")

        # --------------------------------------------------
        # 4) Initial conditions
        # Use pattern[r-1][c-1] as bit (0/1).
        # q = {bit * vdd}, q_bar = {vdd - bit * vdd}
        # --------------------------------------------------
        f.write("* Initial conditions for all cells and read bitlines\n")
        for c in range(1, N + 1):
            f.write(".ic V(rbl_{c}) = 0\n".format(c=c))

        for r in range(1, M + 1):
            for c in range(1, N + 1):
                bit_char = pattern[r - 1][c - 1]
                bit_val = 1 if bit_char == "1" else 0
                line = (
                    ".ic V(q_r{r}_c{c}) = {{{b}*vdd}}  "
                    "V(q_bar_r{r}_c{c}) = {{vdd - {b}*vdd}}\n"
                    .format(r=r, c=c, b=bit_val)
                )
                f.write(line)
                print(line.strip())

    # ------------------------------------------------------
    # 5) Build analysis commands (plot + measures) to inject
    # ------------------------------------------------------
    # Plot: wwl_row_out and first up-to-4 columns of q_r{row_out}_c*
    cols_to_plot = N
    offset = 2
    # plot_terms = []
    plot_terms = [f"wwl_{row_out}"]
    for c in range(cols_to_plot, 0, -1):
        plot_terms.append(f"q_r{row_out}_c{c}+{offset}")
        offset += 2
    plot_terms.append(f"wwl_{row_out}")
    plot_cmd = "gnuplot " + ", ".join(plot_terms)
    # plot_cmd += f"\nhardcopy \"plot{row_in2}.png\""
    # Measures: one per column, at SAMPLE_TIME
    measure_cmds = []
    for c in range(1, N + 1):
        mname = f"v_q_r{row_out}_c{c}"
        node = f"q_r{row_out}_c{c}"
        measure_cmds.append(
            f".measure tran {mname} FIND v({node}) AT={sample_time}"
        )
    measure_cmds.append(
        f".measure tran E_VDD INTEG par('-I(Vdd) * V(vdd)') FROM={ENERGY_TSTART} TO={ENERGY_TSTOP}"
    )

    return plot_cmd, measure_cmds

def inject_plot(template_file, output_file, plot_cmd, measure_cmds):
    """
    Replace the @PLOT_CMD@ and @MEAS_CMD@ placeholders with
    the actual plot and measure commands.
    """
    with open(template_file, "r") as fin, open(output_file, "w") as fout:
        for line in fin:
            if "@PLOT_CMD@" in line:
                fout.write(plot_cmd + "\n")
            elif "@MEAS_CMD@" in line:
                for cmd in measure_cmds:
                    fout.write(cmd + "\n")
            else:
                fout.write(line)


def parse_measures(logfile, row_out, N):
    """
    Parse ngspice log file to extract:
      - measured values v_q_r{row_out}_c1..cN
      - energy E_VDD
    Returns: (list_of_v, energy)
    """
    values = {}
    energy = 0.0

    pattern_v = re.compile(
        r"v_q_r{row}_c(\d+)\s*=\s*([+-]?\d+(?:\.\d*)?(?:[eE][+-]?\d+)?)"
        .format(row=row_out)
    )
    pattern_e = re.compile(
        r"e_vdd\s*=\s*([+-]?\d+(?:\.\d*)?(?:[eE][+-]?\d+)?)"
    )

    with open(logfile, "r") as f:
        for line in f:
            mv = pattern_v.search(line)
            if mv:
                col = int(mv.group(1))
                val = float(mv.group(2))
                values[col] = val

            me = pattern_e.search(line)
            if me:
                energy = float(me.group(1))

    analog_vals = [values.get(c, 0.0) for c in range(1, N + 1)]
    return analog_vals, energy


def analog_to_bits(values, threshold):
    """Convert list of analog voltages to list of '0'/'1' using threshold."""
    bits = []
    for v in values:
        b = "1" if v >= threshold else "0"
        bits.append(b)
    return bits


def main():
    # 0) Read initial pattern from file
    pattern = read_pattern(pattern_file, M, N)
    analog_vals_all = []
    digital_vals_all = []
    energy_all = []

    # 1) Open data_out.txt and write N in the first line
    with open(data_out_file, "w") as fout:
        fout.write(str(N) + "\n")

        # 2) Iterate RCS with row_in2 from 2 to 5 (inclusive)
        for row_in2 in range(2, M):
            print(f"\n=== RCS iteration with row_in2 = {row_in2} ===")

            # 2a) Generate helper netlist + analysis block
            plot_cmd, meas_cmd = generate_instances(
                M, N, row_in1, row_in2, row_out,
                instances_file, pattern, SAMPLE_TIME
            )

            # 2b) Inject analysis commands into top-level run file
            inject_plot(template_file, top_file, plot_cmd, meas_cmd)

            # 2c) Run ngspice in batch mode, log to a file
            log_file = f"ngspice_row_in2_{row_in2}.log"
            try:
                subprocess.run(
                    ["ngspice", "-b", "-o", log_file, top_file],
                    check=True
                )
            except KeyboardInterrupt:
                print("\nSimulation interrupted by user (Ctrl+C). Exiting cleanly.")
                return
            except FileNotFoundError:
                print("Error: ngspice not found. Make sure it is installed and in your PATH.")
                return
            except subprocess.CalledProcessError as e:
                print(f"ngspice exited with error code {e.returncode} for row_in2={row_in2}.")
                continue

            # 2d) Parse measured analog values and energy, then convert to bits
            analog_vals, energy = parse_measures(log_file, row_out, N)
            bits = analog_to_bits(analog_vals, DIGITAL_THRESHOLD)

            analog_vals_all.append(analog_vals)
            digital_vals_all.append(bits)
            energy_all.append(energy)

            # 2e) Write bits as one row into data_out.txt
            fout.write("".join(bits) + "\n")

    print("\nSimulation completed. Results written to", data_out_file)

    # 3) Print final analog, digital, and energy values row by row
    print("\n===== RCS RESULT SUMMARY =====")
    print(f"{'row_in2':>7} | {'Analog values (V)':<40} | {'Bits':<6} | {'Energy [pJ]':>12}")
    print("-" * 80)
    for idx, (analog_row, digital_row, energy) in enumerate(
        zip(analog_vals_all, digital_vals_all, energy_all),
        start=2
    ):
        # energy is in Joules from ngspice (INTEG V*I dt)
        energy_pJ = energy * 1e12
        analog_str = ", ".join(f"{v:.3f}" for v in analog_row)
        bits_str = "".join(digital_row)
        print(f"{idx:7d} | {analog_str:<40} | {bits_str:<6} | {energy_pJ:12.3f}")
    print("-" * 80)
    total_energy_pJ = sum(energy_all) * 1e12
    print(f"Total Energy per multipication [pJ]: {total_energy_pJ:.3f}")

if __name__ == "__main__":
    main()
