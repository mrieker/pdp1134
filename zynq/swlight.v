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

// switches and lights

module swlight (
    input CLOCK, RESET,

    input armwrite,
    input[1:0] armraddr, armwaddr,
    input[31:00] armwdata,
    output[31:00] armrdata,

    input[17:00] a_in_h,
    input[1:0] c_in_h,
    input[15:00] d_in_h,
    input hltgr_in_l,
    input init_in_h,
    input msyn_in_h,

    output reg[15:00] d_out_h,
    output reg hltrq_out_h,
    output init_out_h,
    output reg sack_out_h,
    output reg ssyn_out_h);

    reg enable, haltreq, halted, stepreq, businit;
    reg[1:0] haltstate;
    reg[15:00] lights, switches;

    assign armrdata = (armraddr == 0) ? 32'h534C1001 : // [31:16] = 'SL'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? { lights, switches } :
                      (armraddr == 2) ? { enable, haltreq, halted, stepreq, businit, 27'b0 } :
                      32'hDEADBEEF;

    assign init_out_h = businit;

    always @(posedge CLOCK) begin
        if (RESET) begin
            businit     <= 0;
            enable      <= 0;
            haltreq     <= 0;
            haltstate   <= 0;
            stepreq     <= 0;

            hltrq_out_h <= 0;
            sack_out_h  <= 0;
        end
        if (init_in_h) begin
            d_out_h     <= 0;
            ssyn_out_h  <= 0;
        end

        // arm processor is writing one of the registers
        if (armwrite) begin
            case (armwaddr)
                1: begin
                    switches <= armwdata[15:00];
                end
                2: begin
                    enable  <= armwdata[31];
                    haltreq <= armwdata[30];
                    stepreq <= armwdata[28];
                    businit <= armwdata[27];
                end
            endcase
        end

        // pdp or something else is accessing a register
        else if (~ msyn_in_h) begin
            d_out_h    <= 0;
            ssyn_out_h <= 0;
        end else if (enable & (a_in_h[17:01] == 18'o777570 >> 1) & ~ ssyn_out_h) begin
            ssyn_out_h <= 1;
            if (c_in_h[1]) begin
                if (~ c_in_h[0] |   a_in_h[00]) lights[15:08] <= d_in_h[15:08];
                if (~ c_in_h[0] | ~ a_in_h[00]) lights[07:00] <= d_in_h[07:00];
            end else begin
                d_out_h <= switches;
            end
        end

        if (enable) begin
            case (haltstate)
                0: if (haltreq) begin
                    haltstate   <= 1;
                    hltrq_out_h <= 1;
                end
                1: if (~ hltgr_in_l) begin
                    halted      <= 1;
                    haltstate   <= 2;
                    hltrq_out_h <= 0;
                    sack_out_h  <= 1;
                end
                2: if (hltgr_in_l) begin
                    haltstate   <= 3;
                end
                3: if (~ haltreq) begin
                    halted      <= 0;
                    haltstate   <= 0;
                    sack_out_h  <= 0;
                end
            endcase
        end
    end
endmodule
