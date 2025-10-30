#include "SA3x3.h"

using namespace std;

SC_MODULE(SA3x3_tb)
{

    int matrix_size = 3; // 3x3 matrices for testing

    // Matrix A
    float A[3][3] = {
        {1.0, 2.0, 3.0},
        {4.0, 5.0, 6.0},
        {7.0, 8.0, 9.0}
    };

    // Matrix B
    float W[3][3] = {
        {1.0, 2.0, 3.0},
        {4.0, 5.0, 6.0},
        {7.0, 8.0, 9.0}
    };

    // Clock and reset
    sc_clock clk;
    sc_signal<bool> reset;

    // Check if its output stationary or weight/input stationary
    sc_signal<bool> output_stationary;

    // Input from the top, input from the left, output to the right, output to the bottom
    sc_signal<float> in_top[num_PEs];
    sc_signal<float> in_left[num_PEs];
    sc_signal<float> out_right[num_PEs];
    sc_signal<float> out_bottom[num_PEs];

    // Preloading buffer and signal
    sc_signal<bool> preload_valid;
    sc_signal<float> preload_data[num_PEs * num_PEs]; // Broadcasted for now, but ideally the data must flow through the columns

    // PE instance
    SA3x3* sa3x3;

    // Constructor
    SC_CTOR(SA3x3_tb) : clk("clk", 10, SC_NS) // 10ns clock period
    {
        // Instantiate SA3x3
        sa3x3 = new SA3x3("SA3x3", 1); // array_id = 1
        sa3x3->clk(clk);
        sa3x3->reset(reset);
        sa3x3->output_stationary(output_stationary);
        for (int i = 0; i < num_PEs; i++) {
            sa3x3->in_top[i](in_top[i]);
            sa3x3->in_left[i](in_left[i]);
            sa3x3->out_right[i](out_right[i]);
            sa3x3->out_bottom[i](out_bottom[i]);
        }
        sa3x3->preload_valid(preload_valid);
        for (int i = 0; i < num_PEs * num_PEs; i++) {
            sa3x3->preload_data[i](preload_data[i]);
        }

        cout << "SA3x3 Testbench Created" << endl;

        // Initialize signals
        SC_CTHREAD(testbench, clk);
    }

    void testbench()
    {
        cout << "Starting Testbench..." << endl;
        // First, reset the system
        // Then give inputs and observe outputs for both the types of dataflow
        reset.write(1);
        wait();

        reset.write(0);
        wait();

        // Logic to set signals and pass matrices for input/weight stationary dataflow 
        
        preload_valid.write(1);
        output_stationary.write(0); // Weight stationary
        
        // Preload matrix A
        for(int i = 0; i < num_PEs; i++)
        {
            for(int j = 0; j < num_PEs; j++)
            {    
                if(i < matrix_size && j < matrix_size)
                    preload_data[i * num_PEs + j].write(W[i][j]);
                else
                    preload_data[i * num_PEs + j].write(0);
            }
        }

        wait();

        preload_valid.write(0);
        for(int clks = 0; clks <= 2 * num_PEs - 1; clks++)
        {
            // 0s are passed through the top
            
            // In the case of 3x3 matrix multiplication
            // If clk = 0, in_left[0] = A[0][0], in_left[1] = 0, in_left[2] = 0
            // If clk = 1, in_left[0] = A[1][0], in_left[1] = A[0][1], in_left[2] = 0
            // If clk = 2, in_left[0] = A[2][0], in_left[1] = A[1][1], in_left[2] = A[0][2]
            // If clk = 3, in_left[0] = 0, in_left[1] = A[2][1], in_left[2] = A[1][2]
            // If clk = 4, in_left[0] = 0, in_left[1] = 0, in_left[2] = A[2][2]
            
            // A generalized version of this logic is implemented below
            
            for(int i = 0; i < num_PEs; i++)
            {
                if(clks - i >= 0 && clks - i < num_PEs && i < matrix_size && clks - i < matrix_size)
                    in_left[i].write(A[clks - i][i]);
                else
                    in_left[i].write(0);
                
                in_top[i].write(0); // Always 0s from the top
            }

            wait();

            // cout << "in_left: " << in_left[0].read() << ", " << in_left[1].read() << ", " << in_left[2].read() << endl;
        }
        
        for(int i = 0; i < 5; i++)
        {
            wait(); // Wait for clock cycles
            // cout << "in_left: " << in_left[0].read() << ", " << in_left[1].read() << ", " << in_left[2].read() << endl;
        }

        cout << "Weight stationary dataflow matrix multiplication complete!" << endl;

        // Logic to set signals and pass matrices for output stationary dataflow

        preload_valid.write(1);
        output_stationary.write(1); // Output stationary

        // Preload all 0s
        for(int i = 0; i < num_PEs * num_PEs; i++)
        {
            preload_data[i].write(0);
        }

        wait();

        preload_valid.write(0);
        for(int clks = 0; clks <= 2 * num_PEs - 1; clks++)
        {
            // If A and B are 3x3 matrices and we are doing AxB, the flow is as follows
            // In clk = 0, in_left[0] = A[0][0], in_left[1] = 0, in_left[2] = 0
            //             in_top [0] = B[0][0], in_top [1] = 0, in_top [2] = 0            
            // In clk = 1, in_left[0] = A[0][1], in_left[1] = A[1][0], in_left[2] = 0
            //             in_top [0] = B[1][0], in_top [1] = B[0][1], in_top [2] = 0            
            // In clk = 2, in_left[0] = A[0][2], in_left[1] = A[1][1], in_left[2] = A[2][0]
            //             in_top [0] = B[2][0], in_top [1] = B[1][1], in_top [2] = B[0][2]            
            // In clk = 3, in_left[0] = 0, in_left[1] = A[1][2], in_left[2] = A[2][1]
            //             in_top [0] = 0, in_top [1] = B[2][1], in_top [2] = B[1][2]            
            // In clk = 4, in_left[0] = 0, in_left[1] = 0, in_left[2] = A[2][2]
            //             in_top [0] = 0, in_top [1] = 0, in_top [2] = B[2][2]

            // A generalized version of this logic is implemented below
            for(int i = 0; i < num_PEs; i++)
            {
                if(clks - i >= 0 && clks - i < num_PEs && i < matrix_size && clks - i < matrix_size)
                    in_left[i].write(A[i][clks - i]);
                else
                    in_left[i].write(0);
                
                if(clks - i >= 0 && clks - i < num_PEs && i < matrix_size && clks - i < matrix_size)
                    in_top[i].write(W[clks - i][i]);
                else
                    in_top[i].write(0);
            }
            wait();
        }

        for(int i = 0; i < 5; i++)
        {
            wait(); // Wait for clock cycles
        }

        cout << "Output stationary dataflow matrix multiplication complete!" << endl;

        sc_stop(); // End simulation
    }

    ~SA3x3_tb()
    {
        delete sa3x3;
    }
};


