#include "SAmxn.h" // Include the new generalized header
#include <iostream>
#include <iomanip> // For formatting output

// Define the inner dimension for matrix multiplication
// C[M][N] = A[M][K_DIM] * W[K_DIM][N]
// This will be overridden by compiler flags, but a default is good.
#ifndef K_DIM
#define K_DIM 3
#endif

using namespace std;

SC_MODULE(SA_MxN_tb)
{
    // --- Test Data ---
    // These are now pointers and will be allocated dynamically
    float** A_os; // M x K_DIM
    float** W_os; // K_DIM x N
    
    float** A_ws; // M x M (based on original tb logic)
    float** W_ws; // M x N (based on original tb logic)

    // Clock and reset
    sc_clock clk;
    sc_signal<bool> reset;

    // Control signals
    sc_signal<bool> output_stationary;
    sc_signal<bool> preload_valid;

    // IO signals
    sc_signal<float> in_top[N];
    sc_signal<float> in_left[M];
    sc_signal<float> out_right[M];
    sc_signal<float> out_bottom[N];
    sc_signal<float> preload_data[M * N];

    // DUT (Device Under Test) instance
    SA_MxN* sa_mxn;

    // Helper function to print a matrix
    void print_matrix(const char* name, float** matrix, int rows, int cols) {
        cout << "--- " << name << " (" << rows << "x" << cols << ") ---" << endl;
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                cout << std::setw(6) << matrix[i][j] << " ";
            }
            cout << endl;
        }
        cout << "-------------------------" << endl;
    }

    // Helper to allocate a 2D matrix
    float** allocate_matrix(int rows, int cols) {
        float** matrix = new float*[rows];
        for (int i = 0; i < rows; ++i) {
            matrix[i] = new float[cols];
        }
        return matrix;
    }

    // Helper to delete a 2D matrix
    void delete_matrix(float** matrix, int rows) {
        for (int i = 0; i < rows; ++i) {
            delete[] matrix[i];
        }
        delete[] matrix;
    }


    // Constructor
    SC_CTOR(SA_MxN_tb) : clk("clk", 10, SC_NS)
    {
        // --- 1. Allocate and Populate Test Matrices ---
        
        // Output Stationary test (A[M][K_DIM] * W[K_DIM][N])
        A_os = allocate_matrix(M, K_DIM);
        W_os = allocate_matrix(K_DIM, N);
        for(int i=0; i<M; ++i)
            for(int j=0; j<K_DIM; ++j)
                A_os[i][j] = i*K_DIM + j + 1; // Simple init: 1, 2, 3...
        
        for(int i=0; i<K_DIM; ++i)
            for(int j=0; j<N; ++j)
                W_os[i][j] = i*N + j + 1; // Simple init: 1, 2, 3...

        // Weight Stationary test (A[M][K_DIM] * W[K_DIM][N])
        A_ws = allocate_matrix(M, K_DIM);
        W_ws = allocate_matrix(K_DIM, N);
        for(int i=0; i<M; ++i)
            for(int j=0; j<K_DIM; ++j)
                A_ws[i][j] = i*K_DIM + j + 1; 

        for(int i=0; i<K_DIM; ++i)
            for(int j=0; j<N; ++j)
                W_ws[i][j] = i*N + j + 1;

        // --- 2. Calculate and Print Expected "Golden" Results ---
        cout << "=== Testbench Configuration ===" << endl;
        cout << "M=" << M << ", N=" << N << ", K_DIM=" << K_DIM << endl;
        
        // Golden model for WS Test: C_ws[M][N] = A_ws[M][K_DIM] * W_ws[K_DIM][N]
        float** C_ws = allocate_matrix(M, N);
        for(int i=0; i<M; ++i) {
            for(int j=0; j<N; ++j) {
                C_ws[i][j] = 0;
                for(int k=0; k<K_DIM; ++k) { // Inner dim is K_DIM
                    C_ws[i][j] += A_ws[i][k] * W_ws[k][j];
                }
            }
        }
        print_matrix("Expected WS Result (A_ws*W_ws)", C_ws, M, N);

        // Golden model for OS Test: C_os[M][N] = A_os[M][K_DIM] * W_os[K_DIM][N]
        float** C_os = allocate_matrix(M, N);
        for(int i=0; i<M; ++i) {
            for(int j=0; j<N; ++j) {
                C_os[i][j] = 0;
                for(int k=0; k<K_DIM; ++k) { // Inner dim is K_DIM
                    C_os[i][j] += A_os[i][k] * W_os[k][j];
                }
            }
        }
        print_matrix("Expected OS Result (A_os*W_os)", C_os, M, N);
        
        // (Clean up golden models, not needed anymore)
        delete_matrix(C_ws, M);
        delete_matrix(C_os, M);

        // --- 3. Instantiate and Connect DUT ---
        sa_mxn = new SA_MxN("SA_MxN", 1);
        sa_mxn->clk(clk);
        sa_mxn->reset(reset);
        sa_mxn->output_stationary(output_stationary);

        for (int i = 0; i < N; i++) {
            sa_mxn->in_top[i](in_top[i]);
            sa_mxn->out_bottom[i](out_bottom[i]);
        }
        for (int i = 0; i < M; i++) {
            sa_mxn->in_left[i](in_left[i]);
            sa_mxn->out_right[i](out_right[i]);
        }
        sa_mxn->preload_valid(preload_valid);
        for (int i = 0; i < M * N; i++) {
            sa_mxn->preload_data[i](preload_data[i]);
        }

        cout << "SA_" << M << "x" << N << " Testbench Created" << endl;
        SC_CTHREAD(testbench, clk);
    }

    // --- 4. Testbench Process ---
    void testbench()
    {
        cout << "Starting Testbench Process..." << endl;
        reset.write(true);
        wait();
        reset.write(false);
        wait();

        // --- Test 1: Weight Stationary Dataflow ---
        cout << "Starting Weight Stationary Test..." << endl;
        preload_valid.write(true);
        output_stationary.write(false);
        for(int i = 0; i < M; i++)
        {
            for(int j = 0; j < N; j++)
            {
                if (i < K_DIM) 
                {
                    preload_data[i * N + j].write(W_ws[i][j]);
                }
                else preload_data[i * N + j].write(0);
            }
        }
        wait();
        preload_valid.write(false);
        int ws_clks = (M + N + K_DIM - 1); // Time to feed A[M][K_DIM]
        for(int clks = 0; clks < ws_clks; clks++) 
        {
            // Turning off inactive PEs
            for(int i = 0; i < K_DIM; i++) {
                if(clks - i >= 0 && clks - i < M)
                    in_left[i].write(A_ws[clks - i][i]);
                else
                    in_left[i].write(0);
            }
            for(int j = 0; j < N; j++) {
                in_top[j].write(0);
            }
            wait();
        }
        cout << "Weight stationary test complete!" << endl;
        wait(5);

        // --- Test 2: Output Stationary Dataflow ---
        cout << "Starting Output Stationary Test..." << endl;
        reset.write(true);
        wait();
        reset.write(false);
        wait();
        preload_valid.write(true);
        output_stationary.write(true);
        for(int i = 0; i < M * N; i++)
            preload_data[i].write(0);
        wait();
        preload_valid.write(false);
        int temp;
        if (M > N)
            temp = M;
        else
            temp = N;
        int os_clks = (2*temp + K_DIM - 1);
        for(int clks = 0; clks < os_clks; clks++)
        {
            for(int i = 0; i < M; i++) {
                if(clks - i >= 0 && clks - i < K_DIM)
                    in_left[i].write(A_os[i][clks - i]);
                else
                    in_left[i].write(0);
            }
            for(int j = 0; j < N; j++) {
                if(clks - j >= 0 && clks - j < K_DIM)
                    in_top[j].write(W_os[clks - j][j]);
                else
                    in_top[j].write(0);
            }
            wait();
        }
        wait(5); 
        cout << "Output stationary test complete!" << endl;
        sc_stop();
    }

    // --- 5. Destructor ---
    ~SA_MxN_tb()
    {
        delete sa_mxn;
        // Clean up matrices
        delete_matrix(A_os, M);
        delete_matrix(W_os, K_DIM);
        delete_matrix(A_ws, M);
        delete_matrix(W_ws, K_DIM);
    }
};


