#ifndef SA_MXN_H
#define SA_MXN_H

#include "PE.h"
#include <string>
#include <vector>     
#include <algorithm>  

// Define the dimensions of the Systolic Array
#ifndef M
#define M 3 // Default number of rows
#endif

#ifndef N
#define N 3  // Default number of columns
#endif

using namespace std;

// Systolic Array Module (M x N grid of PEs)
SC_MODULE(SA_MxN)
{
    // Clock and reset
    sc_in<bool> clk;
    sc_in<bool> reset;
    sc_in<bool> output_stationary;

    // Array inputs/outputs
    sc_in<float> in_top[N];
    sc_in<float> in_left[M];
    sc_out<float> out_right[M];
    sc_out<float> out_bottom[N];

    // Preload signals
    sc_in<bool> preload_valid;
    sc_in<float> preload_data[M * N]; 

    // Internal signals connecting PEs
    sc_signal<float> pe_out_right[M][N - 1];
    sc_signal<float> pe_out_bottom[M - 1][N];

    int array_id;
    PE* pe_array[M][N];

    // Constructor
    SA_MxN(sc_module_name name, int array_id) : sc_module(name), array_id(array_id)
    {
        // Instantiate PEs in M x N grid
        for(int i = 0; i < M; i++) {
            for(int j = 0; j < N; j++) {
                string pe_name = "PE_" + to_string(i) + "_" + to_string(j);
                pe_array[i][j] = new PE(pe_name.c_str(), array_id, i, j);
                
                // Connect clock, reset, and control signals
                pe_array[i][j]->clk(clk);
                pe_array[i][j]->reset(reset);
                pe_array[i][j]->output_stationary(output_stationary);
                pe_array[i][j]->preload_valid(preload_valid);
                
                // Connect left input (from array input or previous PE)
                if(j == 0) 
                    pe_array[i][j]->in_left(in_left[i]);
                else       
                    pe_array[i][j]->in_left(pe_out_right[i][j - 1]);
                
                // Connect top input (from array input or previous PE)
                if(i == 0) 
                    pe_array[i][j]->in_top(in_top[j]);
                else       
                    pe_array[i][j]->in_top(pe_out_bottom[i - 1][j]);

                // Connect right output (to next PE or array output)
                if(j == N - 1) 
                    pe_array[i][j]->out_right(out_right[i]);
                else           
                    pe_array[i][j]->out_right(pe_out_right[i][j]);
                
                // Connect bottom output (to next PE or array output)
                if(i == M - 1) 
                    pe_array[i][j]->out_bottom(out_bottom[j]);
                else           
                    pe_array[i][j]->out_bottom(pe_out_bottom[i][j]);

                // Connect preload data
                pe_array[i][j]->preload_data(preload_data[i * N + j]);
            }
        }
        cout << "SA_" << M << "x" << N << " Module Created with array_id: " << array_id << endl;
    }

    ~SA_MxN() {
        for (int i = 0; i < M; ++i) {
            for (int j = 0; j < N; ++j) {
                delete pe_array[i][j];
            }
        }
    }
};

