#include <systemc.h>
#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <sstream>

using namespace std;

/*
 * GENERALIZED MATRIX MULTIPLICATION ACCELERATOR
 * ============================================
 * 
 * Architecture Overview:
 * - 3 Processing Elements (PEs) with dedicated L1 caches
 * - Instruction Fetch and Decode Unit
 * - Shared main memory with round-robin arbiter
 * - Custom instruction set for matrix operations
 * 
 * Custom Instruction Format (32-bit):
 * ===================================
 * Bits [31:26] - Opcode (6 bits)
 * Bits [25:21] - rd (destination register/address) (5 bits)  
 * Bits [20:16] - rs1 (source register 1) (5 bits)
 * Bits [15:11] - rs2 (source register 2) (5 bits)
 * Bits [10:0]  - immediate/offset (11 bits)
 * 
 * Supported Instructions:
 * ======================
 * 1. MADD   (000001) - Matrix Add:           rd = [rs1] + [rs2]
 * 2. MSUB   (000010) - Matrix Subtract:      rd = [rs1] - [rs2]  
 * 3. MMUL   (000011) - Matrix Multiply:      rd = [rs1] * [rs2]
 * 4. MLOAD  (000100) - Matrix Load:          rd = memory[rs1 + imm]
 * 5. MSTORE (000101) - Matrix Store:         memory[rd + imm] = [rs1]
 * 6. MSCAL  (000110) - Matrix Scalar Mult:   rd = [rs1] * imm
 * 7. HALT   (111111) - Stop execution
 * 
 * L1 Cache Design:
 * ===============
 * - Cache Size: 256 bytes (64 cache lines)
 * - Cache Line Size: 4 bytes (1 word of 32-bit data)
 * - Number of Sets: 16 sets  
 * - Associativity: 4-way set associative
 * - Replacement Policy: Least Recently Used (LRU)
 * - Write Policy: Write-back with write allocate
 * 
 * LRU Implementation:
 * - Each cache line has an LRU counter
 * - On access, accessed line counter = 0, others increment
 * - Victim selection: line with highest LRU counter
 * 
 * Address Mapping (16-bit address):
 * - Bits [15:6]: Tag (10 bits)
 * - Bits [5:2]:  Set Index (4 bits) - selects 1 of 16 sets
 * - Bits [1:0]:  Block Offset (2 bits) - byte within cache line
 */

// Instruction structure
struct instruction_t {
    sc_uint<6> opcode;      // Operation code
    sc_uint<5> rd;          // Destination register/address
    sc_uint<5> rs1;         // Source register 1
    sc_uint<5> rs2;         // Source register 2
    sc_int<11> imm;         // Immediate value (sign-extended)
    
    instruction_t() : opcode(0), rd(0), rs1(0), rs2(0), imm(0) {}
    
    instruction_t(sc_uint<32> raw_instr) {
        opcode = raw_instr.range(31, 26);
        rd = raw_instr.range(25, 21);
        rs1 = raw_instr.range(20, 16);
        rs2 = raw_instr.range(15, 11);
        imm = raw_instr.range(10, 0);
    }
};

// Instruction opcodes
enum opcode_t {
    OP_MADD   = 0b000001,  // Matrix addition
    OP_MSUB   = 0b000010,  // Matrix subtraction  
    OP_MMUL   = 0b000011,  // Matrix multiplication
    OP_MLOAD  = 0b000100,  // Matrix load from memory
    OP_MSTORE = 0b000101,  // Matrix store to memory
    OP_MSCAL  = 0b000110,  // Matrix scalar multiplication
    OP_HALT   = 0b111111   // Halt execution
};

// Cache Line Structure
struct cache_line_t {
    bool valid;             // Valid bit
    bool dirty;             // Dirty bit  
    sc_uint<10> tag;        // Tag bits
    sc_int<32> data;        // Data stored
    int lru_counter;        // LRU counter for replacement
    
    cache_line_t() : valid(false), dirty(false), tag(0), data(0), lru_counter(0) {}
};

