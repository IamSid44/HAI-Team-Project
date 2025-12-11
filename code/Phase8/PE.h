#ifndef PE_H
#define PE_H

#include <systemc.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <sys/stat.h>

using namespace std;

// PE Module with Fixed OS Drain Logic
SC_MODULE(PE)
{
    // Clock and reset
    sc_in<bool> clk;
    sc_in<bool> reset;

    // Check if its output stationary or weight/input stationary
    sc_in<bool> output_stationary;

    // Input from the top, input from the left, output to the right, output to the bottom
    sc_in<float> in_top;
    sc_in<float> in_left;
    sc_out<float> out_right;
    sc_out<float> out_bottom;

    // Preloading buffer and signal
    sc_in<bool> preload_valid;
    sc_in<float> preload_data;

    // Internal storage
    float preload_buffer;
    float accumulator_buffer;
    float right_out_buffer;
    float bottom_out_buffer;

    // State for OS Drain: tracks if we've sent our accumulated value
    bool drain_value_sent;

    // PE identification
    int array_id, row_id, col_id;
    string log_filename;
    bool first_write;

    // Custom Constructor with IDs
    PE(sc_module_name name, int array_id, int row_id, int col_id) : 
      sc_module(name), array_id(array_id), row_id(row_id), col_id(col_id), 
      first_write(true), drain_value_sent(false)
    {
        // Create PE_Outputs directory if it doesn't exist
        mkdir("PE_Outputs", 0755);
        
        log_filename = "PE_Outputs/PE_" + to_string(array_id) + "_" + 
                       to_string(row_id) + "_" + to_string(col_id) + ".txt";
        
        SC_THREAD(process);
        sensitive << clk.pos();
        dont_initialize();
    }

    // PE Process with Fixed Drain Logic
    void process()
    {
        while(true)
        {
            if (reset.read() == true)
            {
                // Reset all internal state
                preload_buffer = 0;
                accumulator_buffer = 0; 
                right_out_buffer = 0;
                bottom_out_buffer = 0;
                drain_value_sent = false;

                out_right.write(0);
                out_bottom.write(0);
            }
            else
            {
                if (output_stationary.read() == true)
                {
                    // ===================================
                    // OUTPUT STATIONARY MODE
                    // ===================================
                    if (preload_valid.read() == true) 
                    {
                        // DRAIN PHASE
                        // When preload_valid is high, we're draining results
                        
                        if (drain_value_sent == false) 
                        {
                            // First drain cycle: output our accumulated value
                            bottom_out_buffer = accumulator_buffer;
                            drain_value_sent = true;
                        } 
                        else 
                        {
                            // Subsequent cycles: pass through from top
                            bottom_out_buffer = in_top.read();
                        }
                        
                        right_out_buffer = 0; // Not used during drain
                    } 
                    else 
                    {
                        // ACCUMULATE PHASE
                        // When preload_valid is low, we're accumulating
                        
                        // MAC operation: accumulator += A * W
                        accumulator_buffer += in_top.read() * in_left.read();
                        
                        // Pass inputs to neighbors
                        right_out_buffer = in_left.read();   // Pass A right
                        bottom_out_buffer = in_top.read();   // Pass W down
                        
                        // Reset drain state for next drain phase
                        drain_value_sent = false;
                    }
                }
                else
                {
                    // ===================================
                    // WEIGHT STATIONARY MODE
                    // ===================================
                    if (preload_valid.read() == true)
                    {
                        // Preload weight into local buffer
                        preload_buffer = preload_data.read();
                    }

                    // MAC operation with preloaded weight
                    // accumulator = A * W_preloaded + partial_sum_from_top
                    accumulator_buffer = in_left.read() * preload_buffer + in_top.read();
                    
                    // Output partial sum downward
                    bottom_out_buffer = accumulator_buffer;
                    
                    // Pass activation rightward
                    right_out_buffer = in_left.read();
                    
                    // Reset OS drain state (not used in WS mode)
                    drain_value_sent = false;
                }

                // Write outputs
                out_right.write(right_out_buffer);
                out_bottom.write(bottom_out_buffer);
            }
            
            // Log PE state to file every clock cycle
            write_to_file();
            
            wait();
        }
    }

    // Write PE state to log file
    void write_to_file()
    {
        ios_base::openmode mode = first_write ? ios::trunc : ios::app;
        ofstream file(log_filename, mode);
        if (file.is_open())
        {
            file << "Time: " << sc_time_stamp() << endl;
            file << "  Mode: " << (output_stationary.read() ? "OS" : "WS") << endl;
            file << "  Preload Buffer: " << preload_buffer << endl;
            file << "  Accumulator: " << accumulator_buffer << endl;
            file << "  Inputs: top=" << in_top.read() 
                 << " left=" << in_left.read() << endl;
            file << "  Outputs: right=" << right_out_buffer 
                 << " bottom=" << bottom_out_buffer << endl;
            file << "-----------------------------------------------------" << endl;
            file.close();
            first_write = false;
        }
        else
        {
            cout << "Error: Unable to open file " << log_filename 
                 << " for writing" << endl;
        }
    }
};

#endif // PE_H
