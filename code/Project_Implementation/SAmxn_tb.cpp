#include "SAmxn.h" // Include the new generalized header
#include <iostream>

// Define the inner dimension for matrix multiplication
// C[M][N] = A[M][K_DIM] * W[K_DIM][N]
#define K_DIM 3

using namespace std;

SC_MODULE(SA_MxN_tb)
{
    // --- Test Data ---
    // Note: These are initialized for M=3, K_DIM=3, N=3 for demonstration.
    // For other M, N, K_DIM values, you'll need to update these initializers.

    // Matrix A (M x K_DIM) for Output Stationary
    // We will use this for the standard matrix multiplication C = A * W
    float A_os[M][K_DIM] = {
        {1.0, 2.0, 3.0},
        {4.0, 5.0, 6.0},
        {7.0, 8.0, 9.0}
    };

    // Matrix W (K_DIM x N) for Output Stationary
    float W_os[K_DIM][N] = {
        {1.0, 2.0, 3.0},
        {4.0, 5.0, 6.0},
        {7.0, 8.0, 9.0}
    };

    // --- Data for Weight Stationary Test ---
    // The original testbench's WS logic was specific.
    // It used an A[M][M] matrix and preloaded a W[M][N] matrix.
    // We replicate that behavior here.
    float A_ws[M][M] = {
        {1.0, 2.0, 3.0},
        {4.0, 5.0, 6.0},
        {7.0, 8.0, 9.0}
    };

    float W_ws[M][N] = {
        {1.0, 2.0, 3.0},
        {4.0, 5.0, 6.0},
        {7.0, 8.0, 9.0}
    };


    // Clock and reset
    sc_clock clk;
    sc_signal<bool> reset;

    // Check if its output stationary or weight/input stationary
    sc_signal<bool> output_stationary;

    // Input/Output signals sized with M and N
    sc_signal<float> in_top[N];
    sc_signal<float> in_left[M];
    sc_signal<float> out_right[M];
    sc_signal<float> out_bottom[N];

    // Preloading buffer and signal
    sc_signal<bool> preload_valid;
    sc_signal<float> preload_data[M * N]; // Sized M * N

    // DUT (Device Under Test) instance
    SA_MxN* sa_mxn;

    // Constructor
    SC_CTOR(SA_MxN_tb) : clk("clk", 10, SC_NS) // 10ns clock period
    {
        // Instantiate SA_MxN
        sa_mxn = new SA_MxN("SA_MxN", 1); // array_id = 1
        sa_mxn->clk(clk);
        sa_mxn->reset(reset);
        sa_mxn->output_stationary(output_stationary);

        // Connect top/bottom ports (N ports)
        for (int i = 0; i < N; i++) {
            sa_mxn->in_top[i](in_top[i]);
            sa_mxn->out_bottom[i](out_bottom[i]);
        }
        
        // Connect left/right ports (M ports)
        for (int i = 0; i < M; i++) {
            sa_mxn->in_left[i](in_left[i]);
            sa_mxn->out_right[i](out_right[i]);
        }

        // Connect preload ports (M * N ports)
        sa_mxn->preload_valid(preload_valid);
        for (int i = 0; i < M * N; i++) {
            sa_mxn->preload_data[i](preload_data[i]);
        }

        cout << "SA_" << M << "x" << N << " Testbench Created" << endl;

        // Start the testbench thread
        SC_CTHREAD(testbench, clk);
    }

    void testbench()
    {
        cout << "Starting Testbench..." << endl;
        // First, reset the system
        reset.write(true);
        wait();
        reset.write(false);
        wait();

        // --- Test 1: Weight Stationary Dataflow ---
        // Replicating the logic from the original SA3x3 testbench.
        // This specific test preloads W_ws[M][N] and streams A_ws[M][M].
        cout << "Starting Weight Stationary Test..." << endl;
        
        preload_valid.write(true);
        output_stationary.write(false); // Weight stationary
        
        // Preload matrix W_ws[M][N]
        for(int i = 0; i < M; i++)
        {
            for(int j = 0; j < N; j++)
            {   
                preload_data[i * N + j].write(W_ws[i][j]);
            }
        }
        wait();

        preload_valid.write(false);
        
        // Calculate clocks needed: (M-1) + (M-1) + 1 = 2*M - 1
        // We run for a few extra cycles to flush.
        int ws_clks = (M + M - 1); 
        for(int clks = 0; clks < ws_clks + 5; clks++)
        {
            // Feed A_ws[M][M] from the left, skewed by columns
            for(int i = 0; i < M; i++)
            {
                // i is the row of the SA, but used as the *column* index for A_ws
                if(clks - i >= 0 && clks - i < M && i < M)
                    in_left[i].write(A_ws[clks - i][i]);
                else
                    in_left[i].write(0);
            }
            
            // 0s are passed from the top
            for(int j = 0; j < N; j++)
            {
                in_top[j].write(0); // Always 0s from the top
            }

            wait();
        }

        cout << "Weight stationary dataflow test complete!" << endl;
        wait(5); // Wait a few cycles

        // --- Test 2: Output Stationary Dataflow ---
        // This test implements standard C[M][N] = A[M][K_DIM] * W[K_DIM][N]
        
        cout << "Starting Output Stationary Test..." << endl;

        // Reset the system
        reset.write(true);
        wait();
        reset.write(false);
        wait();

        preload_valid.write(true);
        output_stationary.write(true); // Output stationary

        // Preload all 0s
        for(int i = 0; i < M * N; i++)
        {
            preload_data[i].write(0);
        }
        wait();

        preload_valid.write(false);

        // Clocks needed to feed both matrices: M + K_DIM + N - 2
        // We run for extra cycles to let the computation finish.
        int os_clks = (M + K_DIM + N - 2);
        for(int clks = 0; clks < os_clks + 5; clks++)
        {
            // Feed A_os[M][K_DIM] from the left, skewed by rows
            for(int i = 0; i < M; i++)
            {
                // i is the SA row, and the row of A_os
                // (clks - i) is the column of A_os
                if(clks - i >= 0 && clks - i < K_DIM)
                    in_left[i].write(A_os[i][clks - i]);
                else
                    in_left[i].write(0);
            }
            
            // Feed W_os[K_DIM][N] from the top, skewed by columns
            for(int j = 0; j < N; j++)
            {
                // j is the SA column, and the column of W_os
                // (clks - j) is the row of W_os
                if(clks - j >= 0 && clks - j < K_DIM)
                    in_top[j].write(W_os[clks - j][j]);
                else
                    in_top[j].write(0);
            }
            wait();
        }

        wait(5); // Wait for results to settle
        cout << "Output stationary dataflow matrix multiplication complete!" << endl;

        sc_stop(); // End simulation
    }

    ~SA_MxN_tb()
    {
        delete sa_mxn;
    }
};


