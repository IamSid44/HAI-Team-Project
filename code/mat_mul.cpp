#include <systemc.h>
#include <vector>
#include <iostream>
#include <iomanip>

using namespace std;

/*
 * HANDSHAKING PROTOCOL EXPLANATION:
 * * In SystemC, we use handshaking signals to ensure proper data transfer between modules.
 * A complete 4-phase handshake uses:
 * * 1. REQ (Request): Master asserts when it wants to initiate a transaction
 * 2. ACK (Acknowledge): Slave asserts when it has processed the request
 * 3. VALID: Indicates that data on the bus is valid and stable
 * 4. READY: Indicates that the receiver is ready to accept data
 * * The handshake sequence:
 * 1. Master asserts REQ and puts data on bus
 * 2. Master asserts VALID when data is stable
 * 3. Slave asserts READY when it can accept data
 * 4. Transaction occurs when both VALID and READY are high
 * 5. Slave asserts ACK to confirm completion
 * 6. Master deasserts REQ, slave deasserts ACK (return to idle)
 */

// Memory Module - Acts as a slave device that responds to memory requests
SC_MODULE(memory_module) {
    // Clock and Reset - Standard for all sequential logic in SystemC
    sc_in<bool> clk;        // Clock input - drives all sequential processes
    sc_in<bool> rst_n;      // Active-low reset (when false, module resets)
    
    // 4-Phase Handshake Interface (Memory acts as SLAVE)
    sc_in<bool> mem_req;        // REQUEST: Master wants to access memory
    sc_out<bool> mem_ack;       // ACKNOWLEDGE: Memory confirms transaction complete
    sc_in<bool> mem_valid;      // VALID: Master's data/address is stable and valid
    sc_out<bool> mem_ready;     // READY: Memory is ready to accept request
    
    // Memory Interface Signals
    sc_in<sc_uint<16>> mem_addr;    // Address bus (16-bit = 64K addresses)
    sc_in<bool> mem_wr_en;        // Write Enable (1=write, 0=read)
    sc_in<sc_int<32>> mem_wr_data; // Write data bus (32-bit data)
    sc_out<sc_int<32>> mem_rd_data;// Read data bus (32-bit data)
    sc_out<bool> mem_data_valid;  // Indicates read data is valid
    
    // Internal memory storage - vector acts like RAM
    vector<sc_int<32>> memory;
    static const int MEMORY_SIZE = 1024;    // 1024 words of 32-bit data
    
    // SC_CTOR is SystemC constructor macro
    SC_CTOR(memory_module) : memory(MEMORY_SIZE, 0) {
        // SC_CTHREAD creates a clocked thread (sequential process)
        // Executes on positive clock edge
        SC_CTHREAD(memory_process, clk.pos());
        // reset_signal_is tells SystemC which signal resets this process
        reset_signal_is(rst_n, false);    // Reset when rst_n is false
    }
    
    void memory_process() {
        // RESET BEHAVIOR: Initialize all outputs to safe values
        mem_ack.write(false);
        mem_ready.write(true);      // Memory starts ready
        mem_rd_data.write(0);
        mem_data_valid.write(false);
        
        wait(); // Wait one clock cycle after reset
        
        // MAIN OPERATION LOOP
        while(true) {
            // Check if master wants to start a transaction
            if(mem_req.read() && mem_valid.read() && mem_ready.read()) {
                // All handshake conditions met - process the request
                mem_ready.write(false); // Busy processing, not ready for new requests
                
                if(mem_wr_en.read()) {
                    // WRITE OPERATION
                    cout << "@" << sc_time_stamp() << " Memory: Writing " 
                         << mem_wr_data.read() << " to address " 
                         << mem_addr.read() << endl;
                    memory[mem_addr.read()] = mem_wr_data.read();
                    
                    // Acknowledge write completion
                    mem_ack.write(true);
                    wait(); // Hold ACK for one cycle
                    mem_ack.write(false);
                } else {
                    // READ OPERATION    
                    cout << "@" << sc_time_stamp() << " Memory: Reading from address " 
                         << mem_addr.read() << " = " << memory[mem_addr.read()] << endl;
                    mem_rd_data.write(memory[mem_addr.read()]);
                    mem_data_valid.write(true);
                    
                    // Acknowledge read completion
                    mem_ack.write(true);
                    wait(); // Hold ACK and data for one cycle
                    mem_ack.write(false);
                    mem_data_valid.write(false);
                }
                
                mem_ready.write(true);  // Ready for next transaction
            }
            wait(); // Wait for next clock cycle
        }
    }
};

