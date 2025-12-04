#ifndef SA_MXN_H
#define SA_MXN_H

#include "PE.h"
#include <string>
#include <vector>     
#include <algorithm>  

// Define the dimensions of the Systolic Array
// Use PE_ROWS and PE_COLS to avoid conflicts with SystemC template parameters
#ifndef PE_ROWS
#define PE_ROWS 2 // Default number of rows
#endif

#ifndef PE_COLS
#define PE_COLS 7  // Default number of columns (supports MxN grids)
#endif

// Backward compatibility aliases
#define M PE_ROWS
#define N PE_COLS

using namespace std;

// =================================================================
// Systolic Array Module (MxN grid of PEs)
// =================================================================
SC_MODULE(SA_MxN)
{
    // Clock and reset
    sc_in<bool> clk;
    sc_in<bool> reset;

    // Dataflow mode selection
    sc_in<bool> output_stationary;

    // Array inputs and outputs
    sc_in<float> in_top[N];
    sc_in<float> in_left[M];
    sc_out<float> out_right[M];
    sc_out<float> out_bottom[N];

    // Preloading interface
    sc_in<bool> preload_valid;
    sc_in<float> preload_data[M * N]; 

    // Internal signals for inter-PE connections
    sc_signal<float> pe_out_right[M][N - 1];
    sc_signal<float> pe_out_bottom[M - 1][N];

    int array_id;
    PE* pe_array[M][N];

    // Constructor
    SA_MxN(sc_module_name name, int array_id) : sc_module(name), array_id(array_id)
    {
        // Instantiate PEs in MxN grid
        for(int i = 0; i < M; i++) // Row index
        {
            for(int j = 0; j < N; j++) // Column index
            {
                string pe_name = "PE_" + to_string(i) + "_" + to_string(j);
                pe_array[i][j] = new PE(pe_name.c_str(), array_id, i, j);
                
                // Connect common signals
                pe_array[i][j]->clk(clk);
                pe_array[i][j]->reset(reset);
                pe_array[i][j]->output_stationary(output_stationary);
                pe_array[i][j]->preload_valid(preload_valid);
                
                // Connect left input: from external or from PE on left
                if(j == 0) 
                    pe_array[i][j]->in_left(in_left[i]);
                else       
                    pe_array[i][j]->in_left(pe_out_right[i][j - 1]);
                
                // Connect top input: from external or from PE above
                if(i == 0) 
                    pe_array[i][j]->in_top(in_top[j]);
                else       
                    pe_array[i][j]->in_top(pe_out_bottom[i - 1][j]);

                // Connect right output: to external or to PE on right
                if(j == N - 1) 
                    pe_array[i][j]->out_right(out_right[i]);
                else           
                    pe_array[i][j]->out_right(pe_out_right[i][j]);
                
                // Connect bottom output: to external or to PE below
                if(i == M - 1) 
                    pe_array[i][j]->out_bottom(out_bottom[j]);
                else           
                    pe_array[i][j]->out_bottom(pe_out_bottom[i][j]);

                // Connect preload data
                pe_array[i][j]->preload_data(preload_data[i * N + j]);
            }
        }
        cout << "SA_" << M << "x" << N << " Module Created with array_id: " 
             << array_id << endl;
    }

    ~SA_MxN() {
        for (int i = 0; i < M; ++i) {
            for (int j = 0; j < N; ++j) {
                delete pe_array[i][j];
            }
        }
    }
};

