Here’s a **clean, professional, and well-formatted Markdown version** of your documentation for `SA_MxN_tb.cpp`:

---

# 🧩 SA_MxN Testbench (`SA_MxN_tb.cpp`)

## 1. Overview

This **SystemC module** (`SA_MxN_tb`) serves as a **testbench** for verifying the functionality of a **generalized `SA_MxN` systolic array module**.

### Primary Objectives

* Instantiate and connect the **Device Under Test (DUT)** — the `SA_MxN` module.
* Drive **clock**, **reset**, and **data signals**.
* Simulate two distinct matrix multiplication **dataflows**:

  * **Weight Stationary (WS) Mode**
  * **Output Stationary (OS) Mode**
* Generate a **VCD (Value Change Dump)** trace file `SA_MxN.vcd` for waveform visualization in tools such as **GTKWave**.

---

## 2. Key Components & Configuration

### 🔧 Macros

The testbench uses three compile-time macros (defined in `SA_MxN.h` or via compiler flags):

| Macro   | Description                                                                |
| :------ | :------------------------------------------------------------------------- |
| `M`     | Number of rows in the systolic array                                       |
| `N`     | Number of columns in the systolic array                                    |
| `K_DIM` | Inner matrix dimension for OS test (`C[M][N] = A[M][K_DIM] * W[K_DIM][N]`) |

---

### ⚙️ Key Signals

| Signal              | Type           | Description                                                                                                                   |
| :------------------ | :------------- | :---------------------------------------------------------------------------------------------------------------------------- |
| `output_stationary` | `bool`         | Control signal selecting the mode:<br>• `false (0)` → **Weight Stationary (WS)**<br>• `true (1)` → **Output Stationary (OS)** |
| `preload_valid`     | `bool`         | Enables loading of initial values into PEs                                                                                    |
| `preload_data[M*N]` | `sc_int` array | Data for initializing PE registers                                                                                            |
| `in_left[M]`        | Input ports    | Feed data from the **left side** (one per row)                                                                                |
| `in_top[N]`         | Input ports    | Feed data from the **top side** (one per column)                                                                              |
| `out_right[M]`      | Output ports   | Stream data out of the **right side**                                                                                         |
| `out_bottom[N]`     | Output ports   | Stream data out of the **bottom side**                                                                                        |

---

## 3. Testbench Workflow

The simulation follows this sequence:

1. **Initialization** – Instantiate and connect the `SA_MxN` DUT.
2. **Global Reset** – Apply reset high for one clock cycle to initialize all PEs.
3. **Test 1: Weight Stationary (WS) Mode**
4. **Global Reset** – Reset again to clear state.
5. **Test 2: Output Stationary (OS) Mode**
6. **Simulation End** – Call `sc_stop()` to terminate the simulation.

---

## 4. Test 1: Weight Stationary (WS) Mode

### 🧠 Concept

* **Weights (W)** are **preloaded** and remain fixed inside the PEs.
* **Inputs (A)** are streamed from the **left**.
* **Partial sums** flow **downward** through the array.

> Mode: `output_stationary.write(false);`

---

### 🔹 Inputs

#### Preload Phase

* `preload_valid = true`
* Load `W_ws[M][N]` matrix into the PEs.

  * `PE[i][j]` receives `W_ws[i][j]`.

#### Compute Phase

* `preload_valid = false`
* **Left Inputs (`in_left[M]`)**: Stream matrix `A_ws[M][M]`, column-skewed.

  * `in_left[i]` gets column `i` of `A_ws`.
  * Staggered input pattern:

    ```
    in_left[i].write(A_ws[clks - i][i])   // valid for (clks - i) in bounds
    ```
* **Top Inputs (`in_top[N]`)**: Constant zeros, representing `P_in = 0`.

---

### 🔹 Expected Output

| Output          | Description                                                                                                             |
| :-------------- | :---------------------------------------------------------------------------------------------------------------------- |
| `out_bottom[N]` | Final result matrix **C = A × W**, streamed out (time-skewed).                                                          |
| Skew Behavior   | First valid result `C[0][0]` appears after `M` cycles; last result `C[M-1][N-1]` appears later.                         |
| Verification    | Observe `out_bottom_j` signals in **GTKWave** (`SA_MxN.vcd`). `out_right_i` will show delayed versions of the A matrix. |

---

## 5. Test 2: Output Stationary (OS) Mode

### 🧠 Concept

* The **outputs (partial sums)** are stationary in each PE.
* Matrices **A** and **W** are streamed from **left** and **top**, respectively.
* Each PE computes:

  ```
  P_out = P_internal + (A_in × W_in)
  ```

  and stores `P_out` internally.

> Mode: `output_stationary.write(true);`

---

### 🔹 Inputs

#### Preload Phase

* `preload_valid = true`
* Initialize all internal registers to **zero**.

#### Compute Phase

* `preload_valid = false`

| Input Source | Matrix           | Skew Type         | Logic                                 |
| :----------- | :--------------- | :---------------- | :------------------------------------ |
| `in_left[M]` | `A_os[M][K_DIM]` | **Row-skewed**    | `in_left[i].write(A_os[i][clks - i])` |
| `in_top[N]`  | `W_os[K_DIM][N]` | **Column-skewed** | `in_top[j].write(W_os[clks - j][j])`  |

---

### 🔹 Expected Output

| Output                 | Description                                                                                                                                                    |
| :--------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Internal Registers** | Contain final result `C[M][N]` after `(M + K_DIM + N - 2)` cycles.                                                                                             |
| **out_right[M]**       | Pass-through of A elements (delayed).                                                                                                                          |
| **out_bottom[N]**      | Pass-through of W elements (delayed).                                                                                                                          |
| **Verification**       | Inspect `SA_MxN.PE_i_j.internal_register` signals near the end of simulation (`t ≈ (M + K_DIM + N) * 10 ns`). Compare stored values to expected **C = A × W**. |

---

## 6. How to Verify

1. **Compile** the SystemC project.
2. **Run** the simulation executable.
3. **Open** the generated `SA_MxN.vcd` in **GTKWave**.
4. **Add** key signals to the trace:

   * `clk`, `reset`, `output_stationary`, `preload_valid`
5. **For WS Test:**

   * Add `in_left_*`, `in_top_*`, and `out_bottom_*`.
   * Verify output skew and correctness (`C = A × W`).
6. **For OS Test:**

   * Add `in_left_*`, `in_top_*`, and internal signals (`PE_i_j.internal_reg`).
   * Check final register contents against expected results.

---

### ✅ Summary

| Test | Stationary Type | Preload Data | Streamed Data     | Primary Output        |
| :--- | :-------------- | :----------- | :---------------- | :-------------------- |
| WS   | Weight          | W            | A                 | out_bottom[N]         |
| OS   | Output          | Zeros        | A (left), W (top) | PE internal registers |

---

Would you like me to include **LaTeX-style math notation** (for matrices and equations like ( C = A \times W )) and a **diagram section** (for WS and OS dataflow)? That would make this Markdown ideal for documentation on GitHub or reports.