// Matrix Multiply Accelerator - Acts as a master device that initiates memory requests
SC_MODULE(matrix_mult_accelerator) {
    // Clock and Reset
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    
    // Control Interface (with system controller)
    sc_in<bool> start;      // Start signal from controller
    sc_out<bool> done;      // Completion signal to controller
    sc_out<bool> busy;      // Busy indicator
    
    // 4-Phase Handshake Interface (Accelerator acts as MASTER)
    sc_out<bool> mem_req;       // REQUEST: Accelerator wants memory access
    sc_in<bool> mem_ack;        // ACKNOWLEDGE: Memory confirms completion
    sc_out<bool> mem_valid;     // VALID: Accelerator's address/data is stable
    sc_in<bool> mem_ready;      // READY: Memory can accept request
    
    // Memory Interface (same as memory module)
    sc_out<sc_uint<16>> mem_addr;
    sc_out<bool> mem_wr_en;
    sc_out<sc_int<32>> mem_wr_data;
    sc_in<sc_int<32>> mem_rd_data;
    sc_in<bool> mem_data_valid;
    
    // Configuration inputs (set by system controller)
    sc_in<sc_uint<8>> matrix_size;    // Size of square matrices (NxN)
    sc_in<sc_uint<16>> matrix_a_addr; // Base address of matrix A
    sc_in<sc_uint<16>> matrix_b_addr; // Base address of matrix B    
    sc_in<sc_uint<16>> matrix_c_addr; // Base address of result matrix C
    
    // State Machine States - enum creates named constants
    enum state_t { 
        IDLE,       // Waiting for start signal
        READ_A,     // Reading element from matrix A
        WAIT_A,     // Waiting for A read to complete
        READ_B,     // Reading element from matrix B
        WAIT_B,     // Waiting for B read to complete
        COMPUTE,    // Computing partial product
        WRITE_C,    // Writing result to matrix C
        WAIT_C,     // Waiting for C write to complete
        DONE_STATE  // Signaling completion
    };
    
    // Internal state and control signals
    sc_signal<int> state;           // Current state of state machine
    sc_signal<sc_uint<8>> i, j, k;  // Loop counters for matrix indices
    sc_signal<sc_int<64>> accumulator; // Accumulates partial products (64-bit to prevent overflow)
    sc_signal<sc_int<32>> a_val, b_val; // Temporary storage for matrix elements
    
    SC_CTOR(matrix_mult_accelerator) {
        SC_CTHREAD(accelerator_process, clk.pos());
        reset_signal_is(rst_n, false);
    }
    
    void accelerator_process() {
        // RESET BEHAVIOR
        done.write(false);
        busy.write(false);
        mem_req.write(false);
        mem_valid.write(false);
        mem_addr.write(0);
        mem_wr_en.write(false);
        mem_wr_data.write(0);
        state.write(IDLE);
        i.write(0); j.write(0); k.write(0);
        accumulator.write(0);
        
        wait();
        
        while(true) {
            switch(state.read()) {
                case IDLE:
                    done.write(false);
                    busy.write(false);
                    // Wait for start command from controller
                    if(start.read()) {
                        cout << "@" << sc_time_stamp() << " Accelerator: Starting matrix multiplication" << endl;
                        busy.write(true);
                        i.write(0); j.write(0); k.write(0);
                        accumulator.write(0);
                        state.write(READ_A);
                    }
                    break;
                    
                case READ_A:
                    // Initiate read of A[i][k]
                    mem_req.write(true);        // Assert request
                    mem_valid.write(true);      // Address is valid
                    mem_wr_en.write(false);     // Read operation
                    // Calculate linear address: base + row*size + col
                    mem_addr.write(matrix_a_addr.read() + i.read() * matrix_size.read() + k.read());
                    state.write(WAIT_A);
                    break;
                    
                case WAIT_A:
                    // Wait for memory to complete read operation
                    if(mem_ack.read() && mem_data_valid.read()) {
                        // Transaction complete - capture data
                        a_val.write(mem_rd_data.read());
                        mem_req.write(false);   // Deassert request
                        mem_valid.write(false); // Address no longer valid
                        state.write(READ_B);
                        cout << "@" << sc_time_stamp() << " Accelerator: Read A[" 
                             << i.read() << "][" << k.read() << "] = " << mem_rd_data.read() << endl;
                    }
                    break;
                    
                case READ_B:
                    // Initiate read of B[k][j]
                    mem_req.write(true);
                    mem_valid.write(true);
                    mem_wr_en.write(false);
                    mem_addr.write(matrix_b_addr.read() + k.read() * matrix_size.read() + j.read());
                    state.write(WAIT_B);
                    break;
                    
                case WAIT_B:
                    // Wait for memory to complete read operation
                    if(mem_ack.read() && mem_data_valid.read()) {
                        b_val.write(mem_rd_data.read());
                        mem_req.write(false);
                        mem_valid.write(false);
                        state.write(COMPUTE);
                        cout << "@" << sc_time_stamp() << " Accelerator: Read B[" 
                             << k.read() << "][" << j.read() << "] = " << mem_rd_data.read() << endl;
                    }
                    break;
                    
                case COMPUTE:
                    // Perform multiply-accumulate: C[i][j] += A[i][k] * B[k][j]
                    accumulator.write(accumulator.read() + a_val.read() * b_val.read());
                    cout << "@" << sc_time_stamp() << " Accelerator: Accumulator += " 
                         << a_val.read() << " * " << b_val.read() 
                         << " = " << (accumulator.read() + a_val.read() * b_val.read()) << endl;
                    
                    // Check if we've processed all k values for this C[i][j]
                    if(k.read() == matrix_size.read() - 1) {
                        // Finished computing C[i][j], write result to memory
                        state.write(WRITE_C);
                    } else {
                        // More k values to process
                        k.write(k.read() + 1);
                        state.write(READ_A);
                    }
                    break;
                    
                case WRITE_C:
                    // Write computed result C[i][j] to memory
                    mem_req.write(true);
                    mem_valid.write(true);
                    mem_wr_en.write(true);      // Write operation
                    mem_addr.write(matrix_c_addr.read() + i.read() * matrix_size.read() + j.read());
                    mem_wr_data.write(accumulator.read());
                    state.write(WAIT_C);
                    break;
                    
                case WAIT_C:
                    // Wait for write to complete
                    if(mem_ack.read()) {
                        mem_req.write(false);
                        mem_valid.write(false);
                        mem_wr_en.write(false);
                        cout << "@" << sc_time_stamp() << " Accelerator: Wrote C[" 
                             << i.read() << "][" << j.read() << "] = " << accumulator.read() << endl;
                        
                        // Reset for next element
                        accumulator.write(0);
                        k.write(0);
                        
                        // Update matrix indices (nested loop: for i, for j)
                        if(j.read() == matrix_size.read() - 1) {
                            // End of row, move to next row
                            if(i.read() == matrix_size.read() - 1) {
                                // End of matrix - computation complete
                                state.write(DONE_STATE);
                            } else {
                                // Next row, reset column
                                i.write(i.read() + 1);
                                j.write(0);
                                state.write(READ_A);
                            }
                        } else {
                            // Next column in same row
                            j.write(j.read() + 1);
                            state.write(READ_A);
                        }
                    }
                    break;
                    
                case DONE_STATE:
                    done.write(true);
                    busy.write(false);
                    cout << "@" << sc_time_stamp() << " Accelerator: Matrix multiplication complete" << endl;
                    // Stay in done state until start is deasserted
                    if(!start.read()) {
                        state.write(IDLE);
                    }
                    break;
            }
            wait();
        }
    }
};

