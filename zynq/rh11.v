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

// Most of the RH-11 controller is implemented in the FPGA
// The ARM is used for the transfer parts once the implied seek is complete
// Mostly done because the PDP expects some of the comands be completed immediately
// ...without testing a ready bit

module rh11
    #(parameter[17:00] ADDR=18'o776700,
      parameter[7:0] INTVEC=8'o254) (
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

    reg enable, fastio;
    reg[15:00] rpwc, rpcs2;
    reg[10:04] rpla;
    reg[15:01] rpba;
    reg[15:06] rpcs1;
    reg[5:0] rpcs1s[7:0];
    reg[15:00] rpdas[7:0], rper1s[7:0], rpsns[7:0];
    reg[9:0] rpdcs[7:0], rpccs[7:0], rpccarm;
    reg[7:0] rpgs, rpas, secpertrkm1, fins;
    reg[14:00] qtrsectimer;
    wire[2:0] pdpds = rpcs2[02:00];
    wire[3:0] pdpdsp1 = { 1'b0, pdpds } + 1;

    reg[7:0] mols, wrls, dts, vvs, drys, errs, pips, lsts, sips;
    reg wrt, per, nxm, fer, xgo, wce, armctlclr, doctlclr;
    reg[2:0] drv, exeds;
    reg[4:0] sec, trk;
    reg[9:0] cyl;
    reg[10:00] seekctr;
    reg[5:0] sins[7:0];

    localparam[14:00] qtrsectimem1 = 18847;     // 753.92 uS / 4 - 1 (p43/v3-7)
    localparam[4:0]   maxsec = 21;              // 22 sectors per track
    localparam[4:0]   maxtrk = 18;              // 19 tracks per cylinder
    wire[9:0]         maxcyl = dts[exeds] ? 814 : 410;

    // rpcs1(partial),rhwc,rhba,rhcs2,rhdb - controller registers
    // all others - per-drive register
    // cheat with just one rpla

    // rpcs1[15:14] = common (automatically computed)
    // rpcs1[13]    = MCPE always 0
    // rpcs1[12]    = zero
    // rpcs1[11]    = DVA always 1 (we don't do dual porting)
    // rpcs1[10]    = PSEL always 0 (we don't do dual porting)
    // rpcs1[09:06] = common
    // rpcs1s[pdpds][5:0] = FC,GO for drive pdpds

    assign armrdata = (armraddr == 0) ? 32'h52482009 : // [31:16] = 'RH'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? { mols, wrls, dts, vvs } :
                      (armraddr == 2) ? { wrt, drv, cyl, rpcs1[09:08], rpba, rpcs2[03] } :
                      (armraddr == 3) ? { per, nxm, fer, xgo, wce, trk, armctlclr, sec, rpwc } :
                      (armraddr == 4) ? { enable, fastio, 4'b0, INTVEC, ADDR } :
                      (armraddr == 5) ? { 6'b0, rpccarm, drys, rpas } :
                      32'hDEADBEEF;

    // wake arm when transfer go bit set or controller clear set
    assign armintrq = xgo | armctlclr;

    // interrupt pdp when controller ready or any drive attention
    // - edge triggered by IE bit automatically turned off when granted
    assign intreq = rpcs1[06] & (rpcs1[07] | (rpas != 0));
    assign irvec  = INTVEC;

    // continuously update sector-under-head number
    // same for all drives
    always @(posedge CLOCK) begin
        if (qtrsectimer != qtrsectimem1) begin
            qtrsectimer <= qtrsectimer + 1;
        end else begin
            qtrsectimer <= 0;
            if (rpla[05:04] != 3) begin
                rpla[05:04] <= rpla[05:04] + 1;
            end else begin
                rpla[05:04] <= 0;
                if (rpla[10:06] != secpertrkm1) begin
                    rpla[10:06] <= rpla[10:06] + 1;
                end else begin
                    rpla[10:06] <= 0;
                end
            end
        end
    end

    // continuously update SC (special condition) and TRE (transfer error)
    always @(*) begin
        rpcs1[15] = rpcs1[14] | rpcs1[13] | (rpas != 0);
        rpcs1[14] = (rpcs2[15:08] != 0);
    end

    wire wrhi = ~ c_in_h[0] |   a_in_h[00];
    wire wrlo = ~ c_in_h[0] | ~ a_in_h[00];

    always @(posedge CLOCK) begin
        if (init_in_h) begin
            if (RESET) begin
                enable  <= 0;
                fastio  <= 0;
                exeds   <= 0;
                seekctr <= 0;
                mols    <= 0;
                vvs     <= 0;
                wrls    <= 0;
                dts     <= 0;
            end

            doctlclr   <= 1;
            rpcs2      <= 0;
            d_out_h    <= 0;
            ssyn_out_h <= 0;
        end

        // arm processor is writing one of the registers
        else if (armwrite) begin
            case (armwaddr)
                1: begin
                    mols <= armwdata[31:24];            // media onlines
                    wrls <= armwdata[23:16];            // write locks
                    dts  <= armwdata[15:08];            // drive types (0=RP04; 1=RP06)
                    vvs  <= vvs & ~ armwdata[07:00];    // volume valids
                end
                2: begin
                    cyl          <= armwdata[27:18];    // cylinder at end of transfer
                    rpcs1[09:08] <= armwdata[17:16];    // bus address at end of transfer
                    rpba         <= armwdata[15:01];
                end
                3: begin
                    if (armwdata[21]) begin
                        armctlclr <= 0;                 // arm is just clearing the RH3_CLR bit
                    end else begin
                        per  <= armwdata[31];           // parity error
                        nxm  <= armwdata[30];           // non-existant memory
                        fer  <= armwdata[29];           // file io error
                        xgo  <= armwdata[28];           // transfer go
                        wce  <= armwdata[27];           // write check error
                        trk  <= armwdata[26:22];        // track
                        sec  <= armwdata[20:16];        // sector
                        rpwc <= armwdata[15:00];        // word count
                    end
                end
                4: begin
                    enable <= armwdata[31];             // enable unibus access
                    fastio <= armwdata[30];             // disable seek delays
                end
                5: begin
                    rpas    <= rpas | armwdata[07:00];  // set ATA - attention active
                    rpccarm <= rpccs[armwdata[31:29]];  // current cylinder for drive [31:29]
                end
            endcase
        end

        // RH-11 does its own edge-triggered interrupts by clearing IE bit when interrupt granted
        else if (intgnt & rpcs1[06] & (igvec == irvec)) begin
            rpcs1[06] <= 0;
        end

        // init released / controller clear:
        //  clear per-drive registers
        //  set controller clear to tell arm to reset
        else if (doctlclr) begin
            rpcs1[09:06] <= 4'b0010;
            rpwc         <= 0;
            rpba         <= 0;
            rpgs         <= 0;
            rpas         <= 0;

            rpcs1s[pdpds] <= 0;
            rpdcs[pdpds]  <= 0;
            rpccs[pdpds]  <= 0;
            rpcs2[02:00]  <= rpcs2[02:00] + 1;

            drys <= 8'o377;
            errs <= 0;
            lsts <= 0;
            pips <= 0;
            sips <= 0;

            if (pdpds == 7) begin
                doctlclr  <= 0;     // we are done with our controller clear processing
                armctlclr <= 1;     // wake arm to do its controller clear processing
            end
        end

        // pdp or something else is accessing an i/o register
        else if (~ msyn_in_h & ssyn_out_h) begin
            d_out_h    <= 0;
            ssyn_out_h <= 0;
        end else if (enable & msyn_in_h & (a_in_h[17:06] == ADDR[17:06]) & ~ ssyn_out_h) begin
            ssyn_out_h <= 1;
            if (c_in_h[1]) begin

                // pdp writing a register
                case (a_in_h[05:01])

                    // RPCS1
                     0: begin
                        if (wrhi) begin
                            if (d_in_h[14]) begin           // writing <14> = 1 clears controller error bits
                                rpcs2[15:08] <= 0;          // clear DLT,WCE,UPE,NED,NXM,PGE,MXF,MDPE
                            end
                            rpcs1[09:08] <= d_in_h[09:08];  // A17,A16
                        end
                        if (wrlo) begin
                            rpcs1[06] <= d_in_h[06];                    // IE - common to all drives
                            if (d_in_h[00]) begin                       // check for GO bit
                                if (~ drys[pdpds] |                     // must always have drive ready
                                    d_in_h[05] & ~ rpcs1[07]) begin     // if data xfer, must have ctrlr ready
                                    rpcs2[10] <= 1;                     // ctrlr or drive bussy, set PGE
                                end else begin
                                    drys[pdpds]      <= 0;  // clear DRY
                                    if (d_in_h[05]) begin   // check for data transfer command
                                        rpcs1[07]    <= 0;  // clear RDY
                                        rpcs2[15:08] <= 0;  // clear DLT,WCE,UPE,NED,NXM,PGE,MXF,MDPE
                                    end
                                    rpcs1s[pdpds] <= d_in_h[05:00];     // FC,GO
                                    fins[pdpds]   <= 1;
                                end
                            end
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
                    end

                    // RPCS2
                     4: begin
                        if (wrlo) begin
                            if (d_in_h[05]) begin
                                doctlclr <= 1;
                                rpcs2    <= 0;
                            end else begin
                                rpcs2[13]    <= d_in_h[13];
                                rpcs2[09]    <= d_in_h[09];
                                rpcs2[05:00] <= d_in_h[05:00];
                            end
                        end
                    end

                    // RPER1
                     6: begin
                        if (wrhi) rper1s[pdpds][15:08] <= d_in_h[15:08];
                        if (wrlo) rper1s[pdpds][07:00] <= d_in_h[07:00];
                    end

                    // RPAS
                     7: begin
                        if (wrlo) rpas <= rpas & ~ d_in_h[07:00];
                    end

                    // RPDC
                    14: begin
                        if (wrhi) rpdcs[pdpds][9:8] <= d_in_h[09:08];
                        if (wrlo) rpdcs[pdpds][7:0] <= d_in_h[07:00];
                    end
                endcase
            end else begin

                // pdp reading a register
                case (a_in_h[05:01])
                     0: d_out_h <= { rpcs1[15:14], 3'b0010, rpcs1[09:06], rpcs1s[pdpds] };
                     1: d_out_h <= rpwc;
                     2: d_out_h <= { rpba, 1'b0 };
                     3: d_out_h <= rpdas[pdpds];
                     4: d_out_h <= rpcs2;
                     5: d_out_h <= { rpas[pdpds],               // ATA
                                     errs[pdpds],               // ERR
                                     pips[pdpds],               // PIP
                                     mols[pdpds],               // MOL
                                     wrls[pdpds],               // WRL
                                     lsts[pdpds],               // LST
                                     2'b01,                     // PGM,DPR
                                     drys[pdpds],               // DRY
                                     vvs[pdpds],                // VV
                                     6'b0 };
                     6: d_out_h <= rper1s[pdpds];
                     7: d_out_h <= { 8'b0, rpas };
                     8: d_out_h <= { 5'b0, rpla[10:06] ^ rpdas[pdpds][04:00], rpla[05:04], 4'b0 };
                    // RPDB,RPMR - read as zeroes
                    11: d_out_h <= dts[pdpds] ? 16'o020022 :    // RP06
                                                16'o020020;     // RP04
                    12: d_out_h <= { 4 { pdpdsp1 } };           // RPSN
                    13: d_out_h <= 16'o010000;                  // RPOF<12>=1 : 16-bit word format
                    14: d_out_h <= { 6'b0, rpdcs[pdpds] };
                    15: d_out_h <= { 6'b0, rpccs[pdpds] };
                    // RPER2,RPER3,RPEC1,RPEC2 - read as zeroes
                endcase
            end
        end

        // nothing else to do, execute functions with go bit set
        else begin

            // stepping for explicit or implied seek
            if (sips[exeds]) begin
                if (seekctr == 0) begin                         // step every 163.84 uS
                    if (rpccs[exeds] == rpdcs[exeds]) begin
                        sips[exeds]  <= 0;                      // - cyls match, done stepping
                    end else if (sins[exeds] != 41) begin       // - 7 mS for first step
                        sins[exeds]  <= sins[exeds] + 1;
                    end else if (rpccs[exeds] < rpdcs[exeds]) begin
                        rpccs[exeds] <= rpccs[exeds] + 1;       // - step inward one cylinder
                    end else begin
                        rpccs[exeds] <= rpccs[exeds] - 1;       // - step outward one cylinder
                    end
                end
            end

            // not stepping, process command if GO bit set
            else if (rpcs1s[exeds][00]) begin
                case (rpcs1s[exeds][05:01])

                    // 01 - nop
                    0: begin
                        rpcs1s[exeds][00] <= 0;                 // clear GO
                        drys[exeds]       <= 1;                 // set DRY
                    end

                    // 03 - unload
                    // 07 - recal
                    //   seek to cyl 0 but take 500 mS
                    1, 3: begin
                        if (fins[exeds]) begin
                            pips[exeds]  <= 1;                  // positioning in progress
                            rpccs[exeds] <= 1017;               // will take 167 mS to seek to 0 from here
                            fins[exeds]  <= 0;                  // init complete
                            sins[exeds]  <= 0;
                        end else if (seekctr == 0) begin        // step every 163.84 uS
                            if (rpccs[exeds] != 0) begin
                                rpccs[exeds] <= rpccs[exeds] - 1;
                            end else if (~ sins[exeds][01]) begin
                                rpccs[exeds] <= 1016;           // count 500 mS
                                sins[exeds]  <= sins[exeds] + 1;
                            end else begin
                                pips[exeds]       <= 0;         // positioning complete
                                rpcs1s[exeds][00] <= 0;         // clear GO
                                drys[exeds]       <= 1;         // set DRY
                                rpas[exeds]       <= 1;         // set ATA
                            end
                        end
                    end

                    // 05 - seek
                    // 31 - search
                    2, 12: begin
                        if (rpdcs[exeds] > maxcyl) begin
                            rper1s[exeds][10] <= 1;             // IAE - invalid (disk) address error
                            errs[exeds]       <= 1;             // ERR - error summary
                            drys[exeds]       <= 1;             // DRY - drive ready
                            rpas[exeds]       <= 1;             // ATA - attention
                            rpcs1s[exeds][00] <= 0;             // clear GO
                        end else if (rpccs[exeds] != rpdcs[exeds]) begin
                            pips[exeds]       <= 1;             // positioning in progress
                            sips[exeds]       <= 1;             // stepping in progress
                            sins[exeds]       <= 0;             // take 7 mS for first step
                        end else begin
                            pips[exeds]       <= 0;             // positioning complete
                            rpcs1s[exeds][00] <= 0;             // clear GO
                            drys[exeds]       <= 1;             // set DRY
                            rpas[exeds]       <= 1;             // set ATA
                        end
                    end

                    // 11 - drive clear
                    // 13 - release
                    4, 5: begin
                        rpas[exeds]       <= 0;                 // clear ATA
                        errs[exeds]       <= 0;                 // clear ERR
                        rper1s[exeds]     <= 0;                 // clear RPER1
                        rpcs1s[exeds][00] <= 0;                 // clear GO
                        drys[exeds]       <= 1;                 // set DRY
                    end

                    // 15 - offset
                    // 17 - return to centerline
                    //  10mS delay
                    6, 7: begin
                        if (fins[exeds]) begin
                            fins[exeds] <= 0;                   // init complete
                            pips[exeds] <= 1;                   // positioning in progress
                            sins[exeds] <= 0;                   // start counting 10mS
                        end else if (seekctr == 0) begin        // step every 163.84 uS
                            if (sins[exeds] != 60) begin
                                sins[exeds] <= sins[exeds] + 1;
                            end else begin
                                pips[exeds]       <= 0;         // positioning complete
                                rpcs1s[exeds][00] <= 0;         // clear GO
                                drys[exeds]       <= 1;         // set DRY
                                rpas[exeds]       <= 1;         // set ATA
                            end
                        end
                    end

                    // 21 - read-in-preset (p3-9)
                    8: begin
                        vvs[pdpds]        <= 1;                 // set VV - volume valid
                        rpdas[pdpds]      <= 0;                 // clear sector, track
                        rpdcs[pdpds]      <= 0;                 // clear cylinder
                        rpcs1s[exeds][00] <= 0;                 // clear GO
                        drys[exeds]       <= 1;                 // set DRY
                    end

                    // 23 - pack ack
                    9: begin
                        vvs[pdpds]        <= 1;                 // set VV - volume valid
                        rpcs1s[exeds][00] <= 0;                 // clear GO
                        drys[exeds]       <= 1;                 // set DRY
                    end

                    // 51 - write check
                    // 61 - write data
                    // 71 - read data
                    20, 24, 28: begin
                        if ((rpdcs[exeds] > maxcyl) | (rpdas[exeds][12:08] > maxtrk) | (rpdas[exeds][04:00] > maxsec)) begin
                            rper1s[exeds][10] <= 1;             // IAE - invalid (disk) address error
                            rpcs1[07]         <= 1;             // RDY - controller ready
                            errs[exeds]       <= 1;             // ERR - error summary
                            drys[exeds]       <= 1;             // DRY - drive ready
                            rpas[exeds]       <= 1;             // ATA - attention
                            rpcs1s[exeds][00] <= 0;             // clear GO
                        end else if (~ rpcs1s[exeds][3] & wrls[exeds]) begin
                            rper1s[exeds][11] <= 1;             // WLE - write lock error
                            rpcs1[07]         <= 1;             // RDY - controller ready
                            errs[exeds]       <= 1;             // ERR - error summary
                            drys[exeds]       <= 1;             // DRY - drive ready
                            rpas[exeds]       <= 1;             // ATA - attention
                            rpcs1s[exeds][00] <= 0;             // clear GO
                        end else if (~ xgo & ~ armctlclr) begin

                            //               xgo = 0 : transfer not currently in progress (either hasn't been started or has finished)
                            //         armctlclr = 0 : controller clear not currently in progress
                            // rpcs1s[exeds][00] = 1 : GO bit still set, keep processing command
                            //       fins[exeds] = 1 : command starting
                            //                     0 : command finished

                            // note: xgo = 1 means arm is busy doing transfer
                            //       armctlclr = 1 means arm is still processing controller clear
                            //         so delay starting transfer until it finishes

                            if (fins[exeds]) begin

                                // command starting, arm hasn't been told to start transfer yet
                                if (rpccs[exeds] != rpdcs[exeds]) begin
                                    sips[exeds] <= 1;           // cyl mismatch, do implied seek
                                    sins[exeds] <= 0;           // take 7 mS for first step
                                end else begin
                                    wrt <= ~ rpcs1s[exeds][3];  // doing write
                                    drv <= exeds;               // drive number
                                    cyl <= rpdcs[exeds];        // starting cylinder
                                    xgo <= 1;                   // tell arm to start transfer
                                    wce <= ~ rpcs1s[exeds][4];  // doing write check
                                    trk <= rpdas[exeds][12:08]; // starting track
                                    sec <= rpdas[exeds][04:00]; // starting sector
                                    fins[exeds] <= 0;
                                end
                            end else begin

                                // command finished, arm has completed transfer - post final status
                                rpdcs[exeds] <= cyl;            // final cylinder
                                if (cyl > maxcyl) begin
                                    lsts[exeds]  <= 1;          // last sector on disk
                                    rpccs[exeds] <= maxcyl;     // current cyl pegged at max
                                end else begin
                                    lsts[exeds]  <= 0;          // more sectors on disk
                                    rpccs[exeds] <= cyl;        // actual current cyl
                                end
                                rpdas[exeds][12:08] <= trk;     // final track
                                rpdas[exeds][04:00] <= sec;     // final sector
                                rpcs2[14] <= wce;               // write check error
                                rpcs2[13] <= per;               // memory parity error
                                rpcs2[11] <= nxm;               // non-existant memory
                                rper1s[exeds][06] <= fer;       // hard ecc error
                                rpcs1s[exeds][00] <= 0;         // clear GO
                                drys[exeds]       <= 1;         // set DRY
                                rpcs1[07]         <= 1;         // set RDY
                            end
                        end
                    end

                    // unknown/unsupported command
                    default: begin
                        if (rpcs1s[exeds][5]) rpcs1[07] <= 1;   // RDY - controller ready to do transfer
                        errs[exeds]   <= 1;                     // ERR - error summary
                        drys[exeds]   <= 1;                     // DRY - drive ready
                        rpas[exeds]   <= 1;                     // ATA - attention
                        rper1s[exeds] <= 1;                     // ILF - illegal function
                    end
                endcase
            end

            // seekctr rolls over every 163.84 uS
            // ...but with fastio set, 0.08 uS
            if (exeds == 7) seekctr <= fastio ? 0 : seekctr + 1;

            // do next drive next cycle
            exeds <= exeds + 1;
        end
    end
endmodule
