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

// Implementation of PDP-11/34 processor

module sim1134 (
    input CLOCK,
    input RESET,

    input turbo,
    output[15:00] r0out, pcout, psout,
    output[5:0] stout,
    output waiting,

    input bus_ac_lo_in_l,
    input bus_bbsy_in_l,
    input[7:4] bus_br_in_l,
    input bus_dc_lo_in_l,
    input bus_intr_in_l,
    input bus_npr_in_l,
    input bus_sack_in_l,
    input bus_hltrq_in_l,

    input[17:00] bus_a_in_l,
    input[1:0] bus_c_in_l,
    input[15:00] bus_d_in_l,
    input bus_init_in_l,
    input bus_pa_in_l,
    input bus_pb_in_l,
    input bus_msyn_in_l,
    input bus_ssyn_in_l,

    output reg[17:00] bus_a_out_l,
    output reg[1:0] bus_c_out_l,
    output[15:00] bus_d_out_l,
    output reg bus_bbsy_out_l,
    output reg bus_hltrq_out_l,
    output reg bus_init_out_l,
    output reg bus_msyn_out_l,
    output reg bus_ssyn_out_l,

    output reg[7:4] bus_bg_out_h,
    output reg bus_npg_out_h,
    output reg bus_hltgr_out_h
);

    reg[5:0] state;
    localparam[5:0] S_HALT      = 00;
    localparam[5:0] S_HALT2     = 01;
    localparam[5:0] S_FETCH     = 02;
    localparam[5:0] S_FETCH2    = 03;
    localparam[5:0] S_DECODE    = 04;
    localparam[5:0] S_EXHALT    = 05;
    localparam[5:0] S_EXWAIT    = 06;
    localparam[5:0] S_EXRESET   = 07;
    localparam[5:0] S_BRANCH    = 08;
    localparam[5:0] S_EXSOB     = 09;
    localparam[5:0] S_GETSRC    = 10;
    localparam[5:0] S_WAITSRC   = 11;
    localparam[5:0] S_WAITSRC2  = 12;
    localparam[5:0] S_GETDST    = 13;
    localparam[5:0] S_WAITDST   = 14;
    localparam[5:0] S_WAITDST2  = 15;
    localparam[5:0] S_EXECDD    = 16;
    localparam[5:0] S_EXECDD2   = 17;
    localparam[5:0] S_EXECJSR   = 18;
    localparam[5:0] S_EXECRTS   = 19;
    localparam[5:0] S_EXECRTS2  = 20;
    localparam[5:0] S_EXRTIT    = 21;
    localparam[5:0] S_EXRTIT2   = 22;
    localparam[5:0] S_EXRTIT3   = 23;
    localparam[5:0] S_EXMUL     = 24;
    localparam[5:0] S_EXMUL2    = 25;
    localparam[5:0] S_EXMUL3    = 26;
    localparam[5:0] S_EXMUL4    = 27;
    localparam[5:0] S_EXDIV     = 28;
    localparam[5:0] S_EXDIV3    = 29;
    localparam[5:0] S_EXDIV4    = 30;
    localparam[5:0] S_EXDIV5    = 31;
    localparam[5:0] S_EXDIV6    = 32;
    localparam[5:0] S_EXMFPI    = 33;
    localparam[5:0] S_EXMFPI2   = 34;
    localparam[5:0] S_EXMTPI    = 36;
    localparam[5:0] S_EXMTPI2   = 37;
    localparam[5:0] S_SERVICE   = 39;
    localparam[5:0] S_NPG       = 40;
    localparam[5:0] S_INTR      = 41;
    localparam[5:0] S_TRAP      = 44;
    localparam[5:0] S_TRAP2     = 45;
    localparam[5:0] S_TRAP3     = 46;
    localparam[5:0] S_TRAP4     = 47;
    localparam[5:0] S_TRAP5     = 48;
    localparam[5:0] S_EXTRAP    = 49;
    localparam[5:0] S_EXASH     = 50;
    localparam[5:0] S_EXASH2    = 51;
    localparam[5:0] S_EXASH3    = 52;
    localparam[5:0] S_EXASH4    = 53;
    localparam[5:0] S_EXASHC    = 54;
    localparam[5:0] S_EXASHC2   = 55;
    localparam[5:0] S_EXASHC3   = 56;
    localparam[5:0] S_EXASHC4   = 57;
    localparam[5:0] S_EXMARK    = 60;
    localparam[5:0] S_EXCCS     = 61;
    localparam[5:0] S_EXMARK2   = 63;

    localparam[15:00] YELSTKLIM = 16'o000400;

    // [15:14] = current mode
    // [13:12] = previous mode
    // [07:05] = priority
    // [04] = trace
    // [03] = N
    // [02] = Z
    // [01] = V
    // [00] = C
    // 4-22/p68
    reg[15:00] psw;

    // R00..R05 = R0..R5
    // R06 = KSP
    // R07 = PC
    // R10..R15 = unused
    // R16 = USP
    // R17 = unused
    reg[15:00] gprs[15:00];
    function [3:0] gprx (input[1:0] mode, input[2:0] regn);
        gprx = { (regn == 6) & mode[1], regn };
    endfunction
    wire[3:0] cspgprx = gprx (psw[15:14], 6);    // access current mode stack pointer

    assign waiting = state == S_EXWAIT;
    assign r0out = gprs[0];
    assign pcout = gprs[7];
    assign psout = psw;
    assign stout = state;

    reg[7:0] trapvec;
    localparam[7:0] T_CPUERR  = 8'o004;
    localparam[7:0] T_ILLINST = 8'o010;
    localparam[7:0] T_BPTRACE = 8'o014;
    localparam[7:0] T_IOT     = 8'o020;
    localparam[7:0] T_PWRFAIL = 8'o024;
    localparam[7:0] T_EMT     = 8'o030;
    localparam[7:0] T_TRAP    = 8'o034;
    localparam[7:0] T_PARERR  = 8'o114;
    localparam[7:0] T_MMUTRAP = 8'o250;

    reg[15:00] cpuerr, instreg;

    wire iHALT  = (instreg == 0);
    wire iWAIT  = (instreg == 1);
    wire iRTI   = (instreg == 2);
    wire iBPT   = (instreg == 3);
    wire iIOT   = (instreg == 4);
    wire iRESET = (instreg == 5);
    wire iRTT   = (instreg == 6);
    wire iJMP   = (instreg[15:06] == 10'o0001) & (instreg[05:03] != 0);
    wire iRTS   = (instreg[15:03] == 13'o0020);
    wire iSWAB  = (instreg[15:06] == 10'o0003);
    wire iJSR   = (instreg[15:09] ==   7'o004) & (instreg[05:03] != 0);
    wire iCLRb  = (instreg[14:06] ==   9'o050);
    wire iCOMb  = (instreg[14:06] ==   9'o051);
    wire iINCb  = (instreg[14:06] ==   9'o052);
    wire iDECb  = (instreg[14:06] ==   9'o053);
    wire iNEGb  = (instreg[14:06] ==   9'o054);
    wire iADCb  = (instreg[14:06] ==   9'o055);
    wire iSBCb  = (instreg[14:06] ==   9'o056);
    wire iTSTb  = (instreg[14:06] ==   9'o057);
    wire iRORb  = (instreg[14:06] ==   9'o060);
    wire iROLb  = (instreg[14:06] ==   9'o061);
    wire iASRb  = (instreg[14:06] ==   9'o062);
    wire iASLb  = (instreg[14:06] ==   9'o063);
    wire iMARK  = (instreg[15:06] == 10'o0064);
    wire iMTPS  = (instreg[15:06] == 10'o1064); // move byte to PSW[07:00] (p 4-22)
    wire iMFPID = (instreg[14:06] ==   9'o065);
    wire iMTPID = (instreg[14:06] ==   9'o066);
    wire iSXT   = (instreg[15:06] == 10'o0067);
    wire iMFPS  = (instreg[15:06] == 10'o1067); // move byte from PSW[07:00] (p 4-21)

    wire iMOVb  = (instreg[14:12] ==  3'o1);
    wire iMOVB  = (instreg[15:12] == 4'o11);
    wire iCMPb  = (instreg[14:12] ==  3'o2);
    wire iBITb  = (instreg[14:12] ==  3'o3);
    wire iBICb  = (instreg[14:12] ==  3'o4);
    wire iBISb  = (instreg[14:12] ==  3'o5);
    wire iADD   = (instreg[15:12] == 4'o06);
    wire iSUB   = (instreg[15:12] == 4'o16);

    wire iMUL   = (instreg[15:09] ==  7'o070);
    wire iDIV   = (instreg[15:09] ==  7'o071);
    wire iASH   = (instreg[15:09] ==  7'o072);
    wire iASHC  = (instreg[15:09] ==  7'o073);
    wire iXOR   = (instreg[15:09] ==  7'o074);
    wire iSOB   = (instreg[15:09] ==  7'o077);
    wire iEMT   = (instreg[15:08] == 8'h88);
    wire iTRAP  = (instreg[15:08] == 8'h89);
    wire iBXX   = (instreg[14:11] == 0) & ((instreg[15:08] & 8'o207) != 0);
    wire iCCS   = (instreg[15:05] == 5);

    wire needtoreaddst  = ~ iMOVb & ~ iCLRb & ~ iMFPS & ~ iSXT;
    wire needtowritedst = ~ iCMPb & ~ iBITb & ~ iTSTb & ~ iMTPS & ~ iMUL & ~ iDIV & ~ iASH & ~ iASHC;
    wire byteinstr      = instreg[15] & ~ iSUB;
    wire[15:00] oneval  = byteinstr ? 256 : 1;
    wire[3:0] dstgprx   = gprx (psw[15:14], instreg[02:00]);
    wire[3:0] srcgprx   = gprx (psw[15:14], instreg[08:06]);
    wire[3:0] srcgprx1  = gprx (psw[15:14], instreg[08:06] | 1);

    reg[2:0] getopaddr;
    reg[5:0] getopmode;
    wire[15:00] getopinc = (byteinstr & ~ getopmode[3] & (getopmode[2:1] != 3)) ? 1 : 2;
    wire[3:0]   getgprx  = gprx (psw[15:14], getopmode[2:0]);

    wire intreq4 = ~ bus_br_in_l[4] & (psw[07:05] < 4);
    wire intreq5 = ~ bus_br_in_l[5] & (psw[07:05] < 5);
    wire intreq6 = ~ bus_br_in_l[6] & (psw[07:05] < 6);
    wire intreq7 = ~ bus_br_in_l[7] & (psw[07:05] < 7);
    wire intrqst = intreq4 | intreq5 | intreq6 | intreq7;

    reg[15:00] mmupars[15:00];
    reg[15:00] mmupdrs[15:00];
    reg[15:00] mmr0, mmr2;
    wire mmropen = (mmr0[15:13] == 0);

    wire[7:0] instrapvec = iBPT ? 8'o14 : iIOT ? 8'o20 : iEMT ? 8'o30 : iTRAP ? 8'o34 : 8'o00;

    reg[15:00] parentry, pdrentry, readdata, virtaddr, writedata;
    reg[17:00] physaddr;
    reg[1:0] memfunc, memmode;
    reg membyte, signbit;
    reg[2:0] rwstate;
    reg[9:0] rwdelay;
    reg[19:00] resdelay;
    reg[15:00] dstval, result, srcval;
    reg[31:00] product;
    reg[3:0] counter;
    reg[2:0] intrdelay;
    reg aclock, haltck, halted, nopushpspc, traceck, trapping, yellowck;

    localparam[1:0] MF_RD = 1;  // do a DATI cycle
    localparam[1:0] MF_WR = 2;  // do a DATO[B] cycle
    localparam[1:0] MF_RM = 3;  // do a DATIP cycle

    wire[31:00] multstep = { 1'b0, product[31:16] + (srcval[00] ? dstval : 16'b0), product[15:01] };

    // unibus data output
    reg[15:00] cer_d_out_l, cpu_d_out_l, gpr_d_out_l, mmr_d_out_l, mmv_d_out_l, psw_d_out_l;
    assign bus_d_out_l = cer_d_out_l & cpu_d_out_l & gpr_d_out_l & mmr_d_out_l & mmv_d_out_l & psw_d_out_l;

    // index into mmupars,mmupdrs for unibus access
    //  usr registers: 7776xx
    //  knl registers: 7723xx
    wire[3:0] mmuprbi = { bus_a_in_l[06], ~ bus_a_in_l[03:01] };

    // index into mmupars,mmupdrs for computing physical address from virtual address
    wire[3:0] mmuprxi = { memmode[1], virtaddr[15:13] };

    // branch condition true
    wire[2:0] brindx = { instreg[15], instreg[10:09] };
    reg brtemp, brtrue;
    always @(*) begin
        case (brindx)
            0: brtemp = 0;
            1: brtemp =  ~ psw[2];                      // BNE
            2: brtemp =  ~ psw[3] ^ psw[1];             // BGE
            3: brtemp = (~ psw[3] ^ psw[1]) & ~ psw[2]; // BGT
            4: brtemp =  ~ psw[3];                      // BPL
            5: brtemp =  ~ psw[2] & ~ psw[0];           // BHI
            6: brtemp =  ~ psw[1];                      // BVC
            7: brtemp =  ~ psw[0];                      // BCC/BHIS
        endcase
        brtrue = brtemp ^ instreg[08];
    end

    wire resetting = RESET | ~ bus_ac_lo_in_l & ~ bus_dc_lo_in_l;

    // processor main loop
    always @(posedge CLOCK) begin
        if (resetting) begin
            bus_a_out_l     <= 18'o777777;
            bus_c_out_l     <= 3;
            bus_msyn_out_l  <= 1;
            bus_ssyn_out_l  <= 1;
            bus_bg_out_h    <= 0;
            bus_bbsy_out_l  <= 1;
            bus_init_out_l  <= 0;
            bus_npg_out_h   <= 0;
            bus_hltrq_out_l <= 1;
            bus_hltgr_out_h <= 0;

            cer_d_out_l <= 16'o177777;
            cpu_d_out_l <= 16'o177777;
            gpr_d_out_l <= 16'o177777;
            mmr_d_out_l <= 16'o177777;
            mmv_d_out_l <= 16'o177777;
            psw_d_out_l <= 16'o177777;

            aclock      <= 0;           // don't check AC_LO when powering up
            cpuerr      <= 0;           // haven't had any trap 4s yet
            getopaddr   <= 0;           // not getting operand address
            haltck      <= 1;           // check for halt when we get going
            halted      <= 0;           // starting out by reading power-up vector
            intrdelay   <= 0;           // set up to be ready to receive interrupt vector
            memfunc     <= 0;           // not doing any memory function
            mmr0        <= 0;           // not using mmu to begin with
            nopushpspc  <= 1;           // don't push PC/PS when doing power-up trap
            psw         <= 16'o340;     // start in kernel mode with ints disabled
            resdelay    <= 0;           // not doing RESET instruction
            rwstate     <= 0;           // not accessing memory
            state       <= S_SERVICE;   // start out doing power-up trap after releasing bus_init_out_l
            traceck     <= 1;           // check T-bit
            trapping    <= 0;           // not currently doing a trap
            trapvec     <= T_PWRFAIL;   // start out doing power-up trap
            yellowck    <= 0;           // don't check yellow stack
        end

        //////////////////////////////////
        //  LOWEST LEVEL STATE MACHINE  //
        //////////////////////////////////

        // being active blocks higher level state machines

        // read from or write to unibus
        //  input:
        //   membyte   = 0: word; 1: byte
        //   memfunc   = MF_RD  do a DATI cycle
        //               MF_WR  do a DATO[B] cycle
        //               MF_RM  do a DATIP cycle
        //   memmode   = processor mode for va->pa translation
        //   virtaddr  = virtual address being accessed
        //   writedata = write data for write cycles
        //  output:
        //   memfunc   = cleared to 0 when cycle complete
        //   readdata  = read data for read cycles
        //  other:
        //   jams state = S_SERVICE with trapvec set if error
        else if (memfunc != 0) begin
            case (rwstate)

                // getting started
                0: begin

                    // check for accessing word at an odd address
                    if (~ membyte & virtaddr[00]) begin
                        cpuerr[06] <= 1;
                        memfunc    <= 0;
                        state      <= S_SERVICE;
                        trapvec    <= T_CPUERR;
                    end

                    // if mmu enabled, read page registers then check access
                    else if (mmr0[00]) begin
                        parentry   <= mmupars[mmuprxi];
                        pdrentry   <= mmupdrs[mmuprxi];
                        rwstate    <= 1;
                    end

                    // mmu disabled, compute physical address then start access
                    else begin
                        physaddr   <= { { 2 { virtaddr[15] & virtaddr[14] & virtaddr[13] } }, virtaddr };
                        rwstate    <= 2;
                    end
                end

                // check page access
                1: begin

                    // access codes 0,2 mean no access to the page
                    // also, we only do kernel and user modes
                    if (~ pdrentry[01] | (memmode == 1) | (memmode == 2)) begin
                        if (mmr0[15:13] == 0) begin
                            mmr0[15]    <= 1;  // abort-non-resident
                            mmr0[06:05] <= memmode;
                            mmr0[03:01] <= virtaddr[15:13];
                        end
                        memfunc <= 0;
                        rwstate <= 0;
                        state   <= S_SERVICE;
                        trapvec <= T_MMUTRAP;
                    end

                    // check page length violation
                    else if (pdrentry[03] ? (virtaddr[12:06] < pdrentry[14:08]) : (virtaddr[12:06] > pdrentry[14:08])) begin
                        if (mmr0[15:13] == 0) begin
                            mmr0[14]    <= 1;  // abort-page-length
                            mmr0[06:05] <= memmode;
                            mmr0[03:01] <= virtaddr[15:13];
                        end
                        memfunc <= 0;
                        rwstate <= 0;
                        state   <= S_SERVICE;
                        trapvec <= T_MMUTRAP;
                    end

                    // access codes 1 means read-only access to the page
                    else if (~ pdrentry[02] & ((memfunc == MF_RM) | (memfunc == MF_WR))) begin
                        if (mmr0[15:13] == 0) begin
                            mmr0[13]    <= 1;  // abort-read-only
                            mmr0[06:05] <= memmode;
                            mmr0[03:01] <= virtaddr[15:13];
                        end
                        memfunc <= 0;
                        rwstate <= 0;
                        state   <= S_SERVICE;
                        trapvec <= T_MMUTRAP;
                    end

                    // mmu allows access
                    else begin
                        rwstate <= 2;               // continue on doing memory access
                        if ((memfunc == MF_RM) | (memfunc == MF_WR)) mmupdrs[mmuprxi][06] <= 1;
                    end

                    // compute physical address
                    physaddr <= { parentry[11:00] + { 5'b0, virtaddr[12:06] }, virtaddr[05:00] };
                end

                // hold off if SSYN,BBSY (still busy from an old DMA)
                2: begin
                    if (bus_ssyn_in_l & bus_bbsy_in_l) begin
                        bus_a_out_l    <= ~ physaddr;
                        bus_bbsy_out_l <= 0;
                        bus_c_out_l[1] <= ~  (memfunc == MF_WR);
                        bus_c_out_l[0] <= ~ ((memfunc == MF_WR) ? membyte : (memfunc == MF_RM));
                        rwstate        <= 3;
                        rwdelay        <= 0;
                        if (memfunc == MF_WR) begin
                            cpu_d_out_l[15:08] <= ~ (physaddr[00] ? writedata[07:00] : writedata[15:08]);
                            cpu_d_out_l[07:00] <= ~ (physaddr[00] ? writedata[15:08] : writedata[07:00]);
                        end
                    end
                end

                // give 150nS for signals to flow out the bus and be decoded
                3: begin
                    if ((rwdelay == 15) | turbo) rwstate <= 4;
                                  else rwdelay <= rwdelay + 1;
                end

                // assert MSYN to say it's all valid now
                4: begin
                    bus_msyn_out_l <= 0;
                    rwdelay        <= 0;
                    rwstate        <= 5;
                end

                // wait up to 10uS for SSYN meaning the slave did it
                5: begin
                    if (~ bus_ssyn_in_l) begin
                        rwdelay        <= 0;
                        rwstate        <= 6;
                    end else if (rwdelay == 1000) begin
                        bus_msyn_out_l <= 1;
                        cpuerr[04]     <= 1;
                        rwdelay        <= 0;
                        rwstate        <= 7;
                        state          <= S_SERVICE;
                        trapvec        <= T_CPUERR;
                    end else begin
                        rwdelay        <= rwdelay + 1;
                    end
                end

                // wait 150nS for read data
                // complete write immediately
                6: begin
                    if ((memfunc == MF_WR) | (rwdelay == 15) | turbo) begin
                        if ((memfunc == MF_RM) | (memfunc == MF_RD)) begin
                            readdata[15:08] <= ~ (physaddr[00] ? bus_d_in_l[07:00] : bus_d_in_l[15:08]);
                            readdata[07:00] <= ~ (physaddr[00] ? bus_d_in_l[15:08] : bus_d_in_l[07:00]);
                            if (bus_pa_in_l & ~ bus_pb_in_l) begin
                                state   <= S_SERVICE;
                                trapvec <= T_PARERR;
                            end
                        end
                        bus_msyn_out_l <= 1;
                        rwdelay        <= 0;
                        rwstate        <= 7;
                    end else begin
                        rwdelay        <= rwdelay + 1;
                    end
                end

                // let address, data and function linger for 80nS after dropping MSYN
                // also wait for slave to drop SSYN
                7: begin
                    if ((rwdelay != 8) & ~ turbo) begin
                        rwdelay        <= rwdelay + 1;
                    end else if (bus_ssyn_in_l) begin
                        bus_a_out_l    <= 18'o777777;
                        bus_bbsy_out_l <= 1;
                        bus_c_out_l    <= 3;
                        cpu_d_out_l    <= 16'o177777;
                        memfunc        <= 0;
                        rwstate        <= 0;
                    end
                end
            endcase
        end

        ///////////////////////////////
        //  MID-LEVEL STATE MACHINE  //
        ///////////////////////////////

        // being active blocks higher level state machines

        // get non-register operand address
        //  input:
        //   getopaddr = 1 : start computing
        //   getopmode = 6-bit operand address mode & register
        //   getopinc  = amount to increment/decrement register by for modes 2,3,4,5
        //  output:
        //   getopaddr = 0 : address computed
        //   virtaddr  = operand address
        else if (getopaddr != 0) begin
            if (trapvec != 0) begin
                getopaddr <= 0;
            end case (getopmode[5:3])

                // simple indirect - use registers contents as address
                1: begin
                    getopaddr <= 0;
                    virtaddr  <= gprs[getgprx];
                end

                // autoincrement possibly with indirect
                2, 3: begin
                    case (getopaddr)
                        1: begin
                            gprs[getgprx] <= gprs[getgprx] + getopinc;
                            virtaddr      <= gprs[getgprx];
                            if (getopmode[3]) begin
                                getopaddr <= 2;
                                membyte   <= 0;
                                memfunc   <= MF_RD;
                                memmode   <= psw[15:14];
                            end else begin
                                getopaddr <= 0;
                            end
                        end
                        2: begin
                            getopaddr <= 0;
                            virtaddr  <= readdata;
                        end
                    endcase
                end

                // autodecrement possibly with indirect
                4, 5: begin
                    case (getopaddr)
                        1: begin
                            gprs[getgprx] <= gprs[getgprx] - getopinc;
                            virtaddr      <= gprs[getgprx] - getopinc;
                            if (getopmode[3]) begin
                                getopaddr <= 2;
                                membyte   <= 0;
                                memfunc   <= MF_RD;
                                memmode   <= psw[15:14];
                            end else begin
                                getopaddr <= 0;
                            end
                        end
                        2: begin
                            getopaddr <= 0;
                            virtaddr  <= readdata;
                        end
                    endcase
                end

                // indexed possibly with indirect
                6, 7: begin
                    case (getopaddr)
                        1: begin
                            getopaddr <= 2;
                            gprs[7]   <= gprs[7] + 2;
                            membyte   <= 0;
                            memfunc   <= MF_RD;
                            memmode   <= psw[15:14];
                            virtaddr  <= gprs[7];
                        end
                        2: begin
                            virtaddr <= readdata + gprs[getgprx];
                            if (getopmode[3]) begin
                                getopaddr <= 3;
                                membyte   <= 0;
                                memfunc   <= MF_RD;
                                memmode   <= psw[15:14];
                            end else begin
                                getopaddr <= 0;
                            end
                        end
                        3: begin
                            getopaddr <= 0;
                            virtaddr  <= readdata;
                        end
                    endcase
                end
            endcase
        end

        ///////////////////////////////////
        //  HIGHEST LEVEL STATE MACHINE  //
        ///////////////////////////////////

        else begin
            case (state)

                // assert bus_hltgr_out_h to let front panel know we are halted
                // wait for bus_hltrq_in_l asserted if not already
                // ...so we know front panel knows we are halted
                S_HALT: begin
                    halted <= 1;
                    bus_hltgr_out_h <= 1;
                    if (~ bus_hltrq_in_l) begin
                        state <= S_HALT2;
                    end
                end

                // wait for front panel to negate bus_hltrq_in_l
                // if being jammed by our own bus_hltrq_out_l, only resetting will recover
                S_HALT2: begin
                    if (~ bus_sack_in_l) begin
                        bus_hltgr_out_h <= 0;
                    end else if (bus_hltrq_in_l) begin
                        bus_hltgr_out_h <= 0;
                        haltck <= 0;
                        halted <= 0;
                        state  <= S_SERVICE;
                    end
                end

                // start reading the instruction from memory
                S_FETCH: begin
                    membyte  <= 0;
                    memfunc  <= MF_RD;
                    memmode  <= psw[15:14];
                    state    <= S_FETCH2;
                    virtaddr <= gprs[7];
                    yellowck <= 0;
                    if (mmropen) begin
                        mmr2 <= gprs[7];
                    end
                end

                // got instruction from memory, save and decode
                S_FETCH2: begin
                    gprs[7] <= gprs[7] + 2;
                    instreg <= readdata;
                    state   <= S_DECODE;
                end

                S_DECODE: begin

                    // have both SS and DD fields
                    if (iMOVb | iCMPb | iBITb | iBICb | iBISb | iADD | iSUB) begin
                        state <= S_GETSRC;
                    end
                    else
                    // have DD field, may also have R field
                    if (iCLRb  | iCOMb  | iINCb | iDECb | iNEGb | iADCb |
                        iSBCb  | iTSTb  | iROLb | iRORb | iASRb | iASLb |
                        iMFPID | iMTPID | iSXT  | iMUL  | iDIV  | iASH  |
                        iASHC  | iXOR   | iJSR  | iJMP  | iSWAB | iMFPS | iMTPS) begin
                        state <= S_GETDST;
                    end

                    // misc
                    else if (iRTS) state <= S_EXECRTS;
                    else if (iEMT | iTRAP | iBPT | iIOT) state <= S_EXTRAP;
                    else if (iBXX) state <= S_BRANCH;
                    else if (iRTI | iRTT) state <= S_EXRTIT;
                    else if (iSOB) state <= S_EXSOB;
                    else if (iMARK) state <= S_EXMARK;
                    else if (iCCS) state <= S_EXCCS;
                    else if (iHALT) state <= S_EXHALT;
                    else if (iWAIT) state <= S_EXWAIT;
                    else if (iRESET) state <= S_EXRESET;

                    // illegal opcode
                    else begin
                        state   <= S_SERVICE;
                        trapvec <= T_ILLINST;
                    end
                end

                S_EXHALT: begin
                    if (psw[15:14] == 0) begin
                        bus_hltrq_out_l <= 0;
                        state      <= S_HALT;
                    end else begin
                        cpuerr[07] <= 1;
                        state      <= S_SERVICE;
                        trapvec    <= T_CPUERR;
                    end
                end

                // wait for interrupt or halt
                // process nprs meanwhile
                S_EXWAIT: begin
                    if ((psw[4] | ~ bus_hltrq_in_l | intrqst) & ~ bus_npg_out_h) begin
                        state <= S_SERVICE;
                    end else begin
                        bus_npg_out_h <= ~ bus_npr_in_l;
                    end
                end

                // reset bus - 10mS
                S_EXRESET: begin
                    if (resdelay != 1000000) begin
                        bus_init_out_l <= 0;
                        resdelay       <= resdelay + 1;
                    end else begin
                        resdelay       <= 0;
                        state          <= S_SERVICE;
                    end
                end

                // marked stack return (v 4-57/p 99)
                S_EXMARK: begin
                    gprs[7]       <= gprs[5];
                    gprs[cspgprx] <= gprs[7] + { 9'b0, instreg[05:00], 1'b0 } + 2;
                    membyte       <= 0;
                    memfunc       <= MF_RD;
                    memmode       <= psw[15:14];
                    state         <= S_EXMARK2;
                    virtaddr      <= gprs[7] + { 9'b0, instreg[05:00], 1'b0 };
                end

                S_EXMARK2: begin
                    gprs[5]       <= readdata;
                    state         <= S_SERVICE;
                end

                // set or clear condition code(s)
                S_EXCCS: begin
                    if (instreg[00]) psw[00] <= instreg[04];
                    if (instreg[01]) psw[01] <= instreg[04];
                    if (instreg[02]) psw[02] <= instreg[04];
                    if (instreg[03]) psw[03] <= instreg[04];
                    state <= S_SERVICE;
                end

                // if branch condition is true, add displacement to PC
                S_BRANCH: begin
                    if (brtrue) gprs[7] <= gprs[7] + { { 8 { instreg[07] } }, instreg[06:00], 1'b0 };
                    state <= S_SERVICE;
                end

                // subtract one and branch if non-zero
                S_EXSOB: begin
                    gprs[srcgprx] <= gprs[srcgprx] - 1;
                    if (gprs[srcgprx] != 1) gprs[7] <= gprs[7] - { 9'b0, instreg[05:00], 1'b0 };
                    state <= S_SERVICE;
                end

                // start getting source operand
                S_GETSRC: begin
                    if (instreg[11:09] == 0) begin
                        srcval    <= byteinstr ? { gprs[srcgprx][7:0], 8'b0 } : gprs[srcgprx];
                        state     <= S_GETDST;
                    end else begin
                        getopaddr <= 1;
                        getopmode <= instreg[11:06];
                        state     <= S_WAITSRC;
                    end
                end

                // wait for source operand address to be calculated
                // then start reading source operand value
                S_WAITSRC: begin
                    membyte <= byteinstr;
                    memfunc <= MF_RD;
                    memmode <= psw[15:14];
                    state   <= S_WAITSRC2;
                end

                // wait for source operand value to be read from memory
                S_WAITSRC2: begin
                    srcval <= byteinstr ? { readdata[7:0], 8'b0 } : readdata;
                    state  <= S_GETDST;
                end

                // start getting destination operand
                S_GETDST: begin
                    if (instreg[05:03] == 0) begin
                             if (iMFPID) state <= S_EXMFPI;
                        else if (iMTPID) state <= S_EXMTPI;
                        else begin
                            dstval <= byteinstr ? { gprs[dstgprx][7:0], 8'b0 } : gprs[dstgprx];
                            state  <= S_EXECDD;
                        end
                    end else begin
                        getopaddr  <= 1;
                        getopmode  <= instreg[05:00];
                        state      <= S_WAITDST;
                    end
                end

                // destination operand address now available
                S_WAITDST: begin
                    if (iJMP) begin
                        gprs[7]   <= virtaddr;              // put dst address in PC
                        state     <= S_SERVICE;             // end of instruction
                    end
                    else if (iJSR) begin
                        gprs[cspgprx] <= gprs[cspgprx] - 2; // decrement current stack pointer
                        membyte   <= 0;                     // doing word-sized mem op
                        memfunc   <= MF_WR;                 // start writing to memory
                        memmode   <= psw[15:14];            // current mode mem op
                        readdata  <= virtaddr;              // save dst address where where it will be safe
                        state     <= S_EXECJSR;             // finish JSR next
                        virtaddr  <= gprs[cspgprx] - 2;     // address to write to
                        writedata <= gprs[srcgprx];         // save source register to stack
                    end
                    else if (iMFPID) state <= S_EXMFPI;     // MFPI/MFPD
                    else if (iMTPID) state <= S_EXMTPI;     // MTPI/MTPD
                    else begin
                        membyte   <= byteinstr;
                        if (needtoreaddst &   needtowritedst) memfunc <= MF_RM;
                        if (needtoreaddst & ~ needtowritedst) memfunc <= MF_RD;
                        memmode   <= psw[15:14];
                        state     <= S_WAITDST2;
                    end
                end

                // destination operand value has been read from memory
                S_WAITDST2: begin
                    dstval <= byteinstr ? { readdata[7:0], 8'b0 } : readdata;
                    state  <= S_EXECDD;
                end

                // do arithmetic to compute new destination value
                //    dstval = old dst value if any (byte value in top 8 bits, bottom 8 bits zero)
                //    srcval = src value if any (byte value in top 8 bits, bottom 8 bits zero)
                //  virtaddr = dst virtual address if any
                S_EXECDD: begin
                         if (iMUL)   state <= S_EXMUL;      // MUL
                    else if (iDIV)   state <= S_EXDIV;      // DIV
                    else if (iASH)   state <= S_EXASH;      // ASH
                    else if (iASHC)  state <= S_EXASHC;     // ASHC
                    else if (iMTPS) begin                   // MTPS
                        psw[03:00] <= dstval[11:08];
                        if (psw[15:14] == 0) psw[07:05] <= dstval[15:13];
                        state <= S_SERVICE;
                    end
                    else begin
                        state <= S_EXECDD2;
                             if (iMOVb) result <= srcval;
                        else if (iCMPb) result <= srcval - dstval;
                        else if (iBITb) result <= srcval & dstval;
                        else if (iBICb) result <= dstval & ~ srcval;
                        else if (iBISb) result <= dstval |   srcval;
                        else if (iADD)  result <= dstval + srcval;
                        else if (iSUB)  result <= dstval - srcval;
                        else if (iCLRb) result <= 0;
                        else if (iCOMb) result <= { ~ dstval[15:08], (instreg[15] ? 8'b0 : ~ dstval[07:00]) };
                        else if (iINCb) result <= dstval + oneval;
                        else if (iDECb) result <= dstval - oneval;
                        else if (iNEGb) result <= - dstval;
                        else if (iADCb) result <= dstval + (psw[00] ? oneval : 0);
                        else if (iSBCb) result <= dstval - (psw[00] ? oneval : 0);
                        else if (iTSTb) result <= dstval;
                        else if (iRORb) result <= { psw[00],    dstval[15:09], (instreg[15] ? 8'b0 : dstval[08:01]) };
                        else if (iROLb) result <= instreg[15] ? { dstval[14:08], psw[00], 8'b0 } : { dstval[14:00], psw[00] };
                        else if (iASRb) result <= { dstval[15], dstval[15:09], (instreg[15] ? 8'b0 : dstval[08:01]) };
                        else if (iASLb) result <= { dstval[14:00], 1'b0 };
                        else if (iSXT)  result <= psw[03] ? 16'o177777 : 16'o000000;
                        else if (iXOR)  result <= dstval ^ gprs[srcgprx];
                        else if (iSWAB) result <= { dstval[07:00], dstval[15:08] };
                        else if (iMFPS) result <= { psw[07:00], 8'b0 };
                    end
                end

                // write destination value to register or start writing to memory
                S_EXECDD2: begin

                    // update condition codes
                    if (iSWAB) begin
                        psw[03:00] <= { result[07], result[07:00] == 0, 2'b00 };
                    end else begin
                        psw[03:02] <= { result[15], result == 0 };
                             if (iCMPb) psw[01:00] <= { (srcval[15] ^ dstval[15]) & (~ result[15] ^ dstval[15]), srcval < dstval };
                        else if (iADD)  psw[01:00] <= { (~ srcval[15] ^ dstval[15]) & (result[15] ^ dstval[15]), result < dstval };
                        else if (iSUB)  psw[01:00] <= { (srcval[15] ^ dstval[15]) & (~ result[15] ^ srcval[15]), srcval > dstval };
                        else if (iCLRb) psw[01:00] <= { 2'b00 };
                        else if (iCOMb) psw[01:00] <= { 2'b01 };
                        else if (iINCb) psw[01]    <= { ~ dstval[15] & result[15] };
                        else if (iDECb) psw[01]    <= { dstval[15] & ~ result[15] };
                        else if (iNEGb) psw[01:00] <= { dstval[15] & result[15], result != 0 };
                        else if (iADCb) psw[01:00] <= { ~ dstval[15] & result[15], dstval[15] & ~ result[15] };
                        else if (iSBCb) psw[01:00] <= { dstval[15] & ~ result[15], ~ dstval[15] & result[15] };
                        else if (iTSTb) psw[01:00] <= { 2'b00 };
                        else if (iRORb) psw[01:00] <= { result[15] ^ (instreg[15] ? dstval[08] : dstval[00]), instreg[15] ? dstval[08] : dstval[00] };
                        else if (iROLb) psw[01:00] <= { result[15] ^ dstval[15], dstval[15] };
                        else if (iASRb) psw[01:00] <= { result[15] ^ (instreg[15] ? dstval[08] : dstval[00]), instreg[15] ? dstval[08] : dstval[00] };
                        else if (iASLb) psw[01:00] <= { result[15] ^ dstval[15], dstval[15] };
                                   else psw[01]    <= { 1'b0 };
                    end

                    // write dst register or start writing dst memory
                    // if writing to 777776 (psw), the result supercedes the condition codes
                    if (needtowritedst) begin
                        if (instreg[05:03] == 0) begin
                            if (iMOVB | iMFPS) begin
                                gprs[dstgprx] <= { { 8 { result[15] } }, result[15:08] };
                            end else if (byteinstr) begin
                                gprs[dstgprx][07:00] <= result[15:08];
                            end else begin
                                gprs[dstgprx] <= result;
                            end
                        end else begin
                            memfunc   <= MF_WR;
                            writedata <= byteinstr ? { 8'b0, result[15:08] } : result;
                        end
                    end
                    state <= S_SERVICE;
                end

                // wait for old register contents pushed on stack
                // then save return address and set PC = jumped-to address
                S_EXECJSR: begin
                    if (instreg[08:06] != 7) begin
                        gprs[srcgprx] <= gprs[7];
                    end
                    gprs[7] <= readdata;
                    state   <= S_SERVICE;
                end

                // start popping word from stack
                S_EXECRTS: begin
                    gprs[cspgprx] <= gprs[cspgprx] + 2;
                    membyte  <= 0;
                    memfunc  <= MF_RD;
                    memmode  <= psw[15:14];
                    state    <= S_EXECRTS2;
                    virtaddr <= gprs[cspgprx];
                end
                // rts pc: pop => pc
                //   else: rd  => pc
                //         pop => rd
                S_EXECRTS2: begin
                    if (instreg[02:00] != 7) begin
                        gprs[7] <= gprs[dstgprx];
                    end
                    gprs[dstgprx] <= readdata;
                    state <= S_SERVICE;
                end

                // start reading new PC from stack
                S_EXRTIT: begin
                    membyte   <= 0;
                    memfunc   <= MF_RD;
                    memmode   <= psw[15:14];
                    state     <= S_EXRTIT2;
                    virtaddr  <= gprs[cspgprx];
                end

                // start reading new PS from stack
                S_EXRTIT2: begin
                    membyte   <= 0;
                    memfunc   <= MF_RD;
                    memmode   <= psw[15:14];
                    srcval    <= readdata;
                    state     <= S_EXRTIT3;
                    virtaddr  <= gprs[cspgprx] + 2;
                end

                // update old SP, PC, PS
                S_EXRTIT3: begin
                    gprs[cspgprx] <= gprs[cspgprx] + 4;
                    gprs[7]    <= srcval;
                    if (psw[15:14] == 0) begin
                        psw[15:12] <= readdata[15:12];
                        psw[07:05] <= readdata[07:05];
                    end
                    psw[04:00] <= readdata[04:00];
                    state      <= S_SERVICE;
                    traceck    <= ~ instreg[2];     // RTI=2; RTT=6
                end

                // one of the trap instructions
                S_EXTRAP: begin
                    state   <= S_SERVICE;
                    trapvec <= instrapvec;
                end

                // MUL
                //  dstval = multiplier
                //  instreg[08:06] = multiplicand; destination register
                S_EXMUL: begin
                    srcval  <= gprs[srcgprx];
                    state   <= S_EXMUL2;
                end
                S_EXMUL2: begin
                    counter <= 15;
                    dstval  <= dstval[15] ? - dstval : dstval;
                    product <= 0;
                    signbit <= dstval[15] ^ srcval[15];
                    srcval  <= srcval[15] ? - srcval : srcval;
                    state   <= S_EXMUL3;
                end
                S_EXMUL3: begin
                    product <= (signbit & (counter == 0)) ? - multstep : multstep;
                    srcval  <= { 1'b0, srcval[15:01] };
                    if (counter == 0) state <= S_EXMUL4;
                            else counter <= counter - 1;
                end
                S_EXMUL4: begin
                    psw[3] <= product[31];
                    psw[2] <= product == 0;
                    psw[1] <= 0;
                    psw[0] <= psw[0] | (product > 32'h00007FFF) & (product < 32'hFFFF8000);
                    if (~ instreg[06]) gprs[srcgprx] <= product[31:16];
                    gprs[srcgprx1] <= product[15:00];
                    state  <= S_SERVICE;
                end

                // DIV
                //  dstval = divisor
                //  instreg[08:06] = dividend; destination register
                S_EXDIV: begin
                    product[31:16] <= gprs[srcgprx];
                    product[15:00] <= gprs[srcgprx1];
                    state          <= S_EXDIV3;
                end
                S_EXDIV3: begin
                    dstval     <= dstval[15] ? - dstval : dstval;
                    product    <= product[31] ? - product : product;
                    psw[00]    <= 0;
                    psw[01]    <= 0;
                    srcval[15] <= product[31];
                    signbit    <= product[31] ^ dstval[15];
                    state      <= S_EXDIV4;
                end
                S_EXDIV4: begin
                    if (product[31:15] >= { 1'b0, dstval }) begin
                        psw[00] <= (dstval == 0);
                        psw[01] <= 1;
                        state   <= S_SERVICE;
                    end else begin
                        counter <= 15;
                        state   <= S_EXDIV5;
                    end
                end
                S_EXDIV5: begin
                    if (product[30:15] >= dstval) begin
                        product <= { product[30:15] - dstval, product[14:00], 1'b1 };
                    end else begin
                        product <= { product[30:00], 1'b0 };
                    end
                    if (counter == 0) state <= S_EXDIV6;
                            else counter <= counter - 1;
                end
                // product[31:16] = unsigned remainder
                // product[15:00] = unsigned quotient
                // signbit        = quotient sign
                // srcval[15]     = dividend sign = remainder sign
                S_EXDIV6: begin
                    psw[3] <= signbit & (product[15:00] != 0);
                    psw[2] <= product[15:00] == 0;
                    gprs[srcgprx] <= signbit ? - product[15:00] : product[15:00];
                    gprs[srcgprx1] <= srcval[15] ? - product[31:16] : product[31:16];
                    state  <= S_SERVICE;
                end

                // ASH
                //  dstval[5:0] = shift count
                //  instreg[08:06] = operand; destination register
                S_EXASH: begin
                    product[31:16] <= gprs[srcgprx];
                    psw[1]         <= 0;
                    state          <= dstval[5:0] == 0 ? S_EXASH4 : dstval[5] ? S_EXASH2 : S_EXASH3;
                end
                S_EXASH2: begin
                    dstval[4:0]    <= dstval[4:0] + 1;
                    product[30:16] <= product[31:17];
                    psw[0]         <= product[16];
                    if (dstval[4:0] == 5'b11111) state <= S_EXASH4;
                end
                S_EXASH3: begin
                    dstval[4:0]    <= dstval[4:0] - 1;
                    product[31:16] <= { product[30:16], 1'b0 };
                    psw[0]         <= product[31];
                    psw[1]         <= psw[1] | (product[31] ^ product[30]);
                    if (dstval[4:0] == 5'b00001) state <= S_EXASH4;
                end
                S_EXASH4: begin
                    gprs[srcgprx]  <= product[31:16];
                    psw[2]         <= product[31:16] == 0;
                    psw[3]         <= product[31];
                    state          <= S_SERVICE;
                end

                // ASHC
                //  dstval[5:0] = shift count
                //  instreg[08:06] = operand; destination register
                S_EXASHC: begin
                    product[31:16] <= gprs[srcgprx];
                    product[15:00] <= gprs[srcgprx1];
                    psw[1]         <= 0;
                    state          <= dstval[5:0] == 0 ? S_EXASHC4 : dstval[5] ? S_EXASHC2 : S_EXASHC3;
                end
                S_EXASHC2: begin
                    dstval[4:0]    <= dstval[4:0] + 1;
                    product[30:00] <= product[31:01];
                    psw[0]         <= product[00];
                    if (dstval[4:0] == 5'b11111) state <= S_EXASHC4;
                end
                S_EXASHC3: begin
                    dstval[4:0]    <= dstval[4:0] - 1;
                    product[31:00] <= { product[30:00], 1'b0 };
                    psw[0]         <= product[31];
                    psw[1]         <= psw[1] | (product[31] ^ product[30]);
                    if (dstval[4:0] == 5'b00001) state <= S_EXASHC4;
                end
                S_EXASHC4: begin
                    gprs[srcgprx]  <= product[31:16];
                    gprs[srcgprx1] <= product[15:00];
                    psw[2]         <= product == 0;
                    psw[3]         <= product[31];
                    state          <= S_SERVICE;
                end

                // move from previous address space
                //  virtaddr = address in previous space
                S_EXMFPI: begin
                    if (instreg[05:03] == 0) begin
                        readdata <= gprs[gprx(psw[13:12],instreg[02:00])];
                    end else begin
                        membyte <= 0;
                        memfunc <= MF_RD;
                        memmode <= psw[13:12];
                    end
                    state <= S_EXMFPI2;
                end
                S_EXMFPI2: begin
                    psw[03]       <= readdata[15];
                    psw[02]       <= readdata == 0;
                    psw[01]       <= 0;
                    gprs[cspgprx] <= gprs[cspgprx] - 2;
                    membyte       <= 0;
                    memfunc       <= MF_WR;
                    memmode       <= psw[15:14];
                    state         <= S_SERVICE;
                    virtaddr      <= gprs[cspgprx] - 2;
                    writedata     <= readdata;
                    yellowck      <= 1;
                end

                // move from current stack to previous address space
                //  virtaddr = address in previous space
                S_EXMTPI: begin
                    dstval        <= virtaddr;              // save addr in prev space
                    gprs[cspgprx] <= gprs[cspgprx] + 2;     // increment stack pointer
                    membyte       <= 0;                     // do word-sized mem op
                    memfunc       <= MF_RD;                 // start reading memory
                    memmode       <= psw[15:14];            // access current addr space
                    state         <= S_EXMTPI2;             // do step 2 next
                    virtaddr      <= gprs[cspgprx];         // access top-of-stack word
                end
                S_EXMTPI2: begin
                    psw[03]       <= readdata[15];          // update condition codes
                    psw[02]       <= readdata == 0;
                    psw[01]       <= 0;
                    if (instreg[05:03] == 0) begin          // write register (maybe prev mode SP)
                        gprs[gprx(psw[13:12],instreg[02:00])] <= readdata;
                        state     <= S_SERVICE;
                    end else begin
                        membyte   <= 0;                     // start writing memory
                        memfunc   <= MF_WR;
                        memmode   <= psw[13:12];            // ...prev mode
                        state     <= S_SERVICE;
                        virtaddr  <= dstval;
                        writedata <= readdata;
                    end
                end

                // end of instruction, figure out what to do next
                S_SERVICE: begin
                    bus_init_out_l <= 1;    // in case we got here from powering up or RESET instruction

                    // do traps caused by instruction before checking halt switch
                    if (trapvec != 0) begin
                        state      <= S_TRAP;
                    end else if (yellowck & (psw[15:14] == 0) & (gprs[6] < YELSTKLIM)) begin
                        cpuerr[03] <= 1;
                        state      <= S_TRAP;
                        trapvec    <= T_CPUERR;
                    end

                    // maybe power just failed
                    else if (aclock & ~ bus_ac_lo_in_l) begin
                        aclock     <= 0;
                        state      <= S_TRAP;
                        trapvec    <= T_PWRFAIL;
                    end

                    // check halt switch
                    // suppressed first cycle after continuing from halt for single stepping
                    else if (haltck & ~ bus_hltrq_in_l) begin
                        state <= S_HALT;
                    end

                    // check dma
                    else if (bus_sack_in_l & ~ bus_npr_in_l) begin
                        state <= S_NPG;
                    end

                    // check interrupts
                    else if (bus_sack_in_l & intreq7) begin
                        bus_bg_out_h[7] <= 1;
                        state <= S_INTR;
                    end else if (bus_sack_in_l & intreq6) begin
                        bus_bg_out_h[6] <= 1;
                        state <= S_INTR;
                    end else if (bus_sack_in_l & intreq5) begin
                        bus_bg_out_h[5] <= 1;
                        state <= S_INTR;
                    end else if (bus_sack_in_l & intreq4) begin
                        bus_bg_out_h[4] <= 1;
                        state <= S_INTR;
                    end

                    // check instruction trace
                    else if (traceck & psw[4]) begin
                        state   <= S_TRAP;
                        trapvec <= T_BPTRACE;
                    end

                    // nothing special, fetch next instruction
                    else begin
                        state   <= S_FETCH;
                    end

                    // always enable halt and trace checking
                    haltck  <= 1;
                    traceck <= 1;
                end

                // dma requested, stick around until dma finished
                S_NPG: begin
                    bus_npg_out_h <= ~ bus_npr_in_l;
                    if (bus_bbsy_in_l & bus_npr_in_l & bus_sack_in_l) begin
                        state <= S_SERVICE;
                    end
                end

                // something is interrupting, grant has been sent
                // get interrupt vector while waiting for cycle to complete
                S_INTR: begin

                    // drop the grant when device acknowledges selection
                    if (~ bus_sack_in_l) begin
                        bus_bg_out_h <= 0;
                    end

                    // do vector transfer 80nS after device asserts INTR
                    if (bus_intr_in_l) begin
                        bus_ssyn_out_l <= 1;                // transfer is complete
                        intrdelay <= 0;                     // set up to count 80nS
                    end else if (intrdelay != 7) begin      // see if has been 80nS since INTR
                        intrdelay <= intrdelay + 1;
                    end else if (bus_ssyn_out_l) begin      // clock in first time after 80nS is up
                        bus_ssyn_out_l <= 0;                // tell device transfer complete
                        intrdelay <= 0;                     // reset delay line for next time
                        trapvec   <= ~ bus_d_in_l[07:00];   // save interrupt vector
                    end

                    // cycle completes when BBSY, BR[n], INTR, SACK are all negated
                    // ignore any BR[n] that has a negated BG[n], we only care about the BR[n] we granted
                    // maybe we didn't get a vector if device changed its mind
                    if (bus_bbsy_in_l & bus_intr_in_l & bus_sack_in_l &
                            (~ bus_bg_out_h[4] | bus_br_in_l[4]) &
                            (~ bus_bg_out_h[5] | bus_br_in_l[5]) &
                            (~ bus_bg_out_h[6] | bus_br_in_l[6]) &
                            (~ bus_bg_out_h[7] | bus_br_in_l[7])) begin
                        state <= S_SERVICE;                 // process vector if we got one, or go on to next instruction, etc
                    end
                end

                // do trap via trapvec
                // - start reading new PC into srcval
                S_TRAP: begin
                    if (trapping) begin
                        // was in middle of doing a trap, can't do nested ones
                        state      <= S_HALT;
                        trapping   <= 0;
                        trapvec    <= 0;
                    end else begin
                        // didn't trap from doing a trap, do a trap
                        membyte    <= 0;
                        memfunc    <= MF_RD;
                        memmode    <= 0;
                        state      <= S_TRAP2;
                        trapping   <= 1;                    // if we trap whilst doing this trap, halt
                        trapvec    <= 0;
                        virtaddr   <= { 8'b0, trapvec[7:2], 2'b00 };
                        yellowck   <= trapvec != T_CPUERR;  // check yellow stack only for other than the yellow stack vector
                    end
                end

                // - start reading new PS into dstval
                S_TRAP2: begin
                    membyte      <= 0;
                    memfunc      <= MF_RD;
                    memmode      <= 0;
                    srcval       <= readdata;
                    state        <= S_TRAP3;
                    virtaddr[01] <= 1;
                end

                // - start pushing old PS onto new stack
                //   unless powering up, then just use new PC and PS
                S_TRAP3: begin
                    dstval <= readdata;
                    if (nopushpspc) begin
                        nopushpspc <= 0;
                        state      <= S_TRAP5;
                    end else begin
                        membyte    <= 0;
                        memfunc    <= MF_WR;
                        memmode    <= readdata[15:14];
                        state      <= S_TRAP4;
                        trapping   <= readdata[15:14] == 0;
                        virtaddr   <= gprs[gprx(readdata[15:14],6)] - 2;
                        writedata  <= psw;
                    end
                end

                // - start pushing old PC onto new stack
                S_TRAP4: begin
                    membyte    <= 0;
                    memfunc    <= MF_WR;
                    memmode    <= dstval[15:14];
                    state      <= S_TRAP5;
                    virtaddr   <= gprs[gprx(dstval[15:14],6)] - 4;
                    writedata  <= gprs[7];
                end

                // - activate new PC and PS
                S_TRAP5: begin
                    gprs[7]    <= srcval;
                    gprs[gprx(dstval[15:14],6)] <= gprs[gprx(dstval[15:14],6)] - 4;
                    psw[15:14] <= dstval[15:14];
                    psw[13:12] <= psw[15:14];
                    psw[07:00] <= dstval[07:00];
                    state      <= S_SERVICE;
                    trapping   <= 0;
                end

                // hang if invalid state
                default: begin end
            endcase

            // if AC power is good, arm to detect AC power failure
            if (bus_ac_lo_in_l) aclock <= 1;
        end

        ///////////////////////
        //  SLAVE REGISTERS  //
        ///////////////////////

        if (~ resetting) begin

            // kernel descriptor registers 772300..16  111 111 010 011 00_ __0
            // kernel address registers    772340..56  111 111 010 011 10_ __0
            // user descriptor registers   777600..16  111 111 111 110 00_ __0
            // user address registers      777640..56  111 111 111 110 10_ __0
            if (((~ bus_a_in_l & 18'o777720) == 18'o772300) | ((~ bus_a_in_l & 18'o777720) == 18'o777600)) begin
                if (bus_msyn_in_l) begin
                    mmv_d_out_l    <= 16'o177777;
                    bus_ssyn_out_l <= 1;
                end else if (bus_ssyn_out_l) begin
                    if (bus_c_in_l[1]) begin
                        // read register contents
                        if (bus_a_in_l[05]) begin
                            mmv_d_out_l <= ~ mmupdrs[mmuprbi] | ~ 16'o077516;
                        end else begin
                            mmv_d_out_l <= ~ mmupars[mmuprbi] | ~ 16'o007777;
                        end
                    end else begin
                        // write register contents
                        mmupdrs[mmuprbi][06]            <= 0;   // always clear W bit
                        if (bus_c_in_l[0] | ~ bus_a_in_l[00]) begin
                            if (bus_a_in_l[05]) begin
                                mmupdrs[mmuprbi][14:08] <= ~ bus_d_in_l[14:08];
                            end else begin
                                mmupars[mmuprbi][11:08] <= ~ bus_d_in_l[11:08];
                            end
                        end
                        if (bus_c_in_l[0] |   bus_a_in_l[00]) begin
                            if (bus_a_in_l[05]) begin
                                mmupdrs[mmuprbi][03:01] <= ~ bus_d_in_l[03:01];
                            end else begin
                                mmupars[mmuprbi][07:00] <= ~ bus_d_in_l[07:00];
                            end
                        end
                    end
                    bus_ssyn_out_l <= 0;
                end
            end

            // mmr0..mmr2 register access via 777572..777576
            if ((~ bus_a_in_l & 18'o777772) == 18'o777572) begin
                if (bus_msyn_in_l) begin
                    mmr_d_out_l    <= 16'o177777;
                    bus_ssyn_out_l <= 1;
                end else if (bus_ssyn_out_l) begin
                    if (bus_c_in_l[1]) begin
                        case (bus_a_in_l[02])
                            1: mmr_d_out_l <= ~ mmr0;
                            0: mmr_d_out_l <= ~ mmr2;
                        endcase
                    end else begin
                        if (bus_c_in_l[0] | ~ bus_a_in_l[00]) case (bus_a_in_l[02])
                            1: mmr0[15:08] <= ~ bus_d_in_l[15:08] & 8'o341;
                            0: mmr2[15:08] <= ~ bus_d_in_l[15:08];
                        endcase
                        if (bus_c_in_l[0] |   bus_a_in_l[00]) case (bus_a_in_l[02])
                            1: mmr0[07:00] <= ~ bus_d_in_l[07:00] & 8'o157;
                            0: mmr2[07:00] <= ~ bus_d_in_l[07:00];
                        endcase
                    end
                    bus_ssyn_out_l <= 0;
                end
            end

            // register access via 7777rr
            if (halted & ((bus_a_in_l >> 4) == (~ 18'o777700 >> 4)) & (bus_c_in_l != 0)) begin
                if (bus_msyn_in_l) begin
                    gpr_d_out_l    <= 16'o177777;
                    bus_ssyn_out_l <= 1;
                end else if (bus_ssyn_out_l) begin
                    if (bus_c_in_l[1]) begin
                        gpr_d_out_l <= ~ gprs[~bus_a_in_l[3:0]];
                    end else begin
                        gprs[~bus_a_in_l[3:0]] <= ~ bus_d_in_l;
                    end
                    bus_ssyn_out_l <= 0;
                end
            end

            // cpu error register access via 777766
            //  [07] = illegal halt
            //  [06] = odd address
            //  [04] = unibus timeout
            //  [03] = yellow stack
        /***
            if ((bus_a_in_l >> 1) == (~ 18'o777766 >> 1)) begin
                if (bus_msyn_in_l) begin
                    cer_d_out_l    <= 16'o177777;
                    bus_ssyn_out_l <= 1;
                end else if (bus_ssyn_out_l) begin
                    if (bus_c_in_l[1]) begin
                        cer_d_out_l <= ~ cpuerr;
                    end else begin
                        if (bus_c_in_l[0] | bus_a_in_l[00]) cpuerr[07:00] <= ~ bus_d_in_l[07:00] & 8'o330;
                    end
                    bus_ssyn_out_l <= 0;
                end
            end
        ***/

            // processor status word access via 777776
            if ((bus_a_in_l >> 1) == (~ 18'o777776 >> 1)) begin
                if (bus_msyn_in_l) begin
                    psw_d_out_l    <= 16'o177777;
                    bus_ssyn_out_l <= 1;
                end else if (bus_ssyn_out_l) begin
                    if (bus_c_in_l[1]) begin
                        psw_d_out_l <= ~ psw;
                    end else begin
                        if (bus_c_in_l[0] | ~ bus_a_in_l[00]) psw[15:12] <= ~ bus_d_in_l[15:12];
                        if (bus_c_in_l[0] |   bus_a_in_l[00]) psw[07:05] <= ~ bus_d_in_l[07:05];
                        if (bus_c_in_l[0] |   bus_a_in_l[00]) psw[03:00] <= ~ bus_d_in_l[03:00];
                    end
                    bus_ssyn_out_l <= 0;
                end
            end
        end
    end
endmodule