// System Controller - Orchestrates the entire system operation
SC_MODULE(system_controller) {
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    
    // Accelerator control interface
    sc_out<bool> start_accel;   // Start signal to accelerator
    sc_in<bool> accel_done;     // Done signal from accelerator
    sc_in<bool> accel_busy;     // Busy signal from accelerator
    
    // Memory interface (controller also acts as master)
    sc_out<bool> mem_req;
    sc_in<bool> mem_ack;
    sc_out<bool> mem_valid;
    sc_in<bool> mem_ready;
    sc_out<sc_uint<16>> mem_addr;
    sc_out<bool> mem_wr_en;
    sc_out<sc_int<32>> mem_wr_data;
    sc_in<sc_int<32>> mem_rd_data;
    sc_in<bool> mem_data_valid;
    
    // Configuration outputs to accelerator
    sc_out<sc_uint<8>> matrix_size;
    sc_out<sc_uint<16>> matrix_a_addr;
    sc_out<sc_uint<16>> matrix_b_addr;
    sc_out<sc_uint<16>> matrix_c_addr;
    
    // Controller state machine
    enum ctrl_state_t { 
        INIT,           // Initialize system
        LOAD_MATRICES,  // Load test data into memory
        WAIT_LOAD,      // Wait for memory write to complete
        START_COMPUTE,  // Start accelerator
        WAIT_DONE,      // Wait for computation to complete
        READ_RESULTS,   // Read results from memory
        WAIT_READ,      // Wait for memory read to complete
        FINISHED        // All operations complete
    };
    
    sc_signal<int> ctrl_state;
    sc_signal<int> init_counter;    // Counter for initialization loops
    
    SC_CTOR(system_controller) {
        SC_CTHREAD(controller_process, clk.pos());
        reset_signal_is(rst_n, false);
    }
    
    void controller_process() {
        // RESET BEHAVIOR
        start_accel.write(false);
        mem_req.write(false);
        mem_valid.write(false);
        mem_addr.write(0);
        mem_wr_en.write(false);
        mem_wr_data.write(0);
        
        // Configure matrix operation (3x3 matrices)
        matrix_size.write(3);
        matrix_a_addr.write(0);     // Matrix A starts at address 0
        matrix_b_addr.write(16);    // Matrix B starts at address 16
        matrix_c_addr.write(32);    // Matrix C starts at address 32
        
        ctrl_state.write(INIT);
        init_counter.write(0);
        
        wait();
        
        while(true) {
            switch(ctrl_state.read()) {
                case INIT:
                    cout << "@" << sc_time_stamp() << " Controller: System initialization" << endl;
                    ctrl_state.write(LOAD_MATRICES);
                    break;
                    
                case LOAD_MATRICES:
                    // Load test data: Matrix A and Matrix B
                    if(init_counter.read() < 18) { // 2 matrices of 3x3 = 18 elements
                        if(mem_ready.read()) {  // Only start if memory is ready
                            mem_req.write(true);
                            mem_valid.write(true);
                            mem_wr_en.write(true);
                            
                            if(init_counter.read() < 9) {
                                // Load Matrix A: [1,2,3; 4,5,6; 7,8,9]
                                mem_addr.write(init_counter.read());
                                mem_wr_data.write(init_counter.read() + 1);
                            } else {
                                // Load Matrix B: [1,3,5; 7,9,11; 13,15,17] 
                                mem_addr.write(16 + (init_counter.read() - 9));
                                // mem_wr_data.write((init_counter.read() - 9) * 2 + 1);
                                mem_wr_data.write((init_counter.read() - 9) + 1);
                            }
                            ctrl_state.write(WAIT_LOAD);
                        }
                    } else {
                        cout << "@" << sc_time_stamp() << " Controller: Test matrices loaded" << endl;
                        ctrl_state.write(START_COMPUTE);
                    }
                    break;
                    
                case WAIT_LOAD:
                    if(mem_ack.read()) {
                        mem_req.write(false);
                        mem_valid.write(false);
                        mem_wr_en.write(false);
                        init_counter.write(init_counter.read() + 1);
                        ctrl_state.write(LOAD_MATRICES);
                    }
                    break;
                    
                case START_COMPUTE:
                    start_accel.write(true);
                    cout << "@" << sc_time_stamp() << " Controller: Starting matrix multiplication" << endl;
                    ctrl_state.write(WAIT_DONE);
                    break;
                    
                case WAIT_DONE:
                    if(accel_done.read()) {
                        start_accel.write(false);
                        cout << "@" << sc_time_stamp() << " Controller: Computation completed" << endl;
                        init_counter.write(0);
                        ctrl_state.write(READ_RESULTS);
                    }
                    break;
                    
                case READ_RESULTS:
                    // Read result matrix C and display
                    if(init_counter.read() < 9) { // 3x3 = 9 elements
                        if(mem_ready.read()) {
                            mem_req.write(true);
                            mem_valid.write(true);
                            mem_wr_en.write(false);   // Read operation
                            mem_addr.write(32 + init_counter.read());
                            ctrl_state.write(WAIT_READ);
                        }
                    } else {
                        ctrl_state.write(FINISHED);
                    }
                    break;
                    
                case WAIT_READ:
                    if(mem_ack.read() && mem_data_valid.read()) {
                        mem_req.write(false);
                        mem_valid.write(false);
                        
                        int row = init_counter.read() / 3;
                        int col = init_counter.read() % 3;
                        cout << "Result Matrix C[" << row << "][" << col << "] = " 
                             << mem_rd_data.read() << endl;
                        
                        init_counter.write(init_counter.read() + 1);
                        ctrl_state.write(READ_RESULTS);
                    }
                    break;
                    
                case FINISHED:
                    cout << "@" << sc_time_stamp() << " Controller: All operations completed successfully" << endl;
                    cout << "\nExpected result for C = A*B where:" << endl;
                    cout << "A = [[1, 2, 3], [4, 5, 6], [7, 8, 9]]" << endl;
                    cout << "B = [[1, 3, 5], [7, 9, 11], [13, 15, 17]]" << endl;
                    cout << "C = [[54, 66, 78], [117, 147, 177], [180, 228, 276]]" << endl;
                    sc_stop();  // End simulation
                    break;
            }
            wait();
        }
    }
};