// --- sc_main ---
int sc_main(int argc, char* argv[]) {
    // Determine VCD filename from command-line argument
    const char* vcd_name = (argc > 1) ? argv[1] : "SA_default";

    cout << "=== SA_" << M << "x" << N << " Simulation ===" << endl;
    cout << "Config: M=" << M << ", N=" << N << ", K_DIM=" << K_DIM << endl;
    cout << "VCD Trace File: " << vcd_name << endl << endl;
    
    SA_MxN_tb system("SA_MxN_tb");

    sc_trace_file* tf = sc_create_vcd_trace_file(vcd_name);
    if (tf) {
        tf->set_time_unit(1, SC_NS);
        sc_trace(tf, system.clk, "clk");
        sc_trace(tf, system.reset, "reset");
        sc_trace(tf, system.output_stationary, "output_stationary");
        sc_trace(tf, system.preload_valid, "preload_valid");
        for (int i = 0; i < M * N; i++) {
            sc_trace(tf, system.preload_data[i], "preload_data_" + std::to_string(i));
        }
        for (int i = 0; i < N; i++) {
            sc_trace(tf, system.in_top[i], "in_top_" + std::to_string(i));
            sc_trace(tf, system.out_bottom[i], "out_bottom_" + std::to_string(i));
        }
        for (int i = 0; i < M; i++) {
            sc_trace(tf, system.in_left[i], "in_left_" + std::to_string(i));
            sc_trace(tf, system.out_right[i], "out_right_" + std::to_string(i));
        }
    } else {
        cout << "Error: Could not create VCD trace file." << endl;
    }

    cout << "Starting sc_start..." << endl << endl;
    sc_start(2000, SC_NS); // Run for 2000 ns
    cout << "\n=== Simulation Results ===" << endl;
    cout << "Simulation completed at time: " << sc_time_stamp() << endl;
    
    if (tf) {
        sc_close_vcd_trace_file(tf);
    }
    return 0;
}

