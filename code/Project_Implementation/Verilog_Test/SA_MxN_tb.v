module SA_MxN_tb;

    parameter M = 3;
    parameter N = 3;
    
    reg clk;
    reg reset;
    reg output_stationary;
    reg [64*N-1:0] in_top;
    reg [64*M-1:0] in_left;
    wire [64*M-1:0] out_right;
    wire [64*N-1:0] out_bottom;
    reg preload_valid;
    reg [64*M*N-1:0] preload_data;
    
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
        
        // Feed inputs row by row
        for (i = 0; i < M + N + M - 1; i = i + 1) begin
            @(posedge clk);
            
            // Input left (rows of A)
            if (i < M) begin
                in_left_array[0] = $realtobits((i*3 + 1) * 1.0);
                in_left_array[1] = (i >= 1) ? $realtobits(((i-1)*3 + 2) * 1.0) : 64'h0;
                in_left_array[2] = (i >= 2) ? $realtobits(((i-2)*3 + 3) * 1.0) : 64'h0;
            end else begin
                in_left_array[0] = 64'h0;
                in_left_array[1] = (i - M < M) ? in_left_array[0] : 64'h0;
                in_left_array[2] = (i - M < M - 1) ? in_left_array[1] : 64'h0;
            end
        end
        
        @(posedge clk);
        #20;
        
        $display("TEST 1 complete - Results should match input (identity)");
        
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
        
        // Preload weights W = [[2, 3], [4, 5], [0, 0]]
        @(posedge clk);
        preload_valid = 1;
        preload_array[0] = $realtobits(2.0);
        preload_array[1] = $realtobits(3.0);
        preload_array[2] = $realtobits(0.0);
        preload_array[3] = $realtobits(4.0);
        preload_array[4] = $realtobits(5.0);
        preload_array[5] = $realtobits(0.0);
        preload_array[6] = $realtobits(0.0);
        preload_array[7] = $realtobits(0.0);
        preload_array[8] = $realtobits(0.0);
        
        @(posedge clk);
        preload_valid = 0;
        
        $display("Weights loaded: [[2, 3, 0], [4, 5, 0], [0, 0, 0]]");
        $display("Input A: [[1, 2], [3, 4]]");
        $display("Expected C: [[1*2+2*4, 1*3+2*5], [3*2+4*4, 3*3+4*5]]");
        $display("           = [[10, 13], [22, 29]]");
        
        // Clear inputs
        for (i = 0; i < N; i = i + 1) begin
            in_top_array[i] = 64'h0;
        end
        for (i = 0; i < M; i = i + 1) begin
            in_left_array[i] = 64'h0;
        end
        
        // Cycle 0: Feed first row [1, 2]
        @(posedge clk);
        in_left_array[0] = $realtobits(1.0);
        in_left_array[1] = 64'h0;
        in_left_array[2] = 64'h0;
        
        // Cycle 1: Feed second row [3, 4]
        @(posedge clk);
        in_left_array[0] = $realtobits(2.0);
        in_left_array[1] = $realtobits(3.0);
        in_left_array[2] = 64'h0;
        
        // Cycle 2: Continue feeding
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
        
        $display("\nOutput bottom values:");
        $display("  out_bottom[0] = %f", $bitstoreal(out_bottom_array[0]));
        $display("  out_bottom[1] = %f", $bitstoreal(out_bottom_array[1]));
        $display("  out_bottom[2] = %f", $bitstoreal(out_bottom_array[2]));
        
        $display("\nTEST 2 complete");
        
        // TEST 3: Output Stationary - 2x2 Matrix Multiplication
        $display("\n-------------------------------------------------------");
        $display("TEST 3: Output Stationary - 2x2 Matrix Multiplication");
        $display("-------------------------------------------------------");
        
        errors = 0;
        reset = 1;
        #10;
        reset = 0;
        #10;
        
        output_stationary = 1;
        preload_valid = 0;
        
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
        
        $display("\nDraining results:");
        $display("  out_bottom[0] = %f (expected ~10)", $bitstoreal(out_bottom_array[0]));
        $display("  out_bottom[1] = %f (expected ~13)", $bitstoreal(out_bottom_array[1]));
        
        @(posedge clk);
        #1;
        
        $display("  out_bottom[0] = %f (expected ~22)", $bitstoreal(out_bottom_array[0]));
        $display("  out_bottom[1] = %f (expected ~29)", $bitstoreal(out_bottom_array[1]));
        
        $display("\nTEST 3 complete");
        
        // TEST 4: Weight Stationary - Undersized Matrix (1x1)
        $display("\n-------------------------------------------------------");
        $display("TEST 4: Weight Stationary - Undersized 1x1 Matrix");
        $display("-------------------------------------------------------");
        
        errors = 0;
        reset = 1;
        #10;
        reset = 0;
        #10;
        
        output_stationary = 0;
        
        // Preload single weight
        @(posedge clk);
        preload_valid = 1;
        preload_array[0] = $realtobits(7.0);
        for (i = 1; i < M*N; i = i + 1) begin
            preload_array[i] = 64'h0;
        end
        
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
        #10;
        reset = 0;
        #10;
        
        output_stationary = 0;
        
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
