module Control #(
    parameter M = 3,
    parameter ADDR_WIDTH = 16,
    parameter MEM_SIZE = 65536
)(
    input wire clk,
    input wire reset,
    input wire start,
    output reg done,
    input wire [7:0] K1,
    input wire [7:0] K2,
    input wire [7:0] K3,
    input wire [ADDR_WIDTH-1:0] A_base_addr,
    input wire [ADDR_WIDTH-1:0] W_base_addr,
    input wire [ADDR_WIDTH-1:0] C_base_addr,
    input wire output_stationary
);

    localparam IDLE = 4'd0;
    localparam INIT = 4'd1;
    localparam READ_A = 4'd2;
    localparam READ_W = 4'd3;
    localparam COMPUTE = 4'd4;
    localparam WRITE_C = 4'd5;
    localparam NEXT_TILE = 4'd6;
    localparam DONE_STATE = 4'd7;
    
    reg [3:0] state, next_state;
    
    reg mem_read_enable;
    reg mem_write_enable;
    reg [ADDR_WIDTH-1:0] mem_address;
    reg [63:0] mem_write_data;
    wire [63:0] mem_read_data;
    wire mem_ready;
    
    Memory #(
        .ADDR_WIDTH(ADDR_WIDTH),
        .DATA_WIDTH(64),
        .MEM_SIZE(MEM_SIZE),
        .MEM_FILE("")
    ) memory_inst (
        .clk(clk),
        .reset(reset),
        .read_enable(mem_read_enable),
        .write_enable(mem_write_enable),
        .address(mem_address),
        .write_data(mem_write_data),
        .read_data(mem_read_data),
        .ready(mem_ready)
    );
    
    reg [63:0] A_tile [0:M*M-1];
    reg [63:0] W_tile [0:M*M-1];
    wire [63:0] C_tile [0:M*M-1];
    
    wire [64*M*M-1:0] A_tile_flat;
    wire [64*M*M-1:0] W_tile_flat;
    wire [64*M*M-1:0] C_tile_flat;
    
    genvar flat_idx;
    generate
        for (flat_idx = 0; flat_idx < M*M; flat_idx = flat_idx + 1) begin : flatten_tiles
            assign A_tile_flat[64*(flat_idx+1)-1:64*flat_idx] = A_tile[flat_idx];
            assign W_tile_flat[64*(flat_idx+1)-1:64*flat_idx] = W_tile[flat_idx];
            assign C_tile[flat_idx] = C_tile_flat[64*(flat_idx+1)-1:64*flat_idx];
        end
    endgenerate
    
    reg matmul_start;
    wire matmul_done;
    reg [7:0] tile_k1, tile_k2, tile_k3;
    
    MatMul_Controller #(.M(M), .N(M)) matmul_ctrl (
        .clk(clk),
        .reset(reset),
        .start(matmul_start),
        .done(matmul_done),
        .output_stationary(output_stationary),
        .A_tile_flat(A_tile_flat),
        .W_tile_flat(W_tile_flat),
        .C_tile_flat(C_tile_flat),
        .k1(tile_k1),
        .k2(tile_k2),
        .k3(tile_k3)
    );
    
    reg [7:0] k1_reg, k2_reg, k3_reg;
    reg [ADDR_WIDTH-1:0] a_base_reg, w_base_reg, c_base_reg;
    
    reg [7:0] num_i_tiles, num_j_tiles, num_k_tiles;
    reg [7:0] i_tile, j_tile, k_tile;
    reg [7:0] tile_i, tile_j;
    reg [15:0] mem_counter;
    reg [7:0] actual_i, actual_k, actual_j;
    reg compute_started;
    
    integer idx;
    
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
                if (start) begin
                    next_state = INIT;
                end
            end
            
            INIT: begin
                next_state = READ_A;
            end
            
            READ_A: begin
                if (mem_counter >= M * M) begin
                    next_state = READ_W;
                end
            end
            
            READ_W: begin
                if (mem_counter >= M * M) begin
                    next_state = COMPUTE;
                end
            end
            
            COMPUTE: begin
                if (matmul_done) begin
                    if (k_tile == num_k_tiles - 1) begin
                        next_state = WRITE_C;
                    end else begin
                        next_state = READ_A;
                    end
                end
            end
            
            WRITE_C: begin
                if (mem_counter >= M * M) begin
                    next_state = NEXT_TILE;
                end
            end
            
            NEXT_TILE: begin
                if (j_tile == num_j_tiles - 1 && i_tile == num_i_tiles - 1) begin
                    next_state = DONE_STATE;
                end else begin
                    next_state = INIT;
                end
            end
            
            DONE_STATE: begin
                next_state = IDLE;
            end
            
            default: next_state = IDLE;
        endcase
    end
    
    always @(posedge clk or posedge reset) begin
        if (reset) begin
            done <= 1'b0;
            mem_read_enable <= 1'b0;
            mem_write_enable <= 1'b0;
            mem_address <= {ADDR_WIDTH{1'b0}};
            mem_write_data <= 64'h0;
            matmul_start <= 1'b0;
            
            k1_reg <= 8'd0;
            k2_reg <= 8'd0;
            k3_reg <= 8'd0;
            a_base_reg <= {ADDR_WIDTH{1'b0}};
            w_base_reg <= {ADDR_WIDTH{1'b0}};
            c_base_reg <= {ADDR_WIDTH{1'b0}};
            
            num_i_tiles <= 8'd0;
            num_j_tiles <= 8'd0;
            num_k_tiles <= 8'd0;
            i_tile <= 8'd0;
            j_tile <= 8'd0;
            k_tile <= 8'd0;
            
            tile_i <= 8'd0;
            tile_j <= 8'd0;
            mem_counter <= 16'd0;
            
            tile_k1 <= 8'd0;
            tile_k2 <= 8'd0;
            tile_k3 <= 8'd0;
            
            actual_i <= 8'd0;
            actual_k <= 8'd0;
            actual_j <= 8'd0;
            compute_started <= 1'b0;
            
            for (idx = 0; idx < M*M; idx = idx + 1) begin
                A_tile[idx] <= 64'h0;
                W_tile[idx] <= 64'h0;
            end
            
        end else begin
            case (state)
                IDLE: begin
                    done <= 1'b0;
                    mem_read_enable <= 1'b0;
                    mem_write_enable <= 1'b0;
                    matmul_start <= 1'b0;
                    
                    if (start) begin
                        k1_reg <= K1;
                        k2_reg <= K2;
                        k3_reg <= K3;
                        a_base_reg <= A_base_addr;
                        w_base_reg <= W_base_addr;
                        c_base_reg <= C_base_addr;
                        
                        num_i_tiles <= (K1 + M - 1) / M;
                        num_j_tiles <= (K3 + M - 1) / M;
                        num_k_tiles <= (K2 + M - 1) / M;
                        
                        i_tile <= 8'd0;
                        j_tile <= 8'd0;
                        k_tile <= 8'd0;
                    end
                end
                
                INIT: begin
                    mem_counter <= 16'd0;
                    tile_i <= 8'd0;
                    tile_j <= 8'd0;
                    compute_started <= 1'b0;
                    
                    if (i_tile * M + M <= k1_reg) begin
                        actual_i <= M;
                    end else begin
                        actual_i <= k1_reg - i_tile * M;
                    end
                    
                    if (k_tile * M + M <= k2_reg) begin
                        actual_k <= M;
                    end else begin
                        actual_k <= k2_reg - k_tile * M;
                    end
                    
                    if (j_tile * M + M <= k3_reg) begin
                        actual_j <= M;
                    end else begin
                        actual_j <= k3_reg - j_tile * M;
                    end
                end
                
                READ_A: begin
                    if (mem_counter < M * M) begin
                        tile_i <= mem_counter / M;
                        tile_j <= mem_counter % M;
                        
                        if (((mem_counter / M) < actual_i) && ((mem_counter % M) < actual_k)) begin
                            mem_address <= a_base_reg + ((i_tile * M + (mem_counter / M)) * k2_reg + (k_tile * M + (mem_counter % M))) * 8;
                            mem_read_enable <= 1'b1;
                            mem_write_enable <= 1'b0;
                            
                            if (mem_ready) begin
                                A_tile[mem_counter] <= mem_read_data;
                                mem_counter <= mem_counter + 1;
                                mem_read_enable <= 1'b0;
                            end
                        end else begin
                            A_tile[mem_counter] <= 64'h0;
                            mem_counter <= mem_counter + 1;
                        end
                    end else begin
                        mem_read_enable <= 1'b0;
                        mem_counter <= 16'd0;
                    end
                end
                
                READ_W: begin
                    if (mem_counter < M * M) begin
                        tile_i <= mem_counter / M;
                        tile_j <= mem_counter % M;
                        
                        if (((mem_counter / M) < actual_k) && ((mem_counter % M) < actual_j)) begin
                            mem_address <= w_base_reg + ((k_tile * M + (mem_counter / M)) * k3_reg + (j_tile * M + (mem_counter % M))) * 8;
                            mem_read_enable <= 1'b1;
                            mem_write_enable <= 1'b0;
                            
                            if (mem_ready) begin
                                W_tile[mem_counter] <= mem_read_data;
                                mem_counter <= mem_counter + 1;
                                mem_read_enable <= 1'b0;
                            end
                        end else begin
                            W_tile[mem_counter] <= 64'h0;
                            mem_counter <= mem_counter + 1;
                        end
                    end else begin
                        mem_read_enable <= 1'b0;
                        mem_counter <= 16'd0;
                    end
                end
                
                COMPUTE: begin
                    mem_read_enable <= 1'b0;
                    mem_write_enable <= 1'b0;
                    
                    if (!compute_started && !matmul_done) begin
                        tile_k1 <= actual_i;
                        tile_k2 <= actual_k;
                        tile_k3 <= actual_j;
                        matmul_start <= 1'b1;
                        compute_started <= 1'b1;
                    end else begin
                        matmul_start <= 1'b0;
                    end
                    
                    if (matmul_done) begin
                        if (k_tile == num_k_tiles - 1) begin
                            mem_counter <= 16'd0;
                        end else begin
                            k_tile <= k_tile + 1;
                            mem_counter <= 16'd0;
                        end
                    end
                end
                
                WRITE_C: begin
                    if (mem_counter < M * M) begin
                        tile_i <= mem_counter / M;
                        tile_j <= mem_counter % M;
                        
                        if (((mem_counter / M) < actual_i) && ((mem_counter % M) < actual_j)) begin
                            mem_address <= c_base_reg + ((i_tile * M + (mem_counter / M)) * k3_reg + (j_tile * M + (mem_counter % M))) * 8;
                            mem_write_data <= C_tile[mem_counter];
                            mem_read_enable <= 1'b0;
                            mem_write_enable <= 1'b1;
                            
                            if (mem_ready) begin
                                mem_counter <= mem_counter + 1;
                                mem_write_enable <= 1'b0;
                            end
                        end else begin
                            mem_counter <= mem_counter + 1;
                        end
                    end else begin
                        mem_write_enable <= 1'b0;
                    end
                end
                
                NEXT_TILE: begin
                    k_tile <= 8'd0;
                    
                    if (j_tile == num_j_tiles - 1) begin
                        j_tile <= 8'd0;
                        i_tile <= i_tile + 1;
                    end else begin
                        j_tile <= j_tile + 1;
                    end
                end
                
                DONE_STATE: begin
                    done <= 1'b1;
                end
                
                default: begin
                    done <= 1'b0;
                    mem_read_enable <= 1'b0;
                    mem_write_enable <= 1'b0;
                    matmul_start <= 1'b0;
                end
            endcase
        end
    end

endmodule
