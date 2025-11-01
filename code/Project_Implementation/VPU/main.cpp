#include "systemc.h"
#include "vpu.h"
#include "vpu_tb.cpp"

// --- Define the vector size for this simulation ---
// This constant is used to instantiate the templated modules.
const unsigned int VECTOR_DIMENSION = 4;


int sc_main(int argc, char* argv[]) {
    // Instantiate the testbench
    Testbench<VECTOR_DIMENSION> tb_inst("tb_inst");

    // Start the simulation
    std::cout << "Starting SystemC simulation..." << std::endl;
    sc_start();
    std::cout << "SystemC simulation finished." << std::endl;

    return 0;
}