// Instruction Fetch Unit
SC_MODULE(instruction_fetch) {
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    
    // Control interface
    sc_in<bool> fetch_enable;
    sc_out<bool> instr_valid;
    sc_out<sc_uint<32>> instruction;
    sc_in<bool> instr_consumed;
    
    // Internal signals
    sc_signal<sc_uint<16>> pc;  // Program counter
    vector<sc_uint<32>> program_memory;
    sc_signal<bool> program_loaded;
    
    SC_CTOR(instruction_fetch) {
        SC_CTHREAD(fetch_process, clk.pos());
        reset_signal_is(rst_n, false);
        
        // Load instructions from file
        load_program();
    }
    
    void load_program() {
        program_memory.clear();
        
        // Create a sample program for matrix operations
        // Program: Load matrices, multiply them, store result
        
        // MLOAD r1, r0, 100   - Load matrix A from address 100
        program_memory.push_back(create_instruction(OP_MLOAD, 1, 0, 0, 100));
        
        // MLOAD r2, r0, 200   - Load matrix B from address 200  
        program_memory.push_back(create_instruction(OP_MLOAD, 2, 0, 0, 200));
        
        // MMUL r3, r1, r2     - Multiply A * B, store in r3
        program_memory.push_back(create_instruction(OP_MMUL, 3, 1, 2, 0));
        
        // MSTORE r0, r3, 300  - Store result to address 300
        program_memory.push_back(create_instruction(OP_MSTORE, 0, 3, 0, 300));
        
        // MSCAL r4, r1, 5     - Scalar multiply A by 5
        program_memory.push_back(create_instruction(OP_MSCAL, 4, 1, 0, 5));
        
        // MADD r5, r3, r4     - Add matrices r3 + r4
        program_memory.push_back(create_instruction(OP_MADD, 5, 3, 4, 0));
        
        // MSTORE r0, r5, 400  - Store final result
        program_memory.push_back(create_instruction(OP_MSTORE, 0, 5, 0, 400));
        
        // HALT                - End program
        program_memory.push_back(create_instruction(OP_HALT, 0, 0, 0, 0));
        
        cout << "Program loaded with " << program_memory.size() << " instructions" << endl;
        print_program();
    }
    
    sc_uint<32> create_instruction(opcode_t op, int rd, int rs1, int rs2, int imm) {
        sc_uint<32> instr = 0;
        instr.range(31, 26) = op;
        instr.range(25, 21) = rd;
        instr.range(20, 16) = rs1;
        instr.range(15, 11) = rs2;
        instr.range(10, 0) = imm;
        return instr;
    }
    
    void print_program() {
        cout << "\n=== Loaded Program ===" << endl;
        for(int i = 0; i < program_memory.size(); i++) {
            instruction_t instr(program_memory[i]);
            cout << "PC[" << i << "]: ";
            switch(instr.opcode) {
                case OP_MADD:   cout << "MADD  r" << instr.rd << ", r" << instr.rs1 << ", r" << instr.rs2; break;
                case OP_MSUB:   cout << "MSUB  r" << instr.rd << ", r" << instr.rs1 << ", r" << instr.rs2; break;
                case OP_MMUL:   cout << "MMUL  r" << instr.rd << ", r" << instr.rs1 << ", r" << instr.rs2; break;
                case OP_MLOAD:  cout << "MLOAD r" << instr.rd << ", r" << instr.rs1 << ", " << instr.imm.to_int(); break;
                case OP_MSTORE: cout << "MSTORE r" << instr.rd << ", r" << instr.rs1 << ", " << instr.imm.to_int(); break;
                case OP_MSCAL:  cout << "MSCAL r" << instr.rd << ", r" << instr.rs1 << ", " << instr.imm.to_int(); break;
                case OP_HALT:   cout << "HALT"; break;
                default:        cout << "UNKNOWN"; break;
            }
            cout << endl;
        }
        cout << "=====================" << endl << endl;
    }
    
    void fetch_process() {
        pc.write(0);
        instr_valid.write(false);
        instruction.write(0);
        program_loaded.write(true);
        
        wait();
        
        while(true) {
            if(fetch_enable.read() && program_loaded.read()) {
                int current_pc = pc.read();
                
                if(current_pc < program_memory.size()) {
                    instruction.write(program_memory[current_pc]);
                    instr_valid.write(true);
                    
                    cout << "@" << sc_time_stamp() << " IFetch: PC=" << current_pc 
                         << " Instr=0x" << hex << program_memory[current_pc] << dec << endl;
                    
                    // Wait for instruction to be consumed
                    wait();
                    while(!instr_consumed.read()) {
                        wait();
                    }
                    
                    instr_valid.write(false);
                    pc.write(current_pc + 1);
                } else {
                    // End of program
                    instr_valid.write(false);
                    cout << "@" << sc_time_stamp() << " IFetch: End of program reached" << endl;
                }
            }
            wait();
        }
    }
};

// Instruction Decode Unit
SC_MODULE(instruction_decode) {
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    
    // Instruction fetch interface
    sc_in<sc_uint<32>> instruction;
    sc_in<bool> instr_valid;
    sc_out<bool> instr_consumed;
    
    // PE dispatch interface
    sc_out<bool> dispatch_valid;
    sc_out<instruction_t> decoded_instr;
    sc_out<sc_uint<2>> target_pe;  // Which PE to send instruction to
    sc_in<bool> dispatch_ready;
    
    // System control
    sc_out<bool> halt_system;
    
    SC_CTOR(instruction_decode) {
        SC_CTHREAD(decode_process, clk.pos());
        reset_signal_is(rst_n, false);
    }
    
    void decode_process() {
        instr_consumed.write(false);
        dispatch_valid.write(false);
        halt_system.write(false);
        target_pe.write(0);
        
        wait();
        
        int pe_counter = 0;  // Round-robin PE assignment
        
        while(true) {
            if(instr_valid.read()) {
                instruction_t decoded(instruction.read());
                instr_consumed.write(true);
                
                cout << "@" << sc_time_stamp() << " IDecode: Decoding opcode=" 
                     << decoded.opcode << " rd=" << decoded.rd 
                     << " rs1=" << decoded.rs1 << " rs2=" << decoded.rs2 
                     << " imm=" << decoded.imm.to_int() << endl;
                
                if(decoded.opcode == OP_HALT) {
                    cout << "@" << sc_time_stamp() << " IDecode: HALT instruction - stopping system" << endl;
                    halt_system.write(true);
                    wait();
                } else {
                    // Assign instruction to PE using round-robin
                    decoded_instr.write(decoded);
                    target_pe.write(pe_counter % 3);  // 3 PEs
                    dispatch_valid.write(true);
                    
                    cout << "@" << sc_time_stamp() << " IDecode: Dispatching to PE[" 
                         << (pe_counter % 3) << "]" << endl;
                    
                    // Wait for PE to be ready
                    wait();
                    while(!dispatch_ready.read()) {
                        wait();
                    }
                    
                    dispatch_valid.write(false);
                    pe_counter++;
                }
                
                wait();
                instr_consumed.write(false);
            }
            wait();
        }
    }
};

