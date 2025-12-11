module SA_MxN #(
    parameter M = 3,
    parameter N = 3
)(
    input wire clk,
    input wire reset,
    input wire output_stationary,
    input wire [64*N-1:0] in_top,
    input wire [64*M-1:0] in_left,
    output wire [64*M-1:0] out_right,
    output wire [64*N-1:0] out_bottom,
    input wire preload_valid,
    input wire [64*M*N-1:0] preload_data
);

    wire [63:0] pe_out_right [0:M-1][0:N-1];
    wire [63:0] pe_out_bottom [0:M-1][0:N-1];
    wire [63:0] pe_in_top [0:M-1][0:N-1];
    wire [63:0] pe_in_left [0:M-1][0:N-1];
    
    wire [63:0] in_top_unpacked [0:N-1];
    wire [63:0] in_left_unpacked [0:M-1];
    wire [63:0] out_right_unpacked [0:M-1];
    wire [63:0] out_bottom_unpacked [0:N-1];
    wire [63:0] preload_data_unpacked [0:M*N-1];
    
    genvar i, j;
    generate
        for (i = 0; i < N; i = i + 1) begin : unpack_in_top
            assign in_top_unpacked[i] = in_top[64*(i+1)-1:64*i];
        end
        for (i = 0; i < M; i = i + 1) begin : unpack_in_left
            assign in_left_unpacked[i] = in_left[64*(i+1)-1:64*i];
        end
        for (i = 0; i < M; i = i + 1) begin : pack_out_right
            assign out_right[64*(i+1)-1:64*i] = out_right_unpacked[i];
        end
        for (i = 0; i < N; i = i + 1) begin : pack_out_bottom
            assign out_bottom[64*(i+1)-1:64*i] = out_bottom_unpacked[i];
        end
        for (i = 0; i < M*N; i = i + 1) begin : unpack_preload
            assign preload_data_unpacked[i] = preload_data[64*(i+1)-1:64*i];
        end
    endgenerate

    genvar row, col;
    generate
        for (row = 0; row < M; row = row + 1) begin : row_gen
            for (col = 0; col < N; col = col + 1) begin : col_gen
                
                if (col == 0) begin
                    assign pe_in_left[row][col] = in_left_unpacked[row];
                end else begin
                    assign pe_in_left[row][col] = pe_out_right[row][col-1];
                end
                
                if (row == 0) begin
                    assign pe_in_top[row][col] = in_top_unpacked[col];
                end else begin
                    assign pe_in_top[row][col] = pe_out_bottom[row-1][col];
                end
                
                if (col == N-1) begin
                    assign out_right_unpacked[row] = pe_out_right[row][col];
                end
                
                if (row == M-1) begin
                    assign out_bottom_unpacked[col] = pe_out_bottom[row][col];
                end
                
                PE pe_inst (
                    .clk(clk),
                    .reset(reset),
                    .output_stationary(output_stationary),
                    .in_top(pe_in_top[row][col]),
                    .in_left(pe_in_left[row][col]),
                    .out_right(pe_out_right[row][col]),
                    .out_bottom(pe_out_bottom[row][col]),
                    .preload_valid(preload_valid),
                    .preload_data(preload_data_unpacked[row*N + col])
                );
            end
        end
    endgenerate

endmodule
