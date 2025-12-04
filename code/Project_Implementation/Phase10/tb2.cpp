#include <systemc.h>
#include "Control.h"
#include <iostream>
#include <iomanip>

using namespace std;

// Helper Functions
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
    
    for (int i = 0; i < k1; i++) {
        for (int j = 0; j < k2; j++) {
            int byte_addr = a_base + (i * k2 + j) * 4;
            int float_index = byte_addr / 4;
            int line_index = float_index + 4;
            
            if (line_index < lines.size()) {
                ostringstream oss;
                oss << setw(8) << setfill('0') << byte_addr << ": " 
                    << fixed << setprecision(6) << A[i * k2 + j];
                lines[line_index] = oss.str();
            }
        }
    }
    
    for (int i = 0; i < k2_w; i++) {
        for (int j = 0; j < k3; j++) {
            int byte_addr = w_base + (i * k3 + j) * 4;
            int float_index = byte_addr / 4;
            int line_index = float_index + 4;
            
            if (line_index < lines.size()) {
                ostringstream oss;
                oss << setw(8) << setfill('0') << byte_addr << ": " 
                    << fixed << setprecision(6) << W[i * k3 + j];
                lines[line_index] = oss.str();
            }
        }
    }
    
    ofstream file_out(filename, ios::trunc);
    for (const auto& l : lines) {
        file_out << l << endl;
    }
    file_out.close();
    
    cout << "Memory initialized with test matrices" << endl;
}

