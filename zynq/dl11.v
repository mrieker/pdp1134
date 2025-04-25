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

// PDP-11 teletype interface

module dl11
    #(parameter[17:00] ADDR=18'o777560,
      parameter[7:0] INTVEC=8'o060,
      parameter[2:0] INTPRI=4) (
    input CLOCK, RESET,

    input armwrite,
    input[1:0] armraddr, armwaddr,
    input[31:00] armwdata,
    output[31:00] armrdata,

    output intreq,
    output[7:0] intvec,

    input[17:00] a_in_h,
    input[1:0] c_in_h,
    input[15:00] d_in_h,
    input init_in_h,
    input msyn_in_h,

    output reg[15:00] d_out_h,
    output reg ssyn_out_h);

    reg enable;
    reg[15:00] rcsr, rbuf, xcsr, xbuf;

    assign armrdata = (armraddr == 0) ? 32'h444C1001 : // [31:16] = 'DL'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? { rbuf, rcsr } :
                      (armraddr == 2) ? { xbuf, xcsr } :
                      { enable, 5'b0, INTVEC, ADDR };

    wire rirq = rcsr[07] & rcsr[06];
    wire xirq = xcsr[07] & xcsr[06];
    assign intreq = rirq | xirq;
    assign intvec = { INTVEC[7:3], ~ rirq, 2'b0 };

    always @(posedge CLOCK) begin
        if (init_in_h) begin
            if (RESET) begin
                enable <= 0;
            end
            rcsr       <= 0;
            xcsr       <= 0;
            d_out_h    <= 0;
            ssyn_out_h <= 0;
        end

        // arm processor is writing one of the registers
        else if (armwrite) begin
            case (armwaddr)
                1: { rbuf, rcsr } <= armwdata & 32'h00FF00C0;
                2: { xbuf, xcsr } <= armwdata & 32'h00FF00C0;
                3: enable <= armwdata[31];
            endcase
        end

        // pdp or something else is accessing an i/o register
        else if (~ msyn_in_h) begin
            d_out_h    <= 0;
            ssyn_out_h <= 0;
        end else if (enable & (a_in_h[17:03] == ADDR[17:03]) & ~ ssyn_out_h) begin
            ssyn_out_h <= 1;
            if (c_in_h[1]) begin
                case (a_in_h[02:01])
                    0: begin
                        if (~ c_in_h[0] |   a_in_h[00]) rcsr[15:08] <= 0;//d_in_h[15:08];
                        if (~ c_in_h[0] | ~ a_in_h[00]) rcsr[07:00] <= d_in_h[07:00] & 8'hC0;
                    end
                    1: begin
                        if (~ c_in_h[0] |   a_in_h[00]) rbuf[15:08] <= 0;//d_in_h[15:08];
                        if (~ c_in_h[0] | ~ a_in_h[00]) rbuf[07:00] <= d_in_h[07:00];
                    end
                    2: begin
                        if (~ c_in_h[0] |   a_in_h[00]) xcsr[15:08] <= 0;//d_in_h[15:08];
                        if (~ c_in_h[0] | ~ a_in_h[00]) xcsr[07:00] <= d_in_h[07:00] & 8'hC0;
                    end
                    3: begin
                        if (~ c_in_h[0] |   a_in_h[00]) xbuf[15:08] <= 0;//d_in_h[15:08];
                        if (~ c_in_h[0] | ~ a_in_h[00]) xbuf[07:00] <= d_in_h[07:00];
                        xcsr[07] <= 0;
                    end
                endcase
            end else begin
                case (a_in_h[02:01])
                    0: begin d_out_h <= rcsr; end
                    1: begin d_out_h <= rbuf; rcsr[07] <= 0; end
                    2: begin d_out_h <= xcsr; end
                    3: begin d_out_h <= xbuf; end
                endcase
            end
        end
    end
endmodule
