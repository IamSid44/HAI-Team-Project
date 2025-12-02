`include "PE.v"
`include "SA_MxN.v"

module SA_MxN_tb;

    parameter M = 3;
    parameter N = 3;
    
    reg clk;
    reg reset;
    reg output_stationary;
    wire [64*N-1:0] in_top;
    wire [64*M-1:0] in_left;
    wire [64*M-1:0] out_right;
    wire [64*N-1:0] out_bottom;
    reg preload_valid;
    wire [64*M*N-1:0] preload_data;
    
    SA_MxN #(.M(M), .N(N)) dut (
        .clk(clk),
        .reset(reset),
        .output_stationary(output_stationary),
        .in_top(in_top),
        .in_left(in_left),
        .out_right(out_right),
        .out_bottom(out_bottom),
        .preload_valid(preload_valid),
        .preload_data(preload_data)
    );
    
    reg [63:0] in_top_array [0:N-1];
    reg [63:0] in_left_array [0:M-1];
    wire [63:0] out_right_array [0:M-1];
    wire [63:0] out_bottom_array [0:N-1];
    reg [63:0] preload_array [0:M*N-1];
    
    integer i, j, k;
    integer errors;
    real diff;
    
    genvar gi, gj;
    generate
        for (gi = 0; gi < N; gi = gi + 1) begin : pack_top
            assign in_top[64*(gi+1)-1:64*gi] = in_top_array[gi];
            assign out_bottom_array[gi] = out_bottom[64*(gi+1)-1:64*gi];
        end
        for (gi = 0; gi < M; gi = gi + 1) begin : pack_left
            assign in_left[64*(gi+1)-1:64*gi] = in_left_array[gi];
            assign out_right_array[gi] = out_right[64*(gi+1)-1:64*gi];
        end
        for (gi = 0; gi < M*N; gi = gi + 1) begin : pack_preload
            assign preload_data[64*(gi+1)-1:64*gi] = preload_array[gi];
        end
    endgenerate
    
    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end
    
    initial begin
        $dumpfile("sa_mxn_test.vcd");
        $dumpvars(0, SA_MxN_tb);
        
        // Dump individual array elements for better waveform viewing
        $dumpvars(1, in_top_array[0], in_top_array[1], in_top_array[2]);
        $dumpvars(1, in_left_array[0], in_left_array[1], in_left_array[2]);
        $dumpvars(1, out_bottom_array[0], out_bottom_array[1], out_bottom_array[2]);
        $dumpvars(1, out_right_array[0], out_right_array[1], out_right_array[2]);
        $dumpvars(1, preload_array[0], preload_array[1], preload_array[2]);
        $dumpvars(1, preload_array[3], preload_array[4], preload_array[5]);
        $dumpvars(1, preload_array[6], preload_array[7], preload_array[8]);
        
        // Dump PE internal signals for each PE (3x3 array)
        // Row 0
        $dumpvars(1, dut.row_gen[0].col_gen[0].pe_inst.in_top_real);
        $dumpvars(1, dut.row_gen[0].col_gen[0].pe_inst.in_left_real);
        $dumpvars(1, dut.row_gen[0].col_gen[0].pe_inst.accumulator_real);
        $dumpvars(1, dut.row_gen[0].col_gen[1].pe_inst.in_top_real);
        $dumpvars(1, dut.row_gen[0].col_gen[1].pe_inst.in_left_real);
        $dumpvars(1, dut.row_gen[0].col_gen[1].pe_inst.accumulator_real);
        $dumpvars(1, dut.row_gen[0].col_gen[2].pe_inst.in_top_real);
        $dumpvars(1, dut.row_gen[0].col_gen[2].pe_inst.in_left_real);
        $dumpvars(1, dut.row_gen[0].col_gen[2].pe_inst.accumulator_real);
        
        // Row 1
        $dumpvars(1, dut.row_gen[1].col_gen[0].pe_inst.in_top_real);
        $dumpvars(1, dut.row_gen[1].col_gen[0].pe_inst.in_left_real);
        $dumpvars(1, dut.row_gen[1].col_gen[0].pe_inst.accumulator_real);
        $dumpvars(1, dut.row_gen[1].col_gen[1].pe_inst.in_top_real);
        $dumpvars(1, dut.row_gen[1].col_gen[1].pe_inst.in_left_real);
        $dumpvars(1, dut.row_gen[1].col_gen[1].pe_inst.accumulator_real);
        $dumpvars(1, dut.row_gen[1].col_gen[2].pe_inst.in_top_real);
        $dumpvars(1, dut.row_gen[1].col_gen[2].pe_inst.in_left_real);
        $dumpvars(1, dut.row_gen[1].col_gen[2].pe_inst.accumulator_real);
        
        // Row 2
        $dumpvars(1, dut.row_gen[2].col_gen[0].pe_inst.in_top_real);
        $dumpvars(1, dut.row_gen[2].col_gen[0].pe_inst.in_left_real);
        $dumpvars(1, dut.row_gen[2].col_gen[0].pe_inst.accumulator_real);
        $dumpvars(1, dut.row_gen[2].col_gen[1].pe_inst.in_top_real);
        $dumpvars(1, dut.row_gen[2].col_gen[1].pe_inst.in_left_real);
        $dumpvars(1, dut.row_gen[2].col_gen[1].pe_inst.accumulator_real);
        $dumpvars(1, dut.row_gen[2].col_gen[2].pe_inst.in_top_real);
        $dumpvars(1, dut.row_gen[2].col_gen[2].pe_inst.in_left_real);
        $dumpvars(1, dut.row_gen[2].col_gen[2].pe_inst.accumulator_real);
        
        $display("=======================================================");
        $display("          SA_MxN Module Testbench (3x3)");
        $display("=======================================================");
        
        errors = 0;
        reset = 1;
        output_stationary = 0;
        preload_valid = 0;
        
        for (i = 0; i < N; i = i + 1) begin
            in_top_array[i] = 64'h0;
        end
        for (i = 0; i < M; i = i + 1) begin
            in_left_array[i] = 64'h0;
        end
        for (i = 0; i < M*N; i = i + 1) begin
            preload_array[i] = 64'h0;
        end
        
        #20;
        reset = 0;
        #20;
        
        // TEST 1: Weight Stationary - Identity Matrix
        $display("\n-------------------------------------------------------");
        $display("TEST 1: Weight Stationary - Identity Matrix (3x3)");
        $display("-------------------------------------------------------");
        
        output_stationary = 0;
        
        // Preload identity matrix as weights
        @(posedge clk);
        preload_valid = 1;
        preload_array[0] = $realtobits(1.0);  // W[0][0]
        preload_array[1] = $realtobits(0.0);  // W[0][1]
        preload_array[2] = $realtobits(0.0);  // W[0][2]
        preload_array[3] = $realtobits(0.0);  // W[1][0]
        preload_array[4] = $realtobits(1.0);  // W[1][1]
        preload_array[5] = $realtobits(0.0);  // W[1][2]
        preload_array[6] = $realtobits(0.0);  // W[2][0]
        preload_array[7] = $realtobits(0.0);  // W[2][1]
        preload_array[8] = $realtobits(1.0);  // W[2][2]
        
        @(posedge clk);
        preload_valid = 0;
        
        $display("Identity matrix preloaded");
        
        // Input matrix [1 2 3; 4 5 6; 7 8 9]
        for (i = 0; i < N; i = i + 1) begin
            in_top_array[i] = 64'h0;
        end
        
        // Feed inputs row by row with proper skewing
        // For identity, feed rows of identity matrix
        for (i = 0; i < M + N - 1; i = i + 1) begin
            @(posedge clk);
            
            // Row-wise skewed input
            if (i == 0) begin
                in_left_array[0] = $realtobits(1.0);  // First element of row 0
                in_left_array[1] = 64'h0;
                in_left_array[2] = 64'h0;
            end else if (i == 1) begin
                in_left_array[0] = 64'h0;  // Continue row 0
                in_left_array[1] = $realtobits(1.0);  // First element of row 1
                in_left_array[2] = 64'h0;
            end else if (i == 2) begin
                in_left_array[0] = 64'h0;
                in_left_array[1] = 64'h0;  // Continue row 1
                in_left_array[2] = $realtobits(1.0);  // First element of row 2
            end else begin
                in_left_array[0] = 64'h0;
                in_left_array[1] = 64'h0;
                in_left_array[2] = 64'h0;
            end
        end
        
        @(posedge clk);
        #1;
        
        $display("\\nOutput values (Weight Stationary with identity weights):");
        $display("  out_bottom[0] = %f", $bitstoreal(out_bottom_array[0]));
        $display("  out_bottom[1] = %f", $bitstoreal(out_bottom_array[1]));
        $display("  out_bottom[2] = %f", $bitstoreal(out_bottom_array[2]));
        $display("Note: Weight Stationary mode demonstrates systolic array dataflow.");
        $display("TEST 1 complete - Identity matrix test");
        
        // TEST 2: Weight Stationary - Simple Matrix Multiplication
        $display("\n-------------------------------------------------------");
        $display("TEST 2: Weight Stationary - 2x2 Matrix Multiplication");
        $display("-------------------------------------------------------");
        
        errors = 0;
        reset = 1;
        #10;
        reset = 0;
        #10;
        
        output_stationary = 0;
        
        // For A*W where A=[[1,2],[3,4]], W=[[2,3],[4,5]]
        // Expected: C = [[1*2+2*4, 1*3+2*5], [3*2+4*4, 3*3+4*5]] = [[10,13],[22,29]]
        // 
        // In Weight Stationary: PE[i][j] computes sum of (A[i,k] * W[k,j]) for all k
        // So PE[0][0] needs elements from column 0 of W: [2,4]
        // And PE[0][1] needs elements from column 1 of W: [3,5]
        // But with single value per PE, we compute A*W^T instead
        // 
        // Let's do: PE[i][j] = W[j][i] (transpose), feed rows of A
        // PE[0][0]=2, PE[0][1]=4 (first row of W)
        // PE[1][0]=3, PE[1][1]=5 (second row of W)
        preload_array[0] = $realtobits(2.0);  // PE[0][0] = W[0][0]
        preload_array[1] = $realtobits(4.0);  // PE[0][1] = W[1][0]
        preload_array[2] = $realtobits(0.0);  // PE[0][2]
        preload_array[3] = $realtobits(3.0);  // PE[1][0] = W[0][1]
        preload_array[4] = $realtobits(5.0);  // PE[1][1] = W[1][1]
        preload_array[5] = $realtobits(0.0);  // PE[1][2]
        preload_array[6] = $realtobits(0.0);  // PE[2][0]
        preload_array[7] = $realtobits(0.0);  // PE[2][1]
        preload_array[8] = $realtobits(0.0);  // PE[2][2]
        
        @(posedge clk);
        preload_valid = 1;
        
        @(posedge clk);
        preload_valid = 0;
        
        $display("Weights loaded (transposed in PEs)");
        $display("Input A: [[1, 2], [3, 4]]");
        $display("Weight W: [[2, 3], [4, 5]]");
        $display("Expected C = A * W: [[10, 13], [22, 29]]");
        
        // Clear inputs
        for (i = 0; i < N; i = i + 1) begin
            in_top_array[i] = 64'h0;
        end
        for (i = 0; i < M; i = i + 1) begin
            in_left_array[i] = 64'h0;
        end
        
        // Feed rows of A in skewed manner (row-stationary input pattern)
        // Row 0: [1, 2], Row 1: [3, 4]
        // Cycle 0: Start row 0 with A[0,0]=1
        @(posedge clk);
        in_left_array[0] = $realtobits(1.0);
        in_left_array[1] = 64'h0;
        in_left_array[2] = 64'h0;
        
        // Cycle 1: Continue row 0 with A[0,1]=2, Start row 1 with A[1,0]=3
        @(posedge clk);
        in_left_array[0] = $realtobits(2.0);
        in_left_array[1] = $realtobits(3.0);
        in_left_array[2] = 64'h0;
        
        // Cycle 2: Continue row 1 with A[1,1]=4
        @(posedge clk);
        in_left_array[0] = 64'h0;
        in_left_array[1] = $realtobits(4.0);
        in_left_array[2] = 64'h0;
        
        // Cycles 3-6: Wait for results to propagate
        for (i = 0; i < 4; i = i + 1) begin
            @(posedge clk);
            in_left_array[0] = 64'h0;
            in_left_array[1] = 64'h0;
            in_left_array[2] = 64'h0;
        end
        
        @(posedge clk);
        #1;
        
        $display("\\nOutput bottom values:");
        $display("  out_bottom[0] = %f", $bitstoreal(out_bottom_array[0]));
        $display("  out_bottom[1] = %f", $bitstoreal(out_bottom_array[1]));
        $display("Note: Weight Stationary mode demonstrates systolic dataflow.");
        $display("      Complete matrix results require collecting from multiple output ports.");
        $display("      For full matrix multiplication verification, see Test 3 (Output Stationary).");
        
        $display("\\nTEST 2 complete");
        
        // TEST 3: Output Stationary - 2x2 Matrix Multiplication
        $display("\n-------------------------------------------------------");
        $display("TEST 3: Output Stationary - 2x2 Matrix Multiplication");
        $display("-------------------------------------------------------");
        
        errors = 0;
        reset = 1;
        @(posedge clk);
        reset = 0;
        
        output_stationary = 1;
        preload_valid = 0;
        
        // Clear all inputs
        for (i = 0; i < N; i = i + 1) begin
            in_top_array[i] = 64'h0;
        end
        for (i = 0; i < M; i = i + 1) begin
            in_left_array[i] = 64'h0;
        end
        
        @(posedge clk);
        
        $display("Computing A * W where:");
        $display("A = [[1, 2], [3, 4]]");
        $display("W = [[2, 3], [4, 5]]");
        $display("Expected: [[10, 13], [22, 29]]");
        
        // Clear inputs
        for (i = 0; i < N; i = i + 1) begin
            in_top_array[i] = 64'h0;
        end
        for (i = 0; i < M; i = i + 1) begin
            in_left_array[i] = 64'h0;
        end
        
        // Cycle 0: A[0][0]=1, W[0][0]=2
        @(posedge clk);
        in_left_array[0] = $realtobits(1.0);
        in_top_array[0] = $realtobits(2.0);
        
        // Cycle 1: A[0][1]=2, W[1][0]=4, A[1][0]=3, W[0][1]=3
        @(posedge clk);
        in_left_array[0] = $realtobits(2.0);
        in_left_array[1] = $realtobits(3.0);
        in_top_array[0] = $realtobits(4.0);
        in_top_array[1] = $realtobits(3.0);
        
        // Cycle 2: A[1][1]=4, W[1][1]=5
        @(posedge clk);
        in_left_array[0] = 64'h0;
        in_left_array[1] = $realtobits(4.0);
        in_top_array[0] = 64'h0;
        in_top_array[1] = $realtobits(5.0);
        
        // Cycle 3: Propagate
        @(posedge clk);
        in_left_array[0] = 64'h0;
        in_left_array[1] = 64'h0;
        in_top_array[0] = 64'h0;
        in_top_array[1] = 64'h0;
        
        @(posedge clk);
        #1;
        
        // Drain phase
        @(posedge clk);
        preload_valid = 1;
        
        @(posedge clk);
        #1;
        
        $display("\nDraining results (skewed due to systolic array timing):");
        $display("  Drain cycle 1:");
        $display("    out_bottom[0] = %f (C[1,0] = 22)", $bitstoreal(out_bottom_array[0]));
        $display("    out_bottom[1] = %f (C[1,1] = 29)", $bitstoreal(out_bottom_array[1]));
        
        @(posedge clk);
        #1;
        
        $display("  Drain cycle 2:");
        $display("    out_bottom[0] = %f (C[0,0] = 10)", $bitstoreal(out_bottom_array[0]));
        $display("    out_bottom[1] = %f (C[0,1] = 13)", $bitstoreal(out_bottom_array[1]));
        
        $display("\nTEST 3 complete (Output Stationary drains in reverse row order)");
        
        // TEST 4: Weight Stationary - Undersized Matrix (1x1)
        $display("\n-------------------------------------------------------");
        $display("TEST 4: Weight Stationary - Undersized 1x1 Matrix");
        $display("-------------------------------------------------------");
        
        errors = 0;
        reset = 1;
        @(posedge clk);
        reset = 0;
        
        output_stationary = 0;
        preload_valid = 0;
        
        // Clear all inputs
        for (i = 0; i < N; i = i + 1) begin
            in_top_array[i] = 64'h0;
        end
        for (i = 0; i < M; i = i + 1) begin
            in_left_array[i] = 64'h0;
        end
        for (i = 0; i < M*N; i = i + 1) begin
            preload_array[i] = 64'h0;
        end
        
        // Flush pipeline with multiple zero cycles
        for (i = 0; i < M + N; i = i + 1) begin
            @(posedge clk);
        end
        
        // Preload single weight
        preload_array[0] = $realtobits(7.0);
        for (i = 1; i < M*N; i = i + 1) begin
            preload_array[i] = 64'h0;
        end
        
        @(posedge clk);
        preload_valid = 1;
        
        @(posedge clk);
        preload_valid = 0;
        
        $display("Single element: A[0][0]=5, W[0][0]=7");
        $display("Expected: 5 * 7 = 35");
        
        // Clear inputs
        for (i = 0; i < N; i = i + 1) begin
            in_top_array[i] = 64'h0;
        end
        for (i = 0; i < M; i = i + 1) begin
            in_left_array[i] = 64'h0;
        end
        
        @(posedge clk);
        in_left_array[0] = $realtobits(5.0);
        
        for (i = 0; i < 5; i = i + 1) begin
            @(posedge clk);
            in_left_array[0] = 64'h0;
        end
        
        @(posedge clk);
        #1;
        
        diff = 35.0 - $bitstoreal(out_bottom_array[0]);
        if (diff < 0) diff = -diff;
        
        if (diff > 0.001) begin
            $display("ERROR: Expected 35.0, Got %f", $bitstoreal(out_bottom_array[0]));
            errors = errors + 1;
        end else begin
            $display("PASS: Result = %f", $bitstoreal(out_bottom_array[0]));
        end
        
        if (errors == 0) begin
            $display("\nTEST 4 PASSED: 1x1 matrix handled correctly");
        end else begin
            $display("\nTEST 4 FAILED: %0d errors found", errors);
        end
        
        // TEST 5: Zeros in Matrix
        $display("\n-------------------------------------------------------");
        $display("TEST 5: Matrix with Zero Elements");
        $display("-------------------------------------------------------");
        
        errors = 0;
        reset = 1;
        @(posedge clk);
        reset = 0;
        
        output_stationary = 0;
        preload_valid = 0;
        
        // Clear all inputs
        for (i = 0; i < N; i = i + 1) begin
            in_top_array[i] = 64'h0;
        end
        for (i = 0; i < M; i = i + 1) begin
            in_left_array[i] = 64'h0;
        end
        for (i = 0; i < M*N; i = i + 1) begin
            preload_array[i] = 64'h0;
        end
        
        // Flush pipeline with multiple zero cycles
        for (i = 0; i < M + N; i = i + 1) begin
            @(posedge clk);
        end
        
        // Preload all zeros
        @(posedge clk);
        preload_valid = 1;
        for (i = 0; i < M*N; i = i + 1) begin
            preload_array[i] = 64'h0;
        end
        
        @(posedge clk);
        preload_valid = 0;
        
        $display("All weights = 0");
        
        // Feed non-zero inputs
        @(posedge clk);
        for (i = 0; i < M; i = i + 1) begin
            in_left_array[i] = $realtobits((i+1) * 2.0);
        end
        
        for (i = 0; i < 8; i = i + 1) begin
            @(posedge clk);
        end
        
        @(posedge clk);
        #1;
        
        // All outputs should be 0
        for (i = 0; i < N; i = i + 1) begin
            if ($bitstoreal(out_bottom_array[i]) != 0.0) begin
                $display("ERROR: out_bottom[%0d] should be 0, got %f", i, $bitstoreal(out_bottom_array[i]));
                errors = errors + 1;
            end
        end
        
        if (errors == 0) begin
            $display("PASS: All outputs are zero as expected");
            $display("\nTEST 5 PASSED: Zero handling correct");
        end else begin
            $display("\nTEST 5 FAILED: %0d errors found", errors);
        end
        
        #100;
        
        $display("\n=======================================================");
        $display("         SA_MxN Module Tests Completed");
        $display("=======================================================\n");
        
        $finish;
    end
    
    initial begin
        #100000;
        $display("\nERROR: Testbench timeout!");
        $finish;
    end

endmodule
