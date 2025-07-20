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

// PDP-11 RH-11 disk interface

module rh11
    #(parameter[17:00] ADDR=18'o776700,
      parameter[7:0] INTVEC=8'o254) (
    input CLOCK, RESET,

    input armwrite,
    input[3:0] armraddr, armwaddr,
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
    reg[15:00] rpcs1, rpwc, rpba, rpdaarm, rpcs2, rpdsarm, rper1arm, rpas, rpla, rpdb, rpdtarm, rpsnarm, rpdcarm, rpccarm;
    reg[6:0] rpcs1s[7:0];
    reg[15:00] rpdas[7:0], rper1s[7:0], rpdts[7:0], rpsns[7:0], rpdcs[7:0], rpccs[7:0];
    reg[14:00] rpdss[7:0];
    reg[2:0] armds;
    reg[7:0] secpertrkm1;
    reg[19:00] qtrsectimem1, qtrsectimer;
    wire[2:0] pdpds = rpcs2[02:00];

    // rpcs1(partial),rhwc,rhba,rhcs2,rhdb - controller registers
    // all others - per-drive register
    // cheat with just one rpla

    // rpcs1[15:13] = common ([15:14] = automatically computed)
    // rpcs1[12]    = zero
    // rpcs1[11]    = DVA for drive armds
    // rpcs1[10:06] = common
    // rpcs1[05:00] = Fn,GO for drive armds

    // rpcs1s[pdpds][6]   = DVA for drive pdpds
    // rpcs1s[pdpds][5:0] = Fn,GO for drive pdpds

    assign armrdata = (armraddr ==  0) ? 32'h52483002 : // [31:16] = 'RH'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr ==  1) ? { rpwc,    rpcs1    } :
                      (armraddr ==  2) ? { rpdaarm, rpba     } :
                      (armraddr ==  3) ? { rpdsarm, rpcs2    } :
                      (armraddr ==  4) ? { rpas,    rper1arm } :
                      (armraddr ==  5) ? { rpdb,    rpla     } :
                      (armraddr ==  6) ? { armds, 1'b0, secpertrkm1, qtrsectimem1 } :
                      (armraddr ==  7) ? { rpsnarm, rpdtarm  } :
                      (armraddr ==  8) ? { rpccarm, rpdcarm  } :
                      (armraddr ==  9) ? { enable, fastio, 4'b0, INTVEC, ADDR } :
                      32'hDEADBEEF;

    // wake arm when command written or controller clear set
    assign armintrq = ~ rpcs1[07] | rpcs2[05];

    intreq rpintreq (
        .CLOCK    (CLOCK),
        .RESET    (init_in_h),
        .INTVEC   (INTVEC),
        .rirqlevl ((rpcs1[07] | (rpas != 0)) & rpcs1[06]),
        .xirqlevl (0),
        .intreq   (intreq),
        .irvec    (irvec),
        .intgnt   (intgnt),
        .igvec    (igvec)
    );

    // continuously update sector-under-head number
    always @(*) begin
        rpla[15:14] = 0;
        rpla[03:00] = 0;
    end
    always @(posedge CLOCK) begin
        if (qtrsectimer != qtrsectimem1) begin
            qtrsectimer <= qtrsectimer + 1;
        end else begin
            qtrsectimer <= 0;
            if (rpla[05:04] != 3) begin
                rpla[05:04] <= rpla[05:04] + 1;
            end else begin
                rpla[05:04] <= 0;
                if (rpla[13:06] != secpertrkm1) begin
                    rpla[13:06] <= rpla[15:06] + 1;
                end else begin
                    rpla[13:06] <= 0;
                end
            end
        end
    end

    // continuously update SC (special condition) and TRE (transfer error)
    always @(*) begin
        rpcs1[15] = rpcs1[14] | rpcs1[13] | (rpas != 0);
        rpcs1[14] = (rpcs2[15:08] != 0);
    end

    assign wrhi = ~ c_in_h[0] |   a_in_h[00];
    assign wrlo = ~ c_in_h[0] | ~ a_in_h[00];

    always @(posedge CLOCK) begin
        if (init_in_h) begin
            if (RESET) begin
                enable <= 0;
                fastio <= 0;
            end

            lastinit <= 1;

            rpcs1[13:00] <= 14'o00200;
            rpwc         <= 0;
            rpba         <= 0;
            rpdaarm      <= 0;
            rpcs2        <= 0;
            rpdsarm      <= 0;
            rper1arm     <= 0;
            rpas         <= 0;
            rpdb         <= 0;
            rpdtarm      <= 0;
            rpsnarm      <= 0;
            rpdcarm      <= 0;
            rpccarm      <= 0;

            d_out_h    <= 0;
            ssyn_out_h <= 0;
        end

        // arm processor is writing one of the registers
        else if (armwrite) begin
            case (armwaddr)
                1: begin
                    rpwc <= armwdata[31:16];
                    rpcs1[13]     <= armwdata[13];      // MCPE
                    rpcs1[11]     <= armwdata[11];      // DVA
                    rpcs1[09:07]  <= armwdata[09:07];   // A17,A16,RDY
                    rpcs1[00]     <= armwdata[00];      // GO
                    rpcs1s[armds][6] <= armwdata[11];   // per-drive DVA
                    rpcs1s[armds][0] <= armwdata[00];   // per-drive GO
                end
                2: begin
                    rpdas[armds]  <= armwdata[31:16];
                    rpba[15:01]   <= armwdata[15:01];
                    rpdaarm       <= armwdata[31:16];
                end
                3: begin
                    rpdss[armds]  <= armwdata[30:16];   // update per-drive drive status
                    rpas[armds]   <= armwdata[31];      // update per-drive atten summary bit
                    rpdsarm       <= armwdata[31:16];   // update drive status readback
                    rpcs2         <= armwdata[15:00];
                end
                4: begin
                    rper1s[armds] <= armwdata[15:00];
                    rper1arm      <= armwdata[15:00];
                end
                5: begin
                    rpdb          <= armwdata[31:16];
                end
                6: begin
                    secpertrkm1   <= armwdata[27:20];
                    qtrsectimem1  <= armwdata[19:00];

                    armds         <= armwdata[31:29];
                    rpcs1[11]     <= rpcs1s[armwdata[31:29]][11];
                    rpcs1[05:00]  <= rpcs1s[armwdata[31:29]][05:00];
                    rpdsarm       <= { rpas[armwdata[31:29]], rpdss[armwdata[31:29]] };
                    rpsnarm       <= rpsns[armwdata[31:29]];
                    rpdtarm       <= rpdts[armwdata[31:29]];
                    rpccarm       <= rpccs[armwdata[31:29]];
                    rpdcarm       <= rpdcs[armwdata[31:29]];
                end
                7: begin
                    rpsns[armds]  <= armwdata[31:16];
                    rpdts[armds]  <= armwdata[15:00];
                    rpsnarm       <= armwdata[31:16];
                    rpdtarm       <= armwdata[15:00];
                end
                8: begin
                    rpccs[armds]  <= armwdata[31:16];
                    rpdcs[armds]  <= armwdata[15:00];
                    rpccarm       <= armwdata[31:16];
                    rpdcarm       <= armwdata[15:00];
                end
                9: begin
                    enable        <= armwdata[31];
                    fastio        <= armwdata[30];
                end
            endcase
        end

        // init released:
        //  clear per-drive registers
        //  set controller clear to tell arm to reset
        else if (lastinit) begin
            rpcs1s[pdpds] <= 0;
            rpdss[pdpds]  <= 0;
            rpdts[pdpds]  <= 0;
            rpsns[pdpds]  <= 0;
            rpdcs[pdpds]  <= 0;
            rpccs[pdpds]  <= 0;
            rpcs2[02:00]  <= rpcs2[02:00] + 1;
            if (pdpds == 7) begin
                lastinit  <= 0;
                rpcs2[05] <= 1;
            end
        end

        // pdp or something else is accessing an i/o register
        else if (~ msyn_in_h) begin
            d_out_h    <= 0;
            ssyn_out_h <= 0;
        end else if (enable & (a_in_h[17:06] == ADDR[17:06]) & ~ ssyn_out_h) begin
            ssyn_out_h <= 1;
            if (c_in_h[1]) begin

                // pdp writing a register
                case (a_in_h[05:01])

                    // RPCS1
                     0: begin
                        if (wrhi) begin
                            if (d_in_h[14]) begin           // writing <14> = 1 clears controller error bits
                                rpcs1[13]    <= 0;          // clear MCPE
                                rpcs2[15:08] <= 0;          // clear DLT,TRE,PE,NED,NXM,PGE,MXF,MDPE
                            end
                            rpcs1[10:08] <= d_in_h[10:08];
                        end
                        if (wrlo) begin
                            if (d_in_h[00]) begin                       // check for GO bit
                                if (rpcs1[07] & rpdss[pdpds][07]) begin // make sure ctrlr and drive ready
                                    rpdss[pdpds][07] <= 0;  // clear DRY
                                    rpcs1[07] <= 0;         // clear RDY
                                    if (d_in_h[05]) begin   // check for data transfer command
                                        rpcs1[13] <= 0;     // clear MCPE
                                        rpcs2[15] <= 0;     // clear DLT
                                        rpcs2[14] <= 0;     // clear TRE
                                        rpcs2[13] <= 0;     // clear PE
                                        rpcs2[12] <= 0;     // clear NED
                                        rpcs2[11] <= 0;     // clear NXM
                                        rpcs2[10] <= 0;     // clear PGE
                                        rpcs2[09] <= 0;     // clear MXF
                                        rpcs2[08] <= 0;     // clear MDPE
                                    end
                                end else begin
                                    rpcs2[10] <= 1;         // ctrlr or drive bussy, set PGE
                                end
                            end
                            rpcs1[06] <= d_in_h[06];                    // IE - common to all drives
                            rpcs1s[pdpds][5:0] <= d_in_h[05:00];        // FC,GO - update what pdp is seeing
                            if (armds == pdpds) rpcs1[05:00] <= d_in_h[05:00]; // update what arm is seeing
                        end
                    end

                    // RPWC
                     1: begin
                        if (wrhi) rpwc[15:08]  <= d_in_h[15:08];
                        if (wrlo) rpwc[07:00]  <= d_in_h[07:00];
                    end

                    // RPBA
                     2: begin
                        if (wrhi) rpba[15:08]  <= d_in_h[15:08];
                        if (wrlo) rpba[07:01]  <= d_in_h[07:01];
                    end

                    // RPDA
                     3: begin
                        if (wrhi) rpdas[pdpds][15:08] <= d_in_h[15:08];
                        if (wrlo) rpdas[pdpds][07:00] <= d_in_h[07:00];
                        if (wrhi & (armds == pdpds)) rpdaarm[15:08] <= d_in_h[15:08];
                        if (wrlo & (armds == pdpds)) rpdaarm[07:00] <= d_in_h[07:00];
                    end

                    // RPCSR2
                     4: begin
                        if (wrlo) rpcs2[05:00] <= d_in_h[05:00];
                    end

                    // RPER1
                     6: begin
                        if (wrhi) rper1s[pdpds][15:08] <= d_in_h[15:08];
                        if (wrlo) rper1s[pdpds][07:00] <= d_in_h[07:00];
                        if (wrhi & (armds == pdpds)) rper1arm[15:08] <= d_in_h[15:08];
                        if (wrlo & (armds == pdpds)) rper1arm[07:00] <= d_in_h[07:00];
                    end

                    // RPAS
                     7: begin
                        if (wrlo) rpas[07:00]  <= rpas[07:00] & ~ d_in_h[07:00];
                    end

                    // RPDC
                    14: begin
                        if (wrhi) rpdcs[pdpds][15:08] <= d_in_h[15:08];
                        if (wrlo) rpdcs[pdpds][07:00] <= d_in_h[07:00];
                        if (wrhi & (armds == pdpds)) rpdcarm[15:08] <= d_in_h[15:08];
                        if (wrlo & (armds == pdpds)) rpdcarm[07:00] <= d_in_h[07:00];
                    end
                endcase
            end else begin

                // pdp reading a register
                case (a_in_h[05:01])
                     0: d_out_h <= { rpcs1[15:13], 1'b0, rpcs1s[pdpds][6], rpcs1[10:06], rpcs1s[pdpds][5:0] };
                     1: d_out_h <= rpwc;
                     2: d_out_h <= rpba;
                     3: d_out_h <= rpdas[pdpds];
                     4: d_out_h <= rpcs2;
                     5: d_out_h <= { rpas[pdpds], rpdss[pdpds] };
                     6: d_out_h <= rper1s[pdpds];
                     7: d_out_h <= rpas;
                     8: d_out_h <= rpla;
                     9: d_out_h <= rpdb;
                    11: d_out_h <= rpdts[pdpds];
                    12: d_out_h <= rpsns[pdpds];
                    13: d_out_h <= 16'o010000;  // RPOF<12>=1 : 16-bit word format
                    14: d_out_h <= rpdcs[pdpds];
                    15: d_out_h <= rpccs[pdpds];
                endcase
            end
        end
    end
endmodule