// Matrix Multiplication Controller - manages dataflow to/from systolic array
SC_MODULE(MatMul_Controller)
{
    // Clock and control
    sc_in<bool> clk;
    sc_in<bool> reset;
    sc_in<bool> start; 
    sc_out<bool> done; 

    // Dataflow mode selection
    sc_in<bool> sa_mode_is_output_stationary;

    // Matrix pointers (from external buffers)
    sc_in<float*> A_matrix; 
    sc_in<float*> W_matrix; 
    sc_in<float*> C_matrix; 

    // Matrix dimensions for current tile
    sc_in<int> K1; 
    sc_in<int> K2; 
    sc_in<int> K3; 

    // Systolic array instance
    SA_MxN* sa_grid; 

    // Internal signals to/from systolic array
    sc_signal<bool> sa_reset;
    sc_signal<bool> sa_preload_valid;
    sc_signal<float> sa_in_top[N];
    sc_signal<float> sa_in_left[M];
    sc_signal<float> sa_out_right[M];
    sc_signal<float> sa_out_bottom[N];
    sc_signal<float> sa_preload_data[M * N];

    // Dataflow control FSM
    void tiling_process() 
    {
        // Initial reset
        sa_reset.write(true);
        done.write(false);
        wait(); 
        sa_reset.write(false);
 
        while(true) 
        {
            // Wait for start signal
            while (start.read() == false) wait();
            
            done.write(false);
            
            // Read configuration
            int k1 = K1.read();
            int k2 = K2.read();
            int k3 = K3.read();
            float* A_ptr = A_matrix.read();
            float* W_ptr = W_matrix.read();
            float* C_ptr = C_matrix.read();
            
            if (sa_mode_is_output_stationary.read() == false)
            {
                // ================================================
                // WEIGHT STATIONARY (WS) MODE
                // ================================================
                // In WS mode: weights are preloaded, activations stream through
                
                for (int j_t = 0; j_t < (k3 + N - 1) / N; ++j_t) 
                {
                    for (int k_t = 0; k_t < (k2 + M - 1) / M; ++k_t) 
                    {
                        // Preload W_tile into PEs
                        cout << "    [WS] Preloading weights (k_t=" << k_t << ", j_t=" << j_t << ")" << endl;
                        sa_preload_valid.write(true);
                        for (int i = 0; i < M; ++i) { 
                            for (int j = 0; j < N; ++j) { 
                                int w_row = k_t * M + i;
                                int w_col = j_t * N + j;
                                if (w_row < k2 && w_col < k3) {
                                    // Buffer indexing: direct i,j since W_tile is already the tile we want
                                    float w_val = W_ptr[i * M + j];
                                    sa_preload_data[i * N + j].write(w_val);
                                } else {
                                    sa_preload_data[i * N + j].write(0.0f);
                                }
                            }
                        }
                        wait();
                        sa_preload_valid.write(false);
                        
                        // Clear top inputs
                        for (int j = 0; j < N; j++) sa_in_top[j].write(0.0f);

                        // Stream A_tile and drain C_tile
                        int total_cycles = k1 + min(k2, M) + min(k3, N);
                        cout << "    [WS] Streaming and computing (" << total_cycles << " cycles)..." << endl;
                        
                        for (int clk_cycle = 0; clk_cycle < total_cycles; ++clk_cycle) 
                        {
                            // Feed A_tile (skewed input on left side)
                            for (int i = 0; i < M; ++i) { 
                                int a_row = clk_cycle - i; 
                                int a_col = k_t * M + i; 
                                float a_val = 0.0f;
                                if (a_row >= 0 && a_row < k1 && a_col < k2) {
                                    // Buffer indexing: use (a_col % M) since buffer has stride M
                                    a_val = A_ptr[a_row * M + (a_col % M)];
                                    sa_in_left[i].write(a_val);
                                } else {
                                    sa_in_left[i].write(0.0f);
                                }
                            }
                            
                            // Wait for computation to complete
                            wait();
                            
                            // Wait for outputs to settle
                            wait(SC_ZERO_TIME);
                            
                            // Drain C_tile results (skewed output from bottom)
                            for (int j = 0; j < N; ++j) { 
                                // WS: row r appears at cycle r + M + j
                                int r_out_skewed = clk_cycle - M - j; 
                                
                                if (r_out_skewed >= 0 && r_out_skewed < k1) 
                                {
                                    int c_col = j_t * N + j; 
                                    if (c_col < k3) {
                                        float partial_sum = sa_out_bottom[j].read();
                                        // Buffer indexing: use (c_col % M) since buffer has stride M
                                        C_ptr[r_out_skewed * M + (c_col % M)] += partial_sum;
                                    }
                                }
                            }
                        }
                        cout << "    [WS] Computation complete" << endl;
                    }
                } 
            }
            else 
            {
                // ================================================
                // OUTPUT STATIONARY (OS) MODE
                // ================================================
                // In OS mode: outputs accumulate in PEs, then drain at the end
                
                for (int i_t = 0; i_t < (k1 + M - 1) / M; ++i_t) 
                {
                    for (int j_t = 0; j_t < (k3 + N - 1) / N; ++j_t) 
                    {
                        // Reset PE accumulators for this output tile
                        sa_reset.write(true);
                        wait();
                        sa_reset.write(false);
                        
                        // Accumulation phase
                        sa_preload_valid.write(false); 
                        int stream_cycles = k1 + min(k2, M) + min(k3, N);
                        cout << "    [OS] Accumulating (i_t=" << i_t << ", j_t=" << j_t << ", " << stream_cycles << " cycles)..." << endl;
                        
                        for (int clk_cycle = 0; clk_cycle < stream_cycles; ++clk_cycle)
                        {
                            // Feed A_tile (skewed from left)
                            for (int i = 0; i < M; ++i) {
                                int k = clk_cycle - i; 
                                int a_row = i_t * M + i;
                                float a_val = 0.0f;
                                if (k >= 0 && k < k2 && a_row < k1) {
                                    // Buffer indexing: use (k % M) since buffer has stride M
                                    a_val = A_ptr[i * M + (k % M)];
                                    sa_in_left[i].write(a_val);
                                } else {
                                    sa_in_left[i].write(0.0f);
                                }
                            }
                            
                            // Feed W_tile (skewed from top)
                            for (int j = 0; j < N; ++j) {
                                int k = clk_cycle - j; 
                                int w_col = j_t * N + j;
                                float w_val = 0.0f;
                                if (k >= 0 && k < k2 && w_col < k3) {
                                    // Buffer indexing: use (k % M) since buffer has stride M
                                    w_val = W_ptr[(k % M) * M + j];
                                    sa_in_top[j].write(w_val);
                                } else {
                                    sa_in_top[j].write(0.0f);
                                }
                            }
                            wait();
                        }
                        cout << "    [OS] Accumulation complete" << endl; 

                        // Drain phase - read accumulated outputs
                        cout << "    [OS] Draining results..." << endl;
                        sa_preload_valid.write(true); 
                        for(int j=0; j<N; ++j) sa_in_top[j].write(0.0f); 
                        for(int i=0; i<M; ++i) sa_in_left[i].write(0.0f); 

                        int drain_cycles = M;

                        for (int clk_cycle = 0; clk_cycle < drain_cycles; ++clk_cycle)
                        {
                            // Drain outputs (bottom to top order)
                            for (int j = 0; j < N; ++j) {
                                int c_row = i_t * M + M - clk_cycle - 1;
                                int c_col = j_t * N + j;
                                
                                if (c_row < k1 && c_col < k3) {
                                    float c_val = sa_out_bottom[j].read();
                                    // Buffer indexing: use (c_col % M) since buffer has stride M
                                    C_ptr[c_row * M + (c_col % M)] = c_val;
                                }
                            }
                            wait();
                        }
                        cout << "    [OS] Drain complete" << endl;
                        sa_preload_valid.write(false); 
                    } 
                } 
            } 
            
            done.write(true);
            wait();
        } 
    } 
    
    // Constructor
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
        
        // Register FSM thread
        SC_CTHREAD(tiling_process, clk.pos());
        reset_signal_is(reset, true); 
    }
    
    ~MatMul_Controller() {
        delete sa_grid;
    }
};

#endif // SA_MXN_H