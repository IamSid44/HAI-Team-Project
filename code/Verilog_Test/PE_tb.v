`include "PE.v"

module PE_tb;

    reg clk;
    reg reset;
    reg output_stationary;
    reg [63:0] in_top;
    reg [63:0] in_left;
    wire [63:0] out_right;
    wire [63:0] out_bottom;
    reg preload_valid;
    reg [63:0] preload_data;
    
    PE dut (
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
    
    integer i;
    integer errors;
    real expected_output;
    real actual_output;
    real diff;
    
    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end
    
    initial begin
        $dumpfile("pe_test.vcd");
        $dumpvars(0, PE_tb);
        
        // Dump individual real values for better waveform viewing
        $dumpvars(1, dut.in_top_real);
        $dumpvars(1, dut.in_left_real);
        $dumpvars(1, dut.preload_real);
        $dumpvars(1, dut.accumulator_real);
        $dumpvars(1, dut.result_real);
        
        $display("=======================================================");
        $display("              PE Module Testbench");
        $display("=======================================================");
        
        errors = 0;
        reset = 1;
        output_stationary = 0;
        in_top = 0;
        in_left = 0;
        preload_valid = 0;
        preload_data = 0;
        
        #20;
        reset = 0;
        #20;
        
        // TEST 1: Weight Stationary Mode - Basic MAC Operation
        $display("\n-------------------------------------------------------");
        $display("TEST 1: Weight Stationary Mode - Basic MAC");
        $display("-------------------------------------------------------");
        
        output_stationary = 0;
        
        // Preload weight
        @(posedge clk);
        preload_valid = 1;
        preload_data = $realtobits(2.0);
        
        @(posedge clk);
        preload_valid = 0;
        
        $display("Preloaded weight = 2.0");
        
        // Perform MAC operations
        for (i = 1; i <= 5; i = i + 1) begin
            @(posedge clk);
            in_left = $realtobits(i * 1.0);
            in_top = $realtobits(i * 0.5);
            
            @(posedge clk);
            #1;
            
            expected_output = (i * 1.0) * 2.0 + (i * 0.5);
            actual_output = $bitstoreal(out_bottom);
            diff = expected_output - actual_output;
            if (diff < 0) diff = -diff;
            
            if (diff > 0.0001) begin
                $display("ERROR cycle %0d: Expected %f, Got %f", i, expected_output, actual_output);
                errors = errors + 1;
            end else begin
                $display("PASS cycle %0d: in_left=%f * weight=2.0 + in_top=%f = %f", 
                         i, i*1.0, i*0.5, actual_output);
            end
            
            // Check that left value propagates to right
            if (out_right !== in_left) begin
                $display("ERROR: out_right should equal in_left");
                errors = errors + 1;
            end
        end
        
        if (errors == 0) begin
            $display("\nTEST 1 PASSED: Weight Stationary MAC operations correct");
        end else begin
            $display("\nTEST 1 FAILED: %0d errors found", errors);
        end
        
        // TEST 2: Weight Stationary - Weight Update
        $display("\n-------------------------------------------------------");
        $display("TEST 2: Weight Stationary - Weight Update");
        $display("-------------------------------------------------------");
        
        errors = 0;
        reset = 1;
        #10;
        reset = 0;
        #10;
        
        output_stationary = 0;
        
        // Preload first weight
        @(posedge clk);
        preload_valid = 1;
        preload_data = $realtobits(3.0);
        
        @(posedge clk);
        preload_valid = 0;
        in_left = $realtobits(2.0);
        in_top = $realtobits(1.0);
        
        @(posedge clk);
        #1;
        
        expected_output = 2.0 * 3.0 + 1.0;
        actual_output = $bitstoreal(out_bottom);
        diff = expected_output - actual_output;
        if (diff < 0) diff = -diff;
        
        if (diff > 0.0001) begin
            $display("ERROR with weight 3.0: Expected %f, Got %f", expected_output, actual_output);
            errors = errors + 1;
        end else begin
            $display("PASS with weight 3.0: Result = %f", actual_output);
        end
        
        // Update weight
        @(posedge clk);
        preload_valid = 1;
        preload_data = $realtobits(5.0);
        
        @(posedge clk);
        preload_valid = 0;
        in_left = $realtobits(2.0);
        in_top = $realtobits(1.0);
        
        @(posedge clk);
        #1;
        
        expected_output = 2.0 * 5.0 + 1.0;
        actual_output = $bitstoreal(out_bottom);
        diff = expected_output - actual_output;
        if (diff < 0) diff = -diff;
        
        if (diff > 0.0001) begin
            $display("ERROR with weight 5.0: Expected %f, Got %f", expected_output, actual_output);
            errors = errors + 1;
        end else begin
            $display("PASS with weight 5.0: Result = %f", actual_output);
        end
        
        if (errors == 0) begin
            $display("\nTEST 2 PASSED: Weight update works correctly");
        end else begin
            $display("\nTEST 2 FAILED: %0d errors found", errors);
        end
        
        // TEST 3: Output Stationary Mode - Accumulation
        $display("\n-------------------------------------------------------");
        $display("TEST 3: Output Stationary Mode - Accumulation");
        $display("-------------------------------------------------------");
        
        errors = 0;
        reset = 1;
        in_left = 64'h0;
        in_top = 64'h0;
        #10;
        reset = 0;
        
        @(posedge clk);
        
        output_stationary = 1;
        preload_valid = 0;
        
        $display("Accumulating products...");
        
        // Accumulate: 1*2 + 2*3 + 3*4 = 2 + 6 + 12 = 20
        @(posedge clk);
        in_left = $realtobits(1.0);
        in_top = $realtobits(2.0);
        
        @(posedge clk);
        in_left = $realtobits(2.0);
        in_top = $realtobits(3.0);
        
        @(posedge clk);
        in_left = $realtobits(3.0);
        in_top = $realtobits(4.0);

        @(posedge clk);
        in_left = 64'h0;
        in_top = 64'h0;
        
        @(posedge clk);
        preload_valid = 1;
        in_left = 64'h0;
        in_top = 64'h0;
        
        #1;
        
        expected_output = 20.0;
        actual_output = $bitstoreal(out_bottom);
        diff = expected_output - actual_output;
        if (diff < 0) diff = -diff;
        
        if (diff > 0.0001) begin
            $display("ERROR: Expected accumulated value %f, Got %f", expected_output, actual_output);
            errors = errors + 1;
        end else begin
            $display("PASS: Accumulated value = %f (expected %f)", actual_output, expected_output);
        end
        
        if (errors == 0) begin
            $display("\nTEST 3 PASSED: Output Stationary accumulation correct");
        end else begin
            $display("\nTEST 3 FAILED: %0d errors found", errors);
        end
        
        // TEST 4: Output Stationary - Multiple Accumulation Cycles
        $display("\n-------------------------------------------------------");
        $display("TEST 4: Output Stationary - Multiple Accumulations");
        $display("-------------------------------------------------------");
        
        errors = 0;
        reset = 1;
        #10;
        reset = 0;
        #10;
        
        output_stationary = 1;
        preload_valid = 0;
        
        // First accumulation: 5*2 = 10
        @(posedge clk);
        in_left = $realtobits(5.0);
        in_top = $realtobits(2.0);
        
        @(posedge clk);
        in_left = 64'h0;
        in_top = 64'h0;
        
        @(posedge clk);
        preload_valid = 1;
        in_left = 64'h0;
        in_top = 64'h0;
        
        #1;
        
        actual_output = $bitstoreal(out_bottom);
        if ((actual_output - 10.0) > 0.0001 || (actual_output - 10.0) < -0.0001) begin
            $display("ERROR: First accumulation - Expected 10.0, Got %f", actual_output);
            errors = errors + 1;
        end else begin
            $display("PASS: First accumulation = %f", actual_output);
        end
        
        // Reset for second accumulation
        reset = 1;
        #10;
        reset = 0;
        #10;
        
        preload_valid = 0;
        
        // Second accumulation: 3*4 + 2*1 = 12 + 2 = 14
        @(posedge clk);
        in_left = $realtobits(3.0);
        in_top = $realtobits(4.0);
        
        @(posedge clk);
        in_left = $realtobits(2.0);
        in_top = $realtobits(1.0);
        
        @(posedge clk);
        in_left = 64'h0;
        in_top = 64'h0;
        
        @(posedge clk);
        preload_valid = 1;
        in_left = 64'h0;
        in_top = 64'h0;
        
        #1;
        
        actual_output = $bitstoreal(out_bottom);
        if ((actual_output - 14.0) > 0.0001 || (actual_output - 14.0) < -0.0001) begin
            $display("ERROR: Second accumulation - Expected 14.0, Got %f", actual_output);
            errors = errors + 1;
        end else begin
            $display("PASS: Second accumulation = %f", actual_output);
        end
        
        if (errors == 0) begin
            $display("\nTEST 4 PASSED: Multiple accumulation cycles work correctly");
        end else begin
            $display("\nTEST 4 FAILED: %0d errors found", errors);
        end
        
        // TEST 5: Zero Input Handling
        $display("\n-------------------------------------------------------");
        $display("TEST 5: Zero Input Handling");
        $display("-------------------------------------------------------");
        
        errors = 0;
        reset = 1;
        #10;
        reset = 0;
        #10;
        
        output_stationary = 0;
        
        @(posedge clk);
        preload_valid = 1;
        preload_data = $realtobits(7.0);
        
        @(posedge clk);
        preload_valid = 0;
        in_left = 64'h0;
        in_top = 64'h0;
        
        @(posedge clk);
        #1;
        
        actual_output = $bitstoreal(out_bottom);
        if (actual_output != 0.0) begin
            $display("ERROR: Zero inputs should give 0, Got %f", actual_output);
            errors = errors + 1;
        end else begin
            $display("PASS: Zero inputs correctly produce 0");
        end
        
        if (errors == 0) begin
            $display("\nTEST 5 PASSED: Zero input handling correct");
        end else begin
            $display("\nTEST 5 FAILED: %0d errors found", errors);
        end
        
        // TEST 6: Negative Number Handling
        $display("\n-------------------------------------------------------");
        $display("TEST 6: Negative Number Handling");
        $display("-------------------------------------------------------");
        
        errors = 0;
        reset = 1;
        #10;
        reset = 0;
        #10;
        
        output_stationary = 0;
        
        @(posedge clk);
        preload_valid = 1;
        preload_data = $realtobits(-2.0);
        
        @(posedge clk);
        preload_valid = 0;
        in_left = $realtobits(3.0);
        in_top = $realtobits(1.0);
        
        @(posedge clk);
        #1;
        
        expected_output = 3.0 * (-2.0) + 1.0;  // -6 + 1 = -5
        actual_output = $bitstoreal(out_bottom);
        diff = expected_output - actual_output;
        if (diff < 0) diff = -diff;
        
        if (diff > 0.0001) begin
            $display("ERROR: Expected %f, Got %f", expected_output, actual_output);
            errors = errors + 1;
        end else begin
            $display("PASS: Negative weight handled correctly: %f", actual_output);
        end
        
        // Test with negative input
        @(posedge clk);
        in_left = $realtobits(-4.0);
        in_top = $realtobits(2.0);
        
        @(posedge clk);
        #1;
        
        expected_output = (-4.0) * (-2.0) + 2.0;  // 8 + 2 = 10
        actual_output = $bitstoreal(out_bottom);
        diff = expected_output - actual_output;
        if (diff < 0) diff = -diff;
        
        if (diff > 0.0001) begin
            $display("ERROR: Expected %f, Got %f", expected_output, actual_output);
            errors = errors + 1;
        end else begin
            $display("PASS: Negative input handled correctly: %f", actual_output);
        end
        
        if (errors == 0) begin
            $display("\nTEST 6 PASSED: Negative number handling correct");
        end else begin
            $display("\nTEST 6 FAILED: %0d errors found", errors);
        end
        
        #100;
        
        $display("\n=======================================================");
        $display("            PE Module Tests Completed");
        $display("=======================================================\n");
        
        $finish;
    end
    
    initial begin
        #50000;
        $display("\nERROR: Testbench timeout!");
        $finish;
    end

endmodule
