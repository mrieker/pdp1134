//    Copyright (C) Mike Rieker, Beverly, MA USA
//    www.outerworldapps.com
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; version 2 of the License.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    EXPECT it to FAIL when someone's HeALTh or PROpeRTy is at RISk.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//    http://www.gnu.org/licenses/gpl-2.0.html

// 4KB memory

module lilmem
    #(parameter[17:00] ADDR=18'o000000) (
    input CLOCK, RESET,

    input armwrite,
    input[1:0] armraddr, armwaddr,
    input[31:00] armwdata,
    output[31:00] armrdata,

    input[17:00] a_in_h,
    input[1:0] c_in_h,
    input[15:00] d_in_h,
    input init_in_h,
    input msyn_in_h,

    output reg[15:00] d_out_h,
    output reg ssyn_out_h);

    reg enable;
    reg[7:0] memhi[2047:0], memlo[2047:0];

    reg[11:01] addrptr;
    reg[15:00] dataval;

    assign armrdata = (armraddr == 0) ? 32'h4C4D1001 : // [31:16] = 'LM'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? { 20'b0, addrptr, 1'b0 } :
                      (armraddr == 2) ? { 16'b0, dataval } :
                      { enable, 13'b0, ADDR };

    always @(posedge CLOCK) begin
        if (init_in_h) begin
            if (RESET) begin
                enable  <= 0;
                addrptr <= 0;
                dataval <= 16'hBAAD;
            end
            d_out_h    <= 0;
            ssyn_out_h <= 0;
        end

        // arm processor is writing one of the registers
        else if (armwrite) begin
            case (armwaddr)
                1: begin
                    addrptr <= armwdata[11:01];
                    dataval <= { memhi[armwdata[11:01]], memlo[armwdata[11:01]] };
                end
                2: begin
                    dataval        <= armwdata[15:00];
                    memhi[addrptr] <= armwdata[15:08];
                    memlo[addrptr] <= armwdata[07:00];
                end
                3: begin
                    enable <= armwdata[31];
                end
            endcase
        end

        // pdp or something else is accessing a memory location
        else if (~ msyn_in_h) begin
            d_out_h    <= 0;
            ssyn_out_h <= 0;
        end else if (enable & (a_in_h[17:12] == ADDR[17:12]) & ~ ssyn_out_h) begin
            ssyn_out_h <= 1;
            if (c_in_h[1]) begin
                if (~ c_in_h[0] |   a_in_h[00]) begin
                    memhi[a_in_h[11:01]] <= d_in_h[15:08];
                    if (a_in_h[11:01] == addrptr) dataval[15:08] <= d_in_h[15:08];
                end
                if (~ c_in_h[0] | ~ a_in_h[00]) begin
                    memlo[a_in_h[11:01]] <= d_in_h[07:00];
                    if (a_in_h[11:01] == addrptr) dataval[07:00] <= d_in_h[07:00];
                end
            end else begin
                d_out_h[15:08] <= memhi[a_in_h[11:01]];
                d_out_h[07:00] <= memlo[a_in_h[11:01]];
            end
        end
    end
endmodule
