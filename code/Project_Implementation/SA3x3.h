#ifndef SA3x3_H
#define SA3x3_H

#include "PE.h"

#define num_PEs 4

using namespace std;

// SA3x3 Module
SC_MODULE(SA3x3)
{
    // Clock and reset
    sc_in<bool> clk;
    sc_in<bool> reset;

    // Check if its output stationary or weight/input stationary
    sc_in<bool> output_stationary;

    // Input from the top, input from the left, output to the right, output to the bottom, they are num_PEs floats each
    sc_in<float> in_top[num_PEs];
    sc_in<float> in_left[num_PEs];
    sc_out<float> out_right[num_PEs];
    sc_out<float> out_bottom[num_PEs];

    // Preloading buffer and signal
    sc_in<bool> preload_valid;
    sc_in<float> preload_data[num_PEs * num_PEs]; //c Broadcasted for now, but ideally the data must flow through the columns

    // Internal signals for PE connections, instantiate only those needed
    sc_signal<float> pe_out_right[num_PEs][num_PEs - 1];
    sc_signal<float> pe_out_bottom[num_PEs - 1][num_PEs];

    // Array ID for this SA3x3 instance
    int array_id;

    // Constructor
    // SC_CTOR(SA3x3) : array_id(0)

    // Custom Constructor with array_id
    SA3x3(sc_module_name name, int array_id) : sc_module(name), array_id(array_id)
    {
        // Instantiate PEs
        PE* pe[num_PEs][num_PEs];
        for(int i = 0; i < num_PEs; i++)
        {
            for(int j = 0; j < num_PEs; j++)
            {
                string pe_name = "PE_" + to_string(i) + "_" + to_string(j);
                pe[i][j] = new PE(pe_name.c_str(), array_id, i, j);
                
                // Connect clock and reset
                pe[i][j]->clk(clk);
                pe[i][j]->reset(reset);
                
                // Connect output_stationary
                pe[i][j]->output_stationary(output_stationary);
                
                pe[i][j]->preload_valid(preload_valid);
                
                // Make all the connections here to the internals signals
                if(j == 0)
                    pe[i][j]->in_left(in_left[i]);
                else
                    pe[i][j]->in_left(pe_out_right[i][j - 1]);
                
                if(i == 0)
                    pe[i][j]->in_top(in_top[j]);
                else
                    pe[i][j]->in_top(pe_out_bottom[i - 1][j]);

                if(j == num_PEs - 1)
                    pe[i][j]->out_right(out_right[i]);
                else
                    pe[i][j]->out_right(pe_out_right[i][j]);
                
                if(i == num_PEs - 1)
                    pe[i][j]->out_bottom(out_bottom[j]);
                else
                    pe[i][j]->out_bottom(pe_out_bottom[i][j]);

                pe[i][j]->preload_data(preload_data[i * num_PEs + j]);
            }
        }

        cout << "SA3x3 Module Created with array_id: " << array_id << endl;
    }
};

#endif // SA3x3_H