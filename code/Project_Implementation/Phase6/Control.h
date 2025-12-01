#ifndef CONTROL_H
#define CONTROL_H

#include "Memory.h"
#include "SAmxn.h"

using namespace std;

// Fixed-buffer controller:
// 1. Allocates FIXED-SIZE buffers (M x M) regardless of matrix size
// 2. Performs TILING during memory reads (not in matmul controller)
// 3. Reads one tile at a time, computes, writes results
// 4. Repeats for all tiles in the matrix multiplication

SC_MODULE(MemoryBackedController)
{
    // Clock and control
    sc_in<bool> clk;
    sc_in<bool> reset;
    sc_in<bool> start;
    sc_out<bool> done;
    
    // Matrix dimensions (K1 x K2) * (K2 x K3) = (K1 x K3)
    sc_in<int> K1;
    sc_in<int> K2;
    sc_in<int> K3;
    
    // Memory base addresses (byte addresses)
    sc_in<int> A_base_addr;
    sc_in<int> W_base_addr;
    sc_in<int> C_base_addr;
    
    // Dataflow mode
    sc_in<bool> output_stationary;
    
    // Internal components
    Memory* memory;
    MatMul_Controller* matmul_ctrl;
    
    // Memory interface signals
    sc_signal<bool> mem_read_enable;
    sc_signal<bool> mem_write_enable;
    sc_signal<int> mem_address;
    sc_signal<float> mem_write_data;
    sc_signal<float> mem_read_data;
    sc_signal<bool> mem_ready;
    
    // MatMul controller interface signals
    sc_signal<bool> matmul_start;
    sc_signal<bool> matmul_done;
    sc_signal<float*> A_matrix_ptr;
    sc_signal<float*> W_matrix_ptr;
    sc_signal<float*> C_matrix_ptr;
    sc_signal<int> tile_k1, tile_k2, tile_k3;
    
    // FIXED-SIZE buffers (M x M regardless of actual matrix size)
    float* A_tile;
    float* W_tile;
    float* C_tile;
    
    // Constructor
    SC_CTOR(MemoryBackedController)
    {
        // Allocate FIXED-SIZE buffers: M x M
        A_tile = new float[M * M];
        W_tile = new float[M * M];
        C_tile = new float[M * M];
        
        cout << "Fixed-size buffers allocated: " << M << "x" << M 
             << " (" << (M*M) << " floats each)" << endl;

        // Instantiate memory (4KB = 1024 floats)
        memory = new Memory("memory_block", "memory.txt", 4096);
        memory->clk(clk);
        memory->reset(reset);
        memory->read_enable(mem_read_enable);
        memory->write_enable(mem_write_enable);
        memory->address(mem_address);
        memory->write_data(mem_write_data);
        memory->read_data(mem_read_data);
        memory->ready(mem_ready);
        
        // Instantiate MatMul controller
        matmul_ctrl = new MatMul_Controller("matmul_ctrl");
        matmul_ctrl->clk(clk);
        matmul_ctrl->reset(reset);
        matmul_ctrl->start(matmul_start);
        matmul_ctrl->done(matmul_done);
        matmul_ctrl->sa_mode_is_output_stationary(output_stationary);
        matmul_ctrl->A_matrix(A_matrix_ptr);
        matmul_ctrl->W_matrix(W_matrix_ptr);
        matmul_ctrl->C_matrix(C_matrix_ptr);
        matmul_ctrl->K1(tile_k1);
        matmul_ctrl->K2(tile_k2);
        matmul_ctrl->K3(tile_k3);
        
        SC_THREAD(control_process);
        sensitive << clk.pos();
        dont_initialize();
    }
    
    ~MemoryBackedController()
    {
        delete[] A_tile;
        delete[] W_tile;
        delete[] C_tile;
        delete memory;
        delete matmul_ctrl;
    }
    
    // Helper: Read a tile of matrix A from memory into A_tile buffer
    void read_A_tile(int a_base, int k1, int k2, int i_tile, int k_tile)
    {
        // Clear tile buffer
        for (int i = 0; i < M * M; i++) A_tile[i] = 0.0f;
        
        // Read tile elements from memory
        for (int i = 0; i < M; i++) {
            for (int j = 0; j < M; j++) {
                int global_row = i_tile * M + i;
                int global_col = k_tile * M + j;
                
                // Check if this element exists in the actual matrix
                if (global_row < k1 && global_col < k2) {
                    int byte_addr = a_base + (global_row * k2 + global_col) * 4;
                    
                    mem_address.write(byte_addr);
                    mem_read_enable.write(true);
                    mem_write_enable.write(false);
                    wait();
                    wait(SC_ZERO_TIME);
                    
                    // Store in tile buffer with stride M
                    A_tile[i * M + j] = mem_read_data.read();
                    mem_read_enable.write(false);
                }
            }
        }
    }
    
    // Helper: Read a tile of matrix W from memory into W_tile buffer
    void read_W_tile(int w_base, int k2, int k3, int k_tile, int j_tile)
    {
        // Clear tile buffer
        for (int i = 0; i < M * M; i++) W_tile[i] = 0.0f;
        
        // Read tile elements from memory
        for (int i = 0; i < M; i++) {
            for (int j = 0; j < M; j++) {
                int global_row = k_tile * M + i;
                int global_col = j_tile * M + j;
                
                // Check if this element exists in the actual matrix
                if (global_row < k2 && global_col < k3) {
                    int byte_addr = w_base + (global_row * k3 + global_col) * 4;
                    
                    mem_address.write(byte_addr);
                    mem_read_enable.write(true);
                    mem_write_enable.write(false);
                    wait();
                    wait(SC_ZERO_TIME);
                    
                    // Store in tile buffer with stride M
                    W_tile[i * M + j] = mem_read_data.read();
                    mem_read_enable.write(false);
                }
            }
        }
    }
    
    // Helper: Write C_tile buffer back to memory
    void write_C_tile(int c_base, int k1, int k3, int i_tile, int j_tile)
    {
        for (int i = 0; i < M; i++) {
            for (int j = 0; j < M; j++) {
                int global_row = i_tile * M + i;
                int global_col = j_tile * M + j;
                
                // Only write if this element exists in the actual matrix
                if (global_row < k1 && global_col < k3) {
                    int byte_addr = c_base + (global_row * k3 + global_col) * 4;
                    
                    mem_address.write(byte_addr);
                    mem_write_data.write(C_tile[i * M + j]);
                    mem_read_enable.write(false);
                    mem_write_enable.write(true);
                    wait();
                    
                    mem_write_enable.write(false);
                }
            }
        }
    }
    
    // Main control process with tile-level memory management
    void control_process()
    {
        // Initialize
        done.write(false);
        matmul_start.write(false);
        mem_read_enable.write(false);
        mem_write_enable.write(false);
        
        while (true)
        {
            // Wait for start signal
            while (!start.read()) {
                wait();
            }
            
            done.write(false);
            
            int k1 = K1.read();
            int k2 = K2.read();
            int k3 = K3.read();
            int a_base = A_base_addr.read();
            int w_base = W_base_addr.read();
            int c_base = C_base_addr.read();
            
            cout << endl << "=== Tile-Based Matrix Multiplication ===" << endl;
            cout << "Matrix A: " << k1 << "x" << k2 << " at byte address " << a_base << endl;
            cout << "Matrix W: " << k2 << "x" << k3 << " at byte address " << w_base << endl;
            cout << "Matrix C: " << k1 << "x" << k3 << " at byte address " << c_base << endl;
            cout << "PE Grid Size: " << M << "x" << M << endl;
            
            // Calculate number of tiles in each dimension
            int num_i_tiles = (k1 + M - 1) / M;
            int num_j_tiles = (k3 + M - 1) / M;
            int num_k_tiles = (k2 + M - 1) / M;
            
            cout << "Number of tiles: i=" << num_i_tiles 
                 << ", j=" << num_j_tiles 
                 << ", k=" << num_k_tiles << endl << endl;
            
            // Tiled matrix multiplication: C[i,j] += A[i,k] * W[k,j]
            for (int i_tile = 0; i_tile < num_i_tiles; i_tile++) {
                for (int j_tile = 0; j_tile < num_j_tiles; j_tile++) {
                    
                    // Initialize C_tile to zero for this output tile
                    for (int i = 0; i < M * M; i++) C_tile[i] = 0.0f;
                    
                    // Accumulate across k dimension
                    for (int k_tile = 0; k_tile < num_k_tiles; k_tile++) {
                        
                        cout << "Processing tile: C[" << i_tile << "," << j_tile 
                             << "] += A[" << i_tile << "," << k_tile 
                             << "] * W[" << k_tile << "," << j_tile << "]" << endl;
                        
                        // Read A_tile from memory
                        cout << "  Reading A tile[" << i_tile << "," << k_tile << "]..." << endl;
                        read_A_tile(a_base, k1, k2, i_tile, k_tile);

                        // Print A_tile for debugging
                        cout << "  A_tile:" << endl;
                        for (int i = 0; i < M; i++) {
                            cout << "    ";
                            for (int j = 0; j < M; j++) {
                                cout << A_tile[i * M + j] << " ";
                            }
                            cout << endl;
                        }

                        // Read W_tile from memory
                        cout << "  Reading W tile[" << k_tile << "," << j_tile << "]..." << endl;
                        read_W_tile(w_base, k2, k3, k_tile, j_tile);

                        // Print W_tile for debugging
                        cout << "  W_tile:" << endl;
                        for (int i = 0; i < M; i++) {
                            cout << "    ";
                            for (int j = 0; j < M; j++) {
                                cout << W_tile[i * M + j] << " ";
                            }
                            cout << endl;
                        }

                        // Compute tile multiplication
                        cout << "  Computing tile..." << endl;
                        
                        // Calculate actual tile dimensions (may be < M for edge tiles)
                        int actual_i = min(M, k1 - i_tile * M);
                        int actual_k = min(M, k2 - k_tile * M);
                        int actual_j = min(M, k3 - j_tile * M);
                        
                        // Set tile dimensions for matmul controller
                        tile_k1.write(actual_i);
                        tile_k2.write(actual_k);
                        tile_k3.write(actual_j);
                        
                        // Set buffer pointers
                        A_matrix_ptr.write(A_tile);
                        W_matrix_ptr.write(W_tile);
                        C_matrix_ptr.write(C_tile);
                        
                        // Start computation (no extra wait before start)
                        matmul_start.write(true);
                        wait();
                        matmul_start.write(false);
                        
                        // Wait for computation to complete
                        while (!matmul_done.read()) {
                            wait();
                        }
                        
                        cout << "  Tile computation complete" << endl;
                        
                        // Print C_tile after this k_tile computation
                        cout << "  C_tile after k_tile " << k_tile << ":" << endl;
                        for (int i = 0; i < M; i++) {
                            cout << "    ";
                            for (int j = 0; j < M; j++) {
                                cout << C_tile[i * M + j] << " ";
                            }
                            cout << endl;
                        }
                    }
                    
                    // Write C_tile back to memory (after all k_tiles accumulated)
                    cout << "  Final C_tile for position [" << i_tile << "," << j_tile << "]:" << endl;
                    for (int i = 0; i < M; i++) {
                        cout << "    ";
                        for (int j = 0; j < M; j++) {
                            cout << C_tile[i * M + j] << " ";
                        }
                        cout << endl;
                    }
                    cout << "  Writing C tile[" << i_tile << "," << j_tile << "]..." << endl;
                    write_C_tile(c_base, k1, k3, i_tile, j_tile);
                }
            }
            
            cout << "=== Tiled Multiplication Complete ===" << endl << endl;
            
            // Signal completion
            done.write(true);
            wait();
            done.write(false);
        }
    }
};

#endif // CONTROL_H