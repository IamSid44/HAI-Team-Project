#ifndef MEMORY_H
#define MEMORY_H

#include <systemc.h>
#include <fstream>
#include <sstream>
#include <string>
#include <iomanip>

using namespace std;

// Simple byte-addressed memory module backed by a file
// Each row is byte-addressed (4 bytes per float)
// Uninitialized locations store "XXXX"
SC_MODULE(Memory)
{
    // Clock and control signals
    sc_in<bool> clk;
    sc_in<bool> reset;
    
    // Memory interface
    sc_in<bool> read_enable;
    sc_in<bool> write_enable;
    sc_in<int> address;          // Byte address
    sc_in<float> write_data;
    sc_out<float> read_data;
    sc_out<bool> ready;          // Memory operation complete
    
    // Internal storage
    string memory_file;
    int memory_size_bytes;       // Total memory size in bytes
    int memory_size_floats;      // Total memory size in floats
    
    // Constructor
    Memory(sc_module_name name, const char* mem_file, int size_bytes) 
        : sc_module(name), memory_file(mem_file), memory_size_bytes(size_bytes)
    {
        memory_size_floats = size_bytes / 4;  // 4 bytes per float
        
        // Initialize memory file with XXXX for all locations
        initialize_memory_file();
        
        SC_THREAD(memory_process);
        sensitive << clk.pos();
        dont_initialize();
    }
    
    // Initialize memory file with XXXX markers
    void initialize_memory_file()
    {
        ofstream file(memory_file, ios::trunc);
        if (!file.is_open()) {
            cout << "Error: Cannot create memory file " << memory_file << endl;
            return;
        }
        
        // Write header
        file << "# Byte-Addressed Memory File" << endl;
        file << "# Format: BYTE_ADDRESS: VALUE" << endl;
        file << "# XXXX indicates uninitialized memory" << endl;
        file << "#" << endl;
        
        // Initialize all locations to XXXX
        for (int i = 0; i < memory_size_floats; i++) {
            int byte_addr = i * 4;
            file << setw(8) << setfill('0') << byte_addr << ": XXXX" << endl;
        }
        
        file.close();
        cout << "Memory initialized: " << memory_file 
             << " (" << memory_size_bytes << " bytes, " 
             << memory_size_floats << " floats)" << endl;
    }
    
    // Read a float from memory file at byte address
    float read_from_file(int byte_addr)
    {
        if (byte_addr % 4 != 0) {
            cout << "Warning: Unaligned memory access at byte " << byte_addr << endl;
            return 0.0f;
        }
        
        ifstream file(memory_file);
        if (!file.is_open()) {
            cout << "Error: Cannot open memory file for reading" << endl;
            return 0.0f;
        }
        
        string line;
        int target_index = byte_addr / 4;
        int current_index = 0;
        
        while (getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            
            if (current_index == target_index) {
                size_t colon_pos = line.find(':');
                if (colon_pos != string::npos) {
                    string value_str = line.substr(colon_pos + 1);
                    // Trim whitespace
                    value_str.erase(0, value_str.find_first_not_of(" \t"));
                    value_str.erase(value_str.find_last_not_of(" \t\n\r") + 1);
                    
                    if (value_str == "XXXX") {
                        cout << "Warning: Reading uninitialized memory at byte " 
                             << byte_addr << endl;
                        return 0.0f;
                    }
                    return stof(value_str);
                }
            }
            current_index++;
        }
        
        file.close();
        return 0.0f;
    }
    
    // Write a float to memory file at byte address
    void write_to_file(int byte_addr, float value)
    {
        if (byte_addr % 4 != 0) {
            cout << "Warning: Unaligned memory write at byte " << byte_addr << endl;
            return;
        }
        
        ifstream file_in(memory_file);
        if (!file_in.is_open()) return;
        
        vector<string> lines;
        string line;
        
        // Read all lines
        while (getline(file_in, line)) {
            lines.push_back(line);
        }
        file_in.close();
        
        // Find and update the target line
        int target_index = byte_addr / 4;
        int current_index = 0;
        
        for (size_t i = 0; i < lines.size(); i++) {
            if (lines[i].empty() || lines[i][0] == '#') continue;
            
            if (current_index == target_index) {
                ostringstream oss;
                oss << setw(8) << setfill('0') << byte_addr << ": " 
                    << fixed << setprecision(6) << value;
                lines[i] = oss.str();
                break;
            }
            current_index++;
        }
        
        // Write back all lines
        ofstream file_out(memory_file, ios::trunc);
        for (const auto& l : lines) {
            file_out << l << endl;
        }
        file_out.close();
    }
    
    // Memory process - 1 cycle latency for read/write
    // FIX: Made read and write mutually exclusive to avoid race conditions
    void memory_process()
    {
        while (true)
        {
            if (reset.read()) {
                read_data.write(0.0f);
                ready.write(false);
            }
            else {
                // Default: not ready
                ready.write(false);
                
                // FIX: Use if-else to make operations mutually exclusive
                // This prevents race conditions when both enables are high
                if (read_enable.read() && !write_enable.read()) {
                    // Handle read operation
                    int addr = address.read();
                    float data = read_from_file(addr);
                    read_data.write(data);
                    ready.write(true);
                }
                else if (write_enable.read() && !read_enable.read()) {
                    // Handle write operation
                    int addr = address.read();
                    float data = write_data.read();
                    write_to_file(addr, data);
                    ready.write(true);
                }
                // If both are high, do nothing (error condition)
                else if (read_enable.read() && write_enable.read()) {
                    cout << "ERROR: Both read and write enabled at " 
                         << sc_time_stamp() << endl;
                }
            }
            
            wait();
        }
    }
};

#endif // MEMORY_H
