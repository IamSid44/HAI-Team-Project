module testbench_simple;

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
    real A_matrix [0:8];
    real W_matrix [0:8];
    real C_expected [0:8];
    real C_actual [0:8];
    real diff;
    integer errors;
    
    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end
    
    initial begin
        $dumpfile("systolic_array_simple.vcd");
        $dumpvars(0, testbench_simple);
        
        $display("=======================================================");
        $display("     Simple 3x3 Matrix Test");
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
        
        // Setup 3x3 identity test
        for (i = 0; i < 9; i = i + 1) begin
            A_matrix[i] = i + 1;
        end
        
        W_matrix[0] = 1; W_matrix[1] = 0; W_matrix[2] = 0;
        W_matrix[3] = 0; W_matrix[4] = 1; W_matrix[5] = 0;
        W_matrix[6] = 0; W_matrix[7] = 0; W_matrix[8] = 1;
        
        // Compute expected result
        for (i = 0; i < 3; i = i + 1) begin
            for (j = 0; j < 3; j = j + 1) begin
                C_expected[i*3 + j] = 0;
                for (k = 0; k < 3; k = k + 1) begin
                    C_expected[i*3 + j] = C_expected[i*3 + j] + A_matrix[i*3 + k] * W_matrix[k*3 + j];
                end
            end
        end
        
        // Write to memory
        A_base_addr = 0;
        W_base_addr = 512;
        C_base_addr = 1024;
        
        for (i = 0; i < 9; i = i + 1) begin
            dut.memory_inst.mem_array[A_base_addr/8 + i] = $realtobits(A_matrix[i]);
        end
        
        for (i = 0; i < 9; i = i + 1) begin
            dut.memory_inst.mem_array[W_base_addr/8 + i] = $realtobits(W_matrix[i]);
        end
        
        // Print matrices
        $display("\nMatrix A (3x3):");
        for (i = 0; i < 3; i = i + 1) begin
            $write("  ");
            for (j = 0; j < 3; j = j + 1) begin
                $write("%6.2f ", A_matrix[i*3 + j]);
            end
            $display("");
        end
        
        $display("\nMatrix W (3x3 - Identity):");
        for (i = 0; i < 3; i = i + 1) begin
            $write("  ");
            for (j = 0; j < 3; j = j + 1) begin
                $write("%6.2f ", W_matrix[i*3 + j]);
            end
            $display("");
        end
        
        $display("\nExpected Result C (3x3):");
        for (i = 0; i < 3; i = i + 1) begin
            $write("  ");
            for (j = 0; j < 3; j = j + 1) begin
                $write("%6.2f ", C_expected[i*3 + j]);
            end
            $display("");
        end
        
        // Start computation
        K1 = 3;
        K2 = 3;
        K3 = 3;
        output_stationary = 0;
        
        #20;
        start = 1;
        #10;
        start = 0;
        
        $display("\nStarting computation...");
        $display("State transitions:");
        $monitor("Time=%0t State=%0d done=%b matmul_start=%b matmul_done=%b", $time, dut.state, done, dut.matmul_start, dut.matmul_done);
        
        wait(done == 1);
        $display("Computation complete at time %0t", $time);
        #50;
        
        // Check result
        for (i = 0; i < 9; i = i + 1) begin
            C_actual[i] = $bitstoreal(dut.memory_inst.mem_array[C_base_addr/8 + i]);
        end
        
        $display("\nActual Result C (3x3):");
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
                $display("ERROR at position %0d: Expected %f, Got %f", i, C_expected[i], C_actual[i]);
                errors = errors + 1;
            end
        end
        
        if (errors == 0) begin
            $display("\nTEST PASSED!");
        end else begin
            $display("\nTEST FAILED: %0d errors found", errors);
        end
        
        $display("\n=======================================================");
        $display("              Test Completed");
        $display("=======================================================\n");
        
        $finish;
    end
    
    initial begin
        #10000;
        $display("\nERROR: Testbench timeout!");
        $finish;
    end

endmodule
