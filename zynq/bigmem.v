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

// use external memory

module bigmem (
    input CLOCK, RESET,

    input armwrite,
    input[2:0] armraddr, armwaddr,
    input[31:00] armwdata,
    output[31:00] armrdata,

    input[17:00] a_in_h,
    input[1:0] c_in_h,
    input[15:00] d_in_h,
    input init_in_h,
    input msyn_in_h,

    output reg[15:00] d_out_h,
    output reg ssyn_out_h,

    output reg[16:00] extmemaddr,
    output reg[17:00] extmemdout,
    input[17:00]      extmemdin,
    output reg        extmemenab,
    output reg[1:0]   extmemwena
);

    reg[63:00] enable;
    reg[2:0] armfunc, delayline;
    reg[17:00] armaddr;
    reg[15:00] armdata;

    assign armrdata = (armraddr == 0) ? 32'h424D2004 : // [31:16] = 'BM'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? { enable[31:00] } :
                      (armraddr == 2) ? { enable[63:32] } :
                      (armraddr == 3) ? { armfunc,   11'b0, armaddr } :
                      (armraddr == 4) ? { delayline, 13'b0, armdata } :
                      32'hDEADBEEF;

    always @(posedge CLOCK) begin
        if (init_in_h) begin
            if (RESET) begin
                enable <= 0;
            end
            delayline  <= 0;
            d_out_h    <= 0;
            extmemenab <= 0;
            extmemwena <= 0;
            ssyn_out_h <= 0;
        end

        // arm processor is writing one of the registers
        if (~ RESET & armwrite) begin
            case (armwaddr)
                1: begin
                    enable[31:00] <= armwdata[31:00];
                end
                2: begin
                    enable[61:32] <= armwdata[29:00];
                end
                3: begin
                    armfunc <= armwdata[31:29];
                    armaddr <= armwdata[17:00];
                end
                4: begin
                    armdata <= armwdata[15:00];
                end
            endcase
        end

        else if (~ init_in_h) case (delayline)

            // wait for something to do
            0: begin

                // arm is wanting to access the memory
                if (armfunc != 0) begin
                    delayline     <= 4;
                    extmemaddr    <= armaddr[17:01];
                    extmemdout    <= { 1'b0, armdata[15:08], 1'b0, armdata[07:00] };
                    extmemenab    <= 1;
                    extmemwena[1] <= armfunc[1];
                    extmemwena[0] <= armfunc[0];
                end

                // something on unibus is accessing a memory location
                else if (enable[a_in_h[17:12]] & msyn_in_h) begin
                    delayline  <= 1;
                    extmemaddr <= a_in_h[17:01];
                    extmemenab <= 1;
                    if (c_in_h[1]) begin
                        extmemdout    <= { 1'b0, d_in_h[15:08], 1'b0, d_in_h[07:00] };
                        extmemwena[1] <= ~ c_in_h[00] |   a_in_h[00];
                        extmemwena[0] <= ~ c_in_h[00] | ~ a_in_h[00];
                    end
                end
            end

            // finishing up pdp access
            3: begin
                if (msyn_in_h) begin
                    if (~ c_in_h[1] & ~ ssyn_out_h) begin
                        d_out_h[15:08] <= extmemdin[16:09];
                        d_out_h[07:00] <= extmemdin[07:00];
                    end
                    extmemenab <= 0;
                    extmemwena <= 0;
                    ssyn_out_h <= 1;
                end else begin
                    d_out_h    <= 0;
                    delayline  <= 0;
                    ssyn_out_h <= 0;
                end
            end

            // finishing up arm access
            6: begin
                if (armfunc[2]) begin
                    armdata[15:08] <= extmemdin[16:09];
                    armdata[07:00] <= extmemdin[07:00];
                end
                armfunc <= 0;
                delayline  <= 0;
                extmemenab <= 0;
                extmemwena <= 0;
            end

            default: delayline <= delayline + 1;
        endcase
    end
endmodule
