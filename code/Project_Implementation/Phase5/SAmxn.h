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
// This module (the grid of PEs) does not need to change at all.
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

// =================================================================
// --- MODIFIED MatMul_Controller Module ---
// =================================================================
SC_MODULE(MatMul_Controller)
{
    // --- Ports ---
    sc_in<bool> clk;
    sc_in<bool> reset;
    sc_in<bool> start; 
    sc_out<bool> done; 

    sc_in<bool> sa_mode_is_output_stationary; // Input from top-level

    // Matrix logical dimensions
    sc_in<int> K1; 
    sc_in<int> K2; 
    sc_in<int> K3; 

    // Base addresses for memory
    sc_in<int> A_base_addr;
    sc_in<int> W_base_addr;
    sc_in<int> C_base_addr;

    // --- Ports to/from L1 Memory ---
    // Data from L1 (feeds SA)
    sc_in<float> mem_data_top[N];
    sc_in<float> mem_data_left[M];
    
    // Addresses to L1
    sc_out<int> mem_addr_top[N];
    sc_out<int> mem_addr_left[M];
    // Note: We don't need read_en because L1 is ideal/combinational.
    // We send -1 as an address to signal "don't care" / read 0.

    // Write-back to L1
    sc_out<int> mem_write_addr[N];
    sc_out<bool> mem_write_en[N];

    // --- Ports to/from SA Grid ---
    // We must expose the SA's bottom outputs so the System module
    // can wire them to the L1 memory's inputs.
    sc_out<float> sa_out_bottom_ports[N];

    // --- Internal Components ---
    SA_MxN* sa_grid; 

    // --- Internal Signals ---
    sc_signal<bool> sa_reset;
    sc_signal<bool> sa_preload_valid;
    // We no longer need sa_in_top/sa_in_left signals.
    // The memory ports are wired directly to the SA grid.
    sc_signal<float> sa_out_right[M];
    sc_signal<float> sa_out_bottom_internal[N]; // <--- This is correct
    sc_signal<float> sa_preload_data[M * N];


   // --- Tiling Control Process (FSM) ---
    void tiling_process() 
    {
        // --- Reset ---
        sa_reset.write(true);
        done.write(false);
        sa_preload_valid.write(false);
        for(int i=0; i<N; ++i) {
            mem_addr_top[i].write(-1);
            mem_write_addr[i].write(-1);
            mem_write_en[i].write(false);
        }
        for(int i=0; i<M; ++i) {
            mem_addr_left[i].write(-1);
        }
        wait();
        sa_reset.write(false);
        wait();

        while(true) 
        {
            // --- Wait for Start ---
            while (start.read() == false) wait();
            
            done.write(false);
            
            // Read logical dimensions
            int k1 = K1.read();
            int k2 = K2.read();
            int k3 = K3.read();

            // Read base addresses
            int a_base = A_base_addr.read();
            int w_base = W_base_addr.read();
            int c_base = C_base_addr.read();
            
            // ** ASSUMPTION **
            // We assume the PEs preload buffer is loaded *externally* // before this controller starts. In WS mode, the testbench
            // must load the *first* W tile into the PEs.
            //
            // ** REVISED ASSUMPTION (from user) **
            // The user's SAmxn.h *did* have preload logic for WS.
            // Let's keep that. It means the controller *reads* W
            // from L1 and preloads the PEs.

            if (sa_mode_is_output_stationary.read() == false)
            {
                // =================================================
                // --- 1. WEIGHT STATIONARY (WS) MODE ---
                // =================================================
                // cout << (k3 + N - 1) / N << " " << (k2 + M - 1) / M << endl;
                for (int j_t = 0; j_t < (k3 + N - 1) / N; ++j_t) 
                {
                    for (int k_t = 0; k_t < (k2 + M - 1) / M; ++k_t) 
                    {
                        // --- 1.1 Preload W_tile ---
                        // We need to read from L1 and send to PEs
                        
                        // [FIX] Removed the redundant outer for(i..M) and for(j..N) loops
                        // The FSM logic below runs ONCE per tile.
                        
                        // --- 1.1 Preload FSM ---
                        sa_preload_valid.write(true); // Signal to PEs that a preload is happening
                        for(int j=0; j<N; ++j) mem_write_en[j].write(false); // Disable writes

                        float w_tile_buffer[M][N];

                        // Use 'N' top ports to read 'M' rows
                        for(int i=0; i<M; ++i) {
                            // Request row 'i' of the W tile
                            for(int j=0; j<N; ++j) {
                                int w_row = k_t * M + i;
                                int w_col = j_t * N + j;
                                if (w_row < k2 && w_col < k3) {
                                    mem_addr_top[j].write(w_base + w_row * k3 + w_col);
                                    // cout << "Controller: Requesting W_tile value from L1 addr " 
                                    //      << (w_base + w_row * k3 + w_col) << endl;
                                } else {
                                    mem_addr_top[j].write(-1); // Invalid addr
                                    // cout<<"hi"<<endl;
                                }
                            }
                            // Wait 1 cycle for "ideal" L1 read
                            wait(); 
                            
                            // Latch the data from memory
                            for(int j=0; j<N; ++j) {
                                w_tile_buffer[i][j] = mem_data_top[j].read();
                                cout << "Controller: Loaded W_tile value " << w_tile_buffer[i][j]
                                     << " from L1 addr " << mem_addr_top[j].read() << endl;
                            }
                        }

                        // Now, send all M*N values to the SA
                        for(int i=0; i<M; ++i) {
                            for(int j=0; j<N; ++j) {
                                sa_preload_data[i * N + j].write(w_tile_buffer[i][j]);
                                // cout << "Controller: Preloading W_tile value " << w_tile_buffer[i][j]
                                //      << " to PE[" << i << "][" << j << "]" << endl;
                            }
                        }
                        
                        // Hold preload_valid for one cycle so PEs can latch
                        wait(); 
                        sa_preload_valid.write(false);
                        for (int j = 0; j < N; j++) mem_addr_top[j].write(-1); // Stop reading
                            
                        // --- 1.2 Stream A_tile and Drain C_tile ---
                        int total_cycles = k1 + M + N;
                        
                        for (int clk_cycle = 0; clk_cycle < total_cycles; ++clk_cycle) 
                        {
                            // --- A. Feed A_tile (from L1) ---
                            for (int i = 0; i < M; ++i) { 
                                int a_row = clk_cycle - i; 
                                int a_col = k_t * M + i; 
                                if (a_row >= 0 && a_row < k1 && a_col < k2) {
                                    mem_addr_left[i].write( a_base + a_row * k2 + a_col );
                                } else {
                                    mem_addr_left[i].write(-1); // Invalid
                                }
                            }
                            
                            // --- B. Write C_tile results (to L1) ---
                            for (int j = 0; j < N; ++j) { 
                                int r_out_skewed = clk_cycle - M - j - 1; 
                                int c_col = j_t * N + j; 
                                
                                if (r_out_skewed >= 0 && r_out_skewed < k1 && c_col < k3) {
                                    // Data from sa_out_bottom_ports[j] is
                                    // wired to L1. We just provide the address
                                    // and enable. L1 handles the accumulation.
                                    mem_write_addr[j].write(c_base + r_out_skewed * k3 + c_col);
                                    mem_write_en[j].write(true);
                                } else {
                                    mem_write_addr[j].write(-1);
                                    mem_write_en[j].write(false);
                                }
                            } 
                            wait();
                        } 
                        // Turn off all ports after streaming
                        for(int i=0; i<M; ++i) mem_addr_left[i].write(-1);
                        for(int j=0; j<N; ++j) mem_write_en[j].write(false);
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
                        // --- 2.1 Reset PE Accumulators ---
                        sa_reset.write(true);
                        wait();
                        sa_reset.write(false);
                        
                        // --- 2.2 Accumulate Phase (Stream from L1) ---
                        sa_preload_valid.write(false); // Accumulate mode
                        int stream_cycles = k2 + M + N - 1; 
                        
                        for (int clk_cycle = 0; clk_cycle < stream_cycles; ++clk_cycle)
                        {
                            // Request A data
                            for (int i = 0; i < M; ++i) {
                                int k = clk_cycle - i; 
                                int a_row = i_t * M + i;
                                if (k >= 0 && k < k2 && a_row < k1) {
                                    mem_addr_left[i].write(a_base + a_row * k2 + k);
                                } else {
                                    mem_addr_left[i].write(-1);
                                }
                            }
                            // Request W data
                            for (int j = 0; j < N; ++j) {
                                int k = clk_cycle - j; 
                                int w_col = j_t * N + j;
                                if (k >= 0 && k < k2 && w_col < k3) {
                                    mem_addr_top[j].write(w_base + k * k3 + w_col);
                                } else {
                                    mem_addr_top[j].write(-1);
                                }
                            }
                            wait();
                        } 
                        // Turn off read ports
                        for(int i=0; i<M; ++i) mem_addr_left[i].write(-1);
                        for(int j=0; j<N; ++j) mem_addr_top[j].write(-1);


                        // --- 2.3 Drain Phase (Write to L1) ---
                        sa_preload_valid.write(true); // Drain mode
                        
                        // Need 2 dummy cycles for OS drain logic (from old controller)
                        for(int i = 0; i < 2; i++) {
                            wait();
                        }

                        int drain_cycles = M;
                        for (int clk_cycle = 0; clk_cycle < drain_cycles; ++clk_cycle)
                        {
                            for (int j = 0; j < N; ++j) {
                                int c_row = i_t * M + M - clk_cycle - 1;
                                int c_col = j_t * N + j;
                                
                                if (c_row < k1 && c_col < k3) {
                                    // L1 will perform an OVERWRITE
                                    mem_write_addr[j].write(c_base + c_row * k3 + c_col);
                                    mem_write_en[j].write(true);
                                } else {
                                    mem_write_addr[j].write(-1);
                                    mem_write_en[j].write(false);
                                }
                            }
                            wait();
                        }
                        sa_preload_valid.write(false); 
                        for(int j=0; j<N; ++j) mem_write_en[j].write(false);
                    } 
                } 
            } 
            
            // --- Signal Done ---
            done.write(true);
            wait();

        } 
    } 
    
    // --- NEW: Process to drive outputs ---
    // This process connects the internal grid's output signal
    // to the controller's output port.
    void drive_sa_outputs() {
        while(true) {
            for(int i=0; i<N; ++i) {
                // Read from internal signal, write to output port
                sa_out_bottom_ports[i].write(sa_out_bottom_internal[i].read());
            }
            wait(); // Wait for any change on the internal signals
        }
    }

    // --- Constructor ---
    SC_CTOR(MatMul_Controller) 
    {
        sa_grid = new SA_MxN("sa_grid_inst", 1);
        
        sa_grid->clk(clk);
        sa_grid->reset(sa_reset); 
        sa_grid->output_stationary(sa_mode_is_output_stationary);
        sa_grid->preload_valid(sa_preload_valid);

        // Wire memory data directly to SA inputs
        for(int i=0; i<N; ++i) {
            sa_grid->in_top[i](mem_data_top[i]);
            // Wire SA internal bottom outs to controller's *internal signal*
            sa_grid->out_bottom[i](sa_out_bottom_internal[i]); // <--- This is DRIVER 1 (Correct)
            
            // sa_out_bottom_ports[i](sa_out_bottom_internal[i]); // <--- DELETE THIS LINE (This was DRIVER 2)
        }
        for(int i=0; i<M; ++i) {
            sa_grid->in_left[i](mem_data_left[i]);
            sa_grid->out_right[i](sa_out_right[i]);
        }
        for(int i=0; i<M*N; ++i) {
            sa_grid->preload_data[i](sa_preload_data[i]);
        }
        
        SC_CTHREAD(tiling_process, clk.pos());
        reset_signal_is(reset, true); 

        // --- Register the new output driver process ---
        SC_THREAD(drive_sa_outputs);
        // Make it sensitive to the internal signals
        for(int i=0; i<N; ++i) {
            sensitive << sa_out_bottom_internal[i];
        }
    }
    
    ~MatMul_Controller() {
        delete sa_grid;
    }
};