// L1 Cache Module  
SC_MODULE(l1_cache) {
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    
    // PE Interface
    sc_in<bool> pe_req;
    sc_out<bool> pe_ack;
    sc_in<bool> pe_valid;
    sc_out<bool> pe_ready;
    sc_in<sc_uint<16>> pe_addr;
    sc_in<bool> pe_wr_en;
    sc_in<sc_int<32>> pe_wr_data;
    sc_out<sc_int<32>> pe_rd_data;
    sc_out<bool> pe_data_valid;
    
    // Memory Interface
    sc_out<bool> mem_req;
    sc_in<bool> mem_ack;
    sc_out<bool> mem_valid;
    sc_in<bool> mem_ready;
    sc_out<sc_uint<16>> mem_addr;
    sc_out<bool> mem_wr_en;
    sc_out<sc_int<32>> mem_wr_data;
    sc_in<sc_int<32>> mem_rd_data;
    sc_in<bool> mem_data_valid;
    
    // Cache parameters
    static const int NUM_SETS = 16;
    static const int ASSOCIATIVITY = 4;
    
    // Cache storage
    cache_line_t cache_memory[NUM_SETS][ASSOCIATIVITY];
    
    // Statistics
    sc_signal<int> cache_hits;
    sc_signal<int> cache_misses;
    sc_signal<int> cache_id;
    
    // State machine
    enum cache_state_t {
        CACHE_IDLE,
        CACHE_LOOKUP, 
        CACHE_HIT,
        CACHE_MISS_READ,
        CACHE_MISS_WAIT,
        CACHE_WRITEBACK,
        CACHE_WB_WAIT
    };
    
    sc_signal<int> state;
    sc_signal<sc_uint<16>> current_addr;
    sc_signal<sc_int<32>> current_data;
    sc_signal<bool> current_write;
    sc_signal<int> victim_set, victim_way;
    
    SC_CTOR(l1_cache) {
        SC_CTHREAD(cache_process, clk.pos());
        reset_signal_is(rst_n, false);
    }
    
    void cache_process() {
        // Initialize cache
        for(int s = 0; s < NUM_SETS; s++) {
            for(int w = 0; w < ASSOCIATIVITY; w++) {
                cache_memory[s][w] = cache_line_t();
            }
        }
        
        pe_ack.write(false);
        pe_ready.write(true);
        pe_rd_data.write(0);
        pe_data_valid.write(false);
        
        mem_req.write(false);
        mem_valid.write(false);
        mem_addr.write(0);
        mem_wr_en.write(false);
        mem_wr_data.write(0);
        
        state.write(CACHE_IDLE);
        cache_hits.write(0);
        cache_misses.write(0);
        
        wait();
        
        while(true) {
            switch(state.read()) {
                case CACHE_IDLE:
                    pe_ready.write(true);
                    pe_ack.write(false);
                    pe_data_valid.write(false);
                    
                    if(pe_req.read() && pe_valid.read()) {
                        pe_ready.write(false);
                        current_addr.write(pe_addr.read());
                        current_data.write(pe_wr_data.read());
                        current_write.write(pe_wr_en.read());
                        state.write(CACHE_LOOKUP);
                    }
                    break;
                    
                case CACHE_LOOKUP: {
                    sc_uint<16> addr = current_addr.read();
                    sc_uint<10> tag = addr.range(15, 6);
                    sc_uint<4> set_idx = addr.range(5, 2);
                    
                    bool hit_found = false;
                    int hit_way = -1;
                    
                    // Search for hit
                    for(int w = 0; w < ASSOCIATIVITY; w++) {
                        if(cache_memory[set_idx][w].valid && 
                           cache_memory[set_idx][w].tag == tag) {
                            hit_found = true;
                            hit_way = w;
                            break;
                        }
                    }
                    
                    if(hit_found) {
                        // Cache hit
                        cache_hits.write(cache_hits.read() + 1);
                        update_lru(set_idx, hit_way);
                        
                        if(current_write.read()) {
                            cache_memory[set_idx][hit_way].data = current_data.read();
                            cache_memory[set_idx][hit_way].dirty = true;
                        } else {
                            pe_rd_data.write(cache_memory[set_idx][hit_way].data);
                            pe_data_valid.write(true);
                        }
                        state.write(CACHE_HIT);
                    } else {
                        // Cache miss
                        cache_misses.write(cache_misses.read() + 1);
                        int victim_w = find_lru_victim(set_idx);
                        victim_set.write(set_idx);
                        victim_way.write(victim_w);
                        
                        if(cache_memory[set_idx][victim_w].valid && 
                           cache_memory[set_idx][victim_w].dirty) {
                            state.write(CACHE_WRITEBACK);
                        } else {
                            state.write(CACHE_MISS_READ);
                        }
                    }
                    break;
                }
                
                case CACHE_HIT:
                    pe_ack.write(true);
                    wait();
                    pe_ack.write(false);
                    pe_data_valid.write(false);
                    state.write(CACHE_IDLE);
                    break;
                    
                case CACHE_WRITEBACK: {
                    int v_set = victim_set.read();
                    int v_way = victim_way.read();
                    sc_uint<16> wb_addr = (cache_memory[v_set][v_way].tag << 6) | (v_set << 2);
                    
                    mem_req.write(true);
                    mem_valid.write(true);
                    mem_wr_en.write(true);
                    mem_addr.write(wb_addr);
                    mem_wr_data.write(cache_memory[v_set][v_way].data);
                    
                    state.write(CACHE_WB_WAIT);
                    break;
                }
                
                case CACHE_WB_WAIT:
                    if(mem_ack.read()) {
                        mem_req.write(false);
                        mem_valid.write(false);
                        mem_wr_en.write(false);
                        state.write(CACHE_MISS_READ);
                    }
                    break;
                    
                case CACHE_MISS_READ:
                    mem_req.write(true);
                    mem_valid.write(true);
                    mem_wr_en.write(false);
                    mem_addr.write(current_addr.read());
                    state.write(CACHE_MISS_WAIT);
                    break;
                    
                case CACHE_MISS_WAIT:
                    if(mem_ack.read() && mem_data_valid.read()) {
                        mem_req.write(false);
                        mem_valid.write(false);
                        
                        // Install new line
                        sc_uint<16> addr = current_addr.read();
                        sc_uint<10> tag = addr.range(15, 6);
                        int v_set = victim_set.read();
                        int v_way = victim_way.read();
                        
                        cache_memory[v_set][v_way].valid = true;
                        cache_memory[v_set][v_way].dirty = false;
                        cache_memory[v_set][v_way].tag = tag;
                        cache_memory[v_set][v_way].data = mem_rd_data.read();
                        update_lru(v_set, v_way);
                        
                        // Handle original request
                        if(current_write.read()) {
                            cache_memory[v_set][v_way].data = current_data.read();
                            cache_memory[v_set][v_way].dirty = true;
                        } else {
                            pe_rd_data.write(cache_memory[v_set][v_way].data);
                            pe_data_valid.write(true);
                        }
                        
                        state.write(CACHE_HIT);
                    }
                    break;
            }
            wait();
        }
    }
    
    // LRU update function
    void update_lru(int set, int accessed_way) {
        for(int w = 0; w < ASSOCIATIVITY; w++) {
            if(w == accessed_way) {
                cache_memory[set][w].lru_counter = 0;  // Most recently used
            } else {
                cache_memory[set][w].lru_counter++;    // Age other lines
            }
        }
    }
    
    // Find LRU victim
    int find_lru_victim(int set) {
        int max_lru = -1;
        int victim_way = 0;
        
        for(int w = 0; w < ASSOCIATIVITY; w++) {
            if(!cache_memory[set][w].valid) {
                return w;  // Use invalid line first
            }
            if(cache_memory[set][w].lru_counter > max_lru) {
                max_lru = cache_memory[set][w].lru_counter;
                victim_way = w;
            }
        }
        return victim_way;
    }
};

