// AS gates
module SFQ_NOT ( input wire clk, input wire rst, input wire a, output wire out);
    reg result;
    always @(posedge clk or posedge rst) begin
        if (rst)
            result <= 1'b0;
        else
            result <= ~a;
    end
    assign out = result;
endmodule

module SFQ_DFF ( input wire clk, input wire rst, input wire a, output wire out);
    reg result;
    always @(posedge clk or posedge rst) begin
        if (rst)
            result <= 1'b0;
        else
            result <= a;
    end
    assign out = result;
endmodule

module SFQ_XOR ( input wire clk, input wire rst, input wire a, input wire b, output wire out);
    reg result;
    always @(posedge clk or posedge rst) begin
        if (rst)
            result <= 1'b0;
        else
            result <= a ^ b;
    end
    assign out = result;
endmodule

// SA gates
module SFQ_AND ( input wire a, input wire b, output wire out );
    assign out = a & b;
endmodule

module SFQ_OR ( input wire a, input wire b, output wire out );
    assign out = a | b;
endmodule

// AA gates
module SFQ_CB ( input wire a, input wire b, output wire out );
    assign out = a | b;
endmodule

module SFQ_SPL ( input wire a, output wire out1, output wire out2 );
    assign out1 = a;
    assign out2 = a;
endmodule

/*
    module SFQ_DFF_DFF_AND ( input wire clk, input wire rst, input wire a, input wire b, output wire out);
        reg result;
        always @(posedge clk or posedge rst) begin
            if (rst)
                result <= 1'b0;
            else
                result <= a & b;
        end
        assign out = result;
    endmodule

    module SFQ_DFF_NOT_AND ( input wire clk, input wire rst, input wire a, input wire b, output wire out);
        reg result;
        always @(posedge clk or posedge rst) begin
            if (rst)
                result <= 1'b0;
            else
                result <= a & ~b;
        end
        assign out = result;
    endmodule

    module SFQ_NOT_NOT_AND ( input wire clk, input wire rst, input wire a, input wire b, output wire out);
        reg result;
        always @(posedge clk or posedge rst) begin
            if (rst)
                result <= 1'b0;
            else
                result <= ~a & ~b;
        end
        assign out = result;
    endmodule

    module SFQ_DFF_DFF_OR ( input wire clk, input wire rst, input wire a, input wire b, output wire out);
        reg result;
        always @(posedge clk or posedge rst) begin
            if (rst)
                result <= 1'b0;
            else
                result <= a | b;
        end
        assign out = result;
    endmodule

    module SFQ_DFF_NOT_OR ( input wire clk, input wire rst, input wire a, input wire b, output wire out);
        reg result;
        always @(posedge clk or posedge rst) begin
            if (rst)
                result <= 1'b0;
            else
                result <= a | ~b;
        end
        assign out = result;
    endmodule

    module SFQ_NOT_NOT_OR ( input wire clk, input wire rst, input wire a, input wire b, output wire out);
        reg result;
        always @(posedge clk or posedge rst) begin
            if (rst)
                result <= 1'b0;
            else
                result <= ~a | ~b;
        end
        assign out = result;
    endmodule
*/