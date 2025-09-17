#include <systemc.h>
#include <vector>
#include <iostream>
#include <iomanip>
#include <map>

using namespace std;

// #TODO: Implement latency and optimisation in line 292-298 
/*
 * MEMORY HIERARCHY DESIGN FOR LLM INFERENCE ACCELERATOR
 * =====================================================
 * 
 * Architecture Overview:
 * - 3x3 Grid of Processing Elements (9 PEs total)
 * - Each PE has its own dedicated L1 cache
 * - Shared main memory accessible by all L1 caches
 * - Memory arbiter handles conflicts between multiple cache requests
 * 
 * L1 Cache Design Parameters:
 * - Cache Size: 256 bytes (64 cache lines)
 * - Cache Line Size: 4 bytes (1 word of 32-bit data)
 * - Number of Sets: 16 sets
 * - Associativity: 4-way set associative
 * - Replacement Policy: Least Recently Used (LRU)
 * - Write Policy: Write-through with write allocate
 * 
 * Address Mapping (16-bit address):
 * - Bits [15:6]: Tag (10 bits)
 * - Bits [5:2]:  Set Index (4 bits) - selects 1 of 16 sets
 * - Bits [1:0]:  Block Offset (2 bits) - byte within cache line
*/

// Cache Line Structure - Stores metadata and data for each cache line
struct cache_line_t {
    bool valid;             // Valid bit - is this cache line valid?
    bool dirty;             // Dirty bit - has this line been modified?
    sc_uint<10> tag;        // Tag bits for address matching
    sc_int<32> data;        // Data stored in this cache line (one 32-bit word)
    int lru_counter;        // LRU counter for replacement policy
    
    cache_line_t() : valid(false), dirty(false), tag(0), data(0), lru_counter(0) {}
};

