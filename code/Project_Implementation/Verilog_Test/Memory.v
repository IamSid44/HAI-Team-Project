module Memory #(
    parameter ADDR_WIDTH = 16,
    parameter DATA_WIDTH = 64,
    parameter MEM_SIZE = 65536,
    parameter MEM_FILE = ""
)(
    input wire clk,
    input wire reset,
    input wire read_enable,
    input wire write_enable,
    input wire [ADDR_WIDTH-1:0] address,
    input wire [DATA_WIDTH-1:0] write_data,
    output reg [DATA_WIDTH-1:0] read_data,
    output reg ready
);

    reg [DATA_WIDTH-1:0] mem_array [0:MEM_SIZE/8-1];
    
    localparam IDLE = 2'b00;
    localparam READ = 2'b01;
    localparam WRITE = 2'b10;
    
    reg [1:0] state, next_state;
    
    integer i;
    integer file;
    integer scan_result;
    reg [31:0] byte_addr;
    real value;
    reg [255:0] line;
    
    initial begin
        // Initialize all memory to zero
        for (i = 0; i < MEM_SIZE/8; i = i + 1) begin
            mem_array[i] = 64'h0;
        end
        
        // Load from file if specified
        if (MEM_FILE != "") begin
            file = $fopen(MEM_FILE, "r");
            if (file == 0) begin
                $display("ERROR: Could not open memory file: %s", MEM_FILE);
            end else begin
                $display("Loading memory from file: %s", MEM_FILE);
                
                // Read and parse file lines
                while (!$feof(file)) begin
                    scan_result = $fgets(line, file);
                    if (scan_result != 0) begin
                        // Skip comments and empty lines
                        if (line[7:0] != "#" && line[7:0] != "\n" && line[7:0] != 0) begin
                            // Parse line: "ADDRESS: VALUE" or "ADDRESS: XXXX"
                            scan_result = $sscanf(line, "%d: %f", byte_addr, value);
                            if (scan_result == 2) begin
                                // Valid data line with numeric value
                                mem_array[byte_addr >> 3] = $realtobits(value);
                                if (byte_addr < 512) begin  // Only display first few for verification
                                    $display("  [%08d] = %f", byte_addr, value);
                                end
                            end
                            // Lines with XXXX are left as zero (already initialized)
                        end
                    end
                end
                
                $fclose(file);
                $display("Memory initialization complete");
            end
        end
    end
    
    always @(posedge clk or posedge reset) begin
        if (reset) begin
            state <= IDLE;
        end else begin
            state <= next_state;
        end
    end
    
    always @(*) begin
        next_state = state;
        case (state)
            IDLE: begin
                if (read_enable && !write_enable) begin
                    next_state = READ;
                end else if (write_enable && !read_enable) begin
                    next_state = WRITE;
                end
            end
            READ: begin
                next_state = IDLE;
            end
            WRITE: begin
                next_state = IDLE;
            end
            default: next_state = IDLE;
        endcase
    end
    
    always @(posedge clk or posedge reset) begin
        if (reset) begin
            read_data <= 64'h0;
            ready <= 1'b0;
        end else begin
            case (state)
                IDLE: begin
                    ready <= 1'b0;
                end
                READ: begin
                    read_data <= mem_array[address >> 3];
                    ready <= 1'b1;
                end
                WRITE: begin
                    ready <= 1'b1;
                end
                default: begin
                    ready <= 1'b0;
                end
            endcase
        end
    end
    
    always @(negedge clk) begin
        if (state == WRITE && !reset) begin
            mem_array[address >> 3] <= write_data;
        end
    end

endmodule
