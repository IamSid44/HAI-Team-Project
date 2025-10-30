#ifndef PE_H
#define PE_H

#include <systemc.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <sys/stat.h>

using namespace std;

// PE Module
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
    int FPU_pipeline_delay = 0; 

    // ** NEW ** Internal state for OS Drain
    bool drain_value_sent;

    // PE identification
    int array_id, row_id, col_id;
    string log_filename;
    bool first_write;

    // Custom Constructor with IDs
    PE(sc_module_name name, int array_id, int row_id, int col_id) : 
      sc_module(name), array_id(array_id), row_id(row_id), col_id(col_id), 
      first_write(true), drain_value_sent(false) // Initialize new state
    {
        // Create PE_Outputs directory if it doesn't exist
        mkdir("PE_Outputs", 0755);
        
        log_filename = "PE_Outputs/PE_" + to_string(array_id) + "_" + to_string(row_id) + "_" + to_string(col_id) + ".txt";
        cout << "PE Module Created with ID: " << array_id << "_" << row_id << "_" << col_id << endl;
        SC_THREAD(process);
        sensitive << clk.pos(); // Make sensitive to both clock edge
        dont_initialize();
    }

    void status_print()
    {
        while(true)
        {
            cout << "----------------------------------------" << endl;
            cout << "Time: " << sc_time_stamp() << endl;
            cout << "Preload Buffer : " << preload_buffer << endl;
            cout << "Accumulator Buffer : " << accumulator_buffer << endl;
            cout << "Right Out Buffer : " << right_out_buffer << endl;
            cout << "Bottom Out Buffer : " << bottom_out_buffer << endl;
            cout << "----------------------------------------" << endl;
            wait();       
        }
    }

    // --- MODIFIED PE PROCESS ---
    // Fixes OS Drain logic to remove bubbles
    void process()
    {
        while(true)
        {
            if (reset.read() == true)
            {
                // Reset internal buffers
                preload_buffer = 0;
                accumulator_buffer = 0; 
                right_out_buffer = 0;
                bottom_out_buffer = 0;
                drain_value_sent = false; // Reset drain state

                out_right.write(0);
                out_bottom.write(0);
            }
            else
            {
                if (output_stationary.read() == true)
                {
                    // --- Output Stationary Mode ---
                    if (preload_valid.read() == true) 
                    {
                        // ** OS Drain Phase **
                        if (drain_value_sent == false) 
                        {
                            // First drain cycle: output our stored value
                            bottom_out_buffer = accumulator_buffer;
                            drain_value_sent = true; // Mark as sent
                        } else {
                            // Subsequent cycles: just pass through from top
                            bottom_out_buffer = in_top.read();
                        }
                        right_out_buffer = 0; // Not used in drain
                    } 
                    else 
                    {
                        // ** OS Accumulate Phase **
                        // `preload_valid` is false.
                        accumulator_buffer += in_top.read() * in_left.read();
                        right_out_buffer = in_left.read();  // Pass A right
                        bottom_out_buffer = in_top.read(); // Pass W down
                        drain_value_sent = false; // Reset state for next drain
                    }
                }
                else
                {
                    // --- Weight Stationary Mode ---
                    if (preload_valid.read() == true)
                    {
                        preload_buffer = preload_data.read();
                    }

                    accumulator_buffer = in_left.read() * preload_buffer + in_top.read();
                    bottom_out_buffer = accumulator_buffer;
                    right_out_buffer = in_left.read();
                    drain_value_sent = false; // Reset state
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

    void write_to_file()
    {
        ios_base::openmode mode = first_write ? ios::trunc : ios::app;
        ofstream file(log_filename, mode);
        if (file.is_open())
        {
            file << "Time: " << sc_time_stamp() << endl;
            file << "Preload Buffer: " << preload_buffer << endl;
            file << "Accumulator Buffer: " << accumulator_buffer << endl;
            file << "-----------------------------------------------------" << endl;
            file.close();
            first_write = false; // Set to false after first write
        }
        else
        {
            cout << "Error: Unable to open file " << log_filename << " for writing" << endl;
        }
    }
};

#endif // PE_H

