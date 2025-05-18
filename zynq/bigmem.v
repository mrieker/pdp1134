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

// Provide up to 248KB memory using external block RAM module
// 62 enable bits provide independent enables for each 4KB

module bigmem (
    input CLOCK,
    input powerup,  // fpga is powering up
    input fpgaoff,  // powerup | (fpgamode == FM_OFF)
    input businit,  // fpgaoff | (unibus init)

    input armwrite,
    input[2:0] armraddr, armwaddr,
    input[31:00] armwdata,
    output[31:00] armrdata,

    input[17:00] a_in_h,
    input[1:0] c_in_h,
    input[15:00] d_in_h,
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
    reg[2:0] armfunc;
    reg[3:0] armcount, delayline;
    reg[17:00] armaddr;
    reg[15:00] armdata;
    reg armpehi, armpelo;

    assign armrdata = (armraddr == 0) ? 32'h424D2005 : // [31:16] = 'BM'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? { enable[31:00] } :
                      (armraddr == 2) ? { enable[63:32] } :
                      (armraddr == 3) ? { armfunc, 1'b0, armcount, 6'b0, armaddr } :
                      (armraddr == 4) ? { delayline, 10'b0, armpehi, armpelo, armdata } :
                      32'hDEADBEEF;

    wire perdinhi = ~ extmemdin[17] ^ extmemdin[16] ^ extmemdin[15] ^ extmemdin[14] ^ extmemdin[13] ^ extmemdin[12] ^ extmemdin[11] ^ extmemdin[10] ^ extmemdin[09];
    wire perdinlo = ~ extmemdin[08] ^ extmemdin[07] ^ extmemdin[06] ^ extmemdin[05] ^ extmemdin[04] ^ extmemdin[03] ^ extmemdin[02] ^ extmemdin[01] ^ extmemdin[00];

    wire pdpparhi = ~ d_in_h[15] ^ d_in_h[14] ^ d_in_h[13] ^ d_in_h[12] ^ d_in_h[11] ^ d_in_h[10] ^ d_in_h[09] ^ d_in_h[08];
    wire pdpparlo = ~ d_in_h[07] ^ d_in_h[06] ^ d_in_h[05] ^ d_in_h[04] ^ d_in_h[03] ^ d_in_h[02] ^ d_in_h[01] ^ d_in_h[00];

    wire armparhi = ~ armpehi ^ armdata[15] ^ armdata[14] ^ armdata[13] ^ armdata[12] ^ armdata[11] ^ armdata[10] ^ armdata[09] ^ armdata[08];
    wire armparlo = ~ armpelo ^ armdata[07] ^ armdata[06] ^ armdata[05] ^ armdata[04] ^ armdata[03] ^ armdata[02] ^ armdata[01] ^ armdata[00];

    always @(posedge CLOCK) begin
        if (powerup) begin
            armcount   <= 0;
            armfunc    <= 0;
            enable     <= 0;
        end
        if (fpgaoff | ~ msyn_in_h & (delayline < 5)) begin
            delayline  <= 0;
            extmemenab <= 0;
            extmemwena <= 0;
        end
        if (~ msyn_in_h) begin
            d_out_h    <= 0;
            ssyn_out_h <= 0;
        end

        // arm processor is writing one of the registers
        if (~ powerup & armwrite) begin
            case (armwaddr)
                1: begin
                    enable[31:00] <= armwdata[31:00];
                end
                2: begin
                    enable[62:32] <= armwdata[30:00];
                end
                3: begin
                    armfunc <= armwdata[31:29];
                    armaddr <= armwdata[17:00];
                end
                4: begin
                    armdata <= armwdata[15:00];
                    armpelo <= armwdata[16];
                    armpehi <= armwdata[17];
                end
            endcase
        end

        if (~ powerup) case (delayline)

            // wait for something to do
            0: begin

                // arm is wanting to access the memory
                //  armfunnc = 4: read word
                //             3: write word
                //             2: write upper byte
                //             1: write lower byte
                if (~ armwrite & (armfunc != 0)) begin
                    delayline  <= 5;
                    extmemaddr <= armaddr[17:01];
                    extmemdout[17]    <= armparhi;
                    extmemdout[16:09] <= armdata[15:08];
                    extmemdout[08]    <= armparlo;
                    extmemdout[07:00] <= armdata[07:00];
                    extmemenab <= 1;
                    extmemwena <= armfunc[1:0];
                end

                // something on unibus is accessing a memory location
                else if (enable[a_in_h[17:12]] & msyn_in_h) begin
                    delayline  <= 1;
                    extmemaddr <= a_in_h[17:01];
                    extmemenab <= 1;
                    if (c_in_h[1]) begin
                        extmemdout[17]    <= pdpparhi;
                        extmemdout[16:09] <= d_in_h[15:08];
                        extmemdout[08]    <= pdpparlo;
                        extmemdout[07:00] <= d_in_h[07:00];
                        extmemwena[1] <= ~ c_in_h[00] |   a_in_h[00];
                        extmemwena[0] <= ~ c_in_h[00] | ~ a_in_h[00];
                    end
                end
            end

            // 1, 2, 3: delay for pdp access

            // finishing up pdp access
            4: begin
                if (~ msyn_in_h) begin
                    d_out_h   <= 0;
                    delayline <= 0;
                end else if (~ c_in_h[1] & extmemenab) begin
                    d_out_h[15:08] <= extmemdin[16:09];
                    d_out_h[07:00] <= extmemdin[07:00];
                end
                extmemenab <= 0;
                extmemwena <= 0;
                ssyn_out_h <= msyn_in_h;
            end

            // 5, 6, 7: delay for pdp access

            // finishing up arm access
            8: begin
                if (armfunc[2]) begin
                    armdata[15:08] <= extmemdin[16:09];
                    armdata[07:00] <= extmemdin[07:00];
                    armpehi <= perdinhi;
                    armpelo <= perdinlo;
                end
                armcount   <= armcount + 1;
                armfunc    <= 0;
                delayline  <= 0;
                extmemenab <= 0;
                extmemwena <= 0;
            end

            default: delayline <= delayline + 1;
        endcase
    end
endmodule
