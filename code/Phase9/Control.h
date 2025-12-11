#ifndef CONTROL_H
#define CONTROL_H

#include "Memory.h"
#include "SAmxn.h"
#include <iomanip>

using namespace std;

SC_MODULE(MemoryBackedController)
{
    sc_in<bool> clk;
    sc_in<bool> reset;
    sc_in<bool> start;
    sc_out<bool> done;
    
    sc_in<int> K1;
    sc_in<int> K2;
    sc_in<int> K3;
    
    sc_in<int> A_base_addr;
    sc_in<int> W_base_addr;
    sc_in<int> C_base_addr;
    
    sc_in<bool> output_stationary;
    
    Memory* memory;
    MatMul_Controller* matmul_ctrl;
    
    sc_signal<bool> mem_read_enable;
    sc_signal<bool> mem_write_enable;
    sc_signal<int> mem_address;
    sc_signal<float> mem_write_data;
    sc_signal<float> mem_read_data;
    sc_signal<bool> mem_ready;
    
    sc_signal<bool> matmul_start;
    sc_signal<bool> matmul_done;
    sc_signal<float*> A_matrix_ptr;
    sc_signal<float*> W_matrix_ptr;
    sc_signal<float*> C_matrix_ptr;
    sc_signal<int> tile_k1, tile_k2, tile_k3;
    
    float* A_tile;
    float* W_tile;
    float* C_tile;
    int tile_stride;  // Buffer stride = M (square grid)
    
    // Data reuse optimization: cache entire rows/columns of tiles
    float** A_tile_cache;  // Cache for entire row of A tiles [num_i_tiles][M*M]
    float** W_tile_cache;  // Cache for entire column of W tiles [num_j_tiles][M*M]
    float** C_tile_cache;  // Cache for all output tiles [num_i_tiles*num_j_tiles][M*M]
    int cached_k_tile;     // Which k-tile is currently cached
    
    // Memory access tracking
    int total_memory_reads;
    int total_memory_writes;
    
    SC_CTOR(MemoryBackedController)
    {
        tile_stride = M;  // Square grid: M = N = 7
        A_tile = new float[M * M];
        W_tile = new float[M * M];
        C_tile = new float[M * M];
        
        // Initialize cache pointers to nullptr (allocated dynamically per operation)
        A_tile_cache = nullptr;
        W_tile_cache = nullptr;
        C_tile_cache = nullptr;
        cached_k_tile = -1;
        
        total_memory_reads = 0;
        total_memory_writes = 0;
        
        cout << "Fixed-size buffers allocated: " << M << "x" << M 
             << " (" << (M*M) << " floats each)" << endl;
        cout << "PE Grid Size: " << M << "x" << M << " (SQUARE)" << endl;
        cout << "Tile stride: " << tile_stride << endl;
        cout << "Data Reuse Optimization: ENABLED" << endl;
        
        memory = new Memory("memory_block", "memory.txt", 65536);
        memory->clk(clk);
        memory->reset(reset);
        memory->read_enable(mem_read_enable);
        memory->write_enable(mem_write_enable);
        memory->address(mem_address);
        memory->write_data(mem_write_data);
        memory->read_data(mem_read_data);
        memory->ready(mem_ready);
        
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
        
        // Clean up tile caches if allocated
        if (A_tile_cache != nullptr) {
            // Freed in control_process after each operation
        }
        if (W_tile_cache != nullptr) {
            // Freed in control_process after each operation
        }
        
        delete memory;
        delete matmul_ctrl;
    }
    
    void read_A_tile(int a_base, int k1, int k2, int i_tile, int k_tile)
    {
        for (int i = 0; i < M * M; i++) A_tile[i] = 0.0f;
        
        for (int i = 0; i < M; i++) {
            for (int j = 0; j < M; j++) {
                int global_row = i_tile * M + i;
                int global_col = k_tile * M + j;
                
                if (global_row < k1 && global_col < k2) {
                    int byte_addr = a_base + (global_row * k2 + global_col) * 4;
                    
                    mem_address.write(byte_addr);
                    mem_read_enable.write(true);
                    mem_write_enable.write(false);
                    wait();
                    wait(SC_ZERO_TIME);
                    
                    A_tile[i * M + j] = mem_read_data.read();
                    mem_read_enable.write(false);
                    
                    total_memory_reads++;  // Track memory access
                }
            }
        }
    }
    
    void read_W_tile(int w_base, int k2, int k3, int k_tile, int j_tile)
    {
        for (int i = 0; i < M * M; i++) W_tile[i] = 0.0f;
        
        for (int i = 0; i < M; i++) {
            for (int j = 0; j < M; j++) {  // Square grid: M = N
                int global_row = k_tile * M + i;
                int global_col = j_tile * M + j;
                
                if (global_row < k2 && global_col < k3) {
                    int byte_addr = w_base + (global_row * k3 + global_col) * 4;
                    
                    mem_address.write(byte_addr);
                    mem_read_enable.write(true);
                    mem_write_enable.write(false);
                    wait();
                    wait(SC_ZERO_TIME);
                    
                    W_tile[i * M + j] = mem_read_data.read();
                    mem_read_enable.write(false);
                    
                    total_memory_reads++;  // Track memory access
                }
            }
        }
    }
    
    void write_C_tile(int c_base, int k1, int k3, int i_tile, int j_tile)
    {
        for (int i = 0; i < M; i++) {
            for (int j = 0; j < M; j++) {  // Square grid: M = N
                int global_row = i_tile * M + i;
                int global_col = j_tile * M + j;
                
                if (global_row < k1 && global_col < k3) {
                    int byte_addr = c_base + (global_row * k3 + global_col) * 4;
                    
                    mem_address.write(byte_addr);
                    mem_write_data.write(C_tile[i * M + j]);
                    mem_read_enable.write(false);
                    mem_write_enable.write(true);
                    wait();
                    
                    mem_write_enable.write(false);
                    
                    total_memory_writes++;  // Track memory access
                }
            }
        }
    }
    
    void control_process()
    {
        done.write(false);
        matmul_start.write(false);
        mem_read_enable.write(false);
        mem_write_enable.write(false);
        
        while (true)
        {
            while (!start.read()) {
                wait();
            }
            
            done.write(false);
            
            // Reset memory access counters
            total_memory_reads = 0;
            total_memory_writes = 0;
            
            int k1 = K1.read();
            int k2 = K2.read();
            int k3 = K3.read();
            int a_base = A_base_addr.read();
            int w_base = W_base_addr.read();
            int c_base = C_base_addr.read();
            
            cout << endl << "=== Tile-Based Matrix Multiplication (DATA REUSE) ===" << endl;
            cout << "Matrix A: " << k1 << "x" << k2 << " at byte address " << a_base << endl;
            cout << "Matrix W: " << k2 << "x" << k3 << " at byte address " << w_base << endl;
            cout << "Matrix C: " << k1 << "x" << k3 << " at byte address " << c_base << endl;
            cout << "PE Grid Size: " << M << "x" << M << " (SQUARE)" << endl;
            
            // K-STATIONARY TILING STRATEGY for Data Reuse:
            // Loop order: k → i → j
            // - Outer k-loop: Load A[:,k] and W[k,:] once, reuse for all output tiles
            // - Middle i-loop: Reuse W[k,:] across all rows
            // - Inner j-loop: Reuse A[i,k] across all columns
            
            int num_i_tiles = (k1 + M - 1) / M;  // Rows
            int num_j_tiles = (k3 + M - 1) / M;  // Cols (M = N for square grid)
            int num_k_tiles = (k2 + M - 1) / M;  // K-dimension
            
            cout << "Number of tiles: i=" << num_i_tiles 
                 << ", j=" << num_j_tiles 
                 << ", k=" << num_k_tiles << endl;
            cout << "Tiling strategy: K-STATIONARY (k→i→j loop order)" << endl << endl;
            
            // Allocate tile caches for this operation
            A_tile_cache = new float*[num_i_tiles];
            for (int i = 0; i < num_i_tiles; i++) {
                A_tile_cache[i] = new float[M * M];
            }
            
            W_tile_cache = new float*[num_j_tiles];
            for (int j = 0; j < num_j_tiles; j++) {
                W_tile_cache[j] = new float[M * M];
            }
            
            // Allocate C tile cache and initialize to zero
            C_tile_cache = new float*[num_i_tiles * num_j_tiles];
            for (int idx = 0; idx < num_i_tiles * num_j_tiles; idx++) {
                C_tile_cache[idx] = new float[M * M];
                for (int i = 0; i < M * M; i++) {
                    C_tile_cache[idx][i] = 0.0f;
                }
            }
            
            // K-STATIONARY LOOP: Process one k-tile at a time
            for (int k_tile = 0; k_tile < num_k_tiles; k_tile++) {
                
                cout << "========================================" << endl;
                cout << "K-Tile " << k_tile << "/" << num_k_tiles << " - Loading tiles..." << endl;
                cout << "========================================" << endl;
                
                // PHASE 1: Load all A tiles for this k-slice (A[:,k_tile])
                cout << "  Loading A tiles: A[:," << k_tile << "]" << endl;
                for (int i_tile = 0; i_tile < num_i_tiles; i_tile++) {
                    read_A_tile(a_base, k1, k2, i_tile, k_tile);
                    // Copy to cache
                    for (int idx = 0; idx < M * M; idx++) {
                        A_tile_cache[i_tile][idx] = A_tile[idx];
                    }
                }
                cout << "    → Loaded " << num_i_tiles << " A tiles (reusable across " 
                     << num_j_tiles << " j-tiles)" << endl;
                
                // PHASE 2: Load all W tiles for this k-slice (W[k_tile,:])
                cout << "  Loading W tiles: W[" << k_tile << ",:]" << endl;
                for (int j_tile = 0; j_tile < num_j_tiles; j_tile++) {
                    read_W_tile(w_base, k2, k3, k_tile, j_tile);
                    // Copy to cache
                    for (int idx = 0; idx < M * M; idx++) {
                        W_tile_cache[j_tile][idx] = W_tile[idx];
                    }
                }
                cout << "    → Loaded " << num_j_tiles << " W tiles (reusable across " 
                     << num_i_tiles << " i-tiles)" << endl << endl;
                
                cached_k_tile = k_tile;
                
                // PHASE 3: Compute all output tiles using cached A and W
                cout << "  Computing output tiles using cached data..." << endl;
                for (int i_tile = 0; i_tile < num_i_tiles; i_tile++) {
                    for (int j_tile = 0; j_tile < num_j_tiles; j_tile++) {
                        
                        int c_idx = i_tile * num_j_tiles + j_tile;
                        
                        // Load cached C tile (accumulated across k-tiles)
                        for (int idx = 0; idx < M * M; idx++) {
                            C_tile[idx] = C_tile_cache[c_idx][idx];
                        }
                        
                        // Load cached A and W tiles into working buffers
                        for (int idx = 0; idx < M * M; idx++) {
                            A_tile[idx] = A_tile_cache[i_tile][idx];
                            W_tile[idx] = W_tile_cache[j_tile][idx];
                        }
                        
                        cout << "    C[" << i_tile << "," << j_tile 
                             << "] += A[" << i_tile << "," << k_tile 
                             << "] * W[" << k_tile << "," << j_tile 
                             << "] (from cache)" << endl;
                        
                        int actual_i = min(M, k1 - i_tile * M);
                        int actual_k = min(M, k2 - k_tile * M);
                        int actual_j = min(M, k3 - j_tile * M);
                        
                        tile_k1.write(actual_i);
                        tile_k2.write(actual_k);
                        tile_k3.write(actual_j);
                        
                        A_matrix_ptr.write(A_tile);
                        W_matrix_ptr.write(W_tile);
                        C_matrix_ptr.write(C_tile);
                        
                        wait();
                        
                        matmul_start.write(true);
                        wait();
                        matmul_start.write(false);
                        
                        while (!matmul_done.read()) {
                            wait();
                        }
                        
                        // Save result back to C cache (accumulates across k-tiles)
                        for (int idx = 0; idx < M * M; idx++) {
                            C_tile_cache[c_idx][idx] = C_tile[idx];
                        }
                    }
                }
                
                cout << "  → Computed " << (num_i_tiles * num_j_tiles) 
                     << " output tiles using cached inputs" << endl << endl;
            }
            
            // Write all accumulated C tiles to memory
            cout << "========================================" << endl;
            cout << "Writing final results to memory..." << endl;
            for (int i_tile = 0; i_tile < num_i_tiles; i_tile++) {
                for (int j_tile = 0; j_tile < num_j_tiles; j_tile++) {
                    int c_idx = i_tile * num_j_tiles + j_tile;
                    
                    // Load C tile from cache
                    for (int idx = 0; idx < M * M; idx++) {
                        C_tile[idx] = C_tile_cache[c_idx][idx];
                    }
                    
                    // Write to memory
                    write_C_tile(c_base, k1, k3, i_tile, j_tile);
                }
            }
            cout << "Done writing results" << endl << endl;
            
            // Clean up tile caches
            for (int i = 0; i < num_i_tiles; i++) {
                delete[] A_tile_cache[i];
            }
            delete[] A_tile_cache;
            A_tile_cache = nullptr;
            
            for (int j = 0; j < num_j_tiles; j++) {
                delete[] W_tile_cache[j];
            }
            delete[] W_tile_cache;
            W_tile_cache = nullptr;
            
            for (int idx = 0; idx < num_i_tiles * num_j_tiles; idx++) {
                delete[] C_tile_cache[idx];
            }
            delete[] C_tile_cache;
            C_tile_cache = nullptr;
            
            cout << "========================================" << endl;
            cout << "=== Tiled Multiplication Complete ===" << endl;
            cout << "========================================" << endl;
            cout << "Memory Access Statistics:" << endl;
            cout << "  Total Reads:  " << total_memory_reads << endl;
            cout << "  Total Writes: " << total_memory_writes << endl;
            
            // Calculate theoretical baseline (without data reuse)
            int baseline_reads = num_i_tiles * num_j_tiles * num_k_tiles * 2 * M * M;
            int actual_reads = num_k_tiles * (num_i_tiles + num_j_tiles) * M * M;
            float reduction = 100.0 * (1.0 - (float)actual_reads / baseline_reads);
            
            cout << "  Baseline (no reuse): " << baseline_reads << " reads" << endl;
            cout << "  With data reuse:     " << actual_reads << " reads" << endl;
            cout << "  Memory traffic reduction: " << fixed << setprecision(1) 
                 << reduction << "%" << endl << endl;
            
            done.write(true);
            wait();
            done.write(false);
        }
    }
};

#endif // CONTROL_H