// Processing Element with Matrix Operations
SC_MODULE(processing_element) {
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    
    // Instruction interface
    sc_in<bool> instr_valid;
    sc_in<instruction_t> instruction;
    sc_out<bool> instr_ready;
    
    // Cache interface
    sc_out<bool> cache_req;
    sc_in<bool> cache_ack;
    sc_out<bool> cache_valid;
    sc_in<bool> cache_ready;
    sc_out<sc_uint<16>> cache_addr;
    sc_out<bool> cache_wr_en;
    sc_out<sc_int<32>> cache_wr_data;
    sc_in<sc_int<32>> cache_rd_data;
    sc_in<bool> cache_data_valid;
    
    // Configuration
    sc_signal<int> pe_id;
    
    // Register file (simplified)
    sc_int<32> registers[32];
    
    // State machine
    enum pe_state_t {
        PE_IDLE,
        PE_DECODE,
        PE_EXECUTE,
        PE_MEMORY_READ,
        PE_MEMORY_WRITE,
        PE_WAIT_MEMORY,
        PE_COMPLETE
    };
    
    sc_signal<int> pe_state;
    sc_signal<instruction_t> current_instr;
    sc_signal<sc_int<32>> operand1, operand2, result;
    sc_signal<sc_uint<16>> memory_addr;
    sc_signal<bool> memory_operation;
    
    SC_CTOR(processing_element) {
        SC_CTHREAD(pe_process, clk.pos());
        reset_signal_is(rst_n, false);
        
        // Initialize registers
        for(int i = 0; i < 32; i++) {
            registers[i] = 0;
        }
    }
    
    void pe_process() {
        instr_ready.write(true);
        cache_req.write(false);
        cache_valid.write(false);
        cache_addr.write(0);
        cache_wr_en.write(false);
        cache_wr_data.write(0);
        
        pe_state.write(PE_IDLE);
        
        wait();
        
        while(true) {
            switch(pe_state.read()) {
                case PE_IDLE:
                    instr_ready.write(true);
                    if(instr_valid.read()) {
                        instr_ready.write(false);
                        current_instr.write(instruction.read());
                        pe_state.write(PE_DECODE);
                        
                        cout << "@" << sc_time_stamp() << " PE[" << pe_id.read() 
                             << "]: Received instruction opcode=" << instruction.read().opcode << endl;
                    }
                    break;
                    
                case PE_DECODE: {
                    instruction_t instr = current_instr.read();
                    
                    // Load operands from registers
                    operand1.write(registers[instr.rs1]);
                    operand2.write(registers[instr.rs2]);
                    
                    cout << "@" << sc_time_stamp() << " PE[" << pe_id.read() 
                         << "]: Decoded - rs1=" << registers[instr.rs1] 
                         << " rs2=" << registers[instr.rs2] << endl;
                    
                    if(instr.opcode == OP_MLOAD) {
                        memory_addr.write(registers[instr.rs1] + instr.imm.to_int());
                        pe_state.write(PE_MEMORY_READ);
                    } else if(instr.opcode == OP_MSTORE) {
                        memory_addr.write(registers[instr.rd] + instr.imm.to_int());
                        result.write(registers[instr.rs1]);
                        pe_state.write(PE_MEMORY_WRITE);
                    } else {
                        pe_state.write(PE_EXECUTE);
                    }
                    break;
                }
                
                case PE_EXECUTE: {
                    instruction_t instr = current_instr.read();
                    sc_int<32> op1 = operand1.read();
                    sc_int<32> op2 = operand2.read();
                    sc_int<32> res = 0;
                    
                    switch(instr.opcode) {
                        case OP_MADD:
                            res = op1 + op2;
                            cout << "@" << sc_time_stamp() << " PE[" << pe_id.read() 
                                 << "]: MADD " << op1 << " + " << op2 << " = " << res << endl;
                            break;
                            
                        case OP_MSUB:
                            res = op1 - op2;
                            cout << "@" << sc_time_stamp() << " PE[" << pe_id.read() 
                                 << "]: MSUB " << op1 << " - " << op2 << " = " << res << endl;
                            break;
                            
                        case OP_MMUL:
                            res = op1 * op2;
                            cout << "@" << sc_time_stamp() << " PE[" << pe_id.read() 
                                 << "]: MMUL " << op1 << " * " << op2 << " = " << res << endl;
                            break;
                            
                        case OP_MSCAL:
                            res = op1 * instr.imm.to_int();
                            cout << "@" << sc_time_stamp() << " PE[" << pe_id.read() 
                                 << "]: MSCAL " << op1 << " * " << instr.imm.to_int() << " = " << res << endl;
                            break;
                            
                        default:
                            res = 0;
                            break;
                    }
                    
                    result.write(res);
                    registers[instr.rd] = res;
                    pe_state.write(PE_COMPLETE);
                    break;
                }
                
                case PE_MEMORY_READ:
                    cache_req.write(true);
                    cache_valid.write(true);
                    cache_wr_en.write(false);
                    cache_addr.write(memory_addr.read());
                    pe_state.write(PE_WAIT_MEMORY);
                    memory_operation.write(false);  // Read operation
                    break;
                    
                case PE_MEMORY_WRITE:
                    cache_req.write(true);
                    cache_valid.write(true);
                    cache_wr_en.write(true);
                    cache_addr.write(memory_addr.read());
                    cache_wr_data.write(result.read());
                    pe_state.write(PE_WAIT_MEMORY);
                    memory_operation.write(true);   // Write operation
                    break;
                    
                case PE_WAIT_MEMORY:
                    if(cache_ack.read()) {
                        cache_req.write(false);
                        cache_valid.write(false);
                        cache_wr_en.write(false);
                        
                        if(!memory_operation.read() && cache_data_valid.read()) {
                            // Read operation completed
                            instruction_t instr = current_instr.read();
                            registers[instr.rd] = cache_rd_data.read();
                            
                            cout << "@" << sc_time_stamp() << " PE[" << pe_id.read() 
                                 << "]: MLOAD completed - loaded " << cache_rd_data.read() 
                                 << " into r" << instr.rd << endl;
                        } else {
                            cout << "@" << sc_time_stamp() << " PE[" << pe_id.read() 
                                 << "]: MSTORE completed" << endl;
                        }
                        
                        pe_state.write(PE_COMPLETE);
                    }
                    break;
                    
                case PE_COMPLETE:
                    cout << "@" << sc_time_stamp() << " PE[" << pe_id.read() 
                         << "]: Instruction completed" << endl;
                    pe_state.write(PE_IDLE);
                    break;
            }
            wait();
        }
    }
};

