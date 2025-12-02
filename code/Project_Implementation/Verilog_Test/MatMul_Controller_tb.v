module MatMul_Controller_tb;

    parameter M = 3;
    parameter N = 3;
    
    reg clk;
    reg reset;
    reg start;
    wire done;
    reg output_stationary;
    reg [64*M*M-1:0] A_tile_flat;
    reg [64*M*M-1:0] W_tile_flat;
    wire [64*M*M-1:0] C_tile_flat;
    reg [7:0] k1, k2, k3;
    
    MatMul_Controller #(.M(M), .N(N)) dut (
        .clk(clk),
        .reset(reset),
        .start(start),
        .done(done),
        .output_stationary(output_stationary),
        .A_tile_flat(A_tile_flat),
        .W_tile_flat(W_tile_flat),
        .C_tile_flat(C_tile_flat),
        .k1(k1),
        .k2(k2),
        .k3(k3)
    );
    
    reg [63:0] A_tile [0:M*M-1];
    reg [63:0] W_tile [0:M*M-1];
    wire [63:0] C_tile [0:M*M-1];
    
    real A_matrix [0:M*M-1];
    real W_matrix [0:M*M-1];
    real C_expected [0:M*M-1];
    real C_actual [0:M*M-1];
    
    integer i, j, k_idx;
    integer errors;
    real diff;
    
    genvar gi;
    generate
        for (gi = 0; gi < M*M; gi = gi + 1) begin : tile_packing
            assign A_tile_flat[64*(gi+1)-1:64*gi] = A_tile[gi];
            assign W_tile_flat[64*(gi+1)-1:64*gi] = W_tile[gi];
            assign C_tile[gi] = C_tile_flat[64*(gi+1)-1:64*gi];
        end
    endgenerate
    
    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end
    
    initial begin
        $dumpfile("matmul_controller_test.vcd");
        $dumpvars(0, MatMul_Controller_tb);
        
        $display("=======================================================");
        $display("       MatMul_Controller Module Testbench");
        $display("=======================================================");
        
        errors = 0;
        reset = 1;
        start = 0;
        output_stationary = 0;
        k1 = 0;
        k2 = 0;
        k3 = 0;
        
        for (i = 0; i < M*M; i = i + 1) begin
            A_tile[i] = 64'h0;
            W_tile[i] = 64'h0;
        end
        
        #20;
        reset = 0;
        #20;
        
        // TEST 1: Weight Stationary - Identity Matrix (Optimal 3x3)
        $display("\n-------------------------------------------------------");
        $display("TEST 1: Weight Stationary - Identity Matrix 3x3");
        $display("-------------------------------------------------------");
        
        output_stationary = 0;
        k1 = 3;
        k2 = 3;
        k3 = 3;
        
        // A = [1 2 3; 4 5 6; 7 8 9]
        A_matrix[0] = 1.0; A_matrix[1] = 2.0; A_matrix[2] = 3.0;
        A_matrix[3] = 4.0; A_matrix[4] = 5.0; A_matrix[5] = 6.0;
        A_matrix[6] = 7.0; A_matrix[7] = 8.0; A_matrix[8] = 9.0;
        
        // W = Identity
        W_matrix[0] = 1.0; W_matrix[1] = 0.0; W_matrix[2] = 0.0;
        W_matrix[3] = 0.0; W_matrix[4] = 1.0; W_matrix[5] = 0.0;
        W_matrix[6] = 0.0; W_matrix[7] = 0.0; W_matrix[8] = 1.0;
        
        // Expected: C = A (since W is identity)
        for (i = 0; i < 9; i = i + 1) begin
            C_expected[i] = A_matrix[i];
        end
        
        for (i = 0; i < M*M; i = i + 1) begin
            A_tile[i] = $realtobits(A_matrix[i]);
            W_tile[i] = $realtobits(W_matrix[i]);
        end
        
        $display("Matrix A:");
        for (i = 0; i < 3; i = i + 1) begin
            $write("  ");
            for (j = 0; j < 3; j = j + 1) begin
                $write("%6.2f ", A_matrix[i*3 + j]);
            end
            $display("");
        end
        
        $display("Matrix W (Identity):");
        for (i = 0; i < 3; i = i + 1) begin
            $write("  ");
            for (j = 0; j < 3; j = j + 1) begin
                $write("%6.2f ", W_matrix[i*3 + j]);
            end
            $display("");
        end
        
        #20;
        start = 1;
        #10;
        start = 0;
        
        wait(done == 1);
        #20;
        
        for (i = 0; i < M*M; i = i + 1) begin
            C_actual[i] = $bitstoreal(C_tile[i]);
        end
        
        $display("\nResult C:");
        for (i = 0; i < 3; i = i + 1) begin
            $write("  ");
            for (j = 0; j < 3; j = j + 1) begin
                $write("%6.2f ", C_actual[i*3 + j]);
            end
            $display("");
        end
        
        errors = 0;
        for (i = 0; i < 9; i = i + 1) begin
            diff = C_expected[i] - C_actual[i];
            if (diff < 0) diff = -diff;
            if (diff > 0.01) begin
                $display("ERROR at [%0d]: Expected %f, Got %f", i, C_expected[i], C_actual[i]);
                errors = errors + 1;
            end
        end
        
        if (errors == 0) begin
            $display("\nTEST 1 PASSED: Identity matrix multiplication correct");
        end else begin
            $display("\nTEST 1 FAILED: %0d errors found", errors);
        end
        
        // TEST 2: Weight Stationary - Undersized 2x2
        $display("\n-------------------------------------------------------");
        $display("TEST 2: Weight Stationary - Undersized 2x2");
        $display("-------------------------------------------------------");
        
        reset = 1;
        #10;
        reset = 0;
        #10;
        
        output_stationary = 0;
        k1 = 2;
        k2 = 2;
        k3 = 2;
        
        // A = [1 2; 3 4] (padded with zeros)
        A_matrix[0] = 1.0; A_matrix[1] = 2.0; A_matrix[2] = 0.0;
        A_matrix[3] = 3.0; A_matrix[4] = 4.0; A_matrix[5] = 0.0;
        A_matrix[6] = 0.0; A_matrix[7] = 0.0; A_matrix[8] = 0.0;
        
        // W = [2 3; 4 5] (padded with zeros)
        W_matrix[0] = 2.0; W_matrix[1] = 3.0; W_matrix[2] = 0.0;
        W_matrix[3] = 4.0; W_matrix[4] = 5.0; W_matrix[5] = 0.0;
        W_matrix[6] = 0.0; W_matrix[7] = 0.0; W_matrix[8] = 0.0;
        
        // Expected: C = [[10, 13], [22, 29]]
        C_expected[0] = 10.0; C_expected[1] = 13.0; C_expected[2] = 0.0;
        C_expected[3] = 22.0; C_expected[4] = 29.0; C_expected[5] = 0.0;
        C_expected[6] = 0.0;  C_expected[7] = 0.0;  C_expected[8] = 0.0;
        
        for (i = 0; i < M*M; i = i + 1) begin
            A_tile[i] = $realtobits(A_matrix[i]);
            W_tile[i] = $realtobits(W_matrix[i]);
        end
        
        $display("Matrix A (2x2 in 3x3 tile):");
        $display("  1.00  2.00  0.00");
        $display("  3.00  4.00  0.00");
        $display("  0.00  0.00  0.00");
        
        $display("Matrix W (2x2 in 3x3 tile):");
        $display("  2.00  3.00  0.00");
        $display("  4.00  5.00  0.00");
        $display("  0.00  0.00  0.00");
        
        $display("Expected C:");
        $display(" 10.00 13.00  0.00");
        $display(" 22.00 29.00  0.00");
        $display("  0.00  0.00  0.00");
        
        #20;
        start = 1;
        #10;
        start = 0;
        
        wait(done == 1);
        #20;
        
        for (i = 0; i < M*M; i = i + 1) begin
            C_actual[i] = $bitstoreal(C_tile[i]);
        end
        
        $display("\nActual C:");
        for (i = 0; i < 3; i = i + 1) begin
            $write("  ");
            for (j = 0; j < 3; j = j + 1) begin
                $write("%6.2f ", C_actual[i*3 + j]);
            end
            $display("");
        end
        
        errors = 0;
        for (i = 0; i < 9; i = i + 1) begin
            diff = C_expected[i] - C_actual[i];
            if (diff < 0) diff = -diff;
            if (diff > 0.01) begin
                $display("ERROR at [%0d]: Expected %f, Got %f", i, C_expected[i], C_actual[i]);
                errors = errors + 1;
            end
        end
        
        if (errors == 0) begin
            $display("\nTEST 2 PASSED: Undersized 2x2 matrix correct");
        end else begin
            $display("\nTEST 2 FAILED: %0d errors found", errors);
        end
        
        // TEST 3: Output Stationary - 3x3 Matrix
        $display("\n-------------------------------------------------------");
        $display("TEST 3: Output Stationary - 3x3 Matrix");
        $display("-------------------------------------------------------");
        
        reset = 1;
        #10;
        reset = 0;
        #10;
        
        output_stationary = 1;
        k1 = 3;
        k2 = 3;
        k3 = 3;
        
        // A = [1 2 3; 4 5 6; 7 8 9]
        A_matrix[0] = 1.0; A_matrix[1] = 2.0; A_matrix[2] = 3.0;
        A_matrix[3] = 4.0; A_matrix[4] = 5.0; A_matrix[5] = 6.0;
        A_matrix[6] = 7.0; A_matrix[7] = 8.0; A_matrix[8] = 9.0;
        
        // W = Identity
        W_matrix[0] = 1.0; W_matrix[1] = 0.0; W_matrix[2] = 0.0;
        W_matrix[3] = 0.0; W_matrix[4] = 1.0; W_matrix[5] = 0.0;
        W_matrix[6] = 0.0; W_matrix[7] = 0.0; W_matrix[8] = 1.0;
        
        for (i = 0; i < 9; i = i + 1) begin
            C_expected[i] = A_matrix[i];
        end
        
        for (i = 0; i < M*M; i = i + 1) begin
            A_tile[i] = $realtobits(A_matrix[i]);
            W_tile[i] = $realtobits(W_matrix[i]);
        end
        
        $display("Testing Output Stationary with Identity matrix");
        
        #20;
        start = 1;
        #10;
        start = 0;
        
        wait(done == 1);
        #20;
        
        for (i = 0; i < M*M; i = i + 1) begin
            C_actual[i] = $bitstoreal(C_tile[i]);
        end
        
        $display("\nResult C:");
        for (i = 0; i < 3; i = i + 1) begin
            $write("  ");
            for (j = 0; j < 3; j = j + 1) begin
                $write("%6.2f ", C_actual[i*3 + j]);
            end
            $display("");
        end
        
        errors = 0;
        for (i = 0; i < 9; i = i + 1) begin
            diff = C_expected[i] - C_actual[i];
            if (diff < 0) diff = -diff;
            if (diff > 0.01) begin
                $display("ERROR at [%0d]: Expected %f, Got %f", i, C_expected[i], C_actual[i]);
                errors = errors + 1;
            end
        end
        
        if (errors == 0) begin
            $display("\nTEST 3 PASSED: Output Stationary mode correct");
        end else begin
            $display("\nTEST 3 FAILED: %0d errors found", errors);
        end
        
        // TEST 4: Undersized 1x1 Matrix
        $display("\n-------------------------------------------------------");
        $display("TEST 4: Undersized 1x1 Matrix");
        $display("-------------------------------------------------------");
        
        reset = 1;
        #10;
        reset = 0;
        #10;
        
        output_stationary = 0;
        k1 = 1;
        k2 = 1;
        k3 = 1;
        
        // A = [5]
        A_matrix[0] = 5.0;
        for (i = 1; i < 9; i = i + 1) begin
            A_matrix[i] = 0.0;
        end
        
        // W = [7]
        W_matrix[0] = 7.0;
        for (i = 1; i < 9; i = i + 1) begin
            W_matrix[i] = 0.0;
        end
        
        // Expected: C = [35]
        C_expected[0] = 35.0;
        for (i = 1; i < 9; i = i + 1) begin
            C_expected[i] = 0.0;
        end
        
        for (i = 0; i < M*M; i = i + 1) begin
            A_tile[i] = $realtobits(A_matrix[i]);
            W_tile[i] = $realtobits(W_matrix[i]);
        end
        
        $display("A = [5], W = [7], Expected C = [35]");
        
        #20;
        start = 1;
        #10;
        start = 0;
        
        wait(done == 1);
        #20;
        
        for (i = 0; i < M*M; i = i + 1) begin
            C_actual[i] = $bitstoreal(C_tile[i]);
        end
        
        $display("Result C[0] = %f (expected 35.0)", C_actual[0]);
        
        errors = 0;
        diff = C_expected[0] - C_actual[0];
        if (diff < 0) diff = -diff;
        if (diff > 0.01) begin
            $display("ERROR: Expected 35.0, Got %f", C_actual[0]);
            errors = errors + 1;
        end
        
        if (errors == 0) begin
            $display("\nTEST 4 PASSED: 1x1 matrix handled correctly");
        end else begin
            $display("\nTEST 4 FAILED: %0d errors found", errors);
        end
        
        // TEST 5: Non-square Matrix (2x3 * 3x2)
        $display("\n-------------------------------------------------------");
        $display("TEST 5: Non-square Matrix Multiplication");
        $display("-------------------------------------------------------");
        
        reset = 1;
        #10;
        reset = 0;
        #10;
        
        output_stationary = 0;
        k1 = 2;  // A is 2x3
        k2 = 3;  // shared dimension
        k3 = 2;  // W is 3x2, C is 2x2
        
        // A = [1 2 3; 4 5 6] (2x3)
        A_matrix[0] = 1.0; A_matrix[1] = 2.0; A_matrix[2] = 3.0;
        A_matrix[3] = 4.0; A_matrix[4] = 5.0; A_matrix[5] = 6.0;
        A_matrix[6] = 0.0; A_matrix[7] = 0.0; A_matrix[8] = 0.0;
        
        // W = [1 2; 3 4; 5 6] (3x2)
        W_matrix[0] = 1.0; W_matrix[1] = 2.0; W_matrix[2] = 0.0;
        W_matrix[3] = 3.0; W_matrix[4] = 4.0; W_matrix[5] = 0.0;
        W_matrix[6] = 5.0; W_matrix[7] = 6.0; W_matrix[8] = 0.0;
        
        // C = A * W
        // C[0][0] = 1*1 + 2*3 + 3*5 = 1 + 6 + 15 = 22
        // C[0][1] = 1*2 + 2*4 + 3*6 = 2 + 8 + 18 = 28
        // C[1][0] = 4*1 + 5*3 + 6*5 = 4 + 15 + 30 = 49
        // C[1][1] = 4*2 + 5*4 + 6*6 = 8 + 20 + 36 = 64
        C_expected[0] = 22.0; C_expected[1] = 28.0; C_expected[2] = 0.0;
        C_expected[3] = 49.0; C_expected[4] = 64.0; C_expected[5] = 0.0;
        C_expected[6] = 0.0;  C_expected[7] = 0.0;  C_expected[8] = 0.0;
        
        for (i = 0; i < M*M; i = i + 1) begin
            A_tile[i] = $realtobits(A_matrix[i]);
            W_tile[i] = $realtobits(W_matrix[i]);
        end
        
        $display("A (2x3): [[1, 2, 3], [4, 5, 6]]");
        $display("W (3x2): [[1, 2], [3, 4], [5, 6]]");
        $display("Expected C (2x2): [[22, 28], [49, 64]]");
        
        #20;
        start = 1;
        #10;
        start = 0;
        
        wait(done == 1);
        #20;
        
        for (i = 0; i < M*M; i = i + 1) begin
            C_actual[i] = $bitstoreal(C_tile[i]);
        end
        
        $display("\nActual C:");
        for (i = 0; i < 3; i = i + 1) begin
            $write("  ");
            for (j = 0; j < 3; j = j + 1) begin
                $write("%6.2f ", C_actual[i*3 + j]);
            end
            $display("");
        end
        
        errors = 0;
        for (i = 0; i < 9; i = i + 1) begin
            diff = C_expected[i] - C_actual[i];
            if (diff < 0) diff = -diff;
            if (diff > 0.01) begin
                $display("ERROR at [%0d]: Expected %f, Got %f", i, C_expected[i], C_actual[i]);
                errors = errors + 1;
            end
        end
        
        if (errors == 0) begin
            $display("\nTEST 5 PASSED: Non-square matrix multiplication correct");
        end else begin
            $display("\nTEST 5 FAILED: %0d errors found", errors);
        end
        
        #100;
        
        $display("\n=======================================================");
        $display("    MatMul_Controller Module Tests Completed");
        $display("=======================================================\n");
        
        $finish;
    end
    
    initial begin
        #500000;
        $display("\nERROR: Testbench timeout!");
        $finish;
    end

endmodule
