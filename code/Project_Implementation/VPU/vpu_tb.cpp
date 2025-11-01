#ifndef TB_H
#define TB_H

#include "systemc.h"
#include "vpu.h"
#include <iostream>
#include <iomanip> // For std::setprecision

// The testbench is also templated to match the VPU's vector size
template <unsigned int VECTOR_SIZE>
SC_MODULE(Testbench) {

    // --- DUT Instance ---
    VectorProcessingUnit<VECTOR_SIZE>* vpu_inst;

    // --- Signals for DUT Connection ---
    sc_clock clk;
    sc_signal<bool> rst_n;
    sc_signal<bool> start;
    sc_signal<bool> done;
    sc_signal<sc_uint<1>> op_sel;

    // sc_vector of signals
    sc_vector<sc_signal<double>> vec_in_scores_sig;
    sc_vector<sc_signal<double>> vec_in_values_sig;
    sc_vector<sc_signal<double>> vec_out_norm_sig;
    sc_signal<double> scalar_out_softmax_sig;

    // --- Testbench Process ---
    void stimulus_thread() {
        // --- Reset Phase ---
        std::cout << "@ " << sc_time_stamp() << " Asserting reset" << std::endl;
        rst_n.write(false);
        start.write(false);
        op_sel.write(0);
        for (unsigned int i = 0; i < VECTOR_SIZE; i++) {
            vec_in_scores_sig[i].write(0.0);
            vec_in_values_sig[i].write(0.0);
        }
        wait(25, SC_NS); // Hold reset for 2.5 cycles
        rst_n.write(true);
        std::cout << "@ " << sc_time_stamp() << " De-asserting reset" << std::endl;
        wait(10, SC_NS);

        // --- Test Case 1: L2 Normalization ---
        // (This test case is the same as before)
        double test_vec_1[VECTOR_SIZE] = {1.0, 2.0, 3.0, 4.0};
        
        std::cout << "--- Test Case 1: L2 Normalization ---" << std::endl;
        std::cout << "Input Scores: [ ";
        for(unsigned int i = 0; i < VECTOR_SIZE; i++) {
            vec_in_scores_sig[i].write(test_vec_1[i]);
            std::cout << test_vec_1[i] << " ";
        }
        std::cout << "]" << std::endl;

        op_sel.write(0);  // Select Normalization
        start.write(true); 
        wait(clk.posedge_event()); 
        start.write(false); 

        while (done.read() == false) {
            wait(clk.posedge_event());
        }

        std::cout << "Output Norm:  [ ";
        std::cout << std::fixed << std::setprecision(4); // Format output
        for(unsigned int i = 0; i < VECTOR_SIZE; i++) {
            std::cout << vec_out_norm_sig[i].read() << " ";
        }
        std::cout << "]" << std::endl;
        std::cout << "Expected:     [ 0.1826 0.3651 0.5477 0.7303 ]" << std::endl;
        std::cout << "---------------------------------------" << std::endl;
        wait(20, SC_NS);


        // --- Test Case 2: Softmax Weighted Sum ---
        double test_scores_2[VECTOR_SIZE] = {1.0, 2.0, 0.5, -1.0};
        double test_values_2[VECTOR_SIZE] = {0.1, 0.2, 0.3, 0.4};
        
        // --- Manual Calculation for Verification ---
        // Softmax(scores) = [0.2241, 0.6095, 0.1360, 0.0303]
        // O = (0.2241*0.1) + (0.6095*0.2) + (0.1360*0.3) + (0.0303*0.4)
        // O = 0.02241 + 0.1219 + 0.0408 + 0.01212
        // O = 0.19723
        
        std::cout << "--- Test Case 2: Softmax Weighted Sum ---" << std::endl;
        std::cout << "Input Scores: [ ";
        for(unsigned int i = 0; i < VECTOR_SIZE; i++) {
            vec_in_scores_sig[i].write(test_scores_2[i]);
            std::cout << test_scores_2[i] << " ";
        }
        std::cout << "]" << std::endl;
        std::cout << "Input Values: [ ";
        for(unsigned int i = 0; i < VECTOR_SIZE; i++) {
            vec_in_values_sig[i].write(test_values_2[i]);
            std::cout << test_values_2[i] << " ";
        }
        std::cout << "]" << std::endl;


        op_sel.write(1);  // Select Softmax-Sum
        start.write(true); 
        wait(clk.posedge_event());
        start.write(false);

        while (done.read() == false) {
            wait(clk.posedge_event());
        }

        std::cout << "Output Sum:   " << scalar_out_softmax_sig.read() << std::endl;
        std::cout << "Expected:     0.1972" << std::endl;
        std::cout << "---------------------------------------" << std::endl;
        wait(20, SC_NS);

        // --- End Simulation ---
        std::cout << "@ " << sc_time_stamp() << " Testbench finished" << std::endl;
        sc_stop();
    }

    // --- Constructor ---
    SC_CTOR(Testbench) : clk("clk", 10, SC_NS) { // 10ns clock period
        
        // Initialize signal vectors
        vec_in_scores_sig.init(VECTOR_SIZE);
        vec_in_values_sig.init(VECTOR_SIZE);
        vec_out_norm_sig.init(VECTOR_SIZE);

        // Instantiate the VPU
        vpu_inst = new VectorProcessingUnit<VECTOR_SIZE>("vpu_inst");

        // Connect VPU ports to testbench signals
        vpu_inst->clk(clk);
        vpu_inst->rst_n(rst_n);
        vpu_inst->start(start);
        vpu_inst->done(done);
        vpu_inst->op_sel(op_sel);
        
        // Connect the new/renamed I/O ports
        vpu_inst->vec_in_scores(vec_in_scores_sig);
        vpu_inst->vec_in_values(vec_in_values_sig);
        vpu_inst->vec_out_norm(vec_out_norm_sig);
        vpu_inst->scalar_out_softmax(scalar_out_softmax_sig);


        // Register the stimulus thread
        SC_THREAD(stimulus_thread);
    }

    // Destructor to clean up
    ~Testbench() {
        delete vpu_inst;
    }
};

#endif // TB_H