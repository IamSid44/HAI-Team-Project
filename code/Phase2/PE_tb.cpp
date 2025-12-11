#include "PE.h"

using namespace std;

SC_MODULE(PE_tb)
{
    // Clock and reset
    sc_clock clk;
    sc_signal<bool> reset;

    // Check if its output stationary or weight/input stationary
    sc_signal<bool> output_stationary;

    // Input from the top, input from the left, output to the right, output to the bottom
    sc_signal<float> in_top;
    sc_signal<float> in_left;
    sc_signal<float> out_right;
    sc_signal<float> out_bottom;

    // Preloading buffer and signal
    sc_signal<bool> preload_valid;
    sc_signal<float> preload_data;

    // PE instance
    PE* pe;

    // Constructor
    SC_CTOR(PE_tb) : clk("clk", 10, SC_NS) // 10ns clock period
    {
        // Instantiate PE
        pe = new PE("PE");
        pe->clk(clk);
        pe->reset(reset);
        pe->output_stationary(output_stationary);
        pe->in_top(in_top);
        pe->in_left(in_left);
        pe->out_right(out_right);
        pe->out_bottom(out_bottom);
        pe->preload_valid(preload_valid);
        pe->preload_data(preload_data);

        // Initialize signals
        SC_CTHREAD(testbench, clk);
    }

    void testbench()
    {
        // Reset sequence
        reset.write(true);
        preload_valid.write(false);
        output_stationary.write(true); // Set to output stationary mode
        in_top.write(0);
        in_left.write(0);
        preload_data.write(0);
        
        wait(); // Wait for clock edge
        wait(); // Wait for another clock edge
        
        reset.write(false); // Release reset
        
        wait(); // Wait for clock edge
        wait(); // Wait for another clock edge
        
        // Preload weights into PE
        cout << "@" << sc_time_stamp() << " Preloading weights..." << endl;
        
        preload_data.write(1); // Weight for first row
        preload_valid.write(true);
        in_left.write(2); // Input for first row
        in_top.write(3);
        wait();

        output_stationary.write(false); // Switch to weight/input stationary mode
        preload_data.write(4); // Weight for second row
        in_left.write(5); // Input for second row
        in_top.write(6);
        wait();
        
        preload_valid.write(false);

        for(int i = 0; i < 5; i++) {
            wait(); // Wait for clock cycles
        }

        sc_stop(); // End simulation
    }

    ~PE_tb()
    {
        delete pe;
    }
};


// sc_main is the SystemC equivalent of main() function
int sc_main(int argc, char* argv[]) {
    cout << "=== PE Simulation ===" << endl;
    cout << "Multiplication using 1 PE" << endl << endl;
    
    // Create top level system
    PE_tb system("PE_tb");
    
    // Create VCD (Value Change Dump) trace file for waveform viewing
    sc_trace_file* tf = sc_create_vcd_trace_file("PE");
    tf->set_time_unit(1, SC_NS);    // 1ns time resolution
    
    // Trace all important signals (can be viewed in GTKWave)
    sc_trace(tf, system.clk, "clk");
    sc_trace(tf, system.reset, "reset");
    sc_trace(tf, system.output_stationary, "output_stationary");
    sc_trace(tf, system.in_top, "in_top");
    sc_trace(tf, system.in_left, "in_left");
    sc_trace(tf, system.out_right, "out_right");
    sc_trace(tf, system.out_bottom, "out_bottom");
    sc_trace(tf, system.preload_valid, "preload_valid");
    sc_trace(tf, system.preload_data, "preload_data");
        
    cout << "Starting simulation..." << endl << endl;
    
    sc_start(1000, SC_NS);
    
    cout << "\n=== Simulation Results ===" << endl;
    cout << "Simulation completed at time: " << sc_time_stamp() << endl;
    
    // Close trace file
    sc_close_vcd_trace_file(tf);
    
    return 0;
}