// L1 Cache Module - One instance per PE
SC_MODULE(l1_cache) {
    // Clock and Reset
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    
    // PE Interface (Cache acts as slave to PE)
    sc_in<bool> pe_req;         // PE request signal
    sc_out<bool> pe_ack;        // Cache acknowledge to PE
    sc_in<bool> pe_valid;       // PE address/data valid
    sc_out<bool> pe_ready;      // Cache ready for new request
    sc_in<sc_uint<16>> pe_addr; // Address from PE
    sc_in<bool> pe_wr_en;       // Write enable from PE
    sc_in<sc_int<32>> pe_wr_data; // Write data from PE
    sc_out<sc_int<32>> pe_rd_data; // Read data to PE
    sc_out<bool> pe_data_valid; // Read data valid signal
    
    // Main Memory Interface (Cache acts as master to memory)
    sc_out<bool> mem_req;       // Memory request
    sc_in<bool> mem_ack;        // Memory acknowledge
    sc_out<bool> mem_valid;     // Address/data valid to memory
    sc_in<bool> mem_ready;      // Memory ready signal
    sc_out<sc_uint<16>> mem_addr; // Address to memory
    sc_out<bool> mem_wr_en;     // Write enable to memory
    sc_out<sc_int<32>> mem_wr_data; // Write data to memory
    sc_in<sc_int<32>> mem_rd_data;  // Read data from memory
    sc_in<bool> mem_data_valid;     // Memory data valid
    
    // Cache Configuration Constants
    static const int CACHE_LINES = 64;      // Total cache lines
    static const int NUM_SETS = 16;         // Number of sets
    static const int ASSOCIATIVITY = 4;     // 4-way set associative
    static const int LINES_PER_SET = ASSOCIATIVITY;
    
    // Cache Storage - 2D array: [set][way]
    cache_line_t cache_memory[NUM_SETS][ASSOCIATIVITY];
    
    // Cache Statistics (for debugging and performance analysis)
    sc_signal<int> cache_hits;
    sc_signal<int> cache_misses;
    sc_signal<int> cache_id;    // ID of this cache instance
    
    // Internal state machine
    enum cache_state_t {
        CACHE_IDLE,         // Waiting for PE request
        CACHE_LOOKUP,       // Checking for hit/miss
        CACHE_HIT,          // Cache hit - serving from cache
        CACHE_MISS_READ,    // Cache miss - reading from memory
        CACHE_MISS_WAIT,    // Waiting for memory response
        CACHE_WRITEBACK,    // Writing dirty line back to memory
        CACHE_WB_WAIT       // Waiting for writeback completion
    };
    
    sc_signal<int> state;
    sc_signal<sc_uint<16>> current_addr;
    sc_signal<sc_int<32>> current_data;
    sc_signal<bool> current_write;
    sc_signal<int> victim_set, victim_way;
    sc_signal<int> timeout_counter;  // Add timeout counter
    
    SC_CTOR(l1_cache) {
        SC_CTHREAD(cache_process, clk.pos());
        reset_signal_is(rst_n, false);
    }
    
    void cache_process() {
        // Reset all cache lines and output signals
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

        timeout_counter.write(0);
        
        wait();
        
        while(true) {

            // Add global timeout check
            if(timeout_counter.read() > 100) {
                cout << "@" << sc_time_stamp() << " Cache[" << cache_id.read() 
                     << "]: Timeout in state " << state.read() << " - resetting to IDLE" << endl;
                state.write(CACHE_IDLE);
                timeout_counter.write(0);
                // Reset output signals
                pe_ack.write(false);
                pe_ready.write(true);
                pe_data_valid.write(false);
                mem_req.write(false);
                mem_valid.write(false);
                mem_wr_en.write(false);
            }

            switch(state.read()) {
                case CACHE_IDLE:
                    timeout_counter.write(0);
                    pe_ready.write(true);
                    pe_ack.write(false);
                    pe_data_valid.write(false);
                    
                    // Wait for PE request
                    if(pe_req.read() && pe_valid.read()) {
                        pe_ready.write(false);
                        current_addr.write(pe_addr.read());
                        current_data.write(pe_wr_data.read());
                        current_write.write(pe_wr_en.read());
                        state.write(CACHE_LOOKUP);
                        
                        cout << "@" << sc_time_stamp() << " Cache[" << cache_id.read() 
                             << "]: " << (pe_wr_en.read() ? "WRITE" : "READ") 
                             << " request for addr " << pe_addr.read() << endl;
                    }
                    break;
                    
                case CACHE_LOOKUP: {
                    // Parse address into tag, set, and offset
                    sc_uint<16> addr = current_addr.read();
                    sc_uint<10> tag = addr.range(15, 6);    // Upper 10 bits
                    sc_uint<4> set_idx = addr.range(5, 2);  // Middle 4 bits
                    // sc_uint<2> offset = addr.range(1, 0);   // Lower 2 bits (unused for word access)
                    
                    bool hit_found = false;
                    int hit_way = -1;
                    
                    // Search all ways in the target set
                    for(int w = 0; w < ASSOCIATIVITY; w++) {
                        if(cache_memory[set_idx][w].valid && 
                           cache_memory[set_idx][w].tag == tag) {
                            hit_found = true;
                            hit_way = w;
                            break;
                        }
                    }
                    
                    if(hit_found) {
                        // CACHE HIT
                        cache_hits.write(cache_hits.read() + 1);
                        update_lru(set_idx, hit_way);
                        
                        if(current_write.read()) {
                            // Write hit - update cache and mark dirty
                            cache_memory[set_idx][hit_way].data = current_data.read();
                            cache_memory[set_idx][hit_way].dirty = true;
                            cout << "@" << sc_time_stamp() << " Cache[" << cache_id.read() 
                                 << "]: WRITE HIT - set=" << set_idx << " way=" << hit_way << endl;
                        } else {
                            // Read hit - return data
                            pe_rd_data.write(cache_memory[set_idx][hit_way].data);
                            pe_data_valid.write(true);
                            cout << "@" << sc_time_stamp() << " Cache[" << cache_id.read() 
                                 << "]: READ HIT - set=" << set_idx << " way=" << hit_way 
                                 << " data=" << cache_memory[set_idx][hit_way].data << endl;
                        }
                        state.write(CACHE_HIT);
                    } else {
                        // CACHE MISS
                        cache_misses.write(cache_misses.read() + 1);
                        
                        // Find victim line using LRU policy
                        int victim_w = find_lru_victim(set_idx);
                        victim_set.write(set_idx);
                        victim_way.write(victim_w);
                        
                        cout << "@" << sc_time_stamp() << " Cache[" << cache_id.read() 
                             << "]: MISS - set=" << set_idx << " victim_way=" << victim_w << endl;
                        
                        // Check if victim line is dirty and needs writeback
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
                    // Write dirty victim line back to memory
                    int v_set = victim_set.read();
                    int v_way = victim_way.read();
                    
                    // Reconstruct victim address from tag and set
                    sc_uint<16> wb_addr = (cache_memory[v_set][v_way].tag << 6) | (v_set << 2);
                    
                    mem_req.write(true);
                    mem_valid.write(true);
                    mem_wr_en.write(true);
                    mem_addr.write(wb_addr);
                    mem_wr_data.write(cache_memory[v_set][v_way].data);
                    
                    cout << "@" << sc_time_stamp() << " Cache[" << cache_id.read() 
                         << "]: WRITEBACK addr=" << wb_addr 
                         << " data=" << cache_memory[v_set][v_way].data << endl;
                    
                    state.write(CACHE_WB_WAIT);
                    break;
                }
                
                case CACHE_WB_WAIT:
                    timeout_counter.write(timeout_counter.read() + 1);
                    if(mem_ack.read()) {
                        mem_req.write(false);
                        mem_valid.write(false);
                        mem_wr_en.write(false);
                        state.write(CACHE_MISS_READ);
                        timeout_counter.write(0);
                    }
                    break;
                    
                case CACHE_MISS_READ:
                    // Read missing line from memory
                    mem_req.write(true);
                    mem_valid.write(true);
                    mem_wr_en.write(false);
                    mem_addr.write(current_addr.read());
                    
                    cout << "@" << sc_time_stamp() << " Cache[" << cache_id.read() 
                         << "]: Reading from memory addr=" << current_addr.read() << endl;
                    
                    state.write(CACHE_MISS_WAIT);
                    break;
                    
                case CACHE_MISS_WAIT:
                    timeout_counter.write(timeout_counter.read() + 1);
                    if(mem_ack.read() && mem_data_valid.read()) {
                        mem_req.write(false);
                        mem_valid.write(false);
                        
                        // Install new line in cache
                        sc_uint<16> addr = current_addr.read();
                        sc_uint<10> tag = addr.range(15, 6);
                        sc_uint<4> set_idx = addr.range(5, 2);
                        int v_set = victim_set.read();
                        int v_way = victim_way.read();
                        
                        cache_memory[v_set][v_way].valid = true;
                        cache_memory[v_set][v_way].dirty = false;
                        cache_memory[v_set][v_way].tag = tag;
                        cache_memory[v_set][v_way].data = mem_rd_data.read();
                        update_lru(v_set, v_way);
                        
                        cout << "@" << sc_time_stamp() << " Cache[" << cache_id.read() 
                             << "]: Installed new line - set=" << v_set << " way=" << v_way
                             << " data=" << mem_rd_data.read() << endl;
                        
                        // Handle the original request
                        if(current_write.read()) {
                            cache_memory[v_set][v_way].data = current_data.read();
                            cache_memory[v_set][v_way].dirty = true;
                        } else {
                            pe_rd_data.write(cache_memory[v_set][v_way].data);
                            pe_data_valid.write(true);
                        }
                        
                        state.write(CACHE_HIT);
                        timeout_counter.write(0);
                    }
                    break;
            }
            wait();
        }
    }
    
    // Update LRU counters when a line is accessed
    void update_lru(int set, int accessed_way) {
        for(int w = 0; w < ASSOCIATIVITY; w++) {
            if(w == accessed_way) {
                cache_memory[set][w].lru_counter = 0; // Most recently used
            } else {
                cache_memory[set][w].lru_counter++;   // Age other lines
            }
        }
    }
    
    // Find LRU victim for replacement
    int find_lru_victim(int set) {
        int max_lru = -1;
        int victim_way = 0;
        
        for(int w = 0; w < ASSOCIATIVITY; w++) {
            if(!cache_memory[set][w].valid) {
                return w; // Return first invalid line
            }
            if(cache_memory[set][w].lru_counter > max_lru) {
                max_lru = cache_memory[set][w].lru_counter;
                victim_way = w;
            }
        }
        return victim_way;
    }
};

// Processing Element (PE) - Simplified version of original accelerator
SC_MODULE(processing_element) {
    // Clock and Reset
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    
    // Control Interface
    sc_in<bool> pe_start;       // Start signal from controller
    sc_out<bool> pe_done;       // Completion signal
    sc_out<bool> pe_busy;       // Busy indicator
    
    // L1 Cache Interface
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
    sc_in<sc_uint<8>> pe_id;           // PE identifier (0-8)
    sc_in<sc_uint<16>> work_addr;      // Starting work address
    sc_in<sc_uint<8>> work_size;       // Amount of work to do
    
    // PE State Machine
    enum pe_state_t {
        PE_IDLE,
        PE_WORKING,
        PE_READ_DATA,
        PE_WAIT_READ,
        PE_COMPUTE,
        PE_WRITE_RESULT,
        PE_WAIT_WRITE,
        PE_COMPLETE
    };
    
    sc_signal<int> pe_state;
    sc_signal<int> work_counter;
    sc_signal<sc_int<32>> compute_result;
    
    SC_CTOR(processing_element) {
        SC_CTHREAD(pe_process, clk.pos());
        reset_signal_is(rst_n, false);
    }
    
    void pe_process() {
        // Reset state
        pe_done.write(false);
        pe_busy.write(false);
        cache_req.write(false);
        cache_valid.write(false);
        cache_addr.write(0);
        cache_wr_en.write(false);
        cache_wr_data.write(0);
        pe_state.write(PE_IDLE);
        work_counter.write(0);
        
        wait();
        
        while(true) {
            switch(pe_state.read()) {
                case PE_IDLE:
                    pe_done.write(false);
                    pe_busy.write(false);
                    if(pe_start.read()) {
                        cout << "@" << sc_time_stamp() << " PE[" << pe_id.read() 
                             << "]: Starting work at addr=" << work_addr.read() << endl;
                        pe_busy.write(true);
                        work_counter.write(0);
                        pe_state.write(PE_WORKING);
                    }
                    break;
                    
                case PE_WORKING:
                    if(work_counter.read() < work_size.read()) {
                        pe_state.write(PE_READ_DATA);
                    } else {
                        pe_state.write(PE_COMPLETE);
                    }
                    break;
                    
                case PE_READ_DATA:
                    // Read data from cache
                    cache_req.write(true);
                    cache_valid.write(true);
                    cache_wr_en.write(false);
                    cache_addr.write(work_addr.read() + work_counter.read());
                    pe_state.write(PE_WAIT_READ);
                    break;
                    
                case PE_WAIT_READ:
                    if(cache_ack.read() && cache_data_valid.read()) {
                        cache_req.write(false);
                        cache_valid.write(false);
                        compute_result.write(cache_rd_data.read() * 2); // Simple computation
                        pe_state.write(PE_COMPUTE);
                    }
                    break;
                    
                case PE_COMPUTE:
                    // Simulate computation delay
                    cout << "@" << sc_time_stamp() << " PE[" << pe_id.read() 
                         << "]: Computed " << compute_result.read() << endl;
                    pe_state.write(PE_WRITE_RESULT);
                    break;
                    
                case PE_WRITE_RESULT:
                    // Write result back to cache
                    cache_req.write(true);
                    cache_valid.write(true);
                    cache_wr_en.write(true);
                    cache_addr.write(work_addr.read() + work_counter.read() + 100); // Different address for result
                    cache_wr_data.write(compute_result.read());
                    pe_state.write(PE_WAIT_WRITE);
                    break;
                    
                case PE_WAIT_WRITE:
                    if(cache_ack.read()) {
                        cache_req.write(false);
                        cache_valid.write(false);
                        cache_wr_en.write(false);
                        work_counter.write(work_counter.read() + 1);
                        pe_state.write(PE_WORKING);
                    }
                    break;
                    
                case PE_COMPLETE:
                    pe_done.write(true);
                    pe_busy.write(false);
                    cout << "@" << sc_time_stamp() << " PE[" << pe_id.read() 
                         << "]: Work completed" << endl;
                    if(!pe_start.read()) {
                        pe_state.write(PE_IDLE);
                    }
                    break;
            }
            wait();
        }
    }
};

// Main Memory Module (Similar to original but handles multiple requestors)
SC_MODULE(main_memory) {
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    
    // Memory Interface
    sc_in<bool> mem_req;
    sc_out<bool> mem_ack;
    sc_in<bool> mem_valid;
    sc_out<bool> mem_ready;
    sc_in<sc_uint<16>> mem_addr;
    sc_in<bool> mem_wr_en;
    sc_in<sc_int<32>> mem_wr_data;
    sc_out<sc_int<32>> mem_rd_data;
    sc_out<bool> mem_data_valid;
    
    // Memory storage
    vector<sc_int<32>> memory;
    static const int MEMORY_SIZE = 2048;  // Larger memory for multiple PEs
    
    SC_CTOR(main_memory) : memory(MEMORY_SIZE, 0) {
        SC_CTHREAD(memory_process, clk.pos());
        reset_signal_is(rst_n, false);
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
                    // WRITE
                    cout << "@" << sc_time_stamp() << " Memory: Writing " 
                         << mem_wr_data.read() << " to addr " << mem_addr.read() << endl;
                    if(mem_addr.read() < MEMORY_SIZE) {
                        memory[mem_addr.read()] = mem_wr_data.read();
                    }
                    mem_ack.write(true);
                    wait();
                    mem_ack.write(false);
                } else {
                    // READ
                    sc_int<32> data = 0;
                    if(mem_addr.read() < MEMORY_SIZE) {
                        data = memory[mem_addr.read()];
                    }
                    cout << "@" << sc_time_stamp() << " Memory: Reading addr " 
                         << mem_addr.read() << " = " << data << endl;
                    mem_rd_data.write(data);
                    mem_data_valid.write(true);
                    mem_ack.write(true);
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

// Fixed Memory Arbiter
SC_MODULE(memory_arbiter) {
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    
    // Cache interfaces (9 L1 caches)
    sc_in<bool> cache_req[9];
    sc_out<bool> cache_ack[9];
    sc_in<bool> cache_valid[9];
    sc_out<bool> cache_ready[9];
    sc_in<sc_uint<16>> cache_addr[9];
    sc_in<bool> cache_wr_en[9];
    sc_in<sc_int<32>> cache_wr_data[9];
    sc_out<sc_int<32>> cache_rd_data[9];
    sc_out<bool> cache_data_valid[9];
    
    // Main memory interface
    sc_out<bool> mem_req;
    sc_in<bool> mem_ack;
    sc_out<bool> mem_valid;
    sc_in<bool> mem_ready;
    sc_out<sc_uint<16>> mem_addr;
    sc_out<bool> mem_wr_en;
    sc_out<sc_int<32>> mem_wr_data;
    sc_in<sc_int<32>> mem_rd_data;
    sc_in<bool> mem_data_valid;
    
    // Internal state
    sc_signal<int> current_master;
    sc_signal<int> round_robin_ptr;
    sc_signal<bool> transaction_active;
    
    SC_CTOR(memory_arbiter) {
        SC_CTHREAD(arbiter_process, clk.pos());
        reset_signal_is(rst_n, false);
        
        SC_METHOD(update_outputs);
        sensitive << clk.pos();  // Simplified sensitivity
        dont_initialize();
    }
    
    void arbiter_process() {
        // Reset
        current_master.write(-1);
        round_robin_ptr.write(0);
        transaction_active.write(false);
        
        wait();
        
        while(true) {
            if(!transaction_active.read()) {
                // Look for new request using round-robin
                int start_ptr = round_robin_ptr.read();
                int selected = -1;
                
                for(int i = 0; i < 9; i++) {
                    int idx = (start_ptr + i) % 9;
                    if(cache_req[idx].read() && cache_valid[idx].read()) {
                        selected = idx;
                        break;
                    }
                }
                
                if(selected != -1) {
                    current_master.write(selected);
                    round_robin_ptr.write((selected + 1) % 9);
                    transaction_active.write(true);
                    
                    cout << "@" << sc_time_stamp() << " Arbiter: Selected cache " 
                         << selected << " for addr " << cache_addr[selected].read() << endl;
                }
            } else {
                // Wait for transaction to complete
                if(mem_ack.read()) {
                    cout << "@" << sc_time_stamp() << " Arbiter: Transaction completed for cache " 
                         << current_master.read() << endl;
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
        
        // Memory interface
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
        
        // Cache interfaces
        for(int i = 0; i < 9; i++) {
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

// System Controller for the PE Grid
SC_MODULE(grid_controller) {
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    
    // PE control signals
    sc_out<bool> pe_start[9];
    sc_in<bool> pe_done[9];
    sc_in<bool> pe_busy[9];
    
    // Configuration outputs
    sc_out<sc_uint<8>> pe_id[9];
    sc_out<sc_uint<16>> work_addr[9];
    sc_out<sc_uint<8>> work_size[9];
    
    enum ctrl_state_t {
        CTRL_INIT,
        CTRL_SETUP_PES,
        CTRL_START_PES,
        CTRL_WAIT_COMPLETE,
        CTRL_FINISHED
    };
    
    sc_signal<int> ctrl_state;
    sc_signal<int> finish_wait_counter;  // Add counter to prevent infinite loop
    
    SC_CTOR(grid_controller) {
        SC_CTHREAD(controller_process, clk.pos());
        reset_signal_is(rst_n, false);
    }
    
    void controller_process() {
        // Reset
        for(int i = 0; i < 9; i++) {
            pe_start[i].write(false);
            pe_id[i].write(i);
            work_addr[i].write(i * 10);    // Each PE works on different data
            work_size[i].write(3);         // Reduced work size for faster simulation
        }
        ctrl_state.write(CTRL_INIT);
        finish_wait_counter.write(0);
        
        wait();
        
        while(true) {
            switch(ctrl_state.read()) {
                case CTRL_INIT:
                    cout << "@" << sc_time_stamp() << " Controller: Initializing PE grid (3x3)" << endl;
                    ctrl_state.write(CTRL_SETUP_PES);
                    break;
                    
                case CTRL_SETUP_PES:
                    cout << "@" << sc_time_stamp() << " Controller: Configuring PEs" << endl;
                    wait(5); // Give setup time
                    ctrl_state.write(CTRL_START_PES);
                    break;
                    
                case CTRL_START_PES:
                    cout << "@" << sc_time_stamp() << " Controller: Starting all PEs" << endl;
                    for(int i = 0; i < 9; i++) {
                        pe_start[i].write(true);
                    }
                    ctrl_state.write(CTRL_WAIT_COMPLETE);
                    break;
                    
                case CTRL_WAIT_COMPLETE: {
                    bool all_done = true;
                    for(int i = 0; i < 9; i++) {
                        if(!pe_done[i].read()) {
                            all_done = false;
                            break;
                        }
                    }
                    
                    if(all_done) {
                        cout << "@" << sc_time_stamp() << " Controller: All PEs completed" << endl;
                        for(int i = 0; i < 9; i++) {
                            pe_start[i].write(false);
                        }
                        ctrl_state.write(CTRL_FINISHED);
                    }
                    
                    // Add timeout to prevent infinite simulation
                    finish_wait_counter.write(finish_wait_counter.read() + 1);
                    if(finish_wait_counter.read() > 1000) { // Timeout after 1000 cycles
                        cout << "@" << sc_time_stamp() << " Controller: Timeout - forcing completion" << endl;
                        ctrl_state.write(CTRL_FINISHED);
                    }
                    break;
                }
                
                case CTRL_FINISHED:
                    cout << "@" << sc_time_stamp() << " Controller: Grid computation finished" << endl;
                    wait(10); // Wait a few cycles before stopping
                    sc_stop();
                    return; // Exit the while loop
            }
            wait();
        }
    }
};

// Top Level System - Integrates PE Grid with Memory Hierarchy
SC_MODULE(llm_accelerator_grid) {
    sc_clock clk;
    sc_signal<bool> rst_n;
    
    // Module instances
    processing_element *pe[9];          // 3x3 PE grid
    l1_cache *l1[9];                    // One L1 cache per PE  
    main_memory *mem;                   // Shared main memory
    memory_arbiter *arbiter;            // Memory access arbiter
    grid_controller *controller;        // System controller
    
    // Interconnect signals
    // PE to L1 Cache connections
    sc_signal<bool> pe_cache_req[9], pe_cache_ack[9];
    sc_signal<bool> pe_cache_valid[9], pe_cache_ready[9];
    sc_signal<sc_uint<16>> pe_cache_addr[9];
    sc_signal<bool> pe_cache_wr_en[9];
    sc_signal<sc_int<32>> pe_cache_wr_data[9], pe_cache_rd_data[9];
    sc_signal<bool> pe_cache_data_valid[9];
    
    // L1 Cache to Arbiter connections
    sc_signal<bool> cache_mem_req[9], cache_mem_ack[9];
    sc_signal<bool> cache_mem_valid[9], cache_mem_ready[9];
    sc_signal<sc_uint<16>> cache_mem_addr[9];
    sc_signal<bool> cache_mem_wr_en[9];
    sc_signal<sc_int<32>> cache_mem_wr_data[9], cache_mem_rd_data[9];
    sc_signal<bool> cache_mem_data_valid[9];
    
    // Arbiter to Memory connections
    sc_signal<bool> arbiter_mem_req, arbiter_mem_ack;
    sc_signal<bool> arbiter_mem_valid, arbiter_mem_ready;
    sc_signal<sc_uint<16>> arbiter_mem_addr;
    sc_signal<bool> arbiter_mem_wr_en;
    sc_signal<sc_int<32>> arbiter_mem_wr_data, arbiter_mem_rd_data;
    sc_signal<bool> arbiter_mem_data_valid;
    
    // Control signals
    sc_signal<bool> pe_start[9], pe_done[9], pe_busy[9];
    sc_signal<sc_uint<8>> pe_id[9];
    sc_signal<sc_uint<16>> work_addr[9];
    sc_signal<sc_uint<8>> work_size[9];
    
    SC_CTOR(llm_accelerator_grid) : clk("clk", 10, SC_NS) {
        // Instantiate all modules
        mem = new main_memory("main_memory");
        arbiter = new memory_arbiter("memory_arbiter");
        controller = new grid_controller("grid_controller");
        
        // Instantiate PE grid and L1 caches
        for(int i = 0; i < 9; i++) {
            pe[i] = new processing_element(("pe_" + to_string(i)).c_str());
            l1[i] = new l1_cache(("l1_cache_" + to_string(i)).c_str());
        }
        
        // Connect main memory
        mem->clk(clk);
        mem->rst_n(rst_n);
        mem->mem_req(arbiter_mem_req);
        mem->mem_ack(arbiter_mem_ack);
        mem->mem_valid(arbiter_mem_valid);
        mem->mem_ready(arbiter_mem_ready);
        mem->mem_addr(arbiter_mem_addr);
        mem->mem_wr_en(arbiter_mem_wr_en);
        mem->mem_wr_data(arbiter_mem_wr_data);
        mem->mem_rd_data(arbiter_mem_rd_data);
        mem->mem_data_valid(arbiter_mem_data_valid);
        
        // Connect arbiter
        arbiter->clk(clk);
        arbiter->rst_n(rst_n);
        arbiter->mem_req(arbiter_mem_req);
        arbiter->mem_ack(arbiter_mem_ack);
        arbiter->mem_valid(arbiter_mem_valid);
        arbiter->mem_ready(arbiter_mem_ready);
        arbiter->mem_addr(arbiter_mem_addr);
        arbiter->mem_wr_en(arbiter_mem_wr_en);
        arbiter->mem_wr_data(arbiter_mem_wr_data);
        arbiter->mem_rd_data(arbiter_mem_rd_data);
        arbiter->mem_data_valid(arbiter_mem_data_valid);
        
        // Connect controller
        controller->clk(clk);
        controller->rst_n(rst_n);
        
        // Connect PE grid and caches
        for(int i = 0; i < 9; i++) {
            // PE connections
            pe[i]->clk(clk);
            pe[i]->rst_n(rst_n);
            pe[i]->pe_start(pe_start[i]);
            pe[i]->pe_done(pe_done[i]);
            pe[i]->pe_busy(pe_busy[i]);
            pe[i]->cache_req(pe_cache_req[i]);
            pe[i]->cache_ack(pe_cache_ack[i]);
            pe[i]->cache_valid(pe_cache_valid[i]);
            pe[i]->cache_ready(pe_cache_ready[i]);
            pe[i]->cache_addr(pe_cache_addr[i]);
            pe[i]->cache_wr_en(pe_cache_wr_en[i]);
            pe[i]->cache_wr_data(pe_cache_wr_data[i]);
            pe[i]->cache_rd_data(pe_cache_rd_data[i]);
            pe[i]->cache_data_valid(pe_cache_data_valid[i]);
            pe[i]->pe_id(pe_id[i]);
            pe[i]->work_addr(work_addr[i]);
            pe[i]->work_size(work_size[i]);
            
            // L1 Cache connections  
            l1[i]->clk(clk);
            l1[i]->rst_n(rst_n);
            l1[i]->cache_id.write(i);  // Set cache ID
            
            // PE to L1 interface
            l1[i]->pe_req(pe_cache_req[i]);
            l1[i]->pe_ack(pe_cache_ack[i]);
            l1[i]->pe_valid(pe_cache_valid[i]);
            l1[i]->pe_ready(pe_cache_ready[i]);
            l1[i]->pe_addr(pe_cache_addr[i]);
            l1[i]->pe_wr_en(pe_cache_wr_en[i]);
            l1[i]->pe_wr_data(pe_cache_wr_data[i]);
            l1[i]->pe_rd_data(pe_cache_rd_data[i]);
            l1[i]->pe_data_valid(pe_cache_data_valid[i]);
            
            // L1 to arbiter interface
            l1[i]->mem_req(cache_mem_req[i]);
            l1[i]->mem_ack(cache_mem_ack[i]);
            l1[i]->mem_valid(cache_mem_valid[i]);
            l1[i]->mem_ready(cache_mem_ready[i]);
            l1[i]->mem_addr(cache_mem_addr[i]);
            l1[i]->mem_wr_en(cache_mem_wr_en[i]);
            l1[i]->mem_wr_data(cache_mem_wr_data[i]);
            l1[i]->mem_rd_data(cache_mem_rd_data[i]);
            l1[i]->mem_data_valid(cache_mem_data_valid[i]);
            
            // Connect to arbiter
            arbiter->cache_req[i](cache_mem_req[i]);
            arbiter->cache_ack[i](cache_mem_ack[i]);
            arbiter->cache_valid[i](cache_mem_valid[i]);
            arbiter->cache_ready[i](cache_mem_ready[i]);
            arbiter->cache_addr[i](cache_mem_addr[i]);
            arbiter->cache_wr_en[i](cache_mem_wr_en[i]);
            arbiter->cache_wr_data[i](cache_mem_wr_data[i]);
            arbiter->cache_rd_data[i](cache_mem_rd_data[i]);
            arbiter->cache_data_valid[i](cache_mem_data_valid[i]);
            
            // Connect to controller
            controller->pe_start[i](pe_start[i]);
            controller->pe_done[i](pe_done[i]);
            controller->pe_busy[i](pe_busy[i]);
            controller->pe_id[i](pe_id[i]);
            controller->work_addr[i](work_addr[i]);
            controller->work_size[i](work_size[i]);
        }
        
        // System reset process
        SC_CTHREAD(system_process, clk);
    }
    
    void system_process() {
        // Initialize memory with test data
        rst_n.write(false);
        wait(3);
        rst_n.write(true);
        
        cout << "\n=== LLM Accelerator with Memory Hierarchy ===" << endl;
        cout << "Architecture: 3x3 PE Grid with L1 Caches" << endl;
        cout << "L1 Cache: 64 lines, 4-way set associative, LRU replacement" << endl;
        cout << "Memory Hierarchy: PE -> L1 Cache -> Memory Arbiter -> Main Memory" << endl << endl;
        
        // Initialize some test data in memory
        for(int i = 0; i < 100; i++) {
            mem->memory[i] = i + 1;  // Simple test pattern
        }
        
        while(true) {
            wait();
        }
    }
    
    ~llm_accelerator_grid() {
        delete mem;
        delete arbiter;
        delete controller;
        for(int i = 0; i < 9; i++) {
            delete pe[i];
            delete l1[i];
        }
    }
};

// Main function
int sc_main(int argc, char* argv[]) {
    cout << "=== LLM Inference Accelerator with Memory Hierarchy ===" << endl;
    cout << "System Configuration:" << endl;
    cout << "- 3x3 Processing Element Grid (9 PEs)" << endl;
    cout << "- Individual L1 Cache per PE" << endl;
    cout << "- Shared Main Memory with Arbiter" << endl;
    cout << "- Round-robin memory arbitration" << endl << endl;
    
    llm_accelerator_grid system("llm_accelerator_grid");
    
    // Create trace file for waveform analysis
    sc_trace_file* tf = sc_create_vcd_trace_file("llm_grid_trace");
    tf->set_time_unit(1, SC_NS);
    
    // Trace key signals
    sc_trace(tf, system.clk, "clk");
    sc_trace(tf, system.rst_n, "rst_n");
    
    // Trace some PE and cache signals
    for(int i = 0; i < 3; i++) { // Trace first 3 PEs to avoid too many signals
        string pe_name = "pe" + to_string(i);
        string cache_name = "cache" + to_string(i);
        
        sc_trace(tf, system.pe_start[i], (pe_name + "_start").c_str());
        sc_trace(tf, system.pe_done[i], (pe_name + "_done").c_str());
        sc_trace(tf, system.pe_busy[i], (pe_name + "_busy").c_str());
        
        sc_trace(tf, system.pe_cache_req[i], (cache_name + "_req").c_str());
        sc_trace(tf, system.pe_cache_ack[i], (cache_name + "_ack").c_str());
        sc_trace(tf, system.cache_mem_req[i], (cache_name + "_mem_req").c_str());
        sc_trace(tf, system.l1[i]->cache_hits, (cache_name + "_hits").c_str());
        sc_trace(tf, system.l1[i]->cache_misses, (cache_name + "_misses").c_str());
    }
    
    // Trace arbiter signals
    sc_trace(tf, system.arbiter->current_master, "arbiter_current_master");
    sc_trace(tf, system.arbiter_mem_req, "memory_req");
    sc_trace(tf, system.arbiter_mem_ack, "memory_ack");
    
    cout << "Starting simulation..." << endl;
    sc_start(50000, SC_NS);  // Run for longer to see cache effects
    
    // Print cache statistics
    cout << "\n=== Cache Performance Statistics ===" << endl;
    for(int i = 0; i < 9; i++) {
        int hits = system.l1[i]->cache_hits.read();
        int misses = system.l1[i]->cache_misses.read();
        float hit_rate = (hits + misses > 0) ? (float)hits / (hits + misses) * 100.0 : 0.0;
        
        cout << "L1 Cache[" << i << "]: Hits=" << hits << ", Misses=" << misses 
             << ", Hit Rate=" << fixed << setprecision(1) << hit_rate << "%" << endl;
    }
    
    cout << "\nTrace file saved: llm_grid_trace.vcd" << endl;
    cout << "Use: gtkwave llm_grid_trace.vcd" << endl;
    
    sc_close_vcd_trace_file(tf);
    return 0;
}