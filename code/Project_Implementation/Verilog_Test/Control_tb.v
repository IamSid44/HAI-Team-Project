// ============================================================================
// Control Module Comprehensive Testbench
// ============================================================================
// This testbench validates the complete systolic array matrix multiplication
// system with tiling support for matrices larger than the hardware array size.
//
// COMPILATION:
// ------------
// To compile and run this testbench, you need all the following Verilog files:
//   - Control_tb.v       (this file - testbench)
//   - Control.v          (top-level controller with tiling logic)
//   - MatMul_Controller.v (single tile matrix multiplication controller)
//   - SA_MxN.v           (systolic array grid)
//   - PE.v               (processing element)
//   - Memory.v           (byte-addressed memory module)
//
// COMPILE AND RUN:
// ----------------
// iverilog -g2012 -o sim_control Control_tb.v Control.v MatMul_Controller.v SA_MxN.v PE.v Memory.v
// vvp sim_control
//
// VIEW WAVEFORMS:
// ---------------
// gtkwave control_test.vcd
//
// FEATURES:
// ---------
// - Automatically generates memory.txt file with test data
// - Tests multiple matrix sizes (3x3, 2x2, 5x5, 7x7, 4x6, 1x1, 10x10)
// - Tests both Weight Stationary and Output Stationary modes
// - Validates tiling for matrices larger than 3x3
// - Computes expected results and verifies accuracy
// ============================================================================