// =================================================================
// Matrix Multiplication Controller
// Handles tiling and dataflow control for the systolic array
// =================================================================
SC_MODULE(MatMul_Controller)
{
    // --- Ports ---
    sc_in<bool> clk;
    sc_in<bool> reset;
    sc_in<bool> start; 
    sc_out<bool> done; 

    sc_in<bool> sa_mode_is_output_stationary;

    // Matrix pointers (pointing to tile buffers)
    // NOTE: All tile buffers are M×M with stride M, regardless of PE grid shape (M×N)
    sc_in<float*> A_matrix; 
    sc_in<float*> W_matrix; 
    sc_in<float*> C_matrix; 

    // ACTUAL tile dimensions (may be < M for edge tiles)
    sc_in<int> K1; 
    sc_in<int> K2; 
    sc_in<int> K3;
    sc_in<int> tile_stride;  // Buffer stride = max(M, N)

    // --- Internal Components ---
    SA_MxN* sa_grid; 

    // --- Internal Signals ---
    sc_signal<bool> sa_reset;
    sc_signal<bool> sa_preload_valid;
    sc_signal<float> sa_in_top[N];
    sc_signal<float> sa_in_left[M];
    sc_signal<float> sa_out_right[M];
    sc_signal<float> sa_out_bottom[N];
    sc_signal<float> sa_preload_data[M * N];

   // =================================================================
   // Main Tiling Control Process
   // =================================================================
    void tiling_process() 
    {
        // Initial reset
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
            
            // Read dimensions (these are ACTUAL tile dimensions, may be < M)
            int k1 = K1.read();
            int k2 = K2.read();
            int k3 = K3.read();
            
            // Get buffer pointers (buffers are M×M with stride M)
            float* A_ptr = A_matrix.read();
            float* W_ptr = W_matrix.read();
            float* C_ptr = C_matrix.read();
            
            // =================================================
            // MODE SELECTION: WS or OS
            // =================================================
            if (sa_mode_is_output_stationary.read() == false)
            {
                // =============================================
                // WEIGHT STATIONARY MODE
                // =============================================
                
                // --- Phase 1: Preload Weights ---
                sa_preload_valid.write(true);
                
                for (int i = 0; i < M; ++i) { 
                    for (int j = 0; j < N; ++j) { 
                        // Use tile_stride for buffer access
                        int stride = tile_stride.read();
                        if (i < k2 && j < k3) {
                            sa_preload_data[i * N + j].write(W_ptr[i * stride + j]);
                        } else {
                            sa_preload_data[i * N + j].write(0.0f);
                        }
                    }
                }

                wait();
                
                sa_preload_valid.write(false);
                for (int j = 0; j < N; j++) sa_in_top[j].write(0.0f);

                // --- Phase 2: Stream A_tile and Drain C_tile ---
                int total_cycles = k1 + M + N;
                
                int stride = tile_stride.read();
                
                for (int clk_cycle = 0; clk_cycle < total_cycles; ++clk_cycle) 
                {
                    // Feed A_tile with skew
                    for (int i = 0; i < M; ++i) { 
                        int a_row = clk_cycle - i;
                        
                        if (a_row >= 0 && a_row < k1 && i < k2) {
                            sa_in_left[i].write(A_ptr[a_row * stride + i]);
                        } else {
                            sa_in_left[i].write(0.0f);
                        }
                    }
                    
                    // Read and un-skew C_tile results
                    for (int j = 0; j < N; ++j) { 
                        int r_out = clk_cycle - M - j - 1; 
                        
                        if (r_out >= 0 && r_out < k1 && j < k3) {
                            float partial_sum = sa_out_bottom[j].read();
                            C_ptr[r_out * stride + j] += partial_sum;
                        }
                    } 
                    
                    wait();
                }
            }
            else 
            {
                // =============================================
                // OUTPUT STATIONARY MODE
                // =============================================
                
                // --- Phase 1: Reset PE Accumulators ---
                sa_reset.write(true);
                wait();
                sa_reset.write(false);
                wait(); // CRITICAL: Give PEs one cycle to complete reset
                
                // --- Phase 2: Accumulate Phase ---
                sa_preload_valid.write(false);
                
                int stride = tile_stride.read();
                int stream_cycles = k2 + M + N + 1;
                
                for (int clk_cycle = 0; clk_cycle < stream_cycles; ++clk_cycle)
                {
                    // Stream A_tile from left with skew
                    for (int i = 0; i < M; ++i) {
                        int k = clk_cycle - i;
                        
                        // Use tile_stride for buffer access
                        if (k >= 0 && k < k2 && i < k1) {
                            sa_in_left[i].write(A_ptr[i * stride + k]);
                        } else {
                            sa_in_left[i].write(0.0f);
                        }
                    }
                    
                    // Stream W_tile from top with skew
                    for (int j = 0; j < N; ++j) {
                        int k = clk_cycle - j;
                        
                        // Use tile_stride for buffer access
                        if (k >= 0 && k < k2 && j < k3) {
                            sa_in_top[j].write(W_ptr[k * stride + j]);
                        } else {
                            sa_in_top[j].write(0.0f);
                        }
                    }
                    wait();
                }
                
                // --- Phase 3: Drain Phase ---
                // Clear inputs during drain
                for(int j=0; j<N; ++j) sa_in_top[j].write(0.0f); 
                for(int i=0; i<M; ++i) sa_in_left[i].write(0.0f); 
                
                // Set preload_valid to trigger drain in PEs
                sa_preload_valid.write(true); 
                wait(); // First wait: let PEs see preload_valid and output their accumulators
                wait(1, SC_NS); // Small delay to ensure signal propagation
                
                // Drain for M cycles to get all rows
                for (int clk_cycle = 0; clk_cycle < M; ++clk_cycle)
                {
                    for (int j = 0; j < N; ++j) {
                        int i_out = M - 1 - clk_cycle;
                        
                        // Use tile_stride for buffer access
                        if (i_out >= 0 && i_out < k1 && j < k3) {
                            float result = sa_out_bottom[j].read();
                            C_ptr[i_out * stride + j] += result;  // ACCUMULATE across k-tiles
                        }
                    }
                    
                    // Wait for next drain cycle (except after last one)
                    if (clk_cycle < M - 1) {
                        wait();
                    }
                }
                
                sa_preload_valid.write(false); 
            } 
            
            // Signal completion
            done.write(true);
            wait();
            done.write(false);
        } 
    } 
    
    // --- Constructor ---
    SC_CTOR(MatMul_Controller) 
    {
        // Instantiate systolic array
        sa_grid = new SA_MxN("sa_grid_inst", 1);
        
        // Connect systolic array signals
        sa_grid->clk(clk);
        sa_grid->reset(sa_reset); 
        sa_grid->output_stationary(sa_mode_is_output_stationary);
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
        
        // Register control process
        SC_CTHREAD(tiling_process, clk.pos());
        reset_signal_is(reset, true); 
    }
    
    ~MatMul_Controller() {
        delete sa_grid;
    }
};

#endif // SA_MXN_H