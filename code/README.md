# Project Progress

- Phase 2 has a working implementation of a 3x3 systolic array for both OS and WS dataflows.
- Phase 3 extends it to an mxn systolic array with configurable size.
- Phase 4 implements tiling 
- Phase 5 implements a memory with OS and WS. The buffer is of a variable size.
- Phase 6 implements a buffer to be of the same size as the systolic array. tiles are fetched from larger memory accordingly.
- Phase 7 implements an MxN array to do the same. (Up until now, memory was integrated only to work with symmetric SAs)
- Phase 8 brings tiles of size MxN to work with. (Up until now, tiles were only of size MxM and the PEs were just idle)
- Phase 9 improves on Phase 6 by implementing data-reuse (prior to this each matrix for a multiplication was fetched from memory every time).
- Phase 10 is just a combination of Phases 8 and 9, bringing data-reuse to MxN arrays with MxN tiles.
