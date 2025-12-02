module testbench;

    reg clk;
    reg reset;
    reg start;
    wire done;
    
    reg [7:0] K1, K2, K3;
    reg [15:0] A_base_addr, W_base_addr, C_base_addr;
    reg output_stationary;
    
    parameter M = 3;
    parameter ADDR_WIDTH = 16;
    parameter MEM_SIZE = 65536;
    
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
    real A_matrix [0:24];
    real W_matrix [0:24];
    real C_expected [0:24];
    real C_actual [0:24];
    real diff;
    integer errors;
    
    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end
    
    initial begin
        $dumpfile("systolic_array.vcd");
        $dumpvars(0, testbench);
        
        $display("=======================================================");
        $display("     Systolic Array Testbench with Tiling");
        $display("=======================================================");
        
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
        
        $display("\n-------------------------------------------------------");
        $display("TEST 1: 5x5 Matrix Multiplication (Weight Stationary)");
        $display("-------------------------------------------------------");
        
        for (i = 0; i < 25; i = i + 1) begin
            A_matrix[i] = i + 1;
            C_expected[i] = 0;
        end
        
        W_matrix[0] = 1; W_matrix[1] = 0; W_matrix[2] = 0; W_matrix[3] = 0; W_matrix[4] = 0;
        W_matrix[5] = 0; W_matrix[6] = 1; W_matrix[7] = 0; W_matrix[8] = 0; W_matrix[9] = 0;
        W_matrix[10] = 0; W_matrix[11] = 0; W_matrix[12] = 1; W_matrix[13] = 0; W_matrix[14] = 0;
        W_matrix[15] = 0; W_matrix[16] = 0; W_matrix[17] = 0; W_matrix[18] = 1; W_matrix[19] = 0;
        W_matrix[20] = 0; W_matrix[21] = 0; W_matrix[22] = 0; W_matrix[23] = 0; W_matrix[24] = 1;
        
        for (i = 0; i < 5; i = i + 1) begin
            for (j = 0; j < 5; j = j + 1) begin
                C_expected[i*5 + j] = 0;
                for (k = 0; k < 5; k = k + 1) begin
                    C_expected[i*5 + j] = C_expected[i*5 + j] + A_matrix[i*5 + k] * W_matrix[k*5 + j];
                end
            end
        end
        
        K1 = 5;
        K2 = 5;
        K3 = 5;
        A_base_addr = 0;
        W_base_addr = 512;
        C_base_addr = 1024;
        output_stationary = 0;
        
        for (i = 0; i < 25; i = i + 1) begin
            dut.memory_inst.mem_array[A_base_addr/8 + i] = $realtobits(A_matrix[i]);
        end
        
        for (i = 0; i < 25; i = i + 1) begin
            dut.memory_inst.mem_array[W_base_addr/8 + i] = $realtobits(W_matrix[i]);
        end
        
        $display("Matrix A (5x5):");
        for (i = 0; i < 5; i = i + 1) begin
            $write("  ");
            for (j = 0; j < 5; j = j + 1) begin
                $write("%6.2f ", A_matrix[i*5 + j]);
            end
            $display("");
        end
        
        $display("\nMatrix W (5x5 - Identity):");
        for (i = 0; i < 5; i = i + 1) begin
            $write("  ");
            for (j = 0; j < 5; j = j + 1) begin
                $write("%6.2f ", W_matrix[i*5 + j]);
            end
            $display("");
        end
        
        $display("\nExpected Result C (5x5):");
        for (i = 0; i < 5; i = i + 1) begin
            $write("  ");
            for (j = 0; j < 5; j = j + 1) begin
                $write("%6.2f ", C_expected[i*5 + j]);
            end
            $display("");
        end
        
        #20;
        start = 1;
        #10;
        start = 0;
        
        $display("\nStarting TEST 1 computation...");
        
        wait(done == 1 || $time > 5000);
        $display("\nTEST 1 computation complete at time %0t", $time);
        #50;
        
        $display("\nDebug: Checking tiles after computation:");
        $display("A_tile (first 9):");
        for (i = 0; i < 9; i = i + 1) begin
            $display("  A_tile[%0d] = %f", i, $bitstoreal(dut.A_tile[i]));
        end
        $display("W_tile (first 9):");
        for (i = 0; i < 9; i = i + 1) begin
            $display("  W_tile[%0d] = %f", i, $bitstoreal(dut.W_tile[i]));
        end
        $display("C_tile (first 9):");
        for (i = 0; i < 9; i = i + 1) begin
            $display("  C_tile[%0d] = %f", i, $bitstoreal(dut.C_tile[i]));
        end
        
        for (i = 0; i < 25; i = i + 1) begin
            C_actual[i] = $bitstoreal(dut.memory_inst.mem_array[128 + i]);
        end
        
        $display("\nActual Result C (5x5):");
        for (i = 0; i < 5; i = i + 1) begin
            $write("  ");
            for (j = 0; j < 5; j = j + 1) begin
                $write("%6.2f ", C_actual[i*5 + j]);
            end
            $display("");
        end
        
        errors = 0;
        for (i = 0; i < 25; i = i + 1) begin
            diff = C_expected[i] - C_actual[i];
            if (diff < 0) diff = -diff;
            if (diff > 0.01) begin
                $display("ERROR at position %0d: Expected %f, Got %f", i, C_expected[i], C_actual[i]);
                errors = errors + 1;
            end
        end
        
        if (errors == 0) begin
            $display("\nTEST 1 PASSED: Weight Stationary Mode");
        end else begin
            $display("\nTEST 1 FAILED: %0d errors found", errors);
        end
        
        #100;
        
        $display("\n-------------------------------------------------------");
        $display("TEST 2: 5x5 Matrix Multiplication (Output Stationary)");
        $display("-------------------------------------------------------");
        
        reset = 1;
        #20;
        reset = 0;
        #20;
        
        for (i = 0; i < 25; i = i + 1) begin
            dut.memory_inst.mem_array[A_base_addr/8 + i] = $realtobits(A_matrix[i]);
        end
        
        for (i = 0; i < 25; i = i + 1) begin
            dut.memory_inst.mem_array[W_base_addr/8 + i] = $realtobits(W_matrix[i]);
        end
        
        for (i = 0; i < 25; i = i + 1) begin
            dut.memory_inst.mem_array[C_base_addr/8 + i] = 64'h0;
        end
        
        K1 = 5;
        K2 = 5;
        K3 = 5;
        A_base_addr = 0;
        W_base_addr = 512;
        C_base_addr = 1024;
        output_stationary = 1;
        
        #20;
        start = 1;
        #10;
        start = 0;
        
        wait(done == 1);
        #50;
        
        for (i = 0; i < 25; i = i + 1) begin
            C_actual[i] = $bitstoreal(dut.memory_inst.mem_array[C_base_addr/8 + i]);
        end
        
        $display("\nActual Result C (5x5):");
        for (i = 0; i < 5; i = i + 1) begin
            $write("  ");
            for (j = 0; j < 5; j = j + 1) begin
                $write("%6.2f ", C_actual[i*5 + j]);
            end
            $display("");
        end
        
        errors = 0;
        for (i = 0; i < 25; i = i + 1) begin
            diff = C_expected[i] - C_actual[i];
            if (diff < 0) diff = -diff;
            if (diff > 0.01) begin
                $display("ERROR at position %0d: Expected %f, Got %f", i, C_expected[i], C_actual[i]);
                errors = errors + 1;
            end
        end
        
        if (errors == 0) begin
            $display("\nTEST 2 PASSED: Output Stationary Mode");
        end else begin
            $display("\nTEST 2 FAILED: %0d errors found", errors);
        end
        
        #100;
        
        $display("\n=======================================================");
        $display("              All Tests Completed");
        $display("=======================================================\n");
        
        $finish;
    end
    
    initial begin
        #500000;
        $display("\nERROR: Testbench timeout!");
        $finish;
    end

endmodule