module Control_tb;

    parameter M = 3;
    parameter ADDR_WIDTH = 16;
    parameter MEM_SIZE = 65536;
    
    reg clk;
    reg reset;
    reg start;
    wire done;
    reg [7:0] K1, K2, K3;
    reg [ADDR_WIDTH-1:0] A_base_addr, W_base_addr, C_base_addr;
    reg output_stationary;
    
    Control #(
        .M(M),
        .ADDR_WIDTH(ADDR_WIDTH),
        .MEM_SIZE(MEM_SIZE)
    ) dut (
        .clk(clk),
        .reset(reset),
        .start(start),
        .done(done),
        .K1(K1),
        .K2(K2),
        .K3(K3),
        .A_base_addr(A_base_addr),
        .W_base_addr(W_base_addr),
        .C_base_addr(C_base_addr),
        .output_stationary(output_stationary)
    );
    
    integer i, j, k;
    real A_matrix [0:99];
    real W_matrix [0:99];
    real C_expected [0:99];
    real C_actual [0:99];
    real diff, max_diff;
    integer errors;
    integer max_elements;
    
    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end
    
    task compute_expected_result;
        input integer rows_a, cols_a, cols_w;
        integer ii, jj, kk;
        begin
            for (ii = 0; ii < rows_a; ii = ii + 1) begin
                for (jj = 0; jj < cols_w; jj = jj + 1) begin
                    C_expected[ii * cols_w + jj] = 0.0;
                    for (kk = 0; kk < cols_a; kk = kk + 1) begin
                        C_expected[ii * cols_w + jj] = C_expected[ii * cols_w + jj] + 
                                                        A_matrix[ii * cols_a + kk] * 
                                                        W_matrix[kk * cols_w + jj];
                    end
                end
            end
        end
    endtask
    
    task verify_result;
        input integer rows, cols;
        integer ii, jj;
        begin
            errors = 0;
            max_diff = 0.0;
            
            for (ii = 0; ii < rows; ii = ii + 1) begin
                for (jj = 0; jj < cols; jj = jj + 1) begin
                    diff = C_expected[ii * cols + jj] - C_actual[ii * cols + jj];
                    if (diff < 0) diff = -diff;
                    
                    if (diff > max_diff) begin
                        max_diff = diff;
                    end
                    
                    if (diff > 0.01) begin
                        if (errors < 10) begin  // Only print first 10 errors
                            $display("ERROR at [%0d][%0d]: Expected %f, Got %f (diff=%f)", 
                                     ii, jj, C_expected[ii * cols + jj], C_actual[ii * cols + jj], diff);
                        end
                        errors = errors + 1;
                    end
                end
            end
            
            if (errors > 10) begin
                $display("... and %0d more errors", errors - 10);
            end
        end
    endtask
    
    task generate_memory_file;
        integer file_handle;
        integer addr_idx;
        begin
            file_handle = $fopen("memory.txt", "w");
            
            if (file_handle == 0) begin
                $display("ERROR: Could not create memory.txt file");
                $finish;
            end
            
            $fdisplay(file_handle, "# Byte-Addressed Memory File");
            $fdisplay(file_handle, "# Format: BYTE_ADDRESS: VALUE");
            $fdisplay(file_handle, "# XXXX indicates uninitialized memory");
            $fdisplay(file_handle, "#");
            
            // Write A matrix (addresses 0-799 for up to 10x10 matrix, 8 bytes per element)
            for (addr_idx = 0; addr_idx < 100; addr_idx = addr_idx + 1) begin
                if (addr_idx < max_elements) begin
                    $fdisplay(file_handle, "%08d: %f", addr_idx * 8, A_matrix[addr_idx]);
                end else begin
                    $fdisplay(file_handle, "%08d: XXXX", addr_idx * 8);
                end
            end
            
            // Write W matrix (addresses 2048-2847 for up to 10x10 matrix)
            for (addr_idx = 0; addr_idx < 100; addr_idx = addr_idx + 1) begin
                if (addr_idx < max_elements) begin
                    $fdisplay(file_handle, "%08d: %f", 2048 + addr_idx * 8, W_matrix[addr_idx]);
                end else begin
                    $fdisplay(file_handle, "%08d: XXXX", 2048 + addr_idx * 8);
                end
            end
            
            // Write C matrix placeholder (addresses 4096-4895, all XXXX initially)
            for (addr_idx = 0; addr_idx < 100; addr_idx = addr_idx + 1) begin
                $fdisplay(file_handle, "%08d: XXXX", 4096 + addr_idx * 8);
            end
            
            $fclose(file_handle);
            $display("Generated memory.txt file with %0d elements", max_elements);
        end
    endtask
    
    initial begin
        $dumpfile("control_test.vcd");
        $dumpvars(0, Control_tb);
        
        $display("=======================================================");
        $display("         Control Module Comprehensive Testbench");
        $display("=======================================================");
        
        // Initialize max_elements for the first test
        max_elements = 9;  // Will be updated for each test
        
        reset = 1;
        start = 0;
        K1 = 0;
        K2 = 0;
        K3 = 0;
        A_base_addr = 0;
        W_base_addr = 0;
        C_base_addr = 0;
        output_stationary = 0;
        
        #20;
        reset = 0;
        #20;
        
        // TEST 1: Optimal Size 3x3 (No Tiling) - Weight Stationary
        $display("\n-------------------------------------------------------");
        $display("TEST 1: Optimal 3x3 Matrix - Weight Stationary");
        $display("-------------------------------------------------------");
        
        K1 = 3;
        K2 = 3;
        K3 = 3;
        A_base_addr = 16'd0;
        W_base_addr = 16'd2048;
        C_base_addr = 16'd4096;
        output_stationary = 0;
        
        // A = [1 2 3; 4 5 6; 7 8 9]
        A_matrix[0] = 1.0; A_matrix[1] = 2.0; A_matrix[2] = 3.0;
        A_matrix[3] = 4.0; A_matrix[4] = 5.0; A_matrix[5] = 6.0;
        A_matrix[6] = 7.0; A_matrix[7] = 8.0; A_matrix[8] = 9.0;
        
        // W = Identity
        W_matrix[0] = 1.0; W_matrix[1] = 0.0; W_matrix[2] = 0.0;
        W_matrix[3] = 0.0; W_matrix[4] = 1.0; W_matrix[5] = 0.0;
        W_matrix[6] = 0.0; W_matrix[7] = 0.0; W_matrix[8] = 1.0;
        
        // Generate memory.txt file for this test
        max_elements = 9;
        generate_memory_file();
        
        for (i = 0; i < 9; i = i + 1) begin
            dut.memory_inst.mem_array[A_base_addr/8 + i] = $realtobits(A_matrix[i]);
            dut.memory_inst.mem_array[W_base_addr/8 + i] = $realtobits(W_matrix[i]);
            dut.memory_inst.mem_array[C_base_addr/8 + i] = 64'h0;
        end
        
        compute_expected_result(3, 3, 3);
        
        $display("Starting 3x3 computation (no tiling)...");
        
        start = 1;
        #10;
        start = 0;
        
        wait(done == 1 || $time > 10000);
        
        if (done !== 1) begin
            $display("ERROR: Timeout waiting for done signal");
        end else begin
            $display("Computation completed at time %0t", $time);
        end
        
        #50;
        
        for (i = 0; i < 9; i = i + 1) begin
            C_actual[i] = $bitstoreal(dut.memory_inst.mem_array[C_base_addr/8 + i]);
        end
        
        $display("\nResult Matrix C:");
        for (i = 0; i < 3; i = i + 1) begin
            $write("  ");
            for (j = 0; j < 3; j = j + 1) begin
                $write("%6.2f ", C_actual[i*3 + j]);
            end
            $display("");
        end
        
        verify_result(3, 3);
        
        if (errors == 0) begin
            $display("\nTEST 1 PASSED: 3x3 optimal size (max_diff=%f)", max_diff);
        end else begin
            $display("\nTEST 1 FAILED: %0d errors found", errors);
        end
        
        // TEST 2: Undersized 2x2 Matrix - Weight Stationary
        $display("\n-------------------------------------------------------");
        $display("TEST 2: Undersized 2x2 Matrix - Weight Stationary");
        $display("-------------------------------------------------------");
        
        reset = 1;
        #20;
        reset = 0;
        #20;
        
        K1 = 2;
        K2 = 2;
        K3 = 2;
        A_base_addr = 16'd0;
        W_base_addr = 16'd2048;
        C_base_addr = 16'd4096;
        output_stationary = 0;
        
        // A = [1 2; 3 4]
        A_matrix[0] = 1.0; A_matrix[1] = 2.0;
        A_matrix[2] = 3.0; A_matrix[3] = 4.0;
        
        // W = [2 3; 4 5]
        W_matrix[0] = 2.0; W_matrix[1] = 3.0;
        W_matrix[2] = 4.0; W_matrix[3] = 5.0;
        
        for (i = 0; i < 4; i = i + 1) begin
            dut.memory_inst.mem_array[A_base_addr/8 + i] = $realtobits(A_matrix[i]);
            dut.memory_inst.mem_array[W_base_addr/8 + i] = $realtobits(W_matrix[i]);
            dut.memory_inst.mem_array[C_base_addr/8 + i] = 64'h0;
        end
        
        compute_expected_result(2, 2, 2);
        
        $display("Expected C: [[10, 13], [22, 29]]");
        
        start = 1;
        #10;
        start = 0;
        
        wait(done == 1 || $time > 10000);
        #50;
        
        for (i = 0; i < 4; i = i + 1) begin
            C_actual[i] = $bitstoreal(dut.memory_inst.mem_array[C_base_addr/8 + i]);
        end
        
        $display("\nResult Matrix C:");
        for (i = 0; i < 2; i = i + 1) begin
            $write("  ");
            for (j = 0; j < 2; j = j + 1) begin
                $write("%6.2f ", C_actual[i*2 + j]);
            end
            $display("");
        end
        
        verify_result(2, 2);
        
        if (errors == 0) begin
            $display("\nTEST 2 PASSED: 2x2 undersized matrix");
        end else begin
            $display("\nTEST 2 FAILED: %0d errors found", errors);
        end
        
        // TEST 3: 5x5 Matrix Requiring Tiling - Weight Stationary
        $display("\n-------------------------------------------------------");
        $display("TEST 3: 5x5 Matrix with Tiling - Weight Stationary");
        $display("-------------------------------------------------------");
        
        reset = 1;
        #20;
        reset = 0;
        #20;
        
        K1 = 5;
        K2 = 5;
        K3 = 5;
        A_base_addr = 16'd0;
        W_base_addr = 16'd2048;
        C_base_addr = 16'd4096;
        output_stationary = 0;
        
        // A = [1..25]
        for (i = 0; i < 25; i = i + 1) begin
            A_matrix[i] = i + 1;
        end
        
        // W = Identity 5x5
        for (i = 0; i < 25; i = i + 1) begin
            W_matrix[i] = 0.0;
        end
        W_matrix[0] = 1.0;
        W_matrix[6] = 1.0;
        W_matrix[12] = 1.0;
        W_matrix[18] = 1.0;
        W_matrix[24] = 1.0;
        
        for (i = 0; i < 25; i = i + 1) begin
            dut.memory_inst.mem_array[A_base_addr/8 + i] = $realtobits(A_matrix[i]);
            dut.memory_inst.mem_array[W_base_addr/8 + i] = $realtobits(W_matrix[i]);
            dut.memory_inst.mem_array[C_base_addr/8 + i] = 64'h0;
        end
        
        compute_expected_result(5, 5, 5);
        
        $display("Testing 5x5 matrix (requires tiling into 2x2 tiles of 3x3)");
        $display("Number of tiles: i_tiles=%0d, j_tiles=%0d, k_tiles=%0d", 
                 (5 + M - 1) / M, (5 + M - 1) / M, (5 + M - 1) / M);
        
        start = 1;
        #10;
        start = 0;
        
        wait(done == 1 || $time > 50000);
        
        if (done !== 1) begin
            $display("ERROR: Timeout waiting for done signal");
        end else begin
            $display("Computation completed at time %0t", $time);
        end
        
        #50;
        
        for (i = 0; i < 25; i = i + 1) begin
            C_actual[i] = $bitstoreal(dut.memory_inst.mem_array[C_base_addr/8 + i]);
        end
        
        $display("\nResult Matrix C (first 3 rows):");
        for (i = 0; i < 3; i = i + 1) begin
            $write("  ");
            for (j = 0; j < 5; j = j + 1) begin
                $write("%6.2f ", C_actual[i*5 + j]);
            end
            $display("");
        end
        
        verify_result(5, 5);
        
        if (errors == 0) begin
            $display("\nTEST 3 PASSED: 5x5 tiled matrix multiplication");
        end else begin
            $display("\nTEST 3 FAILED: %0d errors found", errors);
        end
        
        // TEST 4: 7x7 Matrix with More Tiling - Output Stationary
        $display("\n-------------------------------------------------------");
        $display("TEST 4: 7x7 Matrix with Tiling - Output Stationary");
        $display("-------------------------------------------------------");
        
        reset = 1;
        #20;
        reset = 0;
        #20;
        
        K1 = 7;
        K2 = 7;
        K3 = 7;
        A_base_addr = 16'd0;
        W_base_addr = 16'd2048;
        C_base_addr = 16'd4096;
        output_stationary = 1;
        
        // A = [1..49]
        for (i = 0; i < 49; i = i + 1) begin
            A_matrix[i] = i + 1;
        end
        
        // W = Identity 7x7
        for (i = 0; i < 49; i = i + 1) begin
            W_matrix[i] = 0.0;
        end
        for (i = 0; i < 7; i = i + 1) begin
            W_matrix[i * 7 + i] = 1.0;
        end
        
        for (i = 0; i < 49; i = i + 1) begin
            dut.memory_inst.mem_array[A_base_addr/8 + i] = $realtobits(A_matrix[i]);
            dut.memory_inst.mem_array[W_base_addr/8 + i] = $realtobits(W_matrix[i]);
            dut.memory_inst.mem_array[C_base_addr/8 + i] = 64'h0;
        end
        
        compute_expected_result(7, 7, 7);
        
        $display("Testing 7x7 matrix with Output Stationary mode");
        $display("Number of tiles: i_tiles=%0d, j_tiles=%0d, k_tiles=%0d", 
                 (7 + M - 1) / M, (7 + M - 1) / M, (7 + M - 1) / M);
        
        start = 1;
        #10;
        start = 0;
        
        wait(done == 1 || $time > 100000);
        
        if (done !== 1) begin
            $display("ERROR: Timeout waiting for done signal");
        end else begin
            $display("Computation completed at time %0t", $time);
        end
        
        #50;
        
        for (i = 0; i < 49; i = i + 1) begin
            C_actual[i] = $bitstoreal(dut.memory_inst.mem_array[C_base_addr/8 + i]);
        end
        
        $display("\nResult Matrix C (first 3 rows):");
        for (i = 0; i < 3; i = i + 1) begin
            $write("  ");
            for (j = 0; j < 7; j = j + 1) begin
                $write("%6.2f ", C_actual[i*7 + j]);
            end
            $display("");
        end
        
        verify_result(7, 7);
        
        if (errors == 0) begin
            $display("\nTEST 4 PASSED: 7x7 tiled matrix with Output Stationary");
        end else begin
            $display("\nTEST 4 FAILED: %0d errors found", errors);
        end
        
        // TEST 5: Non-square Matrix 4x6 * 6x5
        $display("\n-------------------------------------------------------");
        $display("TEST 5: Non-square 4x6 * 6x5 Matrix");
        $display("-------------------------------------------------------");
        
        reset = 1;
        #20;
        reset = 0;
        #20;
        
        K1 = 4;
        K2 = 6;
        K3 = 5;
        A_base_addr = 16'd0;
        W_base_addr = 16'd2048;
        C_base_addr = 16'd4096;
        output_stationary = 0;
        
        // A = 4x6 matrix [1..24]
        for (i = 0; i < 24; i = i + 1) begin
            A_matrix[i] = i + 1;
        end
        
        // W = 6x5 matrix (simple pattern)
        for (i = 0; i < 30; i = i + 1) begin
            W_matrix[i] = (i % 5) + 1;
        end
        
        for (i = 0; i < 24; i = i + 1) begin
            dut.memory_inst.mem_array[A_base_addr/8 + i] = $realtobits(A_matrix[i]);
        end
        for (i = 0; i < 30; i = i + 1) begin
            dut.memory_inst.mem_array[W_base_addr/8 + i] = $realtobits(W_matrix[i]);
        end
        for (i = 0; i < 20; i = i + 1) begin
            dut.memory_inst.mem_array[C_base_addr/8 + i] = 64'h0;
        end
        
        compute_expected_result(4, 6, 5);
        
        $display("Testing 4x6 * 6x5 = 4x5 matrix multiplication");
        
        start = 1;
        #10;
        start = 0;
        
        wait(done == 1 || $time > 50000);
        
        if (done !== 1) begin
            $display("ERROR: Timeout waiting for done signal");
        end else begin
            $display("Computation completed at time %0t", $time);
        end
        
        #50;
        
        for (i = 0; i < 20; i = i + 1) begin
            C_actual[i] = $bitstoreal(dut.memory_inst.mem_array[C_base_addr/8 + i]);
        end
        
        $display("\nResult Matrix C (4x5):");
        for (i = 0; i < 4; i = i + 1) begin
            $write("  ");
            for (j = 0; j < 5; j = j + 1) begin
                $write("%7.1f ", C_actual[i*5 + j]);
            end
            $display("");
        end
        
        verify_result(4, 5);
        
        if (errors == 0) begin
            $display("\nTEST 5 PASSED: Non-square matrix multiplication");
        end else begin
            $display("\nTEST 5 FAILED: %0d errors found", errors);
        end
        
        // TEST 6: Undersized 1x1 Matrix
        $display("\n-------------------------------------------------------");
        $display("TEST 6: Minimal 1x1 Matrix");
        $display("-------------------------------------------------------");
        
        reset = 1;
        #20;
        reset = 0;
        #20;
        
        K1 = 1;
        K2 = 1;
        K3 = 1;
        A_base_addr = 16'd0;
        W_base_addr = 16'd2048;
        C_base_addr = 16'd4096;
        output_stationary = 0;
        
        A_matrix[0] = 7.0;
        W_matrix[0] = 11.0;
        
        dut.memory_inst.mem_array[A_base_addr/8] = $realtobits(7.0);
        dut.memory_inst.mem_array[W_base_addr/8] = $realtobits(11.0);
        dut.memory_inst.mem_array[C_base_addr/8] = 64'h0;
        
        C_expected[0] = 77.0;
        
        $display("A = [7], W = [11], Expected C = [77]");
        
        start = 1;
        #10;
        start = 0;
        
        wait(done == 1 || $time > 10000);
        #50;
        
        C_actual[0] = $bitstoreal(dut.memory_inst.mem_array[C_base_addr/8]);
        
        $display("Result C = [%f]", C_actual[0]);
        
        verify_result(1, 1);
        
        if (errors == 0) begin
            $display("\nTEST 6 PASSED: 1x1 minimal matrix");
        end else begin
            $display("\nTEST 6 FAILED: %0d errors found", errors);
        end
        
        // TEST 7: Large Matrix 10x10 (Heavy Tiling)
        $display("\n-------------------------------------------------------");
        $display("TEST 7: Large 10x10 Matrix (Heavy Tiling)");
        $display("-------------------------------------------------------");
        
        reset = 1;
        #20;
        reset = 0;
        #20;
        
        K1 = 10;
        K2 = 10;
        K3 = 10;
        A_base_addr = 16'd0;
        W_base_addr = 16'd2048;
        C_base_addr = 16'd4096;
        output_stationary = 0;
        
        // A = simple counting matrix
        for (i = 0; i < 100; i = i + 1) begin
            A_matrix[i] = (i / 10) + 1;
        end
        
        // W = Identity 10x10
        for (i = 0; i < 100; i = i + 1) begin
            W_matrix[i] = 0.0;
        end
        for (i = 0; i < 10; i = i + 1) begin
            W_matrix[i * 10 + i] = 1.0;
        end
        
        for (i = 0; i < 100; i = i + 1) begin
            dut.memory_inst.mem_array[A_base_addr/8 + i] = $realtobits(A_matrix[i]);
            dut.memory_inst.mem_array[W_base_addr/8 + i] = $realtobits(W_matrix[i]);
            dut.memory_inst.mem_array[C_base_addr/8 + i] = 64'h0;
        end
        
        compute_expected_result(10, 10, 10);
        
        $display("Testing 10x10 matrix (requires 4x4x4 = 64 tile operations)");
        
        start = 1;
        #10;
        start = 0;
        
        wait(done == 1 || $time > 200000);
        
        if (done !== 1) begin
            $display("ERROR: Timeout waiting for done signal");
        end else begin
            $display("Computation completed at time %0t", $time);
        end
        
        #50;
        
        for (i = 0; i < 100; i = i + 1) begin
            C_actual[i] = $bitstoreal(dut.memory_inst.mem_array[C_base_addr/8 + i]);
        end
        
        $display("\nResult Matrix C (first 5 rows):");
        for (i = 0; i < 5; i = i + 1) begin
            $write("  ");
            for (j = 0; j < 10; j = j + 1) begin
                $write("%5.1f ", C_actual[i*10 + j]);
            end
            $display("");
        end
        
        verify_result(10, 10);
        
        if (errors == 0) begin
            $display("\nTEST 7 PASSED: 10x10 large matrix with heavy tiling");
        end else begin
            $display("\nTEST 7 FAILED: %0d errors found", errors);
        end
        
        #100;
        
        $display("\n=======================================================");
        $display("        Control Module Tests Completed");
        $display("=======================================================\n");
        
        $finish;
    end
    
    initial begin
        #2000000;
        $display("\nERROR: Testbench timeout!");
        $finish;
    end

endmodule
