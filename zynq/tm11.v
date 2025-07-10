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

// PDP-11 TM11/TU10 tape interface

module tm11
    #(parameter[17:00] ADDR=18'o772520,
      parameter[7:0] INTVEC=8'o224) (
    input CLOCK, RESET,

    input armwrite,
    input[2:0] armraddr, armwaddr,
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

    reg enable, fastio, lastinit;
    reg[15:00] mts, mtc, mtbrc, mtcma, mtd, mtrd;
    reg[7:0] sels, bots, wrls, rews, turs;

    assign armrdata = (armraddr == 0) ? 32'h544D2004 : // [31:16] = 'TM'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? { mtc,   mts   } :
                      (armraddr == 2) ? { mtcma, mtbrc } :
                      (armraddr == 3) ? { mtrd,  mtd   } :
                      (armraddr == 4) ? { enable, fastio, 4'b0, INTVEC, ADDR } :
                      (armraddr == 5) ? { bots, wrls, rews, turs } :
                      (armraddr == 6) ? { 24'b0, sels } :
                      32'hDEADBEEF;

    // wake up arm (ZGINT_TM) whenever go or power clear bits are set
    assign armintrq = mtc[00] | mtc[12];

    // trigger pdp interrupt request when rewind on currently selected drive completes
    // controller must be idle (mtc[07] set) and interrupts enabled (mtc[06] set)
    wire rewdone = mts[01] & armwrite & (armwaddr == 5) & ~ armwdata[{2'b01,mtc[10:08]}];

    // interrupt pdp whenever done bit is set and interrupt enable is set
    // also trigger when rewind on currently selected drive completes
    // - edge triggered
    intreq mtintreq (
        .CLOCK    (CLOCK),
        .RESET    (init_in_h),
        .INTVEC   (INTVEC),
        .rirqlevl (mtc[07] & mtc[06] & ~ rewdone),
        .xirqlevl (0),
        .intreq   (intreq),
        .irvec    (irvec),
        .intgnt   (intgnt),
        .igvec    (igvec)
    );

    always @(*) begin

        // keep composite error bit up-to-date
        mtc[15] = mts[15:07] != 0;

        // keep drive status bits up-to-date
        mts[06] = sels[mtc[10:08]];
        mts[05] = bots[mtc[10:08]];
        mts[04] = 0;
        mts[03] = 0;
        mts[02] = wrls[mtc[10:08]];
        mts[01] = rews[mtc[10:08]];
        mts[00] = turs[mtc[10:08]];
    end

    // toggle mtrd[15] at 10kHz rate with 50% duty cycle
    reg[18:00] twentykhz;
    always @(posedge CLOCK) begin
        if (RESET) begin
            twentykhz <= 0;
            mtrd[15]  <= 0;
        end else if (twentykhz != 499999) begin
            twentykhz <= twentykhz + 1;
        end else begin
            twentykhz <= 0;
            mtrd[15]  <= ~ mtrd[15];
        end
    end

    wire writehi = ~ c_in_h[0] |   a_in_h[00];
    wire writelo = ~ c_in_h[0] | ~ a_in_h[00];

    always @(posedge CLOCK) begin
        if (init_in_h) begin
            if (RESET) begin
                enable <= 0;
                fastio <= 0;
            end

            lastinit   <= 1;
            mts[15:07] <= 0;            // clear error bits
            mtc[14:00] <= 0;            // clear command bits
            d_out_h    <= 0;
            ssyn_out_h <= 0;
        end

        // init just released, flag arm to reset itself
        else if (lastinit) begin
            lastinit   <= 0;
            mtc[14:00] <= 15'o10200;    // power clear, alerts arm to abandon i/o
        end

        // arm processor is writing one of the registers
        else if (armwrite) begin
            case (armwaddr)
                1: begin
                    mtc[14:00]  <= armwdata[30:16];
                    mts[15]     <= armwdata[15] | mts[15];
                    mts[14:07]  <= armwdata[14:07];
                end
                2: begin
                    mtcma       <= armwdata[31:16];
                    mtbrc       <= armwdata[15:00];
                end
                3: begin
                    mtd         <= armwdata[31:16];
                    mtrd[14:00] <= armwdata[14:00];
                end
                4: begin
                    enable      <= armwdata[31];
                    fastio      <= armwdata[30];
                end
                5: begin
                    bots <= armwdata[31:24];
                    wrls <= armwdata[23:16];
                    rews <= armwdata[15:08];
                    turs <= armwdata[07:00];
                end
                6: begin
                    sels <= armwdata[07:00];
                end
            endcase
        end

        // pdp or something else is accessing an i/o register
        else if (~ msyn_in_h) begin
            d_out_h    <= 0;
            ssyn_out_h <= 0;
        end else if (enable & (a_in_h[17:04] == ADDR[17:04]) & ~ ssyn_out_h) begin
            ssyn_out_h <= (a_in_h[03:02] != 3);

            if (c_in_h[1]) begin
                case (a_in_h[03:01])

                    // pdp writing command register
                    1: begin

                        // power clear valid any time
                        // arm will clear it when complete
                        // sets controller ready immediately so pdp can write new command immediately
                        if (writehi & d_in_h[12]) begin
                            mts[15:07] <= 0;
                            mtc[14:08] <= d_in_h[14:08];
                            mtc[07]    <= 1;
                            mtc[00]    <= 0;
                            if (writelo) begin
                                mtc[06:01] <= d_in_h[06:01];
                            end
                        end

                        else if (~ mtc[07]) begin
                            mts[15] <= 1;   // writing command register while controller busy
                        end else begin

                            // controller idle, write upper byte
                            // error bit is calculated above with always statement
                            // - so don't write it directly here
                            // don't allow clearing power clear bit so arm will see if was previously set
                            // only arm should clear power clear bit
                            // setting power clear was handled above
                            if (writehi) begin
                                mtc[14:13] <= d_in_h[14:13];
                                mtc[11:08] <= d_in_h[11:08];
                            end

                            // write lower byte
                            if (writelo) begin

                                // save new bits
                                mtc[06:00] <= d_in_h[06:00];

                                // see if starting a command
                                if (~ mtc[00] & d_in_h[00]) begin

                                    // clear controller ready
                                    mtc[07] <= 0;

                                    // clear error bits
                                    // this clears mtc[15] immediately
                                    mts[15:07] <= 0;

                                    // clear tape unit ready
                                    turs[writehi?d_in_h[10:08]:mtc[10:08]] <= 0;
                                end
                            end
                        end
                    end

                    // pdp writing byte record counter
                    2: begin
                        if (writehi) begin
                            mtbrc[15:08] <= d_in_h[15:08];
                        end
                        if (writelo) begin
                            mtbrc[07:00] <= d_in_h[07:00];
                        end
                    end

                    // pdp writing current memory address
                    3: begin
                        if (writehi) begin
                            mtcma[15:08] <= d_in_h[15:08];
                        end
                        if (writelo) begin
                            mtcma[07:01] <= d_in_h[07:01];
                            mtcma[00]    <= 0;
                        end
                    end
                endcase
            end else begin

                // pdp reading a register
                case (a_in_h[03:01])
                    0: d_out_h <= mts;
                    1: d_out_h <= mtc   & 16'o167776;
                    2: d_out_h <= mtbrc;
                    3: d_out_h <= mtcma & 16'o177776;
                    4: d_out_h <= mtd;
                    5: d_out_h <= mtrd;
                endcase
            end
        end
    end
endmodule
