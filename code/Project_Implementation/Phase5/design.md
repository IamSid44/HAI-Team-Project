# Memory-Backed Systolic Array: Design Documentation

## System Overview

This implementation integrates a systolic array matrix multiplication accelerator with a byte-addressed memory system. The design is clock-cycle accurate and follows a straightforward fetch-compute-writeback architecture without complex FSMs.

## Architecture

```
┌─────────────────────────────────────────────────────┐
│         MemoryBackedController (Control.h)          │
│  ┌──────────────────┐       ┌──────────────────┐   │
│  │     Memory       │       │  MatMul_Controller│  │
│  │   (Memory.h)     │◄─────►│   (SAmxn.h)       │  │
│  │                  │       │                    │  │
│  │ - Byte-addressed │       │ - PE Grid (MxN)   │  │
│  │ - File-backed    │       │ - WS/OS modes     │  │
│  │ - 1-cycle latency│       │ - Tiled execution │  │
│  └──────────────────┘       └──────────────────┘   │
└─────────────────────────────────────────────────────┘
```

## Component Details

### 1. Memory Module (Memory.h)

**Key Features:**
- **Byte-addressed**: Each float occupies 4 bytes
- **File-backed storage**: `memory.txt` stores all memory contents
- **XXXX convention**: Uninitialized locations show "XXXX"
- **1-cycle latency**: Both reads and writes complete in one clock cycle

**Memory File Format:**
```
# Byte-Addressed Memory File
00000000: XXXX          # Uninitialized
00000004: 1.234567      # Initialized float at byte address 4
00000008: 2.345678      # Initialized float at byte address 8
```

**Interface:**
- `read_enable`: Initiates read operation
- `write_enable`: Initiates write operation
- `address`: Byte address (must be 4-byte aligned)
- `write_data`: Data to write
- `read_data`: Data read from memory
- `ready`: Operation complete signal

**Design Rationale:**
- File-backed storage provides persistence and easy inspection
- Single-cycle operations keep the design simple
- Byte addressing matches real hardware memory systems

### 2. Memory-Backed Controller (Control.h)

**Operation Sequence (Clock-Cycle Accurate):**

1. **Phase 1 - Read Matrix A**: 
   - Sequential reads from memory (K1 × K2 cycles)
   - Each element: address → wait(1) → capture data

2. **Phase 2 - Read Matrix W**:
   - Sequential reads from memory (K2 × K3 cycles)
   - Same single-cycle read pattern

3. **Phase 3 - Compute**:
   - Pass matrices to systolic array controller
   - Wait for `matmul_done` signal
   - Array performs tiled computation internally

4. **Phase 4 - Write Matrix C**:
   - Sequential writes to memory (K1 × K3 cycles)
   - Each element: address + data → wait(1) → write complete

**Total Cycles:**
```
Total = (K1 × K2) + (K2 × K3) + [Systolic Array Cycles] + (K1 × K3)
```

**Design Rationale:**
- No FSM needed - purely sequential control flow
- Simple to understand and verify
- Clock-cycle accurate by construction
- Memory bandwidth limited by single-cycle sequential access

### 3. MatMul Controller Integration (SAmxn.h)

The existing `MatMul_Controller` is used without modification:
- Handles tiled execution for large matrices
- Supports both WS (Weight Stationary) and OS (Output Stationary) modes
- Manages PE grid data flow and accumulation

**No changes needed** because:
- Controller already works with in-memory matrices
- Memory fetch/writeback happens before/after computation
- Clean separation between memory access and computation

## Memory Layout

**Recommended Layout:**
```
Address Range     | Content
------------------|---------------------------------
0x0000 - 0x0FFF   | Matrix A (up to 1024 floats)
0x1000 - 0x1FFF   | Matrix W (up to 1024 floats)
0x2000 - 0x2FFF   | Matrix C (up to 1024 floats)
0x3000+           | Free space
```

**Alignment:** All matrix base addresses should be 4-byte aligned (multiples of 4).

## Clock Cycle Accounting

### Example: 4×4 matrices in WS mode

**Phase 1 - Read A (4×4 = 16 elements):** 16 cycles
**Phase 2 - Read W (4×4 = 16 elements):** 16 cycles
**Phase 3 - Compute:** ~50-100 cycles (depends on tiling/dataflow)
**Phase 4 - Write C (4×4 = 16 elements):** 16 cycles

**Total:** ~98-130 cycles

## Testing Strategy

The testbench (`testbench.cpp`) validates:

1. **TEST 1 - WS Mode with Identity Matrix:**
   - A = 4×4 sequential matrix
   - W = 4×4 identity matrix
   - Expected: C = A
   - Verifies: WS dataflow, memory read/write, basic correctness

2. **TEST 2 - OS Mode with Scalar Multiplication:**
   - A = 2×I (scaled identity)
   - W = 4×4 sequential matrix
   - Expected: C = 2×W
   - Verifies: OS dataflow, different computation pattern

**Verification:**
- Element-wise comparison with tolerance (0.001)
- Memory inspection via file output
- PE-level logging in `PE_Outputs/` directory

## Compilation and Execution

```bash
# Compile
g++ testbench.cpp -lsystemc -o tb

# Run
./tb

# Inspect results
cat memory.txt                    # View memory contents
cat PE_Outputs/PE_1_0_0.txt      # View PE[0][0] activity
```

## Key Design Decisions

1. **File-backed memory**: Allows easy inspection and debugging
2. **Sequential memory access**: Simplifies control logic, acceptable for small matrices
3. **1-cycle memory latency**: Realistic for on-chip SRAM, keeps design simple
4. **No controller modifications**: Reuses proven MatMul_Controller unchanged
5. **Byte addressing**: Standard convention, future-proof
6. **XXXX for uninitialized**: Clear visual indicator in memory dumps

## Limitations and Future Enhancements

**Current Limitations:**
- Sequential memory access (no burst/parallel access)
- File I/O has overhead (slower than real SRAM)
- No memory caching
- No address bounds checking

**Possible Enhancements:**
- Burst memory transfers for matrix blocks
- Parallel memory banks for simultaneous access
- DMA controller for autonomous transfers
- Address space protection and bounds checking
- Support for multiple concurrent operations

## Performance Characteristics

For an N×N matrix multiplication on an M×M PE array:

**Memory Bandwidth:**
- Required: 2N² reads + N² writes = 3N² accesses
- Provided: 1 access/cycle
- Bottleneck: Memory bandwidth for small matrices

**Computation:**
- PE utilization depends on tiling efficiency
- Best case: N/M tiles, each ~O(N) cycles
- Worst case: Significant startup/drain overhead for small tiles

**Optimal Use Case:**
- Matrices that fit in single tile (≤64×64 with M=N=64)
- Minimizes tiling overhead and maximizes PE utilization