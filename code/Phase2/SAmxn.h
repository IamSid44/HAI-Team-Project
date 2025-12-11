#ifndef SA_MXN_H
#define SA_MXN_H

#include "PE.h"
#include <string>

// Define the dimensions of the Systolic Array
// You can change these values before including this header
#ifndef M
#define M 5 // Default number of rows
#endif

#ifndef N
#define N 5 // Default number of columns
#endif

using namespace std;

// SA_MxN Module (Generalized Systolic Array)
SC_MODULE(SA_MxN)
{
    // Clock and reset
    sc_in<bool> clk;
    sc_in<bool> reset;

    // Check if its output stationary or weight/input stationary
    sc_in<bool> output_stationary;

    // Input from the top (N inputs, one for each column)
    // Input from the left (M inputs, one for each row)
    // Output to the right (M outputs, one from each row)
    // Output to the bottom (N outputs, one from each column)
    sc_in<float> in_top[N];
    sc_in<float> in_left[M];
    sc_out<float> out_right[M];
    sc_out<float> out_bottom[N];

    // Preloading buffer and signal
    sc_in<bool> preload_valid;
    // Preload data for all M*N PEs.
    // Assumes data is flattened in row-major order (PE[0][0], PE[0][1], ..., PE[M-1][N-1])
    sc_in<float> preload_data[M * N]; 

    // Internal signals for PE connections
    // Horizontal connections: M rows, N-1 signals each
    sc_signal<float> pe_out_right[M][N - 1];
    // Vertical connections: M-1 signals each, N columns
    sc_signal<float> pe_out_bottom[M - 1][N];

    // Array ID for this SA_MxN instance
    int array_id;

    // Array of pointers to PE modules
    PE* pe_array[M][N];

    // Custom Constructor with array_id
    SA_MxN(sc_module_name name, int array_id) : sc_module(name), array_id(array_id)
    {
        // Instantiate PEs (M rows, N columns)
        for(int i = 0; i < M; i++) // Row index
        {
            for(int j = 0; j < N; j++) // Column index
            {
                // Create a unique name for each PE
                string pe_name = "PE_" + to_string(i) + "_" + to_string(j);
                pe_array[i][j] = new PE(pe_name.c_str(), array_id, i, j);
                
                // Connect clock and reset
                pe_array[i][j]->clk(clk);
                pe_array[i][j]->reset(reset);
                
                // Connect output_stationary mode signal
                pe_array[i][j]->output_stationary(output_stationary);
                
                // Connect preload valid signal
                pe_array[i][j]->preload_valid(preload_valid);
                
                // --- Connect PE inputs (data flow) ---

                // Connect left input (from SA boundary or previous PE)
                if(j == 0)
                    pe_array[i][j]->in_left(in_left[i]); // From SA left boundary
                else
                    pe_array[i][j]->in_left(pe_out_right[i][j - 1]); // From PE to the left
                
                // Connect top input (from SA boundary or previous PE)
                if(i == 0)
                    pe_array[i][j]->in_top(in_top[j]); // From SA top boundary
                else
                    pe_array[i][j]->in_top(pe_out_bottom[i - 1][j]); // From PE above

                // --- Connect PE outputs (data flow) ---

                // Connect right output (to SA boundary or next PE)
                if(j == N - 1)
                    pe_array[i][j]->out_right(out_right[i]); // To SA right boundary
                else
                    pe_array[i][j]->out_right(pe_out_right[i][j]); // To PE to the right
                
                // Connect bottom output (to SA boundary or next PE)
                if(i == M - 1)
                    pe_array[i][j]->out_bottom(out_bottom[j]); // To SA bottom boundary
                else
                    pe_array[i][j]->out_bottom(pe_out_bottom[i][j]); // To PE below

                // Connect preload data input (using row-major indexing)
                pe_array[i][j]->preload_data(preload_data[i * N + j]);
            }
        }

        cout << "SA_" << M << "x" << N << " Module Created with array_id: " << array_id << endl;
    }

    // Destructor to clean up dynamically allocated PEs
    ~SA_MxN() {
        for (int i = 0; i < M; ++i) {
            for (int j = 0; j < N; ++j) {
                delete pe_array[i][j];
            }
        }
    }
};

#endif // SA_MXN_H
