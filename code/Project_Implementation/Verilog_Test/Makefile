# Makefile for Verilog Systolic Array Testbenches
# Usage: make <target>

IVERILOG = iverilog -g2012
VVP = vvp
GTKWAVE = gtkwave

# Module sources
PE_SRC = PE.v
SA_SRC = SA_MxN.v PE.v
MEMORY_SRC = Memory.v
MATMUL_SRC = MatMul_Controller.v SA_MxN.v PE.v
CONTROL_SRC = Control.v MatMul_Controller.v SA_MxN.v PE.v Memory.v

# Testbench files
PE_TB = PE_tb.v
SA_TB = SA_MxN_tb.v
MEMORY_TB = Memory_tb.v
MATMUL_TB = MatMul_Controller_tb.v
CONTROL_TB = Control_tb.v
SYSTEM_TB = testbench.v

# Output binaries
PE_BIN = sim_pe
SA_BIN = sim_sa
MEMORY_BIN = sim_memory
MATMUL_BIN = sim_matmul
CONTROL_BIN = sim_control
SYSTEM_BIN = sim

# VCD files
PE_VCD = pe_test.vcd
SA_VCD = sa_mxn_test.vcd
MEMORY_VCD = memory_test.vcd
MATMUL_VCD = matmul_controller_test.vcd
CONTROL_VCD = control_test.vcd
SYSTEM_VCD = systolic_array.vcd

.PHONY: all clean test run_all help view_all

# Default target
all: test

# Help target
help:
	@echo "Verilog Systolic Array Makefile"
	@echo "================================"
	@echo ""
	@echo "Targets:"
	@echo "  all              - Run all tests (default)"
	@echo "  test             - Run all tests"
	@echo "  run_all          - Run all tests with script"
	@echo "  clean            - Remove all generated files"
	@echo ""
	@echo "Individual module targets:"
	@echo "  memory           - Compile and run Memory tests"
	@echo "  pe               - Compile and run PE tests"
	@echo "  sa               - Compile and run SA_MxN tests"
	@echo "  matmul           - Compile and run MatMul_Controller tests"
	@echo "  control          - Compile and run Control tests"
	@echo "  system           - Compile and run system-level tests"
	@echo ""
	@echo "View waveforms:"
	@echo "  view_memory      - View Memory test waveforms"
	@echo "  view_pe          - View PE test waveforms"
	@echo "  view_sa          - View SA_MxN test waveforms"
	@echo "  view_matmul      - View MatMul_Controller test waveforms"
	@echo "  view_control     - View Control test waveforms"
	@echo "  view_system      - View system test waveforms"
	@echo "  view_all         - View all waveforms (opens all in GTKWave)"
	@echo ""
	@echo "Examples:"
	@echo "  make memory           # Run memory tests"
	@echo "  make view_memory      # View memory test waveforms"
	@echo "  make test             # Run all tests"
	@echo "  make clean            # Clean all generated files"

# Test all modules
test: memory pe sa matmul control system
	@echo ""
	@echo "=========================================="
	@echo "All tests completed successfully!"
	@echo "=========================================="

# Run with bash script
run_all:
	./run_all_tests.sh

# Individual module targets
memory: $(MEMORY_BIN)
	@echo "Running Memory tests..."
	$(VVP) $(MEMORY_BIN)

pe: $(PE_BIN)
	@echo "Running PE tests..."
	$(VVP) $(PE_BIN)

sa: $(SA_BIN)
	@echo "Running SA_MxN tests..."
	$(VVP) $(SA_BIN)

matmul: $(MATMUL_BIN)
	@echo "Running MatMul_Controller tests..."
	$(VVP) $(MATMUL_BIN)

control: $(CONTROL_BIN)
	@echo "Running Control tests..."
	$(VVP) $(CONTROL_BIN)

system: $(SYSTEM_BIN)
	@echo "Running System tests..."
	$(VVP) $(SYSTEM_BIN)

# Compilation rules
$(MEMORY_BIN): $(MEMORY_TB) $(MEMORY_SRC)
	@echo "Compiling Memory testbench..."
	$(IVERILOG) -o $@ $(MEMORY_TB) $(MEMORY_SRC)

$(PE_BIN): $(PE_TB) $(PE_SRC)
	@echo "Compiling PE testbench..."
	$(IVERILOG) -o $@ $(PE_TB) $(PE_SRC)

$(SA_BIN): $(SA_TB) $(SA_SRC)
	@echo "Compiling SA_MxN testbench..."
	$(IVERILOG) -o $@ $(SA_TB) $(SA_SRC)

$(MATMUL_BIN): $(MATMUL_TB) $(MATMUL_SRC)
	@echo "Compiling MatMul_Controller testbench..."
	$(IVERILOG) -o $@ $(MATMUL_TB) $(MATMUL_SRC)

$(CONTROL_BIN): $(CONTROL_TB) $(CONTROL_SRC)
	@echo "Compiling Control testbench..."
	$(IVERILOG) -o $@ $(CONTROL_TB) $(CONTROL_SRC)

$(SYSTEM_BIN): $(SYSTEM_TB) $(CONTROL_SRC)
	@echo "Compiling System testbench..."
	$(IVERILOG) -o $@ $(SYSTEM_TB) $(CONTROL_SRC)

# View waveforms
view_memory: $(MEMORY_VCD)
	$(GTKWAVE) $(MEMORY_VCD) &

view_pe: $(PE_VCD)
	$(GTKWAVE) $(PE_VCD) &

view_sa: $(SA_VCD)
	$(GTKWAVE) $(SA_VCD) &

view_matmul: $(MATMUL_VCD)
	$(GTKWAVE) $(MATMUL_VCD) &

view_control: $(CONTROL_VCD)
	$(GTKWAVE) $(CONTROL_VCD) &

view_system: $(SYSTEM_VCD)
	$(GTKWAVE) $(SYSTEM_VCD) &

view_all: $(MEMORY_VCD) $(PE_VCD) $(SA_VCD) $(MATMUL_VCD) $(CONTROL_VCD) $(SYSTEM_VCD)
	@echo "Opening all waveforms in GTKWave..."
	$(GTKWAVE) $(MEMORY_VCD) &
	$(GTKWAVE) $(PE_VCD) &
	$(GTKWAVE) $(SA_VCD) &
	$(GTKWAVE) $(MATMUL_VCD) &
	$(GTKWAVE) $(CONTROL_VCD) &
	$(GTKWAVE) $(SYSTEM_VCD) &

# Clean generated files
clean:
	@echo "Cleaning generated files..."
	rm -f $(MEMORY_BIN) $(PE_BIN) $(SA_BIN) $(MATMUL_BIN) $(CONTROL_BIN) $(SYSTEM_BIN)
	rm -f $(MEMORY_VCD) $(PE_VCD) $(SA_VCD) $(MATMUL_VCD) $(CONTROL_VCD) $(SYSTEM_VCD)
	rm -f *.vcd
	rm -f sim_*
	@echo "Clean complete."
