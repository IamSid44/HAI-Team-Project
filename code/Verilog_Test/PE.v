module PE (
    input wire clk,
    input wire reset,
    input wire output_stationary,
    input wire [63:0] in_top,
    input wire [63:0] in_left,
    output reg [63:0] out_right,
    output reg [63:0] out_bottom,
    input wire preload_valid,
    input wire [63:0] preload_data
);

    reg [63:0] preload_buffer;
    reg [63:0] accumulator_buffer;
    reg drain_value_sent;
    
    real in_top_real, in_left_real, preload_real;
    real accumulator_real, result_real;
    
    always @(posedge clk or posedge reset) begin
        if (reset) begin
            preload_buffer <= 64'h0;
            accumulator_buffer <= 64'h0;
            out_right <= 64'h0;
            out_bottom <= 64'h0;
            drain_value_sent <= 1'b0;
        end else begin
            if (output_stationary) begin
                if (preload_valid) begin
                    if (!drain_value_sent) begin
                        out_bottom <= accumulator_buffer;
                        drain_value_sent <= 1'b1;
                    end else begin
                        out_bottom <= in_top;
                    end
                    out_right <= 64'h0;
                end else begin
                    in_top_real = $bitstoreal(in_top);
                    in_left_real = $bitstoreal(in_left);
                    accumulator_real = $bitstoreal(accumulator_buffer);
                    result_real = accumulator_real + (in_top_real * in_left_real);
                    accumulator_buffer <= $realtobits(result_real);
                    out_right <= in_left;
                    out_bottom <= in_top;
                    drain_value_sent <= 1'b0;
                end
            end else begin
                if (preload_valid) begin
                    preload_buffer <= preload_data;
                end
                in_left_real = $bitstoreal(in_left);
                preload_real = $bitstoreal(preload_buffer);
                in_top_real = $bitstoreal(in_top);
                result_real = (in_left_real * preload_real) + in_top_real;
                accumulator_buffer <= $realtobits(result_real);
                out_bottom <= accumulator_buffer;
                out_right <= in_left;
                drain_value_sent <= 1'b0;
            end
        end
    end

endmodule
