#ifndef L1_H
#define L1_H

#include <systemc.h>
#include <map>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>

// Pull in the SA dimensions so we know the array port widths
#include "SAmxn.h" 

SC_MODULE(L1_Memory)
{
    sc_in<bool> clk;
    sc_in<bool> reset;

    // === Mode (from Controller) ===
    // This tells the memory how to handle writes
    // false = Weight Stationary (accumulate on write)
    // true  = Output Stationary (overwrite on write)
    sc_in<bool> output_stationary_mode;

    // === Read Ports (for SA_top and SA_left) ===
    // We assume an ideal, combinational read (no latency)
    // Address comes in, data goes out in the same cycle.
    sc_in<int> read_addr_top[N];
    sc_in<int> read_addr_left[M];

    sc_out<float> data_out_top[N];
    sc_out<float> data_out_left[M];

    // === Write Ports (from SA_bottom) ===
    sc_in<int> write_addr[N];    // Address to write to
    sc_in<bool> write_en[N];    // Write enable for this channel
    sc_in<float> data_in_bottom[N]; // Data from SA

    // --- Internal Storage ---
    int memory_size;
    std::map<int, float> memory_bank;

    // --- Processes ---
    void read_process() {
        // This is a combinational read, so it's not sensitive to clk
        // We just continuously update outputs based on inputs

        // [FIX] The for-loop that was here has been removed.
        // It only executed once at time 0 and was misleading.

        while(true) {
            // Handle TOP read ports
            for(int i = 0; i < N; ++i) {
                int addr = read_addr_top[i].read();
                // cout << "L1_Memory: Read from TOP addr " << addr << endl;
                if (addr >= 0 && memory_bank.count(addr)) {
                    data_out_top[i].write(memory_bank[addr]);
                } else {
                    data_out_top[i].write(0.0f); // Read from uninitialized/invalid address
                }
                
                // [FIX] You can uncomment this line to see the live address and data
                // cout<<"L1_Memory: Read from TOP addr " << addr << " -> data " << data_out_top[i].read() << endl;
            }

            // Handle LEFT read ports
            for(int i = 0; i < M; ++i) {
                int addr = read_addr_left[i].read();
                if (addr >= 0 && memory_bank.count(addr)) {
                    data_out_left[i].write(memory_bank[addr]);
                } else {
                    data_out_left[i].write(0.0f); // Read from uninitialized/invalid address
                }
            }
            wait(); // Wait for any input change
            // print all read_top_addr
            for(int i = 0; i < N; ++i) {
                // Print all read_top_addr
                cout << "L1_Memory: Read from TOP addr " << read_addr_top[i].read()
                     << " -> data " << memory_bank[read_addr_top[i].read()] << endl;
            }
            
        }
    }

    void write_process() {
        while(true) {
            if (reset.read()) {
                // Don't clear memory on reset, just wait
            } else {
                for(int i = 0; i < N; ++i) {
                    if (write_en[i].read() == true) {
                        int addr = write_addr[i].read();
                        float data = data_in_bottom[i].read();
                        
                        // Check for valid address
                        if (addr >= 0 && addr < memory_size) {
                            if (output_stationary_mode.read() == true) {
                                // OS Mode: Overwrite
                                memory_bank[addr] = data;
                            } else {
                                // WS Mode: Accumulate
                                // Read-modify-write in one cycle
                                if (memory_bank.count(addr)) {
                                    memory_bank[addr] += data;
                                } else {
                                    memory_bank[addr] = data;
                                }
                            }
                        }
                    }
                }
            }
            wait(); // Wait for positive clock edge
        }
    }

    // --- Helper for Testbench ---
    // This allows the testbench to read memory contents after simulation
    float read_debug(int address) {
        if (memory_bank.count(address)) {
            return memory_bank[address];
        }
        return 0.0f; // Return 0 if address not found
    }


    // --- Constructor ---
    SC_CTOR(L1_Memory)
    {
        memory_size = 0;
        memory_bank.clear();
        
        // --- Load Initial Memory State from Config File ---
        std::ifstream config_file("L1_config.txt");
        std::string line;
        
        if (!config_file.is_open()) {
            cout << "Error: [L1_Memory] Could not open L1_config.txt" << endl;
            return;
        }

        cout << "[L1_Memory] Loading config from L1_config.txt..." << endl;
        while (std::getline(config_file, line)) {
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#') {
                continue;
            }

            // Print the line being read
            // cout << "[L1_Memory] Read line: " << line << endl;

            std::stringstream ss(line);
            std::string key;
            ss >> key;

            if (key == "MEMORY_SIZE") {
                ss >> memory_size;
                cout << "[L1_Memory] Set MEMORY_SIZE = " << memory_size << endl;
            } 
            else if (key == "BANK") {
                // This just acts as a section header, we read data until next blank line or EOF
                int bank_id;
                ss >> bank_id;
                cout << "[L1_Memory] Loading data for BANK " << bank_id << "..." << endl;
                
                int loaded_count = 0;
                while (std::getline(config_file, line) && !line.empty()) {
                    if (line[0] == '#') continue; // skip comments
                    
                    std::stringstream data_ss(line);
                    int addr;
                    float value;
                    data_ss >> addr >> value;
                    if (addr >= 0) {
                        memory_bank[addr] = value;
                        // cout << addr << " hi " << memory_bank[addr] << endl;
                        // cout << "[L1_Memory] Loaded addr " << addr << " -> data " << value << endl;
                        loaded_count++;
                    }
                }
                cout << "[L1_Memory] Loaded " << loaded_count << " initial values." << endl;
            }
        }
        config_file.close();
        cout << "[L1_Memory] Config loaded. Bank size: " << memory_bank.size() << " entries." << endl;


        // --- Register Processes ---
        SC_THREAD(read_process);
        // Sensitive to all read address inputs
        for(int i=0; i<N; ++i) sensitive << read_addr_top[i];
        for(int i=0; i<M; ++i) sensitive << read_addr_left[i];

        SC_CTHREAD(write_process, clk.pos());
        reset_signal_is(reset, true);
    }
};

#endif // L1_H
