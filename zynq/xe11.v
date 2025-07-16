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

// PDP-11 DEUNA ethernet interface

module xe11
    #(parameter[17:00] ADDR=18'o774510,
      parameter[7:0] INTVEC=8'o120) (
    input CLOCK, RESET,

    input armwrite,
    input[1:0] armraddr, armwaddr,
    input[31:00] armwdata,
    output[31:00] armrdata,
    output armintrq,

    output intreq,
    output[7:0] irvec,
    input intgnt,
    input[7:0] igvec,

    input[17:00] a_in_h,
    input[1:0] c_in_h,
    input[15:00] d_in_h,
    input init_in_h,
    input msyn_in_h,

    output reg[15:00] d_out_h,
    output reg ssyn_out_h);

    reg enable, lastinit;
    reg[15:00] pcsr0, pcsr1, pcsr2, pcsr3;

    assign armrdata = (armraddr == 0) ? 32'h58451004 : // [31:16] = 'XE'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? { pcsr1, pcsr0 } :
                      (armraddr == 2) ? { pcsr3, pcsr2 } :
                          { enable, 5'b0, INTVEC, ADDR };

    // wake up arm (ZGINT_XE) whenever PCSR0[05] set or PCSR0[03:00] written by PDP
    // PCSR0[04] is hijacked to indicate when that happens (PDP always sees it as zero)
    assign armintrq = pcsr0[04];

    // keep INTR bit up to date
    always @(*) begin
        pcsr0[07] = pcsr0[15:08] != 0;
    end

    // interrupt pdp whenever done bit is set and interrupt enable is set
    // - level triggered (decnet sometimes hangs with edge triggered)
    assign intreq = pcsr0[07] & pcsr0[06];
    assign irvec  = INTVEC;

    wire writehi = ~ c_in_h[0] |   a_in_h[00];
    wire writelo = ~ c_in_h[0] | ~ a_in_h[00];

    always @(posedge CLOCK) begin
        if (init_in_h) begin
            if (RESET) begin
                enable <= 0;
                pcsr1[04] <= 0; // 0=DEUNA; 1=DELUA (settable by arm)
            end

            lastinit <= 1;
            pcsr0[15:08] <= 0;
            pcsr0[06:00] <= 0;
            pcsr1[15:05] <= 0;
            pcsr1[03:00] <= 0;
            pcsr2 <= 0;
            pcsr3 <= 0;

            d_out_h    <= 0;
            ssyn_out_h <= 0;
        end

        // init just released, flag arm to reset itself
        else if (lastinit) begin
            lastinit <= 0;
            pcsr0[05:04] <= 3;
        end

        // arm processor is writing one of the registers
        else if (armwrite) begin
            case (armwaddr)
                1: begin
                    pcsr1[15:07] <= armwdata[31:23];
                    pcsr1[04:00] <= armwdata[20:16];
                    pcsr0[15:08] <= pcsr0[15:08] |   armwdata[15:08];
                    pcsr0[05:04] <= pcsr0[05:04] & ~ armwdata[05:04];
                end
                3: begin
                    enable <= armwdata[31];
                end
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

                    // pdp writing command register
                    0: begin

                        // reset valid any time
                        // arm will clear it when complete
                        // hijack [04] to tell arm pcsr0[05] was set
                        if (writelo & d_in_h[05]) begin
                            pcsr0[15:08] <= 0;
                            pcsr0[06:00] <= 7'o060;
                            pcsr1[15:05] <= 0;
                            pcsr1[03:00] <= 0;
                        end

                        else begin

                            // top bits are write-1-to-clear
                            if (writehi) begin
                                pcsr0[15:08] <= pcsr0[15:08] & ~ d_in_h[15:08];
                            end

                            // bottom bits are normal read/write
                            // bits [03:00] written only if [06] doesn't change
                            // hijack [04] to tell arm pcsr0[03:00] was written
                            if (writelo) begin
                                pcsr0[06]    <= d_in_h[06];
                                if (pcsr0[06] == d_in_h[06]) begin
                                    pcsr0[04]    <= 1;
                                    pcsr0[03:00] <= d_in_h[03:00];
                                end
                            end
                        end
                    end

                    // pdp writing low address bits
                    2: begin
                        if (writehi) begin
                            pcsr2[15:08] <= d_in_h[15:08];
                        end
                        if (writelo) begin
                            pcsr2[07:01] <= d_in_h[07:01];
                        end
                    end

                    // pdp writing high address bits
                    3: begin
                        if (writelo) begin
                            pcsr3[01:00] <= d_in_h[01:00];
                        end
                    end
                endcase
            end else begin

                // pdp reading a register
                case (a_in_h[02:01])
                    0: d_out_h <= pcsr0 & 16'o177717;
                    1: d_out_h <= pcsr1;
                    2: d_out_h <= pcsr2;
                    3: d_out_h <= pcsr3;
                endcase
            end
        end
    end
endmodule
