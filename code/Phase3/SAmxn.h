#ifndef SA_MXN_H
#define SA_MXN_H

#include "PE.h"
#include <string>
#include <vector>     // Added for internal C_tile accumulator
#include <algorithm>  // Added for std::fill

// Define the dimensions of the Systolic Array
// These are now the *physical hardware dimensions*
#ifndef M
#define M 5 // Default number of rows (maps to K2)
#endif

#ifndef N
#define N 5 // Default number of columns (maps to K3)
#endif

using namespace std;

// =================================================================
// --- Original SA_MxN Module (Unchanged) ---
// =================================================================
// This is your structural module that just builds the grid.
SC_MODULE(SA_MxN)
{
    // Clock and reset
    sc_in<bool> clk;
    sc_in<bool> reset;

    // Check if its output stationary or weight/input stationary
    sc_in<bool> output_stationary;

    // Input from the top (N inputs, one for each column)
    // Input from the left (M inputs, one for each row)
    // Output to the right (M outputs, one from each row)
    // Output to the bottom (N outputs, one from each column)
    sc_in<float> in_top[N];
    sc_in<float> in_left[M];
    sc_out<float> out_right[M];
    sc_out<float> out_bottom[N];

    // Preloading buffer and signal
    sc_in<bool> preload_valid;
    // Preload data for all M*N PEs.
    // Assumes data is flattened in row-major order (PE[0][0], PE[0][1], ..., PE[M-1][N-1])
    sc_in<float> preload_data[M * N]; 

    // Internal signals for PE connections
    // Horizontal connections: M rows, N-1 signals each
    sc_signal<float> pe_out_right[M][N - 1];
    // Vertical connections: M-1 signals each, N columns
    sc_signal<float> pe_out_bottom[M - 1][N];

    // Array ID for this SA_MxN instance
    int array_id;

    // Array of pointers to PE modules
    PE* pe_array[M][N];

    // Custom Constructor with array_id
    SA_MxN(sc_module_name name, int array_id) : sc_module(name), array_id(array_id)
    {
        // Instantiate PEs (M rows, N columns)
        for(int i = 0; i < M; i++) // Row index
        {
            for(int j = 0; j < N; j++) // Column index
            {
                // Create a unique name for each PE
                string pe_name = "PE_" + to_string(i) + "_" + to_string(j);
                pe_array[i][j] = new PE(pe_name.c_str(), array_id, i, j);
                
                // Connect clock and reset
                pe_array[i][j]->clk(clk);
                pe_array[i][j]->reset(reset);
                
                // Connect output_stationary mode signal
                pe_array[i][j]->output_stationary(output_stationary);
                
                // Connect preload valid signal
                pe_array[i][j]->preload_valid(preload_valid);
                
                // --- Connect PE inputs (data flow) ---

                // Connect left input (from SA boundary or previous PE)
                if(j == 0)
                    pe_array[i][j]->in_left(in_left[i]); // From SA left boundary
                else
                    pe_array[i][j]->in_left(pe_out_right[i][j - 1]); // From PE to the left
                
                // Connect top input (from SA boundary or previous PE)
                if(i == 0)
                    pe_array[i][j]->in_top(in_top[j]); // From SA top boundary
                else
                    pe_array[i][j]->in_top(pe_out_bottom[i - 1][j]); // From PE above

                // --- Connect PE outputs (data flow) ---

                // Connect right output (to SA boundary or next PE)
                if(j == N - 1)
                    pe_array[i][j]->out_right(out_right[i]); // To SA right boundary
                else
                    pe_array[i][j]->out_right(pe_out_right[i][j]); // To PE to the right
                
                // Connect bottom output (to SA boundary or next PE)
                if(i == M - 1)
                    pe_array[i][j]->out_bottom(out_bottom[j]); // To SA bottom boundary
                else
                    pe_array[i][j]->out_bottom(pe_out_bottom[i][j]); // To PE below

                // Connect preload data input (using row-major indexing)
                pe_array[i][j]->preload_data(preload_data[i * N + j]);
            }
        }

        cout << "SA_" << M << "x" << N << " Module Created with array_id: " << array_id << endl;
    }

    // Destructor to clean up dynamically allocated PEs
    ~SA_MxN() {
        for (int i = 0; i < M; ++i) {
            for (int j = 0; j < N; ++j) {
                delete pe_array[i][j];
            }
        }
    }
};