// Top Level System Module - Integrates all components
SC_MODULE(llm_accelerator_system) {
    // Clock generation - 10ns period = 100MHz
    sc_clock clk;
    sc_signal<bool> rst_n;
    
    // Module instances (pointers for dynamic allocation)
    memory_module *mem;
    matrix_mult_accelerator *accel;
    system_controller *ctrl;
    
    // Memory interface signals (shared between controller and accelerator)
    sc_signal<bool> mem_req_ctrl, mem_req_accel, mem_req;
    sc_signal<bool> mem_valid_ctrl, mem_valid_accel, mem_valid;
    sc_signal<sc_uint<16>> mem_addr_ctrl, mem_addr_accel, mem_addr;
    sc_signal<bool> mem_wr_en_ctrl, mem_wr_en_accel, mem_wr_en;
    sc_signal<sc_int<32>> mem_wr_data_ctrl, mem_wr_data_accel, mem_wr_data;
    sc_signal<bool> mem_ack, mem_ready, mem_data_valid;
    sc_signal<sc_int<32>> mem_rd_data;
    
    // Control signals between controller and accelerator
    sc_signal<bool> start_accel, accel_done, accel_busy;
    sc_signal<sc_uint<8>> matrix_size;
    sc_signal<sc_uint<16>> matrix_a_addr, matrix_b_addr, matrix_c_addr;
    
    SC_CTOR(llm_accelerator_system) : clk("clk", 10, SC_NS) {
        // Instantiate all modules using 'new' operator
        mem = new memory_module("memory_module");
        accel = new matrix_mult_accelerator("matrix_mult_accelerator");
        ctrl = new system_controller("system_controller");
        
        // Connect memory module ports to internal signals
        mem->clk(clk);
        mem->rst_n(rst_n);
        mem->mem_req(mem_req);
        mem->mem_ack(mem_ack);
        mem->mem_valid(mem_valid);
        mem->mem_ready(mem_ready);
        mem->mem_addr(mem_addr);
        mem->mem_wr_en(mem_wr_en);
        mem->mem_wr_data(mem_wr_data);
        mem->mem_rd_data(mem_rd_data);
        mem->mem_data_valid(mem_data_valid);
        
        // Connect accelerator module
        accel->clk(clk);
        accel->rst_n(rst_n);
        accel->start(start_accel);
        accel->done(accel_done);
        accel->busy(accel_busy);
        accel->mem_req(mem_req_accel);
        accel->mem_ack(mem_ack);
        accel->mem_valid(mem_valid_accel);
        accel->mem_ready(mem_ready);
        accel->mem_addr(mem_addr_accel);
        accel->mem_wr_en(mem_wr_en_accel);
        accel->mem_wr_data(mem_wr_data_accel);
        accel->mem_rd_data(mem_rd_data);
        accel->mem_data_valid(mem_data_valid);
        accel->matrix_size(matrix_size);
        accel->matrix_a_addr(matrix_a_addr);
        accel->matrix_b_addr(matrix_b_addr);
        accel->matrix_c_addr(matrix_c_addr);
        
        // Connect controller module
        ctrl->clk(clk);
        ctrl->rst_n(rst_n);
        ctrl->start_accel(start_accel);
        ctrl->accel_done(accel_done);
        ctrl->accel_busy(accel_busy);
        ctrl->mem_req(mem_req_ctrl);
        ctrl->mem_ack(mem_ack);
        ctrl->mem_valid(mem_valid_ctrl);
        ctrl->mem_ready(mem_ready);
        ctrl->mem_addr(mem_addr_ctrl);
        ctrl->mem_wr_en(mem_wr_en_ctrl);
        ctrl->mem_wr_data(mem_wr_data_ctrl);
        ctrl->mem_rd_data(mem_rd_data);
        ctrl->mem_data_valid(mem_data_valid);
        ctrl->matrix_size(matrix_size);
        ctrl->matrix_a_addr(matrix_a_addr);
        ctrl->matrix_b_addr(matrix_b_addr);
        ctrl->matrix_c_addr(matrix_c_addr);
        
        // Memory arbiter - decides who gets access to memory
        // SC_METHOD runs whenever its sensitivity list signals change
        SC_METHOD(memory_arbiter);
        sensitive << mem_req_ctrl << mem_req_accel << mem_valid_ctrl << mem_valid_accel
                  << mem_addr_ctrl << mem_addr_accel << mem_wr_en_ctrl << mem_wr_en_accel
                  << mem_wr_data_ctrl << mem_wr_data_accel << accel_busy;
        
        // System reset and control process
        // FIX IS HERE: Changed clk.pos() to just clk
        SC_CTHREAD(system_process, clk);
    }
    
    void memory_arbiter() {
        // ARBITRATION LOGIC: Accelerator has priority when busy
        // This prevents controller from interfering during computation
        if(accel_busy.read()) {
            // Route accelerator signals to memory
            mem_req.write(mem_req_accel.read());
            mem_valid.write(mem_valid_accel.read());
            mem_addr.write(mem_addr_accel.read());
            mem_wr_en.write(mem_wr_en_accel.read());
            mem_wr_data.write(mem_wr_data_accel.read());
        } else {
            // Route controller signals to memory
            mem_req.write(mem_req_ctrl.read());
            mem_valid.write(mem_valid_ctrl.read());
            mem_addr.write(mem_addr_ctrl.read());
            mem_wr_en.write(mem_wr_en_ctrl.read());
            mem_wr_data.write(mem_wr_data_ctrl.read());
        }
    }
    
    void system_process() {
        // Generate reset sequence
        rst_n.write(false); // Assert reset
        wait(3);            // Hold reset for 3 clock cycles
        rst_n.write(true);  // Release reset
        cout << "@" << sc_time_stamp() << " System: Reset released, starting simulation" << endl;
        
        // Main system loop (could add system-level monitoring here)
        while(true) {
            wait();
        }
    }
    
    // Destructor - clean up dynamically allocated modules
    ~llm_accelerator_system() {
        delete mem;
        delete accel;
        delete ctrl;
    }
};

