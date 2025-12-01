#ifndef PE_H
#define PE_H

#include <systemc.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <sys/stat.h>

using namespace std;

// Processing Element (PE) - Basic compute unit in systolic array
// Supports two dataflow modes:
// 1. Weight Stationary (WS): Weights preloaded, activations stream through
// 2. Output Stationary (OS): Outputs accumulate in PE, then drain
SC_MODULE(PE)
{
    // Clock and reset
    sc_in<bool> clk;
    sc_in<bool> reset;

    // Dataflow mode control
    sc_in<bool> output_stationary;

    // Dataflow ports (4-way connectivity)
    sc_in<float> in_top;        // Input from above
    sc_in<float> in_left;       // Input from left
    sc_out<float> out_right;    // Output to right
    sc_out<float> out_bottom;   // Output to bottom

    // Preload interface (for WS mode)
    sc_in<bool> preload_valid;
    sc_in<float> preload_data;

    // Internal state
    float preload_buffer;       // Holds preloaded weight (WS mode)
    float accumulator_buffer;   // Accumulates partial sums
    float right_out_buffer;     // Output register (right)
    float bottom_out_buffer;    // Output register (bottom)
    bool drain_value_sent;      // OS drain state tracking

    // PE identification for logging
    int array_id, row_id, col_id;
    string log_filename;
    bool first_write;

    // Constructor with PE coordinates
    PE(sc_module_name name, int array_id, int row_id, int col_id) : 
      sc_module(name), array_id(array_id), row_id(row_id), col_id(col_id), 
      first_write(true), drain_value_sent(false)
    {
        // Create output directory if needed
        mkdir("PE_Outputs", 0755);
        
        log_filename = "PE_Outputs/PE_" + to_string(array_id) + "_" + 
                       to_string(row_id) + "_" + to_string(col_id) + ".txt";
        
        SC_THREAD(process);
        sensitive << clk.pos();
        dont_initialize();
    }

    // Main PE processing logic
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
                    // ============================================
                    // OUTPUT STATIONARY MODE
                    // ============================================
                    if (preload_valid.read() == true) 
                    {
                        // DRAIN PHASE: Output accumulated results
                        if (drain_value_sent == false) {
                            // First cycle: output our accumulated value
                            bottom_out_buffer = accumulator_buffer;
                            drain_value_sent = true;
                        } else {
                            // Subsequent cycles: pass through from top
                            bottom_out_buffer = in_top.read();
                        }
                        right_out_buffer = 0;
                    } 
                    else 
                    {
                        // ACCUMULATION PHASE: Compute partial products
                        accumulator_buffer += in_top.read() * in_left.read();
                        right_out_buffer = in_left.read();   // Pass A right
                        bottom_out_buffer = in_top.read();   // Pass W down
                        drain_value_sent = false;
                    }
                }
                else
                {
                    // ============================================
                    // WEIGHT STATIONARY MODE
                    // ============================================
                    if (preload_valid.read() == true)
                    {
                        // Load weight into PE
                        preload_buffer = preload_data.read();
                    }

                    // Compute: MAC with preloaded weight
                    accumulator_buffer = in_left.read() * preload_buffer + in_top.read();
                    bottom_out_buffer = accumulator_buffer;
                    right_out_buffer = in_left.read();
                    drain_value_sent = false;
                }

                // Write outputs
                out_right.write(right_out_buffer);
                out_bottom.write(bottom_out_buffer);
            }
            
            // Log PE state every cycle
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
            file << "Preload Buffer: " << preload_buffer << endl;
            file << "Accumulator Buffer: " << accumulator_buffer << endl;
            file << "-----------------------------------------------------" << endl;
            file.close();
            first_write = false;
        }
        else
        {
            cout << "Error: Unable to open file " << log_filename << " for writing" << endl;
        }
    }
};

#endif // PE_H