// Main Memory
SC_MODULE(main_memory) {
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    
    sc_in<bool> mem_req;
    sc_out<bool> mem_ack;
    sc_in<bool> mem_valid;
    sc_out<bool> mem_ready;
    sc_in<sc_uint<16>> mem_addr;
    sc_in<bool> mem_wr_en;
    sc_in<sc_int<32>> mem_wr_data;
    sc_out<sc_int<32>> mem_rd_data;
    sc_out<bool> mem_data_valid;
    
    vector<sc_int<32>> memory;
    static const int MEMORY_SIZE = 1024;
    
    SC_CTOR(main_memory) : memory(MEMORY_SIZE, 0) {
        SC_CTHREAD(memory_process, clk.pos());
        reset_signal_is(rst_n, false);
        
        // Initialize test matrices
        initialize_test_data();
    }
    
    void initialize_test_data() {
        // Initialize matrix A at address 100-109
        for(int i = 0; i < 10; i++) {
            memory[100 + i] = i + 1;  // Values 1-10
        }
        
        // Initialize matrix B at address 200-209  
        for(int i = 0; i < 10; i++) {
            memory[200 + i] = (i + 1) * 2;  // Values 2,4,6,8...20
        }
        
        cout << "Test matrices initialized in memory" << endl;
    }
    
    void memory_process() {
        mem_ack.write(false);
        mem_ready.write(true);
        mem_rd_data.write(0);
        mem_data_valid.write(false);
        
        wait();
        
        while(true) {
            if(mem_req.read() && mem_valid.read() && mem_ready.read()) {
                mem_ready.write(false);
                
                if(mem_wr_en.read()) {
                    // Write operation
                    if(mem_addr.read() < MEMORY_SIZE) {
                        memory[mem_addr.read()] = mem_wr_data.read();
                        cout << "@" << sc_time_stamp() << " Memory: Write addr=" 
                             << mem_addr.read() << " data=" << mem_wr_data.read() << endl;
                    }
                    mem_ack.write(true);
                    wait();
                    mem_ack.write(false);
                } else {
                    // Read operation
                    sc_int<32> data = 0;
                    if(mem_addr.read() < MEMORY_SIZE) {
                        data = memory[mem_addr.read()];
                    }
                    
                    mem_rd_data.write(data);
                    mem_data_valid.write(true);
                    mem_ack.write(true);
                    
                    cout << "@" << sc_time_stamp() << " Memory: Read addr=" 
                         << mem_addr.read() << " data=" << data << endl;
                    
                    wait();
                    mem_ack.write(false);
                    mem_data_valid.write(false);
                }
                
                mem_ready.write(true);
            }
            wait();
        }
    }
};