// =================================================================
// --- NEW MatMul_Controller Module ---
// =================================================================
// This module wraps the SA_MxN grid and implements the
// control logic for tiling.
// It computes C[K1][K3] = A[K1][K2] * W[K2][K3]
SC_MODULE(MatMul_Controller)
{
    // --- Ports ---
    sc_in<bool> clk;
    sc_in<bool> reset;
    sc_in<bool> start; // Signal to begin computation
    sc_out<bool> done; // Signal when C is fully computed

    // Pointers to large matrices in main memory (from testbench)
    sc_in<float*> A_matrix; // K1 x K2
    sc_in<float*> W_matrix; // K2 x K3
    sc_in<float*> C_matrix; // K1 x K3 (This module writes to this)

    // Matrix dimensions
    sc_in<int> K1; // Rows of A (streamed over time)
    sc_in<int> K2; // Cols of A / Rows of W (tiled by M)
    sc_in<int> K3; // Cols of W (tiled by N)

    // --- Internal Components ---
    SA_MxN* sa_grid; // The M x N PE grid

    // --- Internal Signals (to connect this Controller to sa_grid) ---
    sc_signal<bool> sa_reset;
    sc_signal<bool> sa_output_stationary;
    sc_signal<bool> sa_preload_valid;
    sc_signal<float> sa_in_top[N];
    sc_signal<float> sa_in_left[M];
    sc_signal<float> sa_out_right[M];
    sc_signal<float> sa_out_bottom[N];
    sc_signal<float> sa_preload_data[M * N];


   // --- Tiling Control Process (FSM) ---
    void tiling_process() 
    {
        // Reset logic
        sa_reset.write(true);
        done.write(false);
        wait();
        sa_reset.write(false);
        wait();

        while(true) 
        {
            // Wait for start signal
            while (start.read() == false) wait();
            
            done.write(false);
            
            // Read matrix dimensions and pointers
            int k1 = K1.read();
            int k2 = K2.read();
            int k3 = K3.read();
            float* A_ptr = A_matrix.read();
            float* W_ptr = W_matrix.read();
            float* C_ptr = C_matrix.read();
            
            // --- Tiling Loops ---
            for (int j_t = 0; j_t < (k3 + N - 1) / N; ++j_t) 
            {
                for (int k_t = 0; k_t < (k2 + M - 1) / M; ++k_t) 
                {
                    // --- 1. Preload W_tile ---
                    sa_preload_valid.write(true);
                    sa_output_stationary.write(false); // Use WS mode
                    
                    for (int i = 0; i < M; ++i) { // PE grid rows (maps to k)
                        for (int j = 0; j < N; ++j) { // PE grid cols (maps to j)
                            int w_row = k_t * M + i;
                            int w_col = j_t * N + j;

                            if (w_row < k2 && w_col < k3) {
                                sa_preload_data[i * N + j].write( W_ptr[w_row * k3 + w_col] );
                            } else {
                                sa_preload_data[i * N + j].write(0.0f);
                            }
                        }
                    }
                    wait(); // 1 clock cycle to latch preload data
                    sa_preload_valid.write(false);
                    
                    for (int j = 0; j < N; j++) sa_in_top[j].write(0.0f);


                    // ****************************************************
                    // --- 2. Stream A_tile and Drain C_tile (NEW LOGIC) ---
                    //
                    // We must run for k1 + M + (N-1) cycles.
                    // k1 = number of input rows
                    // M  = pipeline latency to get C[r][0]
                    // N-1 = additional skew to get C[r][N-1]
                    //
                    int total_cycles = k1 + M + N;
                    
                    for (int clk_cycle = 0; clk_cycle < total_cycles; ++clk_cycle) 
                    {
                        // --- A. Feed A_tile ---
                        // We only feed valid data based on the skewed-in time.
                        // After we're done (a_row >= k1), this will naturally feed zeros.
                        for (int i = 0; i < M; ++i) { // PE grid rows (maps to k)
                            int a_row = clk_cycle - i; // Skewed input row index
                            int a_col = k_t * M + i; // Input column block

                            if (a_row >= 0 && a_row < k1 && a_col < k2) {
                                // Valid data from A matrix
                                sa_in_left[i].write( A_ptr[a_row * k2 + a_col] );
                            } else {
                                // Pad with zero (or drain with zeros)
                                sa_in_left[i].write(0.0f);
                            }
                        }
                        
                        // --- B. Read and Un-skew C_tile results ---
                        // The result for C[r][j] emerges at cycle (r + M + j)
                        // Therefore, at a given clk_cycle, the output at sa_out_bottom[j]
                        // belongs to row: r = clk_cycle - M - j
                        
                        for (int j = 0; j < N; ++j) { // PE grid cols (maps to j)
                            
                            int r_out_skewed = clk_cycle - M - j - 1; // This is the *actual* row index
                            
                            // Check if this row is a valid row we care about (0 to k1-1)
                            if (r_out_skewed >= 0 && r_out_skewed < k1) 
                            {
                                int c_col = j_t * N + j; // Final C matrix column
                                
                                if (c_col < k3) {
                                    // Read partial sum and ADD to the *correct* row
                                    float partial_sum = sa_out_bottom[j].read();
                                    C_ptr[r_out_skewed * k3 + c_col] += partial_sum;
                                }
                            }
                        } // end output read loop
                        
                        wait();
                    } // end total_cycles loop
                    // ****************************************************

                } // end k_t loop (inner dim)
            } // end j_t loop (output cols)
            
            // All tiles done!
            done.write(true);
            wait();

        } // end while(true)
    } // end tiling_process()
    
    // --- Constructor ---
    SC_CTOR(MatMul_Controller) 
    {
        // Instantiate the SA_MxN grid
        sa_grid = new SA_MxN("sa_grid_inst", 1); // Use ID=1
        
        // Connect internal signals to the grid's ports
        sa_grid->clk(clk);
        sa_grid->reset(sa_reset); // Drive grid with internal reset
        sa_grid->output_stationary(sa_output_stationary);
        sa_grid->preload_valid(sa_preload_valid);

        for(int i=0; i<N; ++i) {
            sa_grid->in_top[i](sa_in_top[i]);
            sa_grid->out_bottom[i](sa_out_bottom[i]);
        }
        for(int i=0; i<M; ++i) {
            sa_grid->in_left[i](sa_in_left[i]);
            sa_grid->out_right[i](sa_out_right[i]);
        }
        for(int i=0; i<M*N; ++i) {
            sa_grid->preload_data[i](sa_preload_data[i]);
        }
        
        // Launch the controller thread
        SC_CTHREAD(tiling_process, clk.pos());
        reset_signal_is(reset, true); // Use the top-level async reset
    }
    
    // --- Destructor ---
    ~MatMul_Controller() {
        delete sa_grid;
    }
};

#endif // SA_MXN_H