module Memory_tb;

    parameter ADDR_WIDTH = 16;
    parameter DATA_WIDTH = 64;
    parameter MEM_SIZE = 65536;
    
    reg clk;
    reg reset;
    reg read_enable;
    reg write_enable;
    reg [ADDR_WIDTH-1:0] address;
    reg [DATA_WIDTH-1:0] write_data;
    wire [DATA_WIDTH-1:0] read_data;
    wire ready;
    
    Memory #(
        .ADDR_WIDTH(ADDR_WIDTH),
        .DATA_WIDTH(DATA_WIDTH),
        .MEM_SIZE(MEM_SIZE),
        .MEM_FILE("")  // No file loading for this test
    ) dut (
        .clk(clk),
        .reset(reset),
        .read_enable(read_enable),
        .write_enable(write_enable),
        .address(address),
        .write_data(write_data),
        .read_data(read_data),
        .ready(ready)
    );
    
    integer i;
    integer errors;
    reg [DATA_WIDTH-1:0] expected_data;
    
    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end
    
    initial begin
        $dumpfile("memory_test.vcd");
        $dumpvars(0, Memory_tb);
        
        $display("=======================================================");
        $display("            Memory Module Testbench");
        $display("=======================================================");
        
        errors = 0;
        reset = 1;
        read_enable = 0;
        write_enable = 0;
        address = 0;
        write_data = 0;
        
        #20;
        reset = 0;
        #20;
        
        // TEST 1: Basic Write and Read
        $display("\n-------------------------------------------------------");
        $display("TEST 1: Basic Write and Read Operations");
        $display("-------------------------------------------------------");
        
        for (i = 0; i < 10; i = i + 1) begin
            @(posedge clk);
            address = i * 8;
            write_data = $realtobits(i * 1.5);
            write_enable = 1;
            read_enable = 0;
            
            @(posedge clk);
            wait(ready == 1);
            write_enable = 0;
            #10;
        end
        
        $display("Write phase complete. Starting read verification...");
        
        for (i = 0; i < 10; i = i + 1) begin
            @(posedge clk);
            address = i * 8;
            read_enable = 1;
            write_enable = 0;
            expected_data = $realtobits(i * 1.5);
            
            @(posedge clk);
            wait(ready == 1);
            
            if (read_data !== expected_data) begin
                $display("ERROR: Address %0d - Expected %h, Got %h", i*8, expected_data, read_data);
                errors = errors + 1;
            end else begin
                $display("PASS: Address %0d - Data %f", i*8, $bitstoreal(read_data));
            end
            
            read_enable = 0;
            #10;
        end
        
        if (errors == 0) begin
            $display("\nTEST 1 PASSED: All basic read/write operations successful");
        end else begin
            $display("\nTEST 1 FAILED: %0d errors found", errors);
        end
        
        // TEST 2: Sequential Write Pattern
        $display("\n-------------------------------------------------------");
        $display("TEST 2: Sequential Write Pattern");
        $display("-------------------------------------------------------");
        
        errors = 0;
        
        for (i = 0; i < 100; i = i + 1) begin
            @(posedge clk);
            address = i * 8;
            write_data = i + 100;
            write_enable = 1;
            read_enable = 0;
            
            @(posedge clk);
            wait(ready == 1);
            write_enable = 0;
        end
        
        $display("Sequential write complete. Verifying...");
        
        for (i = 0; i < 100; i = i + 1) begin
            @(posedge clk);
            address = i * 8;
            read_enable = 1;
            write_enable = 0;
            
            @(posedge clk);
            wait(ready == 1);
            
            if (read_data !== (i + 100)) begin
                $display("ERROR: Address %0d - Expected %0d, Got %0d", i*8, i+100, read_data);
                errors = errors + 1;
            end
            
            read_enable = 0;
        end
        
        if (errors == 0) begin
            $display("TEST 2 PASSED: Sequential pattern verified (%0d locations)", 100);
        end else begin
            $display("TEST 2 FAILED: %0d errors found", errors);
        end
        
        // TEST 3: Overwrite Test
        $display("\n-------------------------------------------------------");
        $display("TEST 3: Memory Overwrite Test");
        $display("-------------------------------------------------------");
        
        errors = 0;
        address = 16'd1000;
        
        @(posedge clk);
        write_data = 64'hAAAAAAAAAAAAAAAA;
        write_enable = 1;
        read_enable = 0;
        
        @(posedge clk);
        wait(ready == 1);
        write_enable = 0;
        #10;
        
        @(posedge clk);
        read_enable = 1;
        
        @(posedge clk);
        wait(ready == 1);
        
        if (read_data !== 64'hAAAAAAAAAAAAAAAA) begin
            $display("ERROR: First write - Expected AAAA..., Got %h", read_data);
            errors = errors + 1;
        end else begin
            $display("First write verified: %h", read_data);
        end
        
        read_enable = 0;
        #10;
        
        @(posedge clk);
        write_data = 64'h5555555555555555;
        write_enable = 1;
        
        @(posedge clk);
        wait(ready == 1);
        write_enable = 0;
        #10;
        
        @(posedge clk);
        read_enable = 1;
        
        @(posedge clk);
        wait(ready == 1);
        
        if (read_data !== 64'h5555555555555555) begin
            $display("ERROR: Overwrite - Expected 5555..., Got %h", read_data);
            errors = errors + 1;
        end else begin
            $display("Overwrite verified: %h", read_data);
        end
        
        read_enable = 0;
        
        if (errors == 0) begin
            $display("\nTEST 3 PASSED: Overwrite functionality verified");
        end else begin
            $display("\nTEST 3 FAILED: %0d errors found", errors);
        end
        
        // TEST 4: Boundary Address Test
        $display("\n-------------------------------------------------------");
        $display("TEST 4: Boundary Address Test");
        $display("-------------------------------------------------------");
        
        errors = 0;
        
        // Test address 0
        @(posedge clk);
        address = 16'd0;
        write_data = 64'hDEADBEEFDEADBEEF;
        write_enable = 1;
        read_enable = 0;
        
        @(posedge clk);
        wait(ready == 1);
        write_enable = 0;
        #10;
        
        @(posedge clk);
        read_enable = 1;
        
        @(posedge clk);
        wait(ready == 1);
        
        if (read_data !== 64'hDEADBEEFDEADBEEF) begin
            $display("ERROR: Address 0 - Expected DEADBEEF..., Got %h", read_data);
            errors = errors + 1;
        end else begin
            $display("Address 0 verified: %h", read_data);
        end
        
        read_enable = 0;
        #10;
        
        // Test high address
        @(posedge clk);
        address = 16'hFFF8;  // Near end of memory
        write_data = 64'hCAFEBABECAFEBABE;
        write_enable = 1;
        read_enable = 0;
        
        @(posedge clk);
        wait(ready == 1);
        write_enable = 0;
        #10;
        
        @(posedge clk);
        read_enable = 1;
        
        @(posedge clk);
        wait(ready == 1);
        
        if (read_data !== 64'hCAFEBABECAFEBABE) begin
            $display("ERROR: High address - Expected CAFEBABE..., Got %h", read_data);
            errors = errors + 1;
        end else begin
            $display("High address verified: %h", read_data);
        end
        
        read_enable = 0;
        
        if (errors == 0) begin
            $display("\nTEST 4 PASSED: Boundary addresses work correctly");
        end else begin
            $display("\nTEST 4 FAILED: %0d errors found", errors);
        end
        
        // TEST 5: Reset Functionality
        $display("\n-------------------------------------------------------");
        $display("TEST 5: Reset Functionality Test");
        $display("-------------------------------------------------------");
        
        errors = 0;
        
        @(posedge clk);
        address = 16'd2000;
        write_data = 64'h1234567890ABCDEF;
        write_enable = 1;
        
        @(posedge clk);
        wait(ready == 1);
        write_enable = 0;
        
        $display("Data written before reset");
        
        #10;
        reset = 1;
        #20;
        reset = 0;
        #20;
        
        @(posedge clk);
        address = 16'd2000;
        read_enable = 1;
        
        @(posedge clk);
        @(posedge clk);
        
        // After reset, state should be IDLE and ready should be 0
        if (ready !== 0) begin
            $display("ERROR: Ready signal not 0 after reset");
            errors = errors + 1;
        end else begin
            $display("Reset state verified: ready = 0");
        end
        
        read_enable = 0;
        
        if (errors == 0) begin
            $display("\nTEST 5 PASSED: Reset functionality verified");
        end else begin
            $display("\nTEST 5 FAILED: %0d errors found", errors);
        end
        
        #100;
        
        $display("\n=======================================================");
        $display("          Memory Module Tests Completed");
        $display("=======================================================\n");
        
        $finish;
    end
    
    initial begin
        #100000;
        $display("\nERROR: Testbench timeout!");
        $finish;
    end

endmodule