// Memory Arbiter with Round-Robin
SC_MODULE(memory_arbiter) {
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    
    // Cache interfaces (3 L1 caches)
    sc_in<bool> cache_req[3];
    sc_out<bool> cache_ack[3];
    sc_in<bool> cache_valid[3];
    sc_out<bool> cache_ready[3];
    sc_in<sc_uint<16>> cache_addr[3];
    sc_in<bool> cache_wr_en[3];
    sc_in<sc_int<32>> cache_wr_data[3];
    sc_out<sc_int<32>> cache_rd_data[3];
    sc_out<bool> cache_data_valid[3];
    
    // Memory interface
    sc_out<bool> mem_req;
    sc_in<bool> mem_ack;
    sc_out<bool> mem_valid;
    sc_in<bool> mem_ready;
    sc_out<sc_uint<16>> mem_addr;
    sc_out<bool> mem_wr_en;
    sc_out<sc_int<32>> mem_wr_data;
    sc_in<sc_int<32>> mem_rd_data;
    sc_in<bool> mem_data_valid;
    
    sc_signal<int> current_master;
    sc_signal<int> round_robin_ptr;
    sc_signal<bool> transaction_active;
    
    SC_CTOR(memory_arbiter) {
        SC_CTHREAD(arbiter_process, clk.pos());
        reset_signal_is(rst_n, false);
        
        SC_METHOD(update_outputs);
        sensitive << clk.pos();
        dont_initialize();
    }
    
    void arbiter_process() {
        current_master.write(-1);
        round_robin_ptr.write(0);
        transaction_active.write(false);
        
        wait();
        
        while(true) {
            if(!transaction_active.read()) {
                // Look for requests using round-robin
                int start_ptr = round_robin_ptr.read();
                int selected = -1;
                
                for(int i = 0; i < 3; i++) {
                    int idx = (start_ptr + i) % 3;
                    if(cache_req[idx].read() && cache_valid[idx].read()) {
                        selected = idx;
                        break;
                    }
                }
                
                if(selected != -1) {
                    current_master.write(selected);
                    round_robin_ptr.write((selected + 1) % 3);
                    transaction_active.write(true);
                    
                    cout << "@" << sc_time_stamp() << " Arbiter: Selected cache " << selected << endl;
                }
            } else {
                // Wait for transaction completion
                if(mem_ack.read()) {
                    cout << "@" << sc_time_stamp() << " Arbiter: Transaction completed" << endl;
                    transaction_active.write(false);
                    current_master.write(-1);
                }
            }
            wait();
        }
    }
    
    void update_outputs() {
        int master = current_master.read();
        bool active = transaction_active.read();
        
        // Memory interface outputs
        if(active && master >= 0) {
            mem_req.write(true);
            mem_valid.write(true);
            mem_addr.write(cache_addr[master].read());
            mem_wr_en.write(cache_wr_en[master].read());
            mem_wr_data.write(cache_wr_data[master].read());
        } else {
            mem_req.write(false);
            mem_valid.write(false);
            mem_addr.write(0);
            mem_wr_en.write(false);
            mem_wr_data.write(0);
        }
        
        // Cache interface outputs
        for(int i = 0; i < 3; i++) {
            if(i == master && active) {
                cache_ack[i].write(mem_ack.read());
                cache_ready[i].write(mem_ready.read());
                cache_rd_data[i].write(mem_rd_data.read());
                cache_data_valid[i].write(mem_data_valid.read());
            } else {
                cache_ack[i].write(false);
                cache_ready[i].write(true);
                cache_rd_data[i].write(0);
                cache_data_valid[i].write(false);
            }
        }
    }
};

// System Controller
SC_MODULE(system_controller) {
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    
    // Instruction fetch control
    sc_out<bool> fetch_enable;
    
    // Instruction decode interface
    sc_in<bool> dispatch_valid;
    sc_in<instruction_t> decoded_instr;
    sc_in<sc_uint<2>> target_pe;
    sc_out<bool> dispatch_ready;
    
    // PE interfaces
    sc_out<bool> pe_instr_valid[3];
    sc_out<instruction_t> pe_instruction[3];
    sc_in<bool> pe_instr_ready[3];
    
    // System control
    sc_in<bool> halt_system;
    
    SC_CTOR(system_controller) {
        SC_CTHREAD(controller_process, clk.pos());
        reset_signal_is(rst_n, false);
    }
    
    void controller_process() {
        fetch_enable.write(false);
        dispatch_ready.write(true);
        
        for(int i = 0; i < 3; i++) {
            pe_instr_valid[i].write(false);
        }
        
        wait();
        
        // Start instruction fetch
        fetch_enable.write(true);
        
        while(true) {
            if(halt_system.read()) {
                cout << "@" << sc_time_stamp() << " System: HALT received - stopping" << endl;
                fetch_enable.write(false);
                wait(10);
                sc_stop();
                break;
            }
            
            // Handle instruction dispatch
            if(dispatch_valid.read()) {
                int pe = target_pe.read();
                
                if(pe < 3 && pe_instr_ready[pe].read()) {
                    pe_instruction[pe].write(decoded_instr.read());
                    pe_instr_valid[pe].write(true);
                    dispatch_ready.write(false);
                    
                    cout << "@" << sc_time_stamp() << " Controller: Dispatched to PE[" << pe << "]" << endl;
                    
                    wait();
                    while(!pe_instr_ready[pe].read()) {
                        wait();
                    }
                    
                    pe_instr_valid[pe].write(false);
                    dispatch_ready.write(true);
                } else {
                    dispatch_ready.write(false);
                    wait();
                    dispatch_ready.write(true);
                }
            }
            wait();
        }
    }
};

