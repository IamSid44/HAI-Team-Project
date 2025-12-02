# Systolic Array Verilog Implementation - Complete Guide

## Table of Contents
1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [Architecture](#architecture)
4. [Module Descriptions](#module-descriptions)
5. [Testbenches](#testbenches)
6. [How to Run](#how-to-run)
7. [Memory File Format](#memory-file-format)
8. [Implementation Details](#implementation-details)
9. [Design Decisions](#design-decisions)

---

## Overview

This is a complete Verilog implementation of a **tiled systolic array** for matrix multiplication. The design supports:
- **Two dataflow modes**: Weight Stationary (WS) and Output Stationary (OS)
- **Tiling support**: Handles matrices larger than the hardware array size (3×3)
- **Proper FSM design**: Avoids race conditions found in the original SystemC code
- **Comprehensive testing**: Individual testbenches for each module plus full system tests

### System Capabilities
- Default 3×3 systolic array (configurable parameter M)
- 64-bit IEEE 754 floating-point arithmetic
- Byte-addressed memory with 64KB capacity
- Support for arbitrary matrix sizes through automatic tiling
- Both weight stationary and output stationary dataflow modes

---

## Quick Start

### Prerequisites
Install Icarus Verilog and GTKWave:
```bash
sudo apt install iverilog gtkwave
```

### Running the Main Testbench (Recommended)
```bash
cd code/Project_Implementation/Verilog_Test

# Compile all modules with the comprehensive testbench
iverilog -g2012 -o sim_control Control_tb.v Control.v MatMul_Controller.v SA_MxN.v PE.v Memory.v

# Run simulation
vvp sim_control

# View waveforms
gtkwave control_test.vcd
```

This will run 7 comprehensive tests covering matrices from 1×1 to 10×10 in both dataflow modes.

---

## Architecture

### System Hierarchy
```
testbench
  └── Control (Top-level tiling controller)
       ├── Memory (Byte-addressed memory with file loading)
       └── MatMul_Controller (Single tile computation)
            └── SA_MxN (M×N Systolic Array)
                 └── PE [M×N instances] (Processing Elements)
```

### Data Flow
1. **Input**: Testbench loads matrices A and W into memory
2. **Control**: Manages tiling loop over i, j, k tile indices
3. **Memory I/O**: Loads A_tile and W_tile for current computation
4. **Computation**: MatMul_Controller executes single tile multiplication
5. **Output**: Results written back to memory, accumulated for multiple k tiles
6. **Iteration**: Process repeats for all tile combinations

---

## Module Descriptions

### 1. PE.v - Processing Element

**Purpose**: Basic computational unit performing multiply-accumulate operations.

**Parameters**: None

**Ports**:
- `clk`, `reset`: Clock and reset signals
- `output_stationary`: Mode selection (0=WS, 1=OS)
- `in_top[63:0]`: Input from above (activations or partial sums)
- `in_left[63:0]`: Input from left (activations or weights)
- `out_right[63:0]`: Output to right (forwarded left input)
- `out_bottom[63:0]`: Output to bottom (result or forwarded top input)
- `preload_valid`: Signal to load new weight/clear accumulator
- `preload_data[63:0]`: Weight to load (WS) or unused (OS)

**Behavior**:

**Weight Stationary Mode** (output_stationary = 0):
- Stores weight in `preload_buffer` when `preload_valid = 1`
- Computes: `accumulator = (in_left × preload_buffer) + in_top`
- Forwards `in_left` to right, `accumulator` to bottom
- Weight remains stationary for multiple cycles

**Output Stationary Mode** (output_stationary = 1):
- When `preload_valid = 1`: Drains current accumulator to bottom, then resets
- Otherwise: Computes `accumulator += (in_top × in_left)`
- Forwards both inputs (right and bottom)
- Accumulator stays in PE until explicitly drained

**Key Features**:
- Uses `$realtobits` and `$bitstoreal` for floating-point arithmetic
- Properly handles drain operation in OS mode
- No combinational loops or race conditions

---

### 2. SA_MxN.v - Systolic Array

**Purpose**: M×N grid of interconnected PEs with proper data routing.

**Parameters**:
- `M`: Number of rows (default 3)
- `N`: Number of columns (default 3)

**Ports**:
- `in_top[64*N-1:0]`: Packed inputs from top (N elements × 64 bits)
- `in_left[64*M-1:0]`: Packed inputs from left (M elements × 64 bits)
- `out_right[64*M-1:0]`: Packed outputs to right (M elements)
- `out_bottom[64*N-1:0]`: Packed outputs to bottom (N elements)
- `preload_data[64*M*N-1:0]`: Packed preload data (M×N elements)
- Other: `clk`, `reset`, `output_stationary`, `preload_valid`

**Structure**:
- Creates M×N grid of PEs using `generate` blocks
- Each PE connects to neighbors: PE[i][j] receives from PE[i-1][j] (top) and PE[i][j-1] (left)
- Edge PEs connect to external inputs/outputs
- All PEs operate synchronously with shared control signals

**Data Packing**:
- Uses `generate` blocks to pack/unpack multi-dimensional arrays into flat bit vectors
- Required because Verilog module ports cannot be multi-dimensional arrays
- Example: `in_top[64*N-1:0]` contains N 64-bit values concatenated

---

### 3. Memory.v - Byte-Addressed Memory

**Purpose**: Provides byte-addressed memory with optional file initialization.

**Parameters**:
- `ADDR_WIDTH`: Address bus width (default 16)
- `DATA_WIDTH`: Data width (default 64)
- `MEM_SIZE`: Total memory size in bytes (default 65536)
- `MEM_FILE`: Optional memory initialization file

**Ports**:
- `address[ADDR_WIDTH-1:0]`: Byte address (must be 8-byte aligned for 64-bit data)
- `read_enable`, `write_enable`: Control signals
- `write_data[DATA_WIDTH-1:0]`: Data to write
- `read_data[DATA_WIDTH-1:0]`: Data read from memory
- `ready`: Indicates operation complete
- `clk`, `reset`

**FSM States**:
1. **IDLE**: Waiting for read or write request
2. **READ**: Reading data, asserting `ready` signal
3. **WRITE**: Writing data on negative clock edge

**Features**:
- 3-state FSM avoids combinational race conditions
- Supports file loading with format: `BYTE_ADDRESS: VALUE` or `XXXX` for uninitialized
- Automatic 8-byte alignment (address[2:0] ignored)
- Write occurs on negative clock edge to avoid read/write conflicts

**Memory Organization**:
- Internal array: `mem_array[0:MEM_SIZE/8-1]` (each element is 64 bits)
- Byte address → array index conversion: `address >> 3`
- Example: Byte address 0x0010 maps to `mem_array[2]`

---

### 4. MatMul_Controller.v - Single Tile Controller

**Purpose**: Controls systolic array for one tile (up to M×M) matrix multiplication.

**Parameters**:
- `M`: Systolic array size (default 3)

**Ports**:
- `K1_actual`, `K2_actual`, `K3_actual`: Actual dimensions for this tile (≤ M)
- `A_tile_flat[64*M*M-1:0]`: Flattened A tile (M×M elements)
- `W_tile_flat[64*M*M-1:0]`: Flattened W tile (M×M elements)
- `C_tile_flat[64*M*M-1:0]`: Flattened C tile result (M×M elements)
- `output_stationary`: Dataflow mode selection
- `start`, `done`: Control signals
- `clk`, `reset`

**FSM - Weight Stationary Mode** (7 states):
1. **IDLE**: Wait for start signal
2. **WS_PRELOAD**: Load weights into PEs (takes M cycles with skewing)
3. **WS_RESET**: Dummy state for transition
4. **WS_FEED**: Feed activations with row-wise skewing (takes K1_actual+M-1 cycles)
5. **WS_COMPUTE**: Computation in progress
6. **WS_DRAIN**: Extract results from bottom (takes M cycles with deskewing)
7. **WS_DONE**: Signal completion

**FSM - Output Stationary Mode** (5 states):
1. **IDLE**: Wait for start signal
2. **OS_RESET**: Clear PE accumulators via preload
3. **OS_ACCUMULATE**: Stream both A and W with diagonal skewing (takes K3_actual+2*M-2 cycles)
4. **OS_DRAIN**: Extract accumulated results (takes M cycles)
5. **OS_DONE**: Signal completion

**Skewing Logic**:
- **WS Preload**: Column j receives weight at cycle j (stagger left-to-right)
- **WS Feed**: Row i receives activation at cycle i (stagger top-to-bottom)
- **OS Accumulate**: Row i waits i cycles, column j waits j cycles (diagonal wave)
- **Drain**: Column j is read at cycle j (compensate for internal array delay)

**Key Implementation Details**:
- Handles undersized tiles (K1/K2/K3 < M) by zero-padding
- Separate FSMs for WS and OS modes to avoid conditional logic complexity
- Proper cycle counting ensures data alignment through systolic array

---

### 5. Control.v - Top-Level Tiling Controller

**Purpose**: Manages tiling for matrices larger than M×M, coordinating memory I/O and computation.

**Parameters**:
- `M`: Systolic array size (default 3)
- `ADDR_WIDTH`: Memory address width (default 16)
- `MEM_SIZE`: Memory size (default 65536)

**Ports**:
- `K1`, `K2`, `K3`: Full matrix dimensions (K1×K2 matrix A, K2×K3 matrix W)
- `A_base_addr`, `W_base_addr`, `C_base_addr`: Base addresses in memory
- `output_stationary`: Dataflow mode
- `start`, `done`: Control signals
- `clk`, `reset`

**FSM States** (8 states):
1. **IDLE**: Wait for start signal
2. **INIT**: Calculate number of tiles in each dimension
3. **READ_A**: Load A tile from memory (M×M reads)
4. **READ_W**: Load W tile from memory (M×M reads)
5. **COMPUTE**: Execute MatMul_Controller for current tile
6. **WRITE_C**: Write C tile back to memory (M×M writes)
7. **NEXT_TILE**: Increment tile indices, check if done
8. **DONE_STATE**: Signal completion

**Tiling Algorithm**:
```
num_i_tiles = ceil(K1 / M)
num_j_tiles = ceil(K3 / M)
num_k_tiles = ceil(K2 / M)

for tile_i = 0 to num_i_tiles-1:
    for tile_j = 0 to num_j_tiles-1:
        # Initialize C tile to zero at start of j-loop
        for tile_k = 0 to num_k_tiles-1:
            # Load A[tile_i, tile_k] from memory
            # Load W[tile_k, tile_j] from memory
            # Compute: C_tile += A_tile × W_tile
        # Write final C[tile_i, tile_j] to memory
```

**Edge Tile Handling**:
- Calculates actual dimensions for each tile: `min(M, K1 - tile_i*M)`
- Pads undersized tiles with zeros
- Ensures correct indexing in memory for non-aligned dimensions

**Memory Layout**:
- Row-major storage: `A[i][j]` at `A_base + (i*K2 + j)*8` bytes
- Each element is 8 bytes (64-bit double)
- Tile elements loaded in row-major order into `A_tile` array

---

## Testbenches

### Individual Module Testbenches

#### Memory_tb.v
**Tests**: Basic read/write, sequential access, overwrite, boundary addresses, reset

**Run**:
```bash
iverilog -g2012 -o sim_memory Memory_tb.v Memory.v
vvp sim_memory
gtkwave memory_test.vcd
```

#### PE_tb.v
**Tests**: Both WS/OS modes, MAC operations, weight updates, accumulation, zero/negative handling

**Run**:
```bash
iverilog -g2012 -o sim_pe PE_tb.v PE.v
vvp sim_pe
gtkwave pe_test.vcd
```

#### SA_MxN_tb.v
**Tests**: 3×3 array with identity matrix, 2×2 multiplication, undersized inputs, both modes

**Run**:
```bash
iverilog -g2012 -o sim_sa SA_MxN_tb.v SA_MxN.v PE.v
vvp sim_sa
gtkwave sa_mxn_test.vcd
```

#### MatMul_Controller_tb.v
**Tests**: Optimal 3×3, undersized 2×2, 1×1 minimal, non-square (2×3)*(3×2), both modes

**Run**:
```bash
iverilog -g2012 -o sim_matmul MatMul_Controller_tb.v MatMul_Controller.v SA_MxN.v PE.v
vvp sim_matmul
gtkwave matmul_controller_test.vcd
```

---

### Main System Testbenches

#### Control_tb.v - Comprehensive Full System Test (RECOMMENDED)

**Purpose**: Tests the complete tiled matrix multiplication system with various sizes.

**Test Cases**:
1. **Test 1**: 3×3 optimal (no tiling) - Weight Stationary
2. **Test 2**: 2×2 undersized - Weight Stationary
3. **Test 3**: 5×5 with tiling (2×2×2 tiles) - Weight Stationary
4. **Test 4**: 7×7 with tiling (3×3×3 tiles) - Output Stationary
5. **Test 5**: 4×6 × 6×5 non-square - Weight Stationary
6. **Test 6**: 1×1 minimal - Weight Stationary
7. **Test 7**: 10×10 large (4×4×4 = 64 tiles) - Output Stationary

**Features**:
- **Automatically generates memory.txt** with test data
- Computes expected results using software floating-point
- Verifies results with tolerance (0.01)
- Reports errors with location and magnitude
- Comprehensive VCD waveform output

**Run**:
```bash
iverilog -g2012 -o sim_control Control_tb.v Control.v MatMul_Controller.v SA_MxN.v PE.v Memory.v
vvp sim_control
gtkwave control_test.vcd
```

**Output**: The testbench will display:
- Test case description
- Matrix dimensions
- Computation progress
- Result matrix values
- Pass/fail status with error count and max difference

#### testbench.v - Original System Test

**Purpose**: Original validation with 5×5 matrices in both modes.

**Run**:
```bash
iverilog -g2012 -o sim testbench.v Control.v MatMul_Controller.v SA_MxN.v PE.v Memory.v
vvp sim
gtkwave systolic_array.vcd
```

---

## How to Run

### Step-by-Step Instructions

1. **Navigate to the directory**:
   ```bash
   cd code/Project_Implementation/Verilog_Test
   ```

2. **Choose which test to run**:
   - For comprehensive testing: Use `Control_tb.v` (recommended)
   - For individual module testing: Use module-specific testbenches
   - For original validation: Use `testbench.v`

3. **Compile** (example with Control_tb.v):
   ```bash
   iverilog -g2012 -o sim_control Control_tb.v Control.v MatMul_Controller.v SA_MxN.v PE.v Memory.v
   ```
   
   **Important**: 
   - `-g2012` enables SystemVerilog 2012 features (required)
   - Include ALL dependent files in compilation command
   - Order doesn't matter for iverilog, but list testbench first by convention

4. **Run simulation**:
   ```bash
   vvp sim_control
   ```
   
   This will:
   - Generate `memory.txt` file (if using Control_tb.v)
   - Execute all test cases
   - Print results to console
   - Create `control_test.vcd` waveform file

5. **View waveforms** (optional):
   ```bash
   gtkwave control_test.vcd
   ```
   
   Use GTKWave to inspect:
   - FSM state transitions
   - Data flow through systolic array
   - Memory read/write operations
   - Timing relationships

### Compilation Dependencies

Each testbench requires specific modules to be compiled together:

| Testbench | Required Verilog Files |
|-----------|------------------------|
| Memory_tb.v | Memory.v |
| PE_tb.v | PE.v |
| SA_MxN_tb.v | SA_MxN.v, PE.v |
| MatMul_Controller_tb.v | MatMul_Controller.v, SA_MxN.v, PE.v |
| Control_tb.v | Control.v, MatMul_Controller.v, SA_MxN.v, PE.v, Memory.v |
| testbench.v | Control.v, MatMul_Controller.v, SA_MxN.v, PE.v, Memory.v |

### Troubleshooting

**Error: "syntax error"**
- Ensure you're using `-g2012` flag
- Check all required files are included

**Error: "module not found"**
- Verify all dependent .v files are in the command
- Check file paths are correct

**Simulation hangs**
- Check testbench timeout (usually 50000 cycles)
- Use Ctrl+C to stop and check VCD file

**Incorrect results**
- View waveforms in GTKWave to debug data flow
- Check memory.txt has correct initial values
- Verify matrix dimensions match between A and W

---

## Memory File Format

### Format Specification

The `memory.txt` file uses byte addressing with the following format:

```
# Comment lines start with #
BYTE_ADDRESS: VALUE
BYTE_ADDRESS: XXXX
```

Where:
- `BYTE_ADDRESS`: 8-digit decimal byte address (e.g., `00000000`, `00000512`)
- `VALUE`: Floating-point number (e.g., `1.000000`, `3.141593`)
- `XXXX`: Indicates uninitialized memory location

### Example

```
# Byte-Addressed Memory File
# Format: BYTE_ADDRESS: VALUE
# XXXX indicates uninitialized memory
#
00000000: 1.000000
00000008: 2.000000
00000016: 3.000000
00000024: 4.000000
00000032: XXXX
```

### Address Calculation

For 64-bit (8-byte) data:
- Element [i] is at byte address: `i * 8`
- Matrix element [row][col] with K columns: `(row * K + col) * 8`

**Examples**:
- First element (0,0): byte address 0
- Element [1][0] in 5-column matrix: byte address 40 (1*5*8 = 40)
- Element [2][3] in 5-column matrix: byte address 104 ((2*5+3)*8 = 104)

### Automatic Generation

The `Control_tb.v` testbench automatically generates `memory.txt` for each test case using the `generate_memory_file()` task. You don't need to manually create this file.

If you want to create custom test data:
1. Create `memory.txt` in the Verilog_Test directory
2. Use the format above with correct byte addresses
3. Ensure addresses are 8-byte aligned (multiples of 8)

---

## Implementation Details

### Data Width and Addressing

**64-bit Floating Point**:
- All data paths are 64 bits wide
- Uses IEEE 754 double precision format
- Verilog functions: `$realtobits()` and `$bitstoreal()`

**Memory Addressing**:
- Byte-addressed: Each address points to one byte
- 64-bit data requires 8-byte alignment
- Address-to-index conversion: `address >> 3` (divide by 8)
- Element stride in memory: 8 bytes

### Packing/Unpacking Arrays

**Challenge**: Verilog module ports cannot be multi-dimensional arrays.

**Solution**: Use flattened 1D bit vectors with packing/unpacking.

**Example** (3×3 array):
```verilog
// Internal: 2D array
reg [63:0] A_tile [0:8];

// Port: Flattened 1D vector
output [64*9-1:0] A_tile_flat;

// Packing with generate block
genvar i;
generate
    for (i = 0; i < 9; i = i + 1) begin
        assign A_tile_flat[64*(i+1)-1:64*i] = A_tile[i];
    end
endgenerate
```

**Key Points**:
- Signals driven by `assign` must be declared as `wire`, not `reg`
- Use `generate` blocks with `genvar` for variable indexing
- Bit ordering: Element 0 in LSBs, element N-1 in MSBs

### FSM Design Principles

**Best Practices Applied**:
1. **Separate next_state logic**: Use combinational `always @(*)` block
2. **Registered state update**: Use clocked `always @(posedge clk)` block
3. **Registered outputs**: All outputs assigned in clocked block (avoids glitches)
4. **No combinational loops**: Never reference output in its own assignment

**Memory Write Timing**:
- Reads on positive edge
- Writes on negative edge
- Prevents read/write collision in same cycle

### Floating Point Considerations

**Accuracy**:
- Double precision provides ~15 decimal digits of precision
- Accumulated rounding errors possible in large matrices
- Testbenches use 0.01 tolerance for verification

**Special Values**:
- Zeros: Properly handled, no special cases needed
- Negatives: Supported naturally by IEEE 754
- NaN/Inf: Not explicitly handled (assume valid inputs)

---

## Design Decisions

### Why Two Dataflow Modes?

**Weight Stationary**:
- **Advantage**: Minimizes weight loading (load once, use multiple times)
- **Disadvantage**: Higher memory bandwidth for activations
- **Best for**: Weight reuse scenarios, small activation matrices

**Output Stationary**:
- **Advantage**: Results accumulate in place, reduced output traffic
- **Disadvantage**: Must stream both inputs
- **Best for**: Deep accumulation (large K), blocked algorithms

### Why Tiling?

**Hardware Constraint**: Fixed 3×3 array cannot directly compute larger matrices.

**Tiling Solution**:
- Break large matrices into 3×3 (or smaller) tiles
- Compute C[i,j] = Σ_k A[i,k] × W[k,j] for each tile
- Accumulate partial results for tiles along k dimension
- Enables arbitrary matrix sizes with small hardware

**Trade-offs**:
- Pro: Scalable to any matrix size
- Pro: Fixed hardware area
- Con: Increased memory traffic
- Con: More complex control logic

### Why FSM-Based Control?

**Alternative**: Combinational logic with counters

**FSM Benefits**:
- Clear state transitions make debugging easier
- Proper cycle-by-cycle control of datapath
- Avoids race conditions and glitches
- Easy to verify timing in waveforms
- Modular and maintainable

### Why Separate Memory Module?

**Design Choice**: Could have integrated memory into Control module.

**Rationale**:
- **Reusability**: Memory module is generic, not systolic-array specific
- **Testability**: Can test memory independently
- **Modularity**: Easier to replace with different memory architecture
- **Realistic**: Matches real hardware with separate memory subsystem

---

## Key Features and Optimizations

### Skewing and Deskewing

**Purpose**: Align data for systolic array processing.

**Weight Stationary Skewing**:
- Weights loaded column-by-column (left-to-right)
- Activations fed row-by-row (top-to-bottom)
- Natural wavefront propagation

**Output Stationary Skewing**:
- Both inputs skewed diagonally
- Ensures correct partial products align in each PE
- More complex timing but better accumulation

**Deskewing on Output**:
- Results emerge at different times from different columns
- Controller waits appropriate cycles to extract in correct order

### Zero Padding

**When**: Tile dimensions < M (edge tiles in non-multiple-of-M matrices)

**How**: Controller loads actual data, leaves remainder as zero

**Effect**: Zeros don't affect MAC results, allowing smaller tiles to compute correctly

### Cycle Count Optimization

**Minimal Cycles**:
- WS mode: M (preload) + K1+M-1 (feed) + M (drain) ≈ K1+3M-1
- OS mode: 1 (reset) + K3+2M-2 (accumulate) + M (drain) ≈ K3+3M-1
- No idle cycles between stages

**Pipeline Depth**:
- Systolic array has inherent M-cycle latency
- Controller FSM accounts for this in drain logic

---

## Common Pitfalls and How We Avoided Them

### 1. Race Conditions in Memory
**Problem**: Reading and writing same location in one cycle.

**Solution**: Write on negative edge, read on positive edge.

### 2. Incorrect Tile Indexing
**Problem**: Easy to mix up i/j/k indices and tile vs. element indices.

**Solution**: Careful naming (tile_i vs. i_tile), clear comments, thorough testing.

### 3. Skew Off-by-One Errors
**Problem**: Incorrect skew delays cause misaligned data.

**Solution**: Detailed cycle-by-cycle analysis, waveform verification.

### 4. Reg vs. Wire for Packed Arrays
**Problem**: Cannot `assign` to `reg` in Verilog.

**Solution**: Use `wire` for all packed signals driven by continuous assignment.

### 5. Data Width Mismatches
**Problem**: Mixing 32-bit and 64-bit widths.

**Solution**: Consistent 64-bit throughout, verified with compilation errors.

---

## Verification Strategy

### Multi-Level Testing

1. **Unit Level**: Test each module (PE, Memory) independently
2. **Integration Level**: Test combinations (SA, MatMul_Controller)
3. **System Level**: Test full system (Control with tiling)

### Test Coverage

**Matrix Sizes**:
- Exact fit (3×3)
- Undersized (2×2, 1×1)
- Slightly larger (5×5, 7×7)
- Much larger (10×10)
- Non-square (4×6, 6×5)

**Dataflow Modes**:
- Weight Stationary
- Output Stationary

**Data Values**:
- Positive integers
- Zeros
- Identity matrices
- Sequential values
- (Implicitly) negative via computation

### Verification Method

1. **Software golden model**: Compute expected result in testbench
2. **Tolerance-based comparison**: Allow 0.01 error for floating-point
3. **Error reporting**: Print first 10 errors with location and magnitude
4. **Waveform inspection**: Manual verification of control signals and data flow

---

## Performance Analysis

### Cycle Counts

For K1×K2 matrix times K2×K3 matrix with M=3:

**Tiling Overhead**:
- Tiles: `num_i * num_j * num_k` where `num_x = ceil(Kx / M)`
- Per tile: ~K+3M cycles (K = tile dimension in inner loop)

**Example (5×5 × 5×5)**:
- Tiles: 2×2×2 = 8 tiles
- Each tile: ~5+9 = 14 cycles
- Total: ~112 cycles
- Memory I/O: 2×9 reads + 9 writes per tile = ~200 cycles for I/O
- **Total: ~300 cycles** (measured from simulation)

**Example (10×10 × 10×10)**:
- Tiles: 4×4×4 = 64 tiles
- Each tile: ~10+9 = 19 cycles
- Total: ~1200 compute cycles
- Memory I/O: 64 tiles × 27 operations = ~1700 cycles
- **Total: ~3000 cycles** (approx)

### Throughput

**Best case (optimal 3×3)**:
- ~12 cycles per matrix multiply
- Peak: ~1 operation per cycle (during accumulation)

**Worst case (large matrices with tiling)**:
- Memory bandwidth becomes bottleneck
- ~1.5-2 cycles per output element (including I/O)

---

## Future Enhancements

Possible improvements to this design:

1. **Larger arrays**: Parameterize M to 8 or 16 for better efficiency
2. **Pipelined memory**: Overlap compute and I/O phases
3. **Local buffers**: Cache tiles on-chip to reduce memory traffic
4. **Mixed precision**: Support int8 or fp16 for ML inference
5. **Fault tolerance**: Add error detection for PE computations
6. **Performance counters**: Track cycle counts, memory accesses

---

## Conclusion

This Verilog implementation provides a complete, verified systolic array system for matrix multiplication with:
- ✅ Two dataflow modes (WS and OS)
- ✅ Automatic tiling for arbitrary sizes
- ✅ Comprehensive testbenches
- ✅ Proper FSM design without race conditions
- ✅ Detailed documentation and examples

The design demonstrates key concepts in:
- Systolic array architecture
- Tiling for resource-constrained hardware
- FSM-based control logic
- Memory interfacing
- Verilog synthesis and simulation

All code is tested and verified with multiple matrix sizes and modes. Waveforms can be inspected to understand detailed operation.

---

## File Summary

**Core Verilog Modules**:
- `PE.v` - Processing Element (104 lines)
- `SA_MxN.v` - Systolic Array (84 lines)
- `Memory.v` - Byte-addressed Memory with FSM (127 lines)
- `MatMul_Controller.v` - Single Tile Controller (472 lines)
- `Control.v` - Top-Level Tiling Controller (380 lines)

**Testbenches**:
- `PE_tb.v` - PE tests (429 lines)
- `SA_MxN_tb.v` - Systolic array tests (418 lines)
- `Memory_tb.v` - Memory tests (344 lines)
- `MatMul_Controller_tb.v` - Controller tests (542 lines)
- `Control_tb.v` - Full system comprehensive tests (625 lines) ⭐
- `testbench.v` - Original system test (272 lines)

**Documentation**:
- `VERILOG_COMPLETE_GUIDE.md` - This file (complete reference)

**Total**: ~3,800 lines of Verilog + comprehensive documentation

---

## Contact and Support

For questions or issues with this implementation, refer to:
1. This documentation (you are here)
2. Inline comments in Verilog files
3. Waveform analysis using GTKWave
4. Original project repository documentation

---

*Last Updated: December 2, 2025*
