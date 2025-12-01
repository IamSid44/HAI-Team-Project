#include "SAmxn.h" 
#include <iostream>
#include <iomanip> 
#include <cmath>   
#include <vector>

// =================================================================
// --- Test Configuration ---
// =================================================================
#ifndef K1_DIM
#define K1_DIM 5
#endif

#ifndef K2_DIM
#define K2_DIM 5
#endif

#ifndef K3_DIM
#define K3_DIM 5
#endif

// ** NEW ** Set the mode for this test run
// false = Weight Stationary
// true  = Output Stationary
const bool TEST_MODE_IS_OS = true; 

using namespace std;

SC_MODULE(SA_MxN_tb)
{
    // --- Test Data (1D flat arrays) ---
    float* A_mat;      // K1 x K2
    float* W_mat;      // K2 x K3
    float* C_mat_hw;   // K1 x K3 (Result from HW)
    float* C_mat_golden; // K1 x K3 (Result from SW)

    // Clock and reset
    sc_clock clk;
    sc_signal<bool> reset;

    // --- Controller I/O Signals ---
    sc_signal<bool> start;
    sc_signal<bool> done;

    // ** NEW ** Mode selection signal
    sc_signal<bool> sa_output_stationary_in;

    sc_signal<int> K1, K2, K3;
    sc_signal<float*> A_matrix_ptr;
    sc_signal<float*> W_matrix_ptr;
    sc_signal<float*> C_matrix_ptr;


    // DUT (Device Under Test) instance
    MatMul_Controller* controller;

    // Helper function to print a 1D-flat matrix
    void print_matrix(const char* name, float* matrix, int rows, int cols) {
        // ... (function is identical) ...
        cout << "--- " << name << " (" << rows << "x" << cols << ") ---" << endl;
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                cout << std::setw(8) << std::setprecision(2) << matrix[i * cols + j] << " ";
            }
            cout << endl;
        }
        cout << "------------------------------------" << endl;
    }

    // Helper to allocate and init a 1D matrix
    float* allocate_matrix(int rows, int cols, bool init_zeros = false) {
        // ... (function is identical) ...
        float* matrix = new float[rows * cols];
        if (init_zeros) {
            for (int i = 0; i < rows * cols; ++i) matrix[i] = 0.0f;
        }
        return matrix;
    }

    // --- Constructor ---
    SC_CTOR(SA_MxN_tb) : clk("clk", 10, SC_NS)
    {
        cout << "=== Testbench Configuration ===" << endl;
        cout << "Physical Grid: M=" << M << ", N=" << N << endl;
        cout << "Logical MatMul: K1=" << K1_DIM << ", K2=" << K2_DIM << ", K3=" << K3_DIM << endl;
        cout << "Test Mode: " << (TEST_MODE_IS_OS ? "Output Stationary" : "Weight Stationary") << endl;


        // --- 1. Allocate and Populate Test Matrices ---
        A_mat = allocate_matrix(K1_DIM, K2_DIM);
        W_mat = allocate_matrix(K2_DIM, K3_DIM);
        C_mat_hw = allocate_matrix(K1_DIM, K3_DIM, true); // Must be zero-initialized!
        C_mat_golden = allocate_matrix(K1_DIM, K3_DIM, true);

        // ... (matrix initialization is identical) ...
        for (int i = 0; i < K1_DIM; ++i)
            for (int j = 0; j < K2_DIM; ++j)
                A_mat[i * K2_DIM + j] = i * K2_DIM + j + 1;

        for (int i = 0; i < K2_DIM; ++i)
            for (int j = 0; j < K3_DIM; ++j)
                W_mat[i * K3_DIM + j] = (i==j) ? 1.0f : 0.0f; // Identity matrix
        
        cout << "\nCalculating Golden Model..." << endl;
        // --- 2. Calculate and Print Expected "Golden" Result ---
        for (int i = 0; i < K1_DIM; ++i) {
            for (int j = 0; j < K3_DIM; ++j) {
                for (int k = 0; k < K2_DIM; ++k) {
                    C_mat_golden[i * K3_DIM + j] += A_mat[i * K2_DIM + k] * W_mat[k * K3_DIM + j];
                }
            }
        }
        
        if (K1_DIM <= 10 && K2_DIM <= 10 && K3_DIM <= 10) {
            print_matrix("A_mat", A_mat, K1_DIM, K2_DIM);
            print_matrix("W_mat", W_mat, K2_DIM, K3_DIM);
            print_matrix("Expected Golden Result (C)", C_mat_golden, K1_DIM, K3_DIM);
        } else {
            cout << "Matrices are large, skipping print. Golden model is calculated." << endl;
        }


        // --- 3. Instantiate and Connect DUT ---
        controller = new MatMul_Controller("controller_inst");
        
        controller->clk(clk);
        controller->reset(reset);
        controller->start(start);
        controller->done(done);
        
        // ** NEW ** Connect the mode signal
        controller->sa_mode_is_output_stationary(sa_output_stationary_in);

        controller->A_matrix(A_matrix_ptr);
        controller->W_matrix(W_matrix_ptr);
        controller->C_matrix(C_matrix_ptr);
        
        controller->K1(K1);
        controller->K2(K2);
        controller->K3(K3);

        cout << "\nMatMul_Controller Testbench Created." << endl;
        SC_CTHREAD(testbench, clk); // MODIFIED: Make sensitive to pos() edge
    }

    // --- 4. Testbench Process (MODIFIED for optimal startup) ---
    void testbench()
    {
        cout << "Starting Testbench Process..." << endl;
        // --- Reset Phase (Optimized) ---
        reset.write(true);
        start.write(false);
        sa_output_stationary_in.write(false); 
        K1.write(0); K2.write(0); K3.write(0);
        A_matrix_ptr.write(nullptr);
        W_matrix_ptr.write(nullptr);
        C_matrix_ptr.write(nullptr);
        wait(); // MODIFIED: Was wait(2). 1 cycle is enough.
        
        reset.write(false);
        cout << "@" << sc_time_stamp() << ": Reset de-asserted. Setting up controller inputs." << endl;
        
        // --- Setup and Start (Optimized) ---
        A_matrix_ptr.write(A_mat);
        W_matrix_ptr.write(W_mat);
        C_matrix_ptr.write(C_mat_hw);
        
        K1.write(K1_DIM);
        K2.write(K2_DIM);
        K3.write(K3_DIM);
        
        sa_output_stationary_in.write(TEST_MODE_IS_OS);
        
        wait(); // Wait 1 cycle for controller to exit reset.

        cout << "@" << sc_time_stamp() << ": Asserting Start signal." << endl;
        start.write(true);
        wait(); // Hold start high for one cycle
        
        start.write(false);
        
        // --- Wait for Completion ---
        cout << "@" << sc_time_stamp() << ": Waiting for 'done' signal..." << endl;
        while(done.read() == false) {
            wait();
        }

        cout << "@" << sc_time_stamp() << ": 'done' signal received! Computation finished." << endl;
        // REMOVED: Unnecessary wait(5) cycles.

        // --- 5. Verify Results ---
        cout << "\n=== Verification Phase ===" << endl;
        if (K1_DIM <= 10 && K3_DIM <= 10) {
            print_matrix("Actual HW Result (C)", C_mat_hw, K1_DIM, K3_DIM);
        }

        bool pass = true;
        double max_error = 0.0;
        for (int i = 0; i < K1_DIM * K3_DIM; ++i) {
            double error = fabs(C_mat_golden[i] - C_mat_hw[i]);
            if (error > max_error) max_error = error;
            
            if (error > 1e-5) { 
                pass = false;
            }
        }
        
        cout << "------------------------------------" << endl;
        if (pass) {
            cout << "✅ TEST PASSED!" << endl;
        } else {
            cout << "❌ TEST FAILED!" << endl;
        }
        cout << "Max error: " << max_error << endl;
        cout << "------------------------------------" << endl;

        sc_stop();
    }

    // --- 6. Destructor ---
    ~SA_MxN_tb()
    {
        delete controller;
        delete[] A_mat;
        delete[] W_mat;
        delete[] C_mat_hw;
        delete[] C_mat_golden;
    }
};


