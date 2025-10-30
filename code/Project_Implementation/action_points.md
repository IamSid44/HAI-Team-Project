# PE.h

- The PE is reconfigurable to different dataflows.
- Latency of FPU
    - We will need to implement a handshake if we want to pipeline the FP operations to prevent dataloss or data corruption.
- Not implementing memory and instructions for specific units as it will slow down the overall progress a lot. Tests will be done purely based on the testbenches only. Will implement the memory and instruction read blocks on the overall architecture.

# SA3x3.h

- For preloading, the data is currently broadcsted but ideally it must flow through the columns or rows to reduce interconnect complexity.
- Figure out a way to efficiently check accumulator values (printing is getting confusing already).