// sc_main is the SystemC equivalent of main() function
int sc_main(int argc, char* argv[]) {
    cout << "=== LLM Inference Accelerator Simulation ===" << endl;
    cout << "Testing 3x3 matrix multiplication using SystemC" << endl;
    cout << "Handshaking Protocol: REQ/ACK/VALID/READY" << endl << endl;
    
    // Create top level system
    llm_accelerator_system system("llm_accelerator_system");
    
    // Create VCD (Value Change Dump) trace file for waveform viewing
    sc_trace_file* tf = sc_create_vcd_trace_file("llm_accelerator_trace");
    tf->set_time_unit(1, SC_NS);    // 1ns time resolution
    
    // Trace all important signals (can be viewed in GTKWave)
    sc_trace(tf, system.clk, "clk");
    sc_trace(tf, system.rst_n, "rst_n");
    
    // Memory interface signals
    sc_trace(tf, system.mem_req, "mem_req");
    sc_trace(tf, system.mem_ack, "mem_ack");
    sc_trace(tf, system.mem_valid, "mem_valid");
    sc_trace(tf, system.mem_ready, "mem_ready");
    sc_trace(tf, system.mem_addr, "mem_addr");
    sc_trace(tf, system.mem_wr_en, "mem_wr_en");
    sc_trace(tf, system.mem_wr_data, "mem_wr_data");
    sc_trace(tf, system.mem_rd_data, "mem_rd_data");
    sc_trace(tf, system.mem_data_valid, "mem_data_valid");
    
    // Control signals
    sc_trace(tf, system.start_accel, "start_accel");
    sc_trace(tf, system.accel_done, "accel_done");
    sc_trace(tf, system.accel_busy, "accel_busy");
    sc_trace(tf, system.matrix_size, "matrix_size");
    
    // Accelerator internal signals
    sc_trace(tf, system.accel->state, "accel_state");
    sc_trace(tf, system.accel->i, "accel_i");
    sc_trace(tf, system.accel->j, "accel_j");
    sc_trace(tf, system.accel->k, "accel_k");
    sc_trace(tf, system.accel->accumulator, "accumulator");
    sc_trace(tf, system.accel->a_val, "a_val");
    sc_trace(tf, system.accel->b_val, "b_val");
    
    // Controller internal signals
    sc_trace(tf, system.ctrl->ctrl_state, "ctrl_state");
    sc_trace(tf, system.ctrl->init_counter, "init_counter");
    
    // Memory arbiter signals
    sc_trace(tf, system.mem_req_ctrl, "mem_req_ctrl");
    sc_trace(tf, system.mem_req_accel, "mem_req_accel");
    sc_trace(tf, system.mem_valid_ctrl, "mem_valid_ctrl");
    sc_trace(tf, system.mem_valid_accel, "mem_valid_accel");
    
    cout << "Starting simulation..." << endl;
    cout << "Trace file will be saved as: llm_accelerator_trace.vcd" << endl << endl;
    
    // Start simulation with sufficient time for completion
    // Matrix multiplication of 3x3 matrices should complete well within this time
    sc_start(10000, SC_NS);
    
    cout << "\n=== Simulation Results ===" << endl;
    cout << "Simulation completed at time: " << sc_time_stamp() << endl;
    cout << "Check llm_accelerator_trace.vcd for detailed waveforms" << endl;
    cout << "Use: gtkwave llm_accelerator_trace.vcd" << endl;
    
    // Close trace file
    sc_close_vcd_trace_file(tf);
    
    return 0;
}