// --- sc_main ---
int sc_main(int argc, char* argv[]) {
    // ... (sc_main remains unchanged, but I'll include it for completeness) ...
    const char* vcd_name = (argc > 1) ? argv[1] : "SA_tiling_tb";

    cout << "=================================================" << endl;
    cout << "=== SA_" << M << "x" << N << " Tiling Simulation ===" << endl;
    cout << "VCD Trace File: " << vcd_name << ".vcd" << endl;
    cout << "=================================================" << endl << endl;
    
    SA_MxN_tb system("SA_MxN_tb");

    sc_trace_file* tf = sc_create_vcd_trace_file(vcd_name);
    if (tf) {
        tf->set_time_unit(1, SC_NS);
        
        // --- Trace Top-Level Signals ---
        cout << "Tracing top-level signals..." << endl;
        sc_trace(tf, system.clk, "clk");
        sc_trace(tf, system.reset, "reset");
        sc_trace(tf, system.start, "start");
        sc_trace(tf, system.done, "done");
        sc_trace(tf, system.K1, "K1");
        sc_trace(tf, system.K2, "K2");
        sc_trace(tf, system.K3, "K3");
        
        // ** NEW ** Trace the mode signal
        sc_trace(tf, system.sa_output_stationary_in, "sa_output_stationary_in");


        // --- Trace Internal SA Grid I/O (Hierarchical Tracing) ---
        cout << "Tracing internal Systolic Array I/O..." << endl;
        
        sc_trace(tf, system.controller->sa_reset, "controller.sa_reset");
        
        // Trace the *input* to the controller
        sc_trace(tf, system.controller->sa_mode_is_output_stationary, "controller.sa_mode_is_output_stationary");
        // Trace the *output* of the controller (which is connected to the grid)
        // Note: This is now just a wire from the input, but tracing it at the grid level is useful
        sc_trace(tf, system.controller->sa_grid->output_stationary, "controller.sa_grid.output_stationary");
        
        sc_trace(tf, system.controller->sa_preload_valid, "controller.sa_preload_valid");

        for (int i = 0; i < N; i++) {
            string name = "controller.sa_in_top_" + std::to_string(i);
            sc_trace(tf, system.controller->sa_in_top[i], name);
            
            name = "controller.sa_out_bottom_" + std::to_string(i);
            sc_trace(tf, system.controller->sa_out_bottom[i], name);
        }
        for (int i = 0; i < M; i++) {
            string name = "controller.sa_in_left_" + std::to_string(i);
            sc_trace(tf, system.controller->sa_in_left[i], name);
            
            name = "controller.sa_out_right_" + std::to_string(i);
            sc_trace(tf, system.controller->sa_out_right[i], name);
        }
        
        // --- Trace all PE-level signals ---
        cout << "Tracing ALL PE internal signals..." << endl;
        for (int i = 0; i < M; ++i) {
            for (int j = 0; j < N; ++j) {
                string pe_name = "controller.sa_grid.PE_" 
                                 + to_string(i) + "_" + to_string(j);
                PE* pe = system.controller->sa_grid->pe_array[i][j];

                sc_trace(tf, pe->in_top, pe_name + ".in_top");
                sc_trace(tf, pe->in_left, pe_name + ".in_left");
                sc_trace(tf, pe->out_right, pe_name + ".out_right");
                sc_trace(tf, pe->out_bottom, pe_name + ".out_bottom");
                sc_trace(tf, pe->output_stationary, pe_name + ".output_stationary");
                sc_trace(tf, pe->preload_valid, pe_name + ".preload_valid");
                sc_trace(tf, pe->preload_data, pe_name + ".preload_data");
                sc_trace(tf, pe->preload_buffer, pe_name + ".preload_buffer");
                sc_trace(tf, pe->accumulator_buffer, pe_name + ".accumulator_buffer");
                sc_trace(tf, pe->right_out_buffer, pe_name + ".right_out_buffer");
                sc_trace(tf, pe->bottom_out_buffer, pe_name + ".bottom_out_buffer");
            }
        }

    } else {
        cout << "Error: Could not create VCD trace file." << endl;
    }

    cout << "\nStarting sc_start..." << endl << endl;
    sc_start(); // Run until sc_stop()
    cout << "\n=== Simulation Results ===" << endl;
    cout << "Simulation completed at time: " << sc_time_stamp() << endl;
    
    if (tf) {
        sc_close_vcd_trace_file(tf);
    }
    return 0;
}