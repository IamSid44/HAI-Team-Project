#include <systemc.h>
#include "Control.h"
#include <iostream>
#include <iomanip>

using namespace std;

// Helper: Initialize memory with test matrices
void initialize_memory_with_matrices(const char* filename,
                                     int a_base, float* A, int k1, int k2,
                                     int w_base, float* W, int k2_w, int k3)
{
    ifstream file_in(filename);
    if (!file_in.is_open()) {
        cout << "Error: Cannot open memory file for initialization" << endl;
        return;
    }
    
    vector<string> lines;
    string line;
    while (getline(file_in, line)) {
        lines.push_back(line);
    }
    file_in.close();
    
    // Update lines with matrix A values
    for (int i = 0; i < k1; i++) {
        for (int j = 0; j < k2; j++) {
            int byte_addr = a_base + (i * k2 + j) * 4;
            int float_index = byte_addr / 4;
            int line_index = float_index + 4;  // Skip header lines
            
            if (line_index < lines.size()) {
                ostringstream oss;
                oss << setw(8) << setfill('0') << byte_addr << ": " 
                    << fixed << setprecision(6) << A[i * k2 + j];
                lines[line_index] = oss.str();
            }
        }
    }
    
    // Update lines with matrix W values
    for (int i = 0; i < k2_w; i++) {
        for (int j = 0; j < k3; j++) {
            int byte_addr = w_base + (i * k3 + j) * 4;
            int float_index = byte_addr / 4;
            int line_index = float_index + 4;  // Skip header lines
            
            if (line_index < lines.size()) {
                ostringstream oss;
                oss << setw(8) << setfill('0') << byte_addr << ": " 
                    << fixed << setprecision(6) << W[i * k3 + j];
                lines[line_index] = oss.str();
            }
        }
    }
    
    // Write back entire file
    ofstream file_out(filename, ios::trunc);
    for (const auto& l : lines) {
        file_out << l << endl;
    }
    file_out.close();
    
    cout << "Memory initialized with test matrices" << endl;
}

// Helper: Read matrix from memory file
void read_matrix_from_memory(const char* filename, int base_addr, 
                             float* matrix, int rows, int cols)
{
    ifstream file(filename);
    if (!file.is_open()) return;
    
    string line;
    vector<string> data_lines;
    
    // Skip header and collect data lines
    while (getline(file, line)) {
        if (!line.empty() && line[0] != '#') {
            data_lines.push_back(line);
        }
    }
    file.close();
    
    // Read matrix values
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            int byte_addr = base_addr + (i * cols + j) * 4;
            int float_index = byte_addr / 4;
            
            if (float_index < data_lines.size()) {
                size_t colon_pos = data_lines[float_index].find(':');
                if (colon_pos != string::npos) {
                    string value_str = data_lines[float_index].substr(colon_pos + 1);
                    value_str.erase(0, value_str.find_first_not_of(" \t"));
                    value_str.erase(value_str.find_last_not_of(" \t\n\r") + 1);
                    
                    if (value_str != "XXXX") {
                        matrix[i * cols + j] = stof(value_str);
                    } else {
                        matrix[i * cols + j] = 0.0f;
                    }
                }
            }
        }
    }
}

// Helper: Print matrix
void print_matrix(const char* name, float* matrix, int rows, int cols)
{
    cout << name << " (" << rows << "x" << cols << "):" << endl;
    for (int i = 0; i < rows; i++) {
        cout << "  ";
        for (int j = 0; j < cols; j++) {
            cout << setw(10) << fixed << setprecision(4) << matrix[i * cols + j] << " ";
        }
        cout << endl;
    }
    cout << endl;
}

// Helper: Verify result against expected
bool verify_result(float* C_expected, float* C_actual, int k1, int k3, float tolerance = 0.001)
{
    bool passed = true;
    for (int i = 0; i < k1; i++) {
        for (int j = 0; j < k3; j++) {
            float diff = abs(C_expected[i * k3 + j] - C_actual[i * k3 + j]);
            if (diff > tolerance) {
                cout << "Mismatch at C[" << i << "][" << j << "]: "
                     << "expected " << C_expected[i * k3 + j]
                     << ", got " << C_actual[i * k3 + j]
                     << " (diff: " << diff << ")" << endl;
                passed = false;
            }
        }
    }
    return passed;
}

