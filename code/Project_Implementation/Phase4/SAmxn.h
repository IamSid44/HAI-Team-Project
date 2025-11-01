#ifndef SA_MXN_H
#define SA_MXN_H

#include "PE.h"
#include <string>
#include <vector>     
#include <algorithm>  

// Define the dimensions of the Systolic Array
#ifndef M
#define M 5 // Default number of rows
#endif

#ifndef N
#define N 5 // Default number of columns
#endif

using namespace std;

// =================================================================
// --- Original SA_MxN Module (Unchanged) ---
// =================================================================
SC_MODULE(SA_MxN)
{
    // Clock and reset
    sc_in<bool> clk;
    sc_in<bool> reset;

    // Check if its output stationary or weight/input stationary
    sc_in<bool> output_stationary;

    // ... (rest of ports are identical) ...
    sc_in<float> in_top[N];
    sc_in<float> in_left[M];
    sc_out<float> out_right[M];
    sc_out<float> out_bottom[N];

    sc_in<bool> preload_valid;
    sc_in<float> preload_data[M * N]; 

    // Internal signals
    sc_signal<float> pe_out_right[M][N - 1];
    sc_signal<float> pe_out_bottom[M - 1][N];

    int array_id;
    PE* pe_array[M][N];

    // Custom Constructor with array_id
    SA_MxN(sc_module_name name, int array_id) : sc_module(name), array_id(array_id)
    {
        // Instantiate PEs (M rows, N columns)
        for(int i = 0; i < M; i++) // Row index
        {
            for(int j = 0; j < N; j++) // Column index
            {
                // ... (instantiation and connections are identical) ...
                string pe_name = "PE_" + to_string(i) + "_" + to_string(j);
                pe_array[i][j] = new PE(pe_name.c_str(), array_id, i, j);
                
                pe_array[i][j]->clk(clk);
                pe_array[i][j]->reset(reset);
                pe_array[i][j]->output_stationary(output_stationary);
                pe_array[i][j]->preload_valid(preload_valid);
                
                if(j == 0) pe_array[i][j]->in_left(in_left[i]);
                else       pe_array[i][j]->in_left(pe_out_right[i][j - 1]);
                
                if(i == 0) pe_array[i][j]->in_top(in_top[j]);
                else       pe_array[i][j]->in_top(pe_out_bottom[i - 1][j]);

                if(j == N - 1) pe_array[i][j]->out_right(out_right[i]);
                else           pe_array[i][j]->out_right(pe_out_right[i][j]);
                
                if(i == M - 1) pe_array[i][j]->out_bottom(out_bottom[j]);
                else           pe_array[i][j]->out_bottom(pe_out_bottom[i][j]);

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

SC_MODULE(MatMul_Controller)
{
    // --- Ports ---
    sc_in<bool> clk;
    sc_in<bool> reset;
    sc_in<bool> start; 
    sc_out<bool> done; 

    sc_in<bool> sa_mode_is_output_stationary;

    // ... (rest of ports identical) ...
    sc_in<float*> A_matrix; 
    sc_in<float*> W_matrix; 
    sc_in<float*> C_matrix; 

    sc_in<int> K1; 
    sc_in<int> K2; 
    sc_in<int> K3; 

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


   // --- Tiling Control Process (FSM) ---
    void tiling_process() 
    {
        // ... (reset logic identical) ...
        sa_reset.write(true);
        done.write(false);
        wait();
        sa_reset.write(false);
        wait();

        while(true) 
        {
            while (start.read() == false) wait();
            
            done.write(false);
            
            int k1 = K1.read();
            int k2 = K2.read();
            int k3 = K3.read();
            float* A_ptr = A_matrix.read();
            float* W_ptr = W_matrix.read();
            float* C_ptr = C_matrix.read();
            
            if (sa_mode_is_output_stationary.read() == false)
            {
                // =================================================
                // --- 1. WEIGHT STATIONARY (WS) MODE ---
                // =================================================
                
                for (int j_t = 0; j_t < (k3 + N - 1) / N; ++j_t) 
                {
                    for (int k_t = 0; k_t < (k2 + M - 1) / M; ++k_t) 
                    {
                        // --- 1.1 Preload W_tile (identical) ---
                        sa_preload_valid.write(true);
                        for (int i = 0; i < M; ++i) { 
                            for (int j = 0; j < N; ++j) { 
                                int w_row = k_t * M + i;
                                int w_col = j_t * N + j;
                                if (w_row < k2 && w_col < k3) {
                                    sa_preload_data[i * N + j].write( W_ptr[w_row * k3 + w_col] );
                                } else {
                                    sa_preload_data[i * N + j].write(0.0f);
                                }
                            }
                        }
                        wait(); 
                        sa_preload_valid.write(false);
                        for (int j = 0; j < N; j++) sa_in_top[j].write(0.0f);


                        // --- 1.2 Stream A_tile and Drain C_tile ---
                        int total_cycles = k1 + min(k2, M) + min(k3, N);
                        
                        for (int clk_cycle = 0; clk_cycle < total_cycles; ++clk_cycle) 
                        {
                            // --- A. Feed A_tile (identical) ---
                            for (int i = 0; i < M; ++i) { 
                                int a_row = clk_cycle - i; 
                                int a_col = k_t * M + i; 
                                if (a_row >= 0 && a_row < k1 && a_col < k2) {
                                    sa_in_left[i].write( A_ptr[a_row * k2 + a_col] );
                                } else {
                                    sa_in_left[i].write(0.0f);
                                }
                            }
                            
                            // --- B. Read and Un-skew C_tile results ---
                            for (int j = 0; j < N; ++j) { 
                                
                                // ** WS FIX **
                                // The value for logical row 'r' arrives at clk = r + M + j - 1
                                // So, r = clk - M - j + 1
                                int r_out_skewed = clk_cycle - M - j - 1; 
                                
                                if (r_out_skewed >= 0 && r_out_skewed < k1) 
                                {
                                    int c_col = j_t * N + j; 
                                    if (c_col < k3) {
                                        float partial_sum = sa_out_bottom[j].read();
                                        C_ptr[r_out_skewed * k3 + c_col] += partial_sum;
                                    }
                                }
                            } 
                            wait();
                        } 
                    } 
                } 
            }
            else 
            {
                // =================================================
                // --- 2. OUTPUT STATIONARY (OS) MODE ---
                // =================================================
                
                for (int i_t = 0; i_t < (k1 + M - 1) / M; ++i_t) 
                {
                    for (int j_t = 0; j_t < (k3 + N - 1) / N; ++j_t) 
                    {
                        // --- 2.1 Reset PE Accumulators (identical) ---
                        sa_reset.write(true);
                        wait();
                        sa_reset.write(false);
                        
                        // --- 2.2 Accumulate Phase (identical) ---
                        sa_preload_valid.write(false); 
                        int stream_cycles = k2 + M + N - 1; 
                        for (int clk_cycle = 0; clk_cycle < stream_cycles; ++clk_cycle)
                        {
                            for (int i = 0; i < M; ++i) {
                                int k = clk_cycle - i; 
                                int a_row = i_t * M + i;
                                if (k >= 0 && k < k2 && a_row < k1) {
                                    sa_in_left[i].write(A_ptr[a_row * k2 + k]);
                                } else {
                                    sa_in_left[i].write(0.0f);
                                }
                            }
                            for (int j = 0; j < N; ++j) {
                                int k = clk_cycle - j; 
                                int w_col = j_t * N + j;
                                if (k >= 0 && k < k2 && w_col < k3) {
                                    sa_in_top[j].write(W_ptr[k * k3 + w_col]);
                                } else {
                                    sa_in_top[j].write(0.0f);
                                }
                            }
                            wait();
                        } 

                        // --- 2.3 Drain Phase (identical) ---
                        sa_preload_valid.write(true); 
                        for(int j=0; j<N; ++j) sa_in_top[j].write(0.0f); 
                        for(int i=0; i<M; ++i) sa_in_left[i].write(0.0f); 

                        int drain_cycles = M;
                        for(int i = 0; i < 2; i++)
                        {
                            wait();
                        }

                        for (int clk_cycle = 0; clk_cycle < drain_cycles; ++clk_cycle)
                        {
                            for (int j = 0; j < N; ++j) {
                                
                                // ** OS FIX **
                                // Data from physical row 'i' exits at clk = (M-i) + j
                                // The 'i' we want is for the first cycle (clk=j+1), which is i=M-1.
                                // Formula: i = (M-1) - (clk - j - 1)
                                // int i_out_skewed = (M - 1) - (clk_cycle - j - 1);
                                int i_out_skewed = j;
                                
                                // if (i_out_skewed >= 0 && i_out_skewed < M) 
                                // {
                                int c_row = i_t * M + M - clk_cycle - 1;
                                int c_col = j_t * N + j;
                                
                                if (c_row < k1 && c_col < k3) {
                                    C_ptr[c_row * k3 + c_col] = sa_out_bottom[j].read();
                                    // cout << "Writing C[" << c_row << "][" << c_col << "] = " << C_ptr[c_row * k3 + c_col] << endl;
                                }
                                // }
                            }
                            wait();
                        }
                        sa_preload_valid.write(false); 
                    } 
                } 
            } 
            
            done.write(true);
            wait();

        } 
    } 
    
    // --- Constructor (identical) ---
    SC_CTOR(MatMul_Controller) 
    {
        sa_grid = new SA_MxN("sa_grid_inst", 1);
        
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
        
        SC_CTHREAD(tiling_process, clk.pos());
        reset_signal_is(reset, true); 
    }
    
    ~MatMul_Controller() {
        delete sa_grid;
    }
};

#endif // SA_MXN_H