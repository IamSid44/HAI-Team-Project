#ifndef CONTROL_H
#define CONTROL_H

#include "Memory.h"
#include "SAmxn.h"

using namespace std;

// Simple controller that:
// 1. Reads matrices A and W from memory
// 2. Computes C = A * W using systolic array
// 3. Writes result C back to memory
// Clock cycle accurate - no FSM, just sequential operations

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
    
    // Internal storage for matrices
    float* A_data;
    float* W_data;
    float* C_data;
    int max_matrix_size;
    
    // Constructor
    SC_CTOR(MemoryBackedController)
    {
        // Allocate maximum matrix storage
        max_matrix_size = M * M * 2;  // Conservative allocation
        A_data = new float[max_matrix_size];
        W_data = new float[max_matrix_size];
        C_data = new float[max_matrix_size];
        
        // Instantiate memory (64KB = 16384 floats)
        memory = new Memory("memory_block", "memory.txt", 65536);
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
        matmul_ctrl->K1(K1);
        matmul_ctrl->K2(K2);
        matmul_ctrl->K3(K3);
        
        SC_THREAD(control_process);
        sensitive << clk.pos();
        dont_initialize();
    }
    
    ~MemoryBackedController()
    {
        delete[] A_data;
        delete[] W_data;
        delete[] C_data;
        delete memory;
        delete matmul_ctrl;
    }
    
    // Main control process
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
            
            cout << endl << "=== Starting Memory-Backed Matrix Multiplication ===" << endl;
            cout << "Matrix A: " << k1 << "x" << k2 << " at byte address " << a_base << endl;
            cout << "Matrix W: " << k2 << "x" << k3 << " at byte address " << w_base << endl;
            cout << "Matrix C: " << k1 << "x" << k3 << " at byte address " << c_base << endl;
            
            // ===================================================
            // PHASE 1: READ MATRIX A FROM MEMORY
            // ===================================================
            cout << "Phase 1: Reading matrix A from memory..." << endl;
            for (int i = 0; i < k1; i++) {
                for (int j = 0; j < k2; j++) {
                    int byte_addr = a_base + (i * k2 + j) * 4;
                
                    // Issue read request
                    mem_address.write(byte_addr);
                    mem_read_enable.write(true);
                    mem_write_enable.write(false);
                    wait();  // Memory responds in 1 cycle
                    
                    wait(SC_ZERO_TIME);
                    // Capture data
                    A_data[i * k2 + j] = mem_read_data.read();
                    mem_read_enable.write(false);
                }
            }
            cout << "  Matrix A loaded (" << k1 * k2 << " elements)" << endl;
            // print base for debugging
            // cout << "A base address: " << a_base << endl;
            // print A_data for debugging
            cout << "Matrix A data:" << endl;
            for (int i = 0; i < k1; i++) {

                for (int j = 0; j < k2; j++) {
                    cout << setw(10) << fixed << setprecision(4) << A_data[i * k2 + j] << " ";
                }
                cout << endl;
            }
            

            
            // ===================================================
            // PHASE 2: READ MATRIX W FROM MEMORY
            // ===================================================
            cout << "Phase 2: Reading matrix W from memory..." << endl;
            for (int i = 0; i < k2; i++) {
                for (int j = 0; j < k3; j++) {
                    int byte_addr = w_base + (i * k3 + j) * 4;
                    
                    // Issue read request
                    mem_address.write(byte_addr);
                    mem_read_enable.write(true);
                    mem_write_enable.write(false);
                    wait();  // Memory responds in 1 cycle
                    
                    wait(SC_ZERO_TIME);
                    
                    // Capture data
                    W_data[i * k3 + j] = mem_read_data.read();
                    mem_read_enable.write(false);
                }
            }
            cout << "  Matrix W loaded (" << k2 * k3 << " elements)" << endl;
            // print W_data for debugging
            cout << "Matrix W data:" << endl;
            for (int i = 0; i < k2; i++) {
                for (int j = 0; j < k3; j++) {
                    cout << setw(10) << fixed << setprecision(4) << W_data[i * k3 + j] << " ";
                }
                cout << endl;
            }   
            
            // Initialize C matrix to zero
            for (int i = 0; i < k1 * k3; i++) {
                C_data[i] = 0.0f;
            }
            
            // ===================================================
            // PHASE 3: COMPUTE C = A * W USING SYSTOLIC ARRAY
            // ===================================================
            cout << "Phase 3: Computing matrix multiplication..." << endl;
            
            // Set matrix pointers
            A_matrix_ptr.write(A_data);
            W_matrix_ptr.write(W_data);
            C_matrix_ptr.write(C_data);

            wait();
            
            // Start computation
            matmul_start.write(true);
            wait();
            matmul_start.write(false);
            
            // Wait for computation to complete
            while (!matmul_done.read()) {
                wait();
            }
            wait();
            cout << "  Computation complete" << endl;
            
            // ===================================================
            // PHASE 4: WRITE MATRIX C TO MEMORY
            // ===================================================
            cout << "Phase 4: Writing matrix C to memory..." << endl;
            for (int i = 0; i < k1; i++) {
                for (int j = 0; j < k3; j++) {
                    int byte_addr = c_base + (i * k3 + j) * 4;
                    
                    // Issue write request
                    mem_address.write(byte_addr);
                    mem_write_data.write(C_data[i * k3 + j]);
                    mem_read_enable.write(false);
                    mem_write_enable.write(true);
                    wait();  // Memory writes in 1 cycle
                    
                    mem_write_enable.write(false);
                }
            }
            cout << "  Matrix C written (" << k1 * k3 << " elements)" << endl;
            cout << "=== Operation Complete ===" << endl << endl;
            
            // Signal completion
            done.write(true);
            wait();
            done.write(false);
        }
    }
};

#endif // CONTROL_H