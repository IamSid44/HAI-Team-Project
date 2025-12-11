module adder_tree;
    
    // Parameters
    parameter MAX_ROWS = 64;
    parameter MAX_WIDTH = 64;
    
    // Storage for input data
    reg [MAX_WIDTH-1:0] partial_products [0:MAX_ROWS-1];
    reg [MAX_WIDTH*2-1:0] tree_level [0:MAX_ROWS-1];
    reg [MAX_WIDTH*2-1:0] next_level [0:MAX_ROWS-1];
    reg [MAX_WIDTH*2-1:0] final_result;
    reg [MAX_WIDTH*2-1:0] result_mask;
    
    integer num_bits;
    integer num_rows;
    integer result_width;
    integer current_count;
    integer next_count;
    integer level;
    integer i, j;
    integer add_count;
    integer file_handle;
    integer scan_result;
    
    initial begin
        // Initialize variables
        final_result = 0;
        add_count = 0;
        level = 0;
        
        // Open trace file
        $dumpfile("adder_tree_trace.vcd");
        $dumpvars(0, adder_tree);
        
        // Display header
        $display("=====================================");
        $display("   ADDER TREE PROCESSOR");
        $display("=====================================");
        $display("");
        
        // Open and read the input file
        file_handle = $fopen("data_out.txt", "r");
        
        if (file_handle == 0) begin
            $display("ERROR: Could not open data_out.txt");
            $finish;
        end
        
        // Read number of bits and number of rows
        scan_result = $fscanf(file_handle, "%d %d\n", num_bits, num_rows);
        
        if (scan_result != 2) begin
            $display("ERROR: Could not read bit width and number of rows");
            $fclose(file_handle);
            $finish;
        end
        
        // Calculate result width (twice the input bit width)
        result_width = num_bits * 2;
        
        // Create mask for result display
        result_mask = (1 << result_width) - 1;
        
        $display("Bit width per row: %0d", num_bits);
        $display("Number of partial products: %0d", num_rows);
        $display("Result width: %0d bits", result_width);
        $display("");
        
        if (num_rows > MAX_ROWS || num_rows <= 0) begin
            $display("ERROR: Invalid number of rows (must be 1-%0d)", MAX_ROWS);
            $fclose(file_handle);
            $finish;
        end
        
        if (num_bits > MAX_WIDTH || num_bits <= 0) begin
            $display("ERROR: Invalid bit width (must be 1-%0d)", MAX_WIDTH);
            $fclose(file_handle);
            $finish;
        end
        
        // Read all partial products
        $display("Reading partial products from file:");
        $display("-------------------------------------");
        for (i = 0; i < num_rows; i = i + 1) begin
            scan_result = $fscanf(file_handle, "%b\n", partial_products[i]);
            if (scan_result != 1) begin
                $display("ERROR: Could not read row %0d", i);
                $fclose(file_handle);
                $finish;
            end
        end
        
        $fclose(file_handle);
        
        // Initialize first level with shifted partial products
        $display("Initializing shifted partial products:");
        $display("-------------------------------------");
        for (i = 0; i < num_rows; i = i + 1) begin
            tree_level[i] = partial_products[i] << i;
            $display("Level 0, Index %0d: Input=%0d, Shift=%0d, Result=%0d", 
                     i, partial_products[i], i, tree_level[i] & result_mask);
        end
        $display("");
        
        current_count = num_rows;
        
        // Build adder tree level by level
        $display("Building Adder Tree:");
        $display("-------------------------------------");
        
        while (current_count > 1) begin
            level = level + 1;
            next_count = 0;
            
            $display("Level %0d: Processing %0d values", level, current_count);
            
            // Add pairs
            for (i = 0; i < current_count; i = i + 2) begin
                if (i + 1 < current_count) begin
                    // Add pair
                    next_level[next_count] = tree_level[i] + tree_level[i+1];
                    add_count = add_count + 1;
                    $display("  Add[%0d]: %0d + %0d = %0d", 
                             next_count, 
                             tree_level[i] & result_mask, 
                             tree_level[i+1] & result_mask, 
                             next_level[next_count] & result_mask);
                    next_count = next_count + 1;
                end else begin
                    // Odd one out, pass through
                    next_level[next_count] = tree_level[i];
                    $display("  Pass[%0d]: %0d (no pair)", 
                             next_count, 
                             tree_level[i] & result_mask);
                    next_count = next_count + 1;
                end
            end
            
            // Copy next level to current level
            for (j = 0; j < next_count; j = j + 1) begin
                tree_level[j] = next_level[j];
            end
            current_count = next_count;
            $display("");
        end
        
        final_result = tree_level[0];
        
        // Display summary
        $display("=====================================");
        $display("   RESULTS SUMMARY");
        $display("=====================================");
        $display("Input bit width: %0d", num_bits);
        $display("Result bit width: %0d (2 x input width)", result_width);
        $display("Number of adder tree levels: %0d", level);
        $display("Total number of ADD operations: %0d", add_count);
        
        // Display result with proper bit width
        $display("");
        $display("Final result:");
        for (i = result_width - 1; i >= 0; i = i - 1) begin
            $write("%0d", (final_result >> i) & 1'b1);
            if (i > 0 && i % 4 == 0) $write(" ");
        end
        $display(" (binary)");
        $display("%0d (decimal)", final_result & result_mask);
        $display("0x%0h (hex)", final_result & result_mask);
        $display("");
        $display("Trace file generated: adder_tree_trace.vcd");
        $display("=====================================");
        
        #10;
        $finish;
    end
    
endmodule