// sc_main function
int sc_main(int argc, char* argv[]) {
    // Check if M and N are defined, default if not
    // This is just for the print message
    #ifndef M
    #define M 3
    #endif
    #ifndef N
    #define N 3
    #endif

    cout << "=== SA_" << M << "x" << N << " Simulation ===" << endl;
    cout << M << "x" << N << " Matrix Multiplication (with K=" << K_DIM << ")" << endl << endl;
    
    // Create top level system
    SA_MxN_tb system("SA_MxN_tb");

    // Create VCD (Value Change Dump) trace file
    sc_trace_file* tf = sc_create_vcd_trace_file("SA_MxN");
    if (tf) {
        tf->set_time_unit(1, SC_NS); // 1ns time resolution
        
        // Trace all important signals
        sc_trace(tf, system.clk, "clk");
        sc_trace(tf, system.reset, "reset");
        sc_trace(tf, system.output_stationary, "output_stationary");
        sc_trace(tf, system.preload_valid, "preload_valid");

        // Trace top/bottom ports
        for (int i = 0; i < N; i++)
        {
            sc_trace(tf, system.in_top[i], "in_top_" + std::to_string(i));
            sc_trace(tf, system.out_bottom[i], "out_bottom_" + std::to_string(i));
        }

        // Trace left/right ports
        for (int i = 0; i < M; i++)
        {
            sc_trace(tf, system.in_left[i], "in_left_" + std::to_string(i));
            sc_trace(tf, system.out_right[i], "out_right_" + std::to_string(i));
        }

        // Trace preload data
        for (int i = 0; i < M * N; i++) 
        {
            sc_trace(tf, system.preload_data[i], "preload_data_" + std::to_string(i));
        }
    } else {
        cout << "Error: Could not create VCD trace file." << endl;
    }


    cout << "Starting simulation..." << endl << endl;
    
    sc_start(1000, SC_NS); // Run for 1000 ns
    
    cout << "\n=== Simulation Results ===" << endl;
    cout << "Simulation completed at time: " << sc_time_stamp() << endl;
    
    // Close trace file
    if (tf) {
        sc_close_vcd_trace_file(tf);
    }
    
    return 0;
}
