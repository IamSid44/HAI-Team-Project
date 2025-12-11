#ifndef CONTROL_H
#define CONTROL_H

#include "Memory.h"
#include "SAmxn.h"

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
    
    SC_CTOR(MemoryBackedController)
    {
        A_tile = new float[M * M];
        W_tile = new float[M * M];
        C_tile = new float[M * M];
        
        cout << "Fixed-size buffers allocated: " << M << "x" << M 
             << " (" << (M*M) << " floats each)" << endl;
        cout << "PE Grid Size: " << M << "x" << N << endl;
        
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
                }
            }
        }
    }
    
    void read_W_tile(int w_base, int k2, int k3, int k_tile, int j_tile, int tile_width)
    {
        for (int i = 0; i < M * M; i++) W_tile[i] = 0.0f;
        
        for (int i = 0; i < M; i++) {
            for (int j = 0; j < M; j++) {
                int global_row = k_tile * M + i;
                int global_col = j_tile * tile_width + j;  // Use tile_width for column tiling
                
                if (global_row < k2 && global_col < k3) {
                    int byte_addr = w_base + (global_row * k3 + global_col) * 4;
                    
                    mem_address.write(byte_addr);
                    mem_read_enable.write(true);
                    mem_write_enable.write(false);
                    wait();
                    wait(SC_ZERO_TIME);
                    
                    W_tile[i * M + j] = mem_read_data.read();
                    mem_read_enable.write(false);
                }
            }
        }
    }
    
    void write_C_tile(int c_base, int k1, int k3, int i_tile, int j_tile, int tile_width)
    {
        for (int i = 0; i < M; i++) {
            for (int j = 0; j < M; j++) {
                int global_row = i_tile * M + i;
                int global_col = j_tile * tile_width + j;  // Use tile_width for column tiling
                
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
            cout << "PE Grid Size: " << M << "x" << N << endl;
            
            // Effective tile dimensions:
            // - Row tiles: M rows (PE rows)
            // - Col tiles: min(M,N) cols (limited by both PE rows and cols)
            // - K tiles: M elements (PE rows for weights)
            int tile_width = (M < N) ? M : N;  // min(M, N)
            
            int num_i_tiles = (k1 + M - 1) / M;  // Rows: based on M (PE rows)
            int num_j_tiles = (k3 + tile_width - 1) / tile_width;  // Cols: based on min(M,N)
            int num_k_tiles = (k2 + M - 1) / M;  // K-dim: based on M (PE rows=weight rows)
            
            cout << "Number of tiles: i=" << num_i_tiles 
                 << ", j=" << num_j_tiles 
                 << ", k=" << num_k_tiles << endl << endl;
            
            for (int i_tile = 0; i_tile < num_i_tiles; i_tile++) {
                for (int j_tile = 0; j_tile < num_j_tiles; j_tile++) {
                    
                    for (int i = 0; i < M * M; i++) C_tile[i] = 0.0f;
                    
                    for (int k_tile = 0; k_tile < num_k_tiles; k_tile++) {
                        
                        cout << "Processing tile: C[" << i_tile << "," << j_tile 
                             << "] += A[" << i_tile << "," << k_tile 
                             << "] * W[" << k_tile << "," << j_tile << "]" << endl;
                        
                        cout << "  Reading A tile..." << endl;
                        read_A_tile(a_base, k1, k2, i_tile, k_tile);
                        
                        cout << "  Reading W tile..." << endl;
                        read_W_tile(w_base, k2, k3, k_tile, j_tile, tile_width);
                        
                        cout << "  Computing tile..." << endl;
                        
                        int actual_i = min(M, k1 - i_tile * M);
                        int actual_k = min(M, k2 - k_tile * M);
                        int actual_j = min(tile_width, k3 - j_tile * tile_width);  // Use tile_width
                        
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
                        
                        cout << "  Tile computation complete" << endl;
                    }
                    
                    cout << "  Writing C tile to memory..." << endl;
                    write_C_tile(c_base, k1, k3, i_tile, j_tile, tile_width);
                }
            }
            
            cout << "=== Tiled Multiplication Complete ===" << endl << endl;
            
            done.write(true);
            wait();
            done.write(false);
        }
    }
};

#endif // CONTROL_H