// =================================================================
// --- NEW Top-Level System Module ---
// =================================================================
// This module instantiates the L1 memory and the controller
// and wires them together. The testbench will instantiate this.

#include "L1.h" // Include the new memory module

SC_MODULE(System)
{
    // --- External Ports (for Testbench) ---
    sc_in<bool> clk;
    sc_in<bool> reset;
    sc_in<bool> start;
    sc_out<bool> done;

    sc_in<bool> sa_mode_is_output_stationary;

    sc_in<int> K1, K2, K3;
    sc_in<int> A_base_addr;
    sc_in<int> W_base_addr;
    sc_in<int> C_base_addr;

    // --- Internal Components ---
    MatMul_Controller* controller;
    L1_Memory* l1_mem;

    // --- Internal Signals ---
    // Wires between Controller and L1
    sc_signal<float> mem_data_top[N];
    sc_signal<float> mem_data_left[M];
    sc_signal<int>   mem_addr_top[N];
    sc_signal<int>   mem_addr_left[M];
    sc_signal<int>   mem_write_addr[N];
    sc_signal<bool>  mem_write_en[N];

    // Wire from Controller (SA) to L1
    sc_signal<float> sa_to_l1_bottom_data[N];

    // --- Constructor ---
    SC_CTOR(System)
    {
        // 1. Instantiate Components
        controller = new MatMul_Controller("controller_inst");
        l1_mem = new L1_Memory("l1_mem_inst");
        
        // 2. Connect Top-Level Ports
        controller->clk(clk);
        controller->reset(reset);
        controller->start(start);
        controller->done(done);
        controller->sa_mode_is_output_stationary(sa_mode_is_output_stationary);
        controller->K1(K1);
        controller->K2(K2);
        controller->K3(K3);
        controller->A_base_addr(A_base_addr);
        controller->W_base_addr(W_base_addr);
        controller->C_base_addr(C_base_addr);

        l1_mem->clk(clk);
        l1_mem->reset(reset);
        l1_mem->output_stationary_mode(sa_mode_is_output_stationary);

        // 3. Connect Controller and L1
        // L1 -> Controller (SA inputs)
        for(int i=0; i<N; ++i) l1_mem->data_out_top[i](mem_data_top[i]);
        for(int i=0; i<M; ++i) l1_mem->data_out_left[i](mem_data_left[i]);
        
        for(int i=0; i<N; ++i) controller->mem_data_top[i](mem_data_top[i]);
        for(int i=0; i<M; ++i) controller->mem_data_left[i](mem_data_left[i]);

        // Controller -> L1 (Read addresses)
        for(int i=0; i<N; ++i) controller->mem_addr_top[i](mem_addr_top[i]);
        for(int i=0; i<M; ++i) controller->mem_addr_left[i](mem_addr_left[i]);

        for(int i=0; i<N; ++i) l1_mem->read_addr_top[i](mem_addr_top[i]);
        for(int i=0; i<M; ++i) l1_mem->read_addr_left[i](mem_addr_left[i]);
        
        // Controller -> L1 (Write addresses/enables)
        for(int i=0; i<N; ++i) {
            controller->mem_write_addr[i](mem_write_addr[i]);
            controller->mem_write_en[i](mem_write_en[i]);
            
            l1_mem->write_addr[i](mem_write_addr[i]);
            l1_mem->write_en[i](mem_write_en[i]);
        }
        
        // Controller (SA) -> L1 (Write data)
        for(int i=0; i<N; ++i) {
            controller->sa_out_bottom_ports[i](sa_to_l1_bottom_data[i]);
            l1_mem->data_in_bottom[i](sa_to_l1_bottom_data[i]);
        }
    }

    ~System() {
        delete controller;
        delete l1_mem;
    }
};


#endif // SA_MXN_H