// Testbench module
SC_MODULE(Testbench)
{
    // Clock and reset
    sc_clock clk;
    sc_signal<bool> reset;
    
    // Control signals
    sc_signal<bool> start;
    sc_signal<bool> done;
    
    // Matrix dimensions
    sc_signal<int> K1, K2, K3;
    
    // Memory addresses
    sc_signal<int> A_base_addr;
    sc_signal<int> W_base_addr;
    sc_signal<int> C_base_addr;
    
    // Dataflow mode
    sc_signal<bool> output_stationary;
    
    // Device Under Test
    MemoryBackedController* dut;
    
    // VCD trace file
    sc_trace_file* vcd_file;
    
    // Constructor
    SC_CTOR(Testbench) : clk("clk", 10, SC_NS)
    {
        // Create VCD trace file
        vcd_file = sc_create_vcd_trace_file("waveform");
        vcd_file->set_time_unit(1, SC_NS);
        
        // Instantiate DUT
        dut = new MemoryBackedController("dut");
        dut->clk(clk);
        dut->reset(reset);
        dut->start(start);
        dut->done(done);
        dut->K1(K1);
        dut->K2(K2);
        dut->K3(K3);
        dut->A_base_addr(A_base_addr);
        dut->W_base_addr(W_base_addr);
        dut->C_base_addr(C_base_addr);
        dut->output_stationary(output_stationary);
        
        // ====================================================
        // TRACE TOP-LEVEL SIGNALS
        // ====================================================
        sc_trace(vcd_file, clk, "clk");
        sc_trace(vcd_file, reset, "reset");
        sc_trace(vcd_file, start, "start");
        sc_trace(vcd_file, done, "done");
        sc_trace(vcd_file, K1, "K1");
        sc_trace(vcd_file, K2, "K2");
        sc_trace(vcd_file, K3, "K3");
        sc_trace(vcd_file, A_base_addr, "A_base_addr");
        sc_trace(vcd_file, W_base_addr, "W_base_addr");
        sc_trace(vcd_file, C_base_addr, "C_base_addr");
        sc_trace(vcd_file, output_stationary, "output_stationary");
        
        // ====================================================
        // TRACE CONTROLLER INTERNAL SIGNALS
        // ====================================================
        sc_trace(vcd_file, dut->mem_read_enable, "mem_read_enable");
        sc_trace(vcd_file, dut->mem_write_enable, "mem_write_enable");
        sc_trace(vcd_file, dut->mem_address, "mem_address");
        sc_trace(vcd_file, dut->mem_write_data, "mem_write_data");
        sc_trace(vcd_file, dut->mem_read_data, "mem_read_data");
        sc_trace(vcd_file, dut->mem_ready, "mem_ready");
        sc_trace(vcd_file, dut->matmul_start, "matmul_start");
        sc_trace(vcd_file, dut->matmul_done, "matmul_done");
        
        // ====================================================
        // TRACE SYSTOLIC ARRAY SIGNALS
        // ====================================================
        // MatMul Controller signals
        sc_trace(vcd_file, dut->matmul_ctrl->sa_reset, "sa_reset");
        sc_trace(vcd_file, dut->matmul_ctrl->sa_preload_valid, "sa_preload_valid");
        
        // Systolic array inputs (left side - activations)
        for (int i = 0; i < M; i++) {
            char name[32];
            sprintf(name, "sa_in_left[%d]", i);
            sc_trace(vcd_file, dut->matmul_ctrl->sa_in_left[i], name);
        }
        
        // Systolic array inputs (top side - weights)
        for (int j = 0; j < N; j++) {
            char name[32];
            sprintf(name, "sa_in_top[%d]", j);
            sc_trace(vcd_file, dut->matmul_ctrl->sa_in_top[j], name);
        }
        
        // Systolic array outputs (right side)
        for (int i = 0; i < M; i++) {
            char name[32];
            sprintf(name, "sa_out_right[%d]", i);
            sc_trace(vcd_file, dut->matmul_ctrl->sa_out_right[i], name);
        }
        
        // Systolic array outputs (bottom side - results)
        for (int j = 0; j < N; j++) {
            char name[32];
            sprintf(name, "sa_out_bottom[%d]", j);
            sc_trace(vcd_file, dut->matmul_ctrl->sa_out_bottom[j], name);
        }
        
        // Preload data signals
        for (int i = 0; i < min(M * N, 9); i++) {  // Limit to first 9 for readability
            char name[32];
            sprintf(name, "sa_preload_data[%d]", i);
            sc_trace(vcd_file, dut->matmul_ctrl->sa_preload_data[i], name);
        }
        
        cout << "VCD tracing enabled: waveform.vcd" << endl;
        cout << "Tracing " << M*N << " PE array with all I/O signals" << endl;
        
        SC_THREAD(test_process);
    }
    
    ~Testbench()
    {
        sc_close_vcd_trace_file(vcd_file);
        delete dut;
    }
    
    void test_process()
    {
        // ====================================================
        // TEST 1: 5x5 Matrix Multiplication (WS Mode)
        // ====================================================
        cout << "\n╔═══════════════════════════════════════════════╗" << endl;
        cout << "║  TEST 1: 5x5 Matrix Multiplication (WS Mode)  ║" << endl;
        cout << "╚═══════════════════════════════════════════════╝" << endl;
        
        int k1 = 5, k2 = 5, k3 = 5;
        
        // Test matrix A (5x5)
        float A1[25] = {
            1.0, 2.0, 3.0, 4.0, 5.0,
            6.0, 7.0, 8.0, 9.0, 10.0,
            11.0, 12.0, 13.0, 14.0, 15.0,
            16.0, 17.0, 18.0, 19.0, 20.0,
            21.0, 22.0, 23.0, 24.0, 25.0
        };
        
        // Test matrix W (5x5 identity)
        float W1[25] = {
            1.0, 0.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 1.0, 0.0,
            0.0, 0.0, 0.0, 0.0, 1.0
        };
        
        // Calculate expected result (A * I = A)
        float C1_expected[25];
        for (int i = 0; i < 25; i++) C1_expected[i] = 0.0f;
        for (int i = 0; i < k1; i++) {
            for (int j = 0; j < k3; j++) {
                for (int k = 0; k < k2; k++) {
                    C1_expected[i * k3 + j] += A1[i * k2 + k] * W1[k * k3 + j];
                }
            }
        }
        
        // Memory layout
        int a_base = 0;
        int w_base = 256;
        int c_base = 512;
        
        // Reset sequence
        reset.write(true);
        start.write(false);
        wait(20, SC_NS);
        reset.write(false);
        wait(20, SC_NS);
        
        // Initialize memory
        initialize_memory_with_matrices("memory.txt", 
                                       a_base, A1, k1, k2,
                                       w_base, W1, k2, k3);
        
        // Configure test
        K1.write(k1);
        K2.write(k2);
        K3.write(k3);
        A_base_addr.write(a_base);
        W_base_addr.write(w_base);
        C_base_addr.write(c_base);
        output_stationary.write(false);  // WS mode
        
        print_matrix("Input A", A1, k1, k2);
        print_matrix("Input W", W1, k2, k3);
        
        // Start computation
        wait(20, SC_NS);
        start.write(true);
        wait(10, SC_NS);
        start.write(false);
        
        // Wait for completion
        while (!done.read()) {
            wait(10, SC_NS);
        }
        
        wait(50, SC_NS);
        
        // Read result from memory
        float C1_actual[25];
        read_matrix_from_memory("memory.txt", c_base, C1_actual, k1, k3);
        
        print_matrix("Result C (from memory)", C1_actual, k1, k3);
        print_matrix("Expected C", C1_expected, k1, k3);
        
        if (verify_result(C1_expected, C1_actual, k1, k3)) {
            cout << "✓ TEST 1 PASSED" << endl;
        } else {
            cout << "✗ TEST 1 FAILED" << endl;
        }
        
        wait(100, SC_NS);
        
        // ====================================================
        // TEST 2: Same test in OS Mode
        // ====================================================
        // cout << "\n╔═══════════════════════════════════════════════╗" << endl;
        // cout << "║  TEST 2: 5x5 Matrix Multiplication (OS Mode)  ║" << endl;
        // cout << "╚═══════════════════════════════════════════════╝" << endl;
        
        // // Reset
        // reset.write(true);
        // wait(20, SC_NS);
        // reset.write(false);
        // wait(20, SC_NS);
        
        // // Reinitialize memory
        // initialize_memory_with_matrices("memory.txt", 
        //                                a_base, A1, k1, k2,
        //                                w_base, W1, k2, k3);
        
        // // Configure for OS mode
        // output_stationary.write(true);  // OS mode
        
        // // Start computation
        // wait(20, SC_NS);
        // start.write(true);
        // wait(10, SC_NS);
        // start.write(false);
        
        // // Wait for completion
        // while (!done.read()) {
        //     wait(10, SC_NS);
        // }
        
        // wait(50, SC_NS);
        
        // // Read result
        // float C2_actual[25];
        // read_matrix_from_memory("memory.txt", c_base, C2_actual, k1, k3);
        
        // print_matrix("Result C (from memory)", C2_actual, k1, k3);
        // print_matrix("Expected C", C1_expected, k1, k3);
        
        // if (verify_result(C1_expected, C2_actual, k1, k3)) {
        //     cout << "✓ TEST 2 PASSED" << endl;
        // } else {
        //     cout << "✗ TEST 2 FAILED" << endl;
        // }
        
        // wait(100, SC_NS);
        
        // cout << "\n╔═══════════════════════════════════════════════╗" << endl;
        // cout << "║           All Tests Completed                 ║" << endl;
        // cout << "╚═══════════════════════════════════════════════╝\n" << endl;
        
        sc_stop();
    }
};

// Main function
int sc_main(int argc, char* argv[])
{
    Testbench tb("tb");
    
    // Run simulation
    sc_start();
    
    return 0;
}