// sc_main is the SystemC equivalent of main() function
int sc_main(int argc, char* argv[]) {
    cout << "=== SA3x3 Simulation ===" << endl;
    cout << "3x3 Matrix Multiplication using SA3x3 Module" << endl << endl;
    
    // Create top level system
    SA3x3_tb system("SA3x3_tb");

    // Create VCD (Value Change Dump) trace file for waveform viewing
    sc_trace_file* tf = sc_create_vcd_trace_file("SA3x3");
    tf->set_time_unit(1, SC_NS);    // 1ns time resolution
    
    // Trace all important signals (can be viewed in GTKWave)
    sc_trace(tf, system.clk, "clk");
    sc_trace(tf, system.reset, "reset");
    sc_trace(tf, system.output_stationary, "output_stationary");
    sc_trace(tf, system.preload_valid, "preload_valid");

    for (int i = 0; i < num_PEs; i++)
    {
        // Trace top/left inputs
        sc_trace(tf, system.in_top[i], "in_top_" + std::to_string(i));
        sc_trace(tf, system.in_left[i], "in_left_" + std::to_string(i));

        // Trace bottom/right outputs
        sc_trace(tf, system.out_right[i], "out_right_" + std::to_string(i));
        sc_trace(tf, system.out_bottom[i], "out_bottom_" + std::to_string(i));
    }

    for (int i = 0; i < num_PEs * num_PEs; i++) 
    {
        sc_trace(tf, system.preload_data[i], "preload_data_" + std::to_string(i));
    }


    cout << "Starting simulation..." << endl << endl;
    
    sc_start(1000, SC_NS);
    
    cout << "\n=== Simulation Results ===" << endl;
    cout << "Simulation completed at time: " << sc_time_stamp() << endl;
    
    // Close trace file
    sc_close_vcd_trace_file(tf);
    
    return 0;
}