// Top Level System
SC_MODULE(matrix_accelerator) {
    sc_clock clk;
    sc_signal<bool> rst_n;
    
    // Module instances
    instruction_fetch *ifetch;
    instruction_decode *idecode;
    processing_element *pe[3];
    l1_cache *cache[3];
    main_memory *memory;
    memory_arbiter *arbiter;
    system_controller *controller;
    
    // Interconnect signals
    sc_signal<sc_uint<32>> fetched_instruction;
    sc_signal<bool> instr_valid, instr_consumed;
    sc_signal<bool> fetch_enable;
    
    sc_signal<bool> dispatch_valid;
    sc_signal<instruction_t> decoded_instr;
    sc_signal<sc_uint<2>> target_pe;
    sc_signal<bool> dispatch_ready;
    sc_signal<bool> halt_system;
    
    sc_signal<bool> pe_instr_valid[3];
    sc_signal<instruction_t> pe_instruction[3];
    sc_signal<bool> pe_instr_ready[3];
    
    // Cache-PE connections
    sc_signal<bool> pe_cache_req[3], pe_cache_ack[3];
    sc_signal<bool> pe_cache_valid[3], pe_cache_ready[3];
    sc_signal<sc_uint<16>> pe_cache_addr[3];
    sc_signal<bool> pe_cache_wr_en[3];
    sc_signal<sc_int<32>> pe_cache_wr_data[3], pe_cache_rd_data[3];
    sc_signal<bool> pe_cache_data_valid[3];
    
    // Cache-Arbiter connections
    sc_signal<bool> cache_mem_req[3], cache_mem_ack[3];
    sc_signal<bool> cache_mem_valid[3], cache_mem_ready[3];
    sc_signal<sc_uint<16>> cache_mem_addr[3];
    sc_signal<bool> cache_mem_wr_en[3];
    sc_signal<sc_int<32>> cache_mem_wr_data[3], cache_mem_rd_data[3];
    sc_signal<bool> cache_mem_data_valid[3];
    
    // Arbiter-Memory connections
    sc_signal<bool> arbiter_mem_req, arbiter_mem_ack;
    sc_signal<bool> arbiter_mem_valid, arbiter_mem_ready;
    sc_signal<sc_uint<16>> arbiter_mem_addr;
    sc_signal<bool> arbiter_mem_wr_en;
    sc_signal<sc_int<32>> arbiter_mem_wr_data, arbiter_mem_rd_data;
    sc_signal<bool> arbiter_mem_data_valid;
    
    SC_CTOR(matrix_accelerator) : clk("clk", 10, SC_NS) {
        // Instantiate modules
        ifetch = new instruction_fetch("instruction_fetch");
        idecode = new instruction_decode("instruction_decode");
        memory = new main_memory("main_memory");
        arbiter = new memory_arbiter("memory_arbiter");
        controller = new system_controller("system_controller");
        
        for(int i = 0; i < 3; i++) {
            pe[i] = new processing_element(("pe_" + to_string(i)).c_str());
            cache[i] = new l1_cache(("l1_cache_" + to_string(i)).c_str());
        }
        
        // Connect instruction fetch
        ifetch->clk(clk);
        ifetch->rst_n(rst_n);
        ifetch->fetch_enable(fetch_enable);
        ifetch->instr_valid(instr_valid);
        ifetch->instruction(fetched_instruction);
        ifetch->instr_consumed(instr_consumed);
        
        // Connect instruction decode
        idecode->clk(clk);
        idecode->rst_n(rst_n);
        idecode->instruction(fetched_instruction);
        idecode->instr_valid(instr_valid);
        idecode->instr_consumed(instr_consumed);
        idecode->dispatch_valid(dispatch_valid);
        idecode->decoded_instr(decoded_instr);
        idecode->target_pe(target_pe);
        idecode->dispatch_ready(dispatch_ready);
        idecode->halt_system(halt_system);
        
        // Connect system controller
        controller->clk(clk);
        controller->rst_n(rst_n);
        controller->fetch_enable(fetch_enable);
        controller->dispatch_valid(dispatch_valid);
        controller->decoded_instr(decoded_instr);
        controller->target_pe(target_pe);
        controller->dispatch_ready(dispatch_ready);
        controller->halt_system(halt_system);
        
        // Connect PEs and caches
        for(int i = 0; i < 3; i++) {
            // PE connections
            pe[i]->clk(clk);
            pe[i]->rst_n(rst_n);
            pe[i]->pe_id.write(i);
            pe[i]->instr_valid(pe_instr_valid[i]);
            pe[i]->instruction(pe_instruction[i]);
            pe[i]->instr_ready(pe_instr_ready[i]);
            
            // PE-Cache connections
            pe[i]->cache_req(pe_cache_req[i]);
            pe[i]->cache_ack(pe_cache_ack[i]);
            pe[i]->cache_valid(pe_cache_valid[i]);
            pe[i]->cache_ready(pe_cache_ready[i]);
            pe[i]->cache_addr(pe_cache_addr[i]);
            pe[i]->cache_wr_en(pe_cache_wr_en[i]);
            pe[i]->cache_wr_data(pe_cache_wr_data[i]);
            pe[i]->cache_rd_data(pe_cache_rd_data[i]);
            pe[i]->cache_data_valid(pe_cache_data_valid[i]);
            
            // Cache connections
            cache[i]->clk(clk);
            cache[i]->rst_n(rst_n);
            cache[i]->cache_id.write(i);
            
            // Cache-PE interface
            cache[i]->pe_req(pe_cache_req[i]);
            cache[i]->pe_ack(pe_cache_ack[i]);
            cache[i]->pe_valid(pe_cache_valid[i]);
            cache[i]->pe_ready(pe_cache_ready[i]);
            cache[i]->pe_addr(pe_cache_addr[i]);
            cache[i]->pe_wr_en(pe_cache_wr_en[i]);
            cache[i]->pe_wr_data(pe_cache_wr_data[i]);
            cache[i]->pe_rd_data(pe_cache_rd_data[i]);
            cache[i]->pe_data_valid(pe_cache_data_valid[i]);
            
            // Cache-Arbiter interface
            cache[i]->mem_req(cache_mem_req[i]);
            cache[i]->mem_ack(cache_mem_ack[i]);
            cache[i]->mem_valid(cache_mem_valid[i]);
            cache[i]->mem_ready(cache_mem_ready[i]);
            cache[i]->mem_addr(cache_mem_addr[i]);
            cache[i]->mem_wr_en(cache_mem_wr_en[i]);
            cache[i]->mem_wr_data(cache_mem_wr_data[i]);
            cache[i]->mem_rd_data(cache_mem_rd_data[i]);
            cache[i]->mem_data_valid(cache_mem_data_valid[i]);
            
            // Controller-PE interface
            controller->pe_instr_valid[i](pe_instr_valid[i]);
            controller->pe_instruction[i](pe_instruction[i]);
            controller->pe_instr_ready[i](pe_instr_ready[i]);
        }
        
        // Connect memory arbiter
        arbiter->clk(clk);
        arbiter->rst_n(rst_n);
        
        for(int i = 0; i < 3; i++) {
            arbiter->cache_req[i](cache_mem_req[i]);
            arbiter->cache_ack[i](cache_mem_ack[i]);
            arbiter->cache_valid[i](cache_mem_valid[i]);
            arbiter->cache_ready[i](cache_mem_ready[i]);
            arbiter->cache_addr[i](cache_mem_addr[i]);
            arbiter->cache_wr_en[i](cache_mem_wr_en[i]);
            arbiter->cache_wr_data[i](cache_mem_wr_data[i]);
            arbiter->cache_rd_data[i](cache_mem_rd_data[i]);
            arbiter->cache_data_valid[i](cache_mem_data_valid[i]);
        }
        
        // Arbiter-Memory interface
        arbiter->mem_req(arbiter_mem_req);
        arbiter->mem_ack(arbiter_mem_ack);
        arbiter->mem_valid(arbiter_mem_valid);
        arbiter->mem_ready(arbiter_mem_ready);
        arbiter->mem_addr(arbiter_mem_addr);
        arbiter->mem_wr_en(arbiter_mem_wr_en);
        arbiter->mem_wr_data(arbiter_mem_wr_data);
        arbiter->mem_rd_data(arbiter_mem_rd_data);
        arbiter->mem_data_valid(arbiter_mem_data_valid);
        
        // Connect main memory
        memory->clk(clk);
        memory->rst_n(rst_n);
        memory->mem_req(arbiter_mem_req);
        memory->mem_ack(arbiter_mem_ack);
        memory->mem_valid(arbiter_mem_valid);
        memory->mem_ready(arbiter_mem_ready);
        memory->mem_addr(arbiter_mem_addr);
        memory->mem_wr_en(arbiter_mem_wr_en);
        memory->mem_wr_data(arbiter_mem_wr_data);
        memory->mem_rd_data(arbiter_mem_rd_data);
        memory->mem_data_valid(arbiter_mem_data_valid);
        
        // System initialization
        SC_CTHREAD(system_process, clk);
    }
    
    void system_process() {
        rst_n.write(false);
        wait(5);
        rst_n.write(true);
        
        cout << "\n=== Matrix Multiplication Accelerator ===" << endl;
        cout << "Architecture: 3 PEs with L1 Caches + Instruction Pipeline" << endl;
        cout << "Custom Instructions: MADD, MSUB, MMUL, MLOAD, MSTORE, MSCAL, HALT" << endl;
        cout << "L1 Cache: 64 lines, 4-way set associative, LRU replacement" << endl;
        cout << "Memory Access: Round-robin arbitration" << endl;
        cout << "=============================================" << endl << endl;
        
        while(true) {
            wait();
        }
    }
    
    ~matrix_accelerator() {
        delete ifetch;
        delete idecode;
        delete memory;
        delete arbiter;
        delete controller;
        for(int i = 0; i < 3; i++) {
            delete pe[i];
            delete cache[i];
        }
    }
};

// Main function
int sc_main(int argc, char* argv[]) {
    cout << "=== Generalized Matrix Multiplication Accelerator ===" << endl;
    cout << "Features:" << endl;
    cout << "- Custom instruction set for matrix operations" << endl;
    cout << "- Instruction fetch and decode pipeline" << endl;
    cout << "- 3 Processing Elements with L1 caches" << endl;
    cout << "- LRU cache replacement policy" << endl;
    cout << "- Round-robin memory arbitration" << endl << endl;
    
    matrix_accelerator system("matrix_accelerator");
    
    // Create trace file
    sc_trace_file* tf = sc_create_vcd_trace_file("matrix_accelerator_trace");
    tf->set_time_unit(1, SC_NS);
    
    // Trace key signals
    sc_trace(tf, system.clk, "clk");
    sc_trace(tf, system.rst_n, "rst_n");
    sc_trace(tf, system.instr_valid, "instr_valid");
    sc_trace(tf, system.dispatch_valid, "dispatch_valid");
    sc_trace(tf, system.halt_system, "halt_system");
    
    for(int i = 0; i < 3; i++) {
        string pe_name = "pe" + to_string(i);
        sc_trace(tf, system.pe_instr_valid[i], (pe_name + "_instr_valid").c_str());
        sc_trace(tf, system.pe_instr_ready[i], (pe_name + "_instr_ready").c_str());
        sc_trace(tf, system.cache[i]->cache_hits, (pe_name + "_cache_hits").c_str());
        sc_trace(tf, system.cache[i]->cache_misses, (pe_name + "_cache_misses").c_str());
    }
    
    cout << "Starting simulation..." << endl;
    sc_start(10000, SC_NS);
    
    // Print final statistics
    cout << "\n=== Final Cache Statistics ===" << endl;
    for(int i = 0; i < 3; i++) {
        int hits = system.cache[i]->cache_hits.read();
        int misses = system.cache[i]->cache_misses.read();
        float hit_rate = (hits + misses > 0) ? (float)hits / (hits + misses) * 100.0 : 0.0;
        
        cout << "Cache[" << i << "]: Hits=" << hits << ", Misses=" << misses 
             << ", Hit Rate=" << fixed << setprecision(1) << hit_rate << "%" << endl;
    }
    
    cout << "\nTrace saved to: matrix_accelerator_trace.vcd" << endl;
    sc_close_vcd_trace_file(tf);
    
    return 0;
}