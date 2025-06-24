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

// PDP-11 8-line teletype interface

module dz11 (
    input CLOCK, RESET,

    input armwrite,
    input[2:0] armraddr, armwaddr,
    input[31:00] armwdata,
    output[31:00] armrdata,

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
    output reg pb_out_h,
    output reg ssyn_out_h);

    wire[15:00] csr, rbr;

    reg enable;
    reg[17:00] addres;
    reg[7:0] intvec;

    reg[7:0] kbful, prful;
    reg[7:0] prbuf[7:0];

    // receiver 'silo'
    reg[7:0] rxenab;                        //DZ_LPR[12] receiver on
    reg[3:0] silorem;
    reg[4:0] siloctr;
    reg[10:00] silo[3:0];
    wire[3:0] siloins = silorem + siloctr[3:0];
    wire siloful = siloctr[4];
    wire[10:00] rbuf = silo[silorem];       //DZ_RBUF[10:00] corresponding keyboard char

    // transmit 'silo'
    wire[7:0] txemp = txenab & ~ prful;     // which enabled transmit lines are empty
    reg[7:0] txenab;                        //DZ_TCR[07:00] line enable bits
    reg[2:0] txline;                        // line the pdp can transmit on

    // 16 bits per line from ARM perspective:
    //  <15> R-O : set: arm has written kb char to <07:00> and pdp hasn't read it yet; clr: arm can write a kb char to <07:00>
    //  <14> R-O : set: pdp has written pr char to <07:00> and arm hasn't read it yet; clr: pdp can write a pr char to <07:00>
    //  <13> W-O : set: set <15> indicating this write is writing a kb char to <07:00>; clr: do nothing
    //  <12> W-O : set: clear <14> indicating arm is printing the char; clr: do nothing
    //  <11:08>  : unused
    //  <07:00> R-O : printer character from pdp
    //          W-O : keyboard character from arm
    // this alows two processes to write to their own 16-bit half of a 32-bit word without messing with thhe other's half
    // as they must set <13> or <12> on their half, always leaving them clear on the other half.

    assign armrdata = (armraddr == 0) ? 32'h445A2002 : // [31:16] = 'DZ'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? { enable, 5'b0, intvec, addres } :
                      (armraddr == 2) ? { rbr, csr } :
                      (armraddr == 3) ? { txenab, 7'b0, siloctr, silorem, rxenab } :
                      (armraddr == 4) ? { kbful[1], prful[1], 6'b0, prbuf[1],  kbful[0], prful[0], 6'b0, prbuf[0] } :
                      (armraddr == 5) ? { kbful[3], prful[3], 6'b0, prbuf[3],  kbful[2], prful[2], 6'b0, prbuf[2] } :
                      (armraddr == 6) ? { kbful[5], prful[5], 6'b0, prbuf[5],  kbful[4], prful[4], 6'b0, prbuf[4] } :
                      (armraddr == 7) ? { kbful[7], prful[7], 6'b0, prbuf[7],  kbful[6], prful[6], 6'b0, prbuf[6] } :
                      32'hDEADBEEF;

    // control/status register bits
    wire csr_15_trdy  = txemp[txline];
    reg  csr_14_tie;
    wire csr_13_sa    = siloful;
    reg  csr_12_sae;
    wire[10:08] csr_1008_tline = txline;
    wire csr_07_rdone = siloctr != 0;
    reg  csr_06_rie;
    reg  csr_05_mse;
    reg  csr_04_clr;
    reg  csr_03_mai;

    assign csr = { csr_15_trdy, csr_14_tie, csr_13_sa, csr_12_sae, 1'b0, csr_1008_tline,
                    csr_07_rdone, csr_06_rie, csr_05_mse, csr_04_clr, csr_03_mai, 3'b0 };

    assign rbr = { csr_07_rdone, 4'b0, rbuf };

    wire[2:0] armevn = { armwaddr[1:0], 1'b0 };
    wire[2:0] armodd = { armwaddr[1:0], 1'b1 };

    // level-style interrupt
    assign intreq = (csr_14_tie & csr_15_trdy) | (csr_06_rie & (csr_12_sae ? csr_13_sa : csr_07_rdone));
    assign irvec  = intvec;

    always @(posedge CLOCK) begin
        if (init_in_h) begin
            if (RESET) begin
                addres <= 0;
                enable <= 0;
                intvec <= 0;
            end
            csr_14_tie <= 0;
            csr_12_sae <= 0;
            csr_06_rie <= 0;
            csr_05_mse <= 0;
            csr_04_clr <= 0;
            csr_03_mai <= 0;
            kbful      <= 0;
            prful      <= 0;
            rxenab     <= 0;
            siloctr    <= 0;
            silorem    <= 0;
            txenab     <= 0;
            txline     <= 0;
            d_out_h    <= 0;
            pb_out_h   <= 0;
            ssyn_out_h <= 0;
        end

        // arm processor is writing one of the registers
        else if (armwrite) begin
            case (armwaddr)
                1: begin
                    enable <= armwdata[31];
                    intvec <= armwdata[25:18] &  8'o374;
                    addres <= armwdata[17:00] & 18'o777770;
                end

                4,5,6,7: begin
                    case ({ armwdata[29:28], armwdata[13:12] })

                        // [12]: arm has completed printing armevn character
                        //       if loopback turned on, put in silo
                        1: begin
                            prful[armevn] <= 0;
                            if (csr_03_mai & ~ siloful & rxenab[armevn]) begin
                                silo[siloins] <= { armevn, prbuf[armevn] };
                                siloctr <= siloctr + 1;
                            end
                        end

                        // [13]: arm has a keyboard character to put in silo
                        2: begin
                            if (~ siloful & rxenab[armevn]) begin
                                silo[siloins] <= { armevn, armwdata[07:00] };
                                siloctr <= siloctr + 1;
                            end
                        end

                        // [28]: arm has completed printing armodd character
                        //       if loopback turned on, put in silo
                        4: begin
                            prful[armodd] <= 0;
                            if (csr_03_mai & ~ siloful & rxenab[armodd]) begin
                                silo[siloins] <= { armodd, prbuf[armodd] };
                                siloctr <= siloctr + 1;
                            end
                        end

                        // [29]: arm has a keyboard character to put in silo
                        8: begin
                            if (~ siloful & rxenab[armodd]) begin
                                silo[siloins] <= { armodd, armwdata[07:00] };
                                siloctr <= siloctr + 1;
                            end
                        end
                    endcase
                end
            endcase
        end

        // pdp or something else is accessing an i/o register
        else if (~ msyn_in_h & ssyn_out_h) begin
            d_out_h    <= 0;
            pb_out_h   <= 0;
            ssyn_out_h <= 0;
        end else if (enable & (a_in_h[17:03] == addres[17:03]) & msyn_in_h & ~ ssyn_out_h) begin
            ssyn_out_h <= 1;
            if (c_in_h[1]) begin
                case (a_in_h[02:01])

                    0: begin
                        if (~ c_in_h[0] |   a_in_h[00]) begin
                            csr_14_tie = d_in_h[14];
                            csr_12_sae = d_in_h[12];
                        end
                        if (~ c_in_h[0] | ~ a_in_h[00]) begin
                            csr_06_rie = d_in_h[06];
                            csr_05_mse = d_in_h[05];
                            csr_04_clr = d_in_h[04];
                            csr_03_mai = d_in_h[03];
                        end
                    end

                    1: begin
                        if (~ c_in_h[0] | ~ a_in_h[00]) begin
                            rxenab[d_in_h[02:00]] <= d_in_h[12];    // LPR: save 'receiver on' bit for the line
                        end
                    end

                    2: begin
                        if (~ c_in_h[0] | ~ a_in_h[00]) txenab <= d_in_h[07:00];
                    end

                    3: begin
                        if (~ c_in_h[0] | ~ a_in_h[00]) begin
                            prbuf[txline] <= d_in_h[07:00];         // print char on line 'txline'
                            prful[txline] <= 1;
                        end
                    end
                endcase
            end else begin

                // pdp reading a register
                case (a_in_h[02:01])
                    0: begin d_out_h <= csr; end
                    1: begin
                        d_out_h <= rbr;
                        if (rbr[15]) begin
                            silorem <= silorem + 1;
                            siloctr <= siloctr - 1;
                        end
                    end
                    2: begin d_out_h <= { 8'b0, txenab }; end
                    3: begin d_out_h <= 0; end
                endcase
            end
        end

        // process CLR bit
        else if (csr_04_clr) begin
            csr_14_tie <= 0;
            csr_12_sae <= 0;
            csr_06_rie <= 0;
            csr_05_mse <= 0;
            csr_04_clr <= 0;
            csr_03_mai <= 0;
            kbful      <= 0;
            prful      <= 0;
            rxenab     <= 0;
            siloctr    <= 0;
            silorem    <= 0;
            txenab     <= 0;
            txline     <= 0;
        end

        // look for an empty enabled transmit slot if we don't have one already
        else if (~ csr_15_trdy) txline <= txline + 1;
    end
endmodule