void read_matrix_from_memory(const char* filename, int base_addr, 
                             float* matrix, int rows, int cols)
{
    ifstream file(filename);
    if (!file.is_open()) return;
    
    string line;
    vector<string> data_lines;
    
    while (getline(file, line)) {
        if (!line.empty() && line[0] != '#') {
            data_lines.push_back(line);
        }
    }
    file.close();
    
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

void print_matrix_truncated(const char* name, float* matrix, int rows, int cols)
{
    cout << name << " (" << rows << "x" << cols << "):" << endl;
    int max_rows = min(rows, 5);
    int max_cols = min(cols, 10);
    
    for (int i = 0; i < max_rows; i++) {
        cout << "  ";
        for (int j = 0; j < max_cols; j++) {
            cout << setw(10) << fixed << setprecision(4) << matrix[i * cols + j] << " ";
        }
        if (cols > 10) cout << "...";
        cout << endl;
    }
    if (rows > 5) cout << "  ..." << endl;
    cout << endl;
}

bool verify_result(float* C_expected, float* C_actual, int k1, int k3, float tolerance = 0.001)
{
    bool passed = true;
    int mismatch_count = 0;
    for (int i = 0; i < k1; i++) {
        for (int j = 0; j < k3; j++) {
            float diff = abs(C_expected[i * k3 + j] - C_actual[i * k3 + j]);
            if (diff > tolerance) {
                if (mismatch_count < 10) {
                    cout << "Mismatch at C[" << i << "][" << j << "]: "
                         << "expected " << C_expected[i * k3 + j]
                         << ", got " << C_actual[i * k3 + j]
                         << " (diff: " << diff << ")" << endl;
                }
                mismatch_count++;
                passed = false;
            }
        }
    }
    if (mismatch_count > 10) {
        cout << "... and " << (mismatch_count - 10) << " more mismatches" << endl;
    }
    return passed;
}

SC_MODULE(Testbench)
{
    sc_clock clk;
    sc_signal<bool> reset;
    sc_signal<bool> start;
    sc_signal<bool> done;
    sc_signal<int> K1, K2, K3;
    sc_signal<int> A_base_addr;
    sc_signal<int> W_base_addr;
    sc_signal<int> C_base_addr;
    sc_signal<bool> output_stationary;
    
    MemoryBackedController* dut;
    sc_trace_file* vcd_file;
    
    SC_CTOR(Testbench) : clk("clk", 10, SC_NS)
    {
        vcd_file = sc_create_vcd_trace_file("waveform");
        vcd_file->set_time_unit(1, SC_NS);
        
        sc_trace(vcd_file, clk, "clk");
        sc_trace(vcd_file, reset, "reset");
        sc_trace(vcd_file, start, "start");
        sc_trace(vcd_file, done, "done");
        sc_trace(vcd_file, K1, "K1");
        sc_trace(vcd_file, K2, "K2");
        sc_trace(vcd_file, K3, "K3");
        sc_trace(vcd_file, output_stationary, "output_stationary");
        
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
        
        SC_THREAD(test_process);
    }
    
    ~Testbench()
    {
        sc_close_vcd_trace_file(vcd_file);
        delete dut;
    }
    
    void test_process()
    {
        cout << "╔════════════════════════════════════════════════╗" << endl;
        cout << "║  Systolic Array Matrix Multiplication Tests   ║" << endl;
        cout << "╚════════════════════════════════════════════════╝" << endl;
        cout << "SA Grid Size: " << PE_ROWS << "x" << PE_COLS << endl;
        cout << endl;

        int tests_passed = 0;
        int tests_failed = 0;
        
        // =====================================================
        // TEST 1: 16x16 (WS Mode)
        // =====================================================
        cout << "\n╔════════════════════════════════════════════════╗" << endl;
        cout << "║  TEST 1: 16x16 x 16x16 Matrix Mult (WS Mode)  ║" << endl;
        cout << "╚════════════════════════════════════════════════╝" << endl;
        
        int k1 = 16, k2 = 16, k3 = 16;
        
        float A1[256] = {
            6.0, -1.0, -4.0, 4.0, 7.0, -3.0, 1.0, -5.0, -5.0, -2.0, 5.0, 3.0, 0.0, 2.0, 8.0, -8.0, 
            8.0, 8.0, -9.0, 5.0, 9.0, 6.0, -1.0, 8.0, 6.0, -8.0, -2.0, 4.0, -1.0, 2.0, 9.0, -5.0, 
            -7.0, 6.0, -8.0, -4.0, 1.0, 2.0, -9.0, -7.0, -1.0, -5.0, -5.0, 3.0, -1.0, -6.0, -1.0, 6.0, 
            8.0, 9.0, 6.0, 3.0, -7.0, 1.0, -3.0, 6.0, 8.0, -7.0, -2.0, -1.0, -4.0, -4.0, -7.0, -9.0, 
            3.0, 3.0, 4.0, 3.0, -7.0, 8.0, -6.0, 1.0, -6.0, -6.0, -8.0, 1.0, 0.0, -1.0, -5.0, 8.0, 
            -8.0, -9.0, -3.0, -6.0, -5.0, -4.0, 6.0, 9.0, -4.0, -5.0, -7.0, -4.0, 0.0, -5.0, -2.0, 4.0, 
            -4.0, 7.0, 9.0, -7.0, 6.0, 5.0, -4.0, -8.0, -7.0, -2.0, 8.0, 5.0, 6.0, 7.0, 2.0, 0.0, 
            1.0, 8.0, -9.0, 0.0, -5.0, 5.0, 1.0, -1.0, -7.0, -9.0, 9.0, -6.0, 6.0, -6.0, -5.0, 1.0, 
            9.0, 3.0, -1.0, 9.0, 6.0, -2.0, -6.0, -7.0, 7.0, 8.0, 1.0, 4.0, 3.0, -5.0, 2.0, -5.0, 
            9.0, 5.0, -2.0, -3.0, -5.0, 1.0, 2.0, 9.0, 2.0, 8.0, -8.0, -6.0, -2.0, -6.0, 4.0, -1.0, 
            -7.0, 5.0, 5.0, 2.0, 8.0, -8.0, -5.0, -9.0, 1.0, 0.0, -5.0, -2.0, 6.0, -5.0, -5.0, 9.0, 
            9.0, -2.0, 6.0, -5.0, -3.0, -8.0, 3.0, 5.0, -7.0, 0.0, -3.0, 2.0, 1.0, -1.0, -7.0, 1.0, 
            9.0, 3.0, -6.0, 5.0, -6.0, 1.0, 4.0, 6.0, 7.0, -3.0, 6.0, 5.0, -3.0, -4.0, -4.0, 0.0, 
            0.0, -6.0, 0.0, -5.0, -5.0, 5.0, 5.0, 7.0, 2.0, 8.0, -9.0, -8.0, 8.0, 8.0, -3.0, -5.0, 
            6.0, 4.0, -1.0, 0.0, 2.0, 8.0, -1.0, -8.0, -3.0, -2.0, -9.0, 8.0, 7.0, -1.0, -1.0, 0.0, 
            -7.0, -9.0, 3.0, 4.0, -1.0, -7.0, -8.0, -2.0, 0.0, -7.0, -6.0, 5.0, 1.0, -6.0, 0.0, 2.0
        };

        float W1[256] = {
            3.0, 0.0, 6.0, 0.0, -6.0, -2.0, 3.0, -7.0, 6.0, -1.0, 1.0, 3.0, 2.0, -3.0, 3.0, 6.0, 
            -2.0, 5.0, 1.0, -8.0, -2.0, -6.0, 1.0, 7.0, 9.0, -8.0, -3.0, 9.0, -2.0, 2.0, 3.0, 6.0, 
            -3.0, 4.0, -6.0, 6.0, 6.0, 0.0, 4.0, 3.0, 4.0, 2.0, -7.0, -5.0, 8.0, -6.0, 3.0, -5.0, 
            9.0, 8.0, -5.0, 6.0, 6.0, -3.0, 3.0, -2.0, -3.0, -1.0, 2.0, 3.0, -9.0, 5.0, 8.0, 2.0, 
            -9.0, 7.0, 7.0, 4.0, -8.0, 9.0, -6.0, -6.0, 2.0, 4.0, 5.0, -4.0, -1.0, -5.0, -6.0, 2.0, 
            7.0, -3.0, -9.0, -7.0, -2.0, 8.0, -7.0, -1.0, -7.0, 4.0, -1.0, 3.0, -1.0, 8.0, 4.0, 1.0, 
            8.0, 4.0, 0.0, 3.0, 1.0, -8.0, 9.0, 0.0, 8.0, 3.0, -8.0, 4.0, -1.0, -4.0, -5.0, 7.0, 
            6.0, -6.0, -6.0, 0.0, -2.0, 5.0, -6.0, -7.0, 7.0, -9.0, -4.0, -8.0, 5.0, 5.0, -7.0, -4.0, 
            -8.0, -9.0, 9.0, 9.0, 3.0, -4.0, -9.0, 2.0, 6.0, -8.0, -7.0, -5.0, -3.0, -6.0, 2.0, -3.0, 
            -5.0, 5.0, 4.0, 3.0, 2.0, -1.0, 1.0, -5.0, -5.0, -3.0, -4.0, -5.0, 5.0, -4.0, -1.0, -7.0, 
            -3.0, 0.0, 0.0, 6.0, -4.0, 1.0, 7.0, -2.0, 2.0, 1.0, -1.0, 3.0, -1.0, -8.0, -1.0, 8.0, 
            3.0, 0.0, -1.0, -1.0, -1.0, 8.0, 3.0, 2.0, -9.0, 3.0, 1.0, 7.0, -9.0, 1.0, 3.0, 3.0, 
            -7.0, 8.0, -8.0, -1.0, -9.0, -2.0, -6.0, -5.0, -1.0, -2.0, -1.0, -3.0, -6.0, 3.0, 5.0, 0.0, 
            4.0, 2.0, 3.0, 6.0, 9.0, 9.0, -2.0, -4.0, -4.0, 6.0, 7.0, 5.0, 4.0, 1.0, -3.0, 3.0, 
            -7.0, 2.0, -9.0, -8.0, -6.0, -4.0, -1.0, 2.0, -6.0, 4.0, 0.0, 3.0, -1.0, -1.0, 3.0, 1.0, 
            3.0, 7.0, -8.0, -7.0, -7.0, 4.0, 9.0, -4.0, -6.0, 9.0, 8.0, -9.0, -7.0, 3.0, -2.0, 3.0
        };
        
        cout << "Computing expected result..." << endl;
        float* C1_expected = new float[k1 * k3];
        for (int i = 0; i < k1 * k3; i++) C1_expected[i] = 0.0f;
        
        for (int i = 0; i < k1; i++) {
            for (int j = 0; j < k3; j++) {
                for (int k = 0; k < k2; k++) {
                    C1_expected[i * k3 + j] += A1[i * k2 + k] * W1[k * k3 + j];
                }
            }
            if (i % 10 == 0) cout << "  Progress: " << i << "/" << k1 << " rows" << endl;
        }
        
        long long theoretical_muls = (long long)k1 * k2 * k3;
        long long theoretical_adds = (long long)k1 * k3 * (k2 - 1);
        long long theoretical_total = theoretical_muls + theoretical_adds;
        
        cout << "\n--- Theoretical Analysis ---" << endl;
        cout << "Dimensions: " << k1 << "x" << k2 << " * " << k2 << "x" << k3 << endl;
        cout << "Multiplications: " << theoretical_muls << endl;
        cout << "Additions: " << theoretical_adds << endl;
        cout << "Total operations: " << theoretical_total << endl;
        
        int a_base = 0;
        int w_base = 10000;
        int c_base = 20000;
        
        reset.write(true);
        start.write(false);
        wait(20, SC_NS);
        reset.write(false);
        wait(20, SC_NS);
        
        initialize_memory_with_matrices("memory.txt", a_base, A1, k1, k2, w_base, W1, k2, k3);
        
        K1.write(k1);
        K2.write(k2);
        K3.write(k3);
        A_base_addr.write(a_base);
        W_base_addr.write(w_base);
        C_base_addr.write(c_base);
        output_stationary.write(false);
        
        print_matrix_truncated("Input A", A1, k1, k2);
        print_matrix_truncated("Input W", W1, k2, k3);
        
        dut->total_memory_reads = 0;
        dut->total_memory_writes = 0;
        
        cout << "\n--- Starting Computation ---" << endl;
        sc_time start_time = sc_time_stamp();
        wait(20, SC_NS);
        start.write(true);
        wait(10, SC_NS);
        start.write(false);
        
        int cycle_count = 0;
        while (!done.read()) {
            wait(10, SC_NS);
            cycle_count++;
            if (cycle_count % 1000 == 0) {
                cout << "  " << cycle_count << " cycles..." << endl;
            }
        }
        sc_time end_time = sc_time_stamp();
        
        wait(50, SC_NS);
        
        float* C1_actual = new float[k1 * k3];
        read_matrix_from_memory("memory.txt", c_base, C1_actual, k1, k3);
        
        print_matrix_truncated("Result C", C1_actual, k1, k3);
        
        cout << "\n--- Performance Metrics ---" << endl;
        cout << "Clock cycles: " << cycle_count << endl;
        cout << "Simulation time: " << (end_time - start_time) << endl;
        cout << "Memory reads: " << dut->total_memory_reads << endl;
        cout << "Memory writes: " << dut->total_memory_writes << endl;
        cout << "Total memory accesses: " << (dut->total_memory_reads + dut->total_memory_writes) << endl;
        
        if (verify_result(C1_expected, C1_actual, k1, k3)) {
            cout << "\n✓ TEST 1 PASSED" << endl;
            tests_passed++;
        } else {
            cout << "\n✗ TEST 1 FAILED" << endl;
            tests_failed++;
        }
        
        // delete[] A1;
        // delete[] W1;
        // delete[] C1_expected;
        // delete[] C1_actual;
        
        wait(100, SC_NS);
        
        // =====================================================
        // TEST 2: 16x8 x 8x16 (WS Mode)
        // =====================================================
        cout << "\n╔════════════════════════════════════════════════╗" << endl;
        cout << "║  TEST 2: 16x8 x 8x16 Matrix Mult (WS Mode)  ║" << endl;
        cout << "╚════════════════════════════════════════════════╝" << endl;

        k1 = 16; k2 = 8; k3 = 16;

        float A2[128] = {
            6.0, -1.0, -4.0, 4.0, 7.0, -3.0, 1.0, -5.0, 
            8.0, 8.0, -9.0, 5.0, 9.0, 6.0, -1.0, 8.0, 
            -7.0, 6.0, -8.0, -4.0, 1.0, 2.0, -9.0, -7.0, 
            8.0, 9.0, 6.0, 3.0, -7.0, 1.0, -3.0, 6.0, 
            3.0, 3.0, 4.0, 3.0, -7.0, 8.0, -6.0, 1.0, 
            -8.0, -9.0, -3.0, -6.0, -5.0, -4.0, 6.0, 9.0, 
            -4.0, 7.0, 9.0, -7.0, 6.0, 5.0, -4.0, -8.0, 
            1.0, 8.0, -9.0, 0.0, -5.0, 5.0, 1.0, -1.0, 
            9.0, 3.0, -1.0, 9.0, 6.0, -2.0, -6.0, -7.0, 
            9.0, 5.0, -2.0, -3.0, -5.0, 1.0, 2.0, 9.0, 
            -7.0, 5.0, 5.0, 2.0, 8.0, -8.0, -5.0, -9.0, 
            9.0, -2.0, 6.0, -5.0, -3.0, -8.0, 3.0, 5.0, 
            9.0, 3.0, -6.0, 5.0, -6.0, 1.0, 4.0, 6.0, 
            0.0, -6.0, 0.0, -5.0, -5.0, 5.0, 5.0, 7.0, 
            6.0, 4.0, -1.0, 0.0, 2.0, 8.0, -1.0, -8.0, 
            -7.0, -9.0, 3.0, 4.0, -1.0, -7.0, -8.0, -2.0
        };

        float W2[128] = {
            -5.0, 7.0, -2.0, 8.0, 5.0, 1.0, 3.0, 4.0, 0.0, 3.0, 2.0, -5.0, 8.0, 2.0, -8.0, -5.0, 
            6.0, 2.0, -8.0, 8.0, -2.0, -8.0, 4.0, -6.0, -1.0, -2.0, 2.0, -6.0, 9.0, 4.0, -5.0, -1.0, 
            -1.0, 1.0, -5.0, 0.0, -5.0, -5.0, 3.0, -2.0, -1.0, 6.0, -6.0, -5.0, -1.0, -5.0, 6.0, 9.0, 
            8.0, -7.0, -7.0, 0.0, -2.0, -3.0, -1.0, 2.0, -4.0, 1.0, -4.0, -1.0, -7.0, -7.0, -9.0, 1.0, 
            -6.0, 7.0, -6.0, -3.0, -8.0, 6.0, 1.0, 5.0, 0.0, -3.0, -1.0, -4.0, -5.0, -4.0, 8.0, 0.0, 
            -4.0, 2.0, -5.0, 8.0, -7.0, -9.0, -4.0, -8.0, 0.0, 8.0, -5.0, 8.0, -2.0, -3.0, 4.0, -5.0, 
            -7.0, -3.0, -2.0, -2.0, 8.0, -9.0, 5.0, 8.0, 6.0, 7.0, 7.0, -1.0, 2.0, -1.0, 0.0, 0.0, 
            -7.0, 0.0, -9.0, -7.0, 9.0, -6.0, -6.0, 5.0, 6.0, 1.0, -6.0, -6.0, -5.0, 0.0, 1.0, 2.0
        };

        cout << "Computing expected result..." << endl;
        float* C2_expected = new float[k1 * k3];
        for (int i = 0; i < k1 * k3; i++) C2_expected[i] = 0.0f;
        
        for (int i = 0; i < k1; i++) {
            for (int j = 0; j < k3; j++) {
                for (int k = 0; k < k2; k++) {
                    C2_expected[i * k3 + j] += A2[i * k2 + k] * W2[k * k3 + j];
                }
            }
            if (i % 10 == 0) cout << "  Progress: " << i << "/" << k1 << " rows" << endl;
        }
        
        theoretical_muls = (long long)k1 * k2 * k3;
        theoretical_adds = (long long)k1 * k3 * (k2 - 1);
        theoretical_total = theoretical_muls + theoretical_adds;
        
        cout << "\n--- Theoretical Analysis ---" << endl;
        cout << "Dimensions: " << k1 << "x" << k2 << " * " << k2 << "x" << k3 << endl;
        cout << "Multiplications: " << theoretical_muls << endl;
        cout << "Additions: " << theoretical_adds << endl;
        cout << "Total operations: " << theoretical_total << endl;

        a_base = 0;
        w_base = 12288;
        c_base = 24576;

        reset.write(true);
        wait(20, SC_NS);
        reset.write(false);
        wait(20, SC_NS);

        initialize_memory_with_matrices("memory.txt", a_base, A2, k1, k2, w_base, W2, k2, k3);

        K1.write(k1);
        K2.write(k2);
        K3.write(k3);
        A_base_addr.write(a_base);
        W_base_addr.write(w_base);
        C_base_addr.write(c_base);
        output_stationary.write(false);

        print_matrix_truncated("Input A", A2, k1, k2);
        print_matrix_truncated("Input W", W2, k2, k3);

        dut->total_memory_reads = 0;
        dut->total_memory_writes = 0;

        cout << "\n--- Starting Computation ---" << endl;
        start_time = sc_time_stamp();
        wait(20, SC_NS);
        start.write(true);
        wait(10, SC_NS);
        start.write(false);

        cycle_count = 0;
        while (!done.read()) {
            wait(10, SC_NS);
            cycle_count++;
            if (cycle_count % 1000 == 0) {
                cout << "  " << cycle_count << " cycles..." << endl;
            }
        }
        end_time = sc_time_stamp();

        wait(50, SC_NS);

        float* C2_actual = new float[k1 * k3];
        read_matrix_from_memory("memory.txt", c_base, C2_actual, k1, k3);

        print_matrix_truncated("Result C", C2_actual, k1, k3);

        cout << "\n--- Performance Metrics ---" << endl;
        cout << "Clock cycles: " << cycle_count << endl;
        cout << "Simulation time: " << (end_time - start_time) << endl;
        cout << "Memory reads: " << dut->total_memory_reads << endl;
        cout << "Memory writes: " << dut->total_memory_writes << endl;
        cout << "Total memory accesses: " << (dut->total_memory_reads + dut->total_memory_writes) << endl;

        if (verify_result(C2_expected, C2_actual, k1, k3)) {
            cout << "\n✓ TEST 2 PASSED" << endl;
            tests_passed++;
        } else {
            cout << "\n✗ TEST 2 FAILED" << endl;
            tests_failed++;
        }

        wait(100, SC_NS);
        
        cout << "\n╔════════════════════════════════════════════════╗" << endl;
        cout << "║            Tests Completed                     ║" << endl;
        cout << "╚════════════════════════════════════════════════╝" << endl;
        cout << "Tests Passed: " << tests_passed << endl;
        cout << "Tests Failed: " << tests_failed << endl;
        
        sc_stop();
    }
};

int sc_main(int argc, char* argv[])
{
    Testbench tb("tb");
    sc_start();
    return 0;
}
