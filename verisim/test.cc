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

// Test pdp1134.v by sending random instruction stream and checking the bus cycles

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "obj_dir/Vpdp1134.h"

#define S_FETCH 2U

// single word-sized operand instruction
//  opcode   = 16-bit opcode with 00 for the dd field
//  readdst  = true to read old dst value
//  writedst = true to write new dst value
//  function = computation function (dstval = old dst value)
//  pswv     = true to set V bit; false to clear V bit
//  pswc     = true to set C bit; false to clear C bit
#define SINGLEWORD(instr,opcode,readdst,writedst,function,pswv,pswc) \
    uint16_t dd; \
    do dd = genranddd (true, 0); \
    while ((dd & 076) == 006); \
    printf ("%12llu0 : %s%s\n", cyclectr, instr, ddstr (ddbuff, dd)); \
    sendfetch (opcode | dd); \
    uint16_t dstval, result; \
    if ((dd & 070) == 0) { \
        dstval = readdst ? gprs[curgprx(dd)] : 0; \
        result = function; \
        if (writedst) gprs[curgprx(dd)] = result; \
    } else { \
        uint16_t ddva = sendfetchdd (dd, true); \
        dstval = randbits (16); \
        if (readdst)  sendword (ddva, dstval, psw >> 14, writedst, "dst value"); \
        result = function; \
        if (writedst) recvword (ddva, result, psw >> 14, "dst value"); \
    } \
    writepsw ((psw & 0177760) | ((result & 0100000) ? 010 : 0) | ((result == 0) ? 004 : 0) | ((pswv) ? 002 : 0) | ((pswc) ? 001 : 0));

#define SINGLEBYTE(instr,opcode,readdst,writedst,function,pswv,pswc) \
    uint16_t dd; \
    do dd = genranddd (true, 0); \
    while ((dd & 076) == 006); \
    printf ("%12llu0 : %s%s\n", cyclectr, instr, ddstr (ddbuff, dd)); \
    sendfetch (opcode | dd); \
    uint8_t dstval, result; \
    if ((dd & 070) == 0) { \
        uint16_t *dr = &gprs[curgprx(dd)]; \
        dstval = readdst ? *dr : 0; \
        result = function; \
        if (writedst) *dr = (*dr & 0177400) | result; \
    } else { \
        uint16_t ddva = sendfetchdd (dd, false); \
        dstval = randbits (16); \
        if (readdst)  sendbyte (ddva, dstval, psw >> 14, writedst, "dst value"); \
        result = function; \
        if (writedst) recvbyte (ddva, result, psw >> 14, "dst value"); \
    } \
    writepsw ((psw & 0177760) | ((result & 0000200) ? 010 : 0) | ((result == 0) ? 004 : 0) | ((pswv) ? 002 : 0) | ((pswc) ? 001 : 0));

#define DOUBLEWORD(instr,opcode,readdst,writedst,function,pswv,pswc) \
    uint16_t ss = genranddd (true, 0); \
    uint16_t dd; \
    do dd = genranddd (true, ss); \
    while ((dd & 076) == 006); \
    printf ("%12llu0 : %s %s,%s\n", cyclectr, instr, ddstr (ssbuff, ss), ddstr (ddbuff, dd)); \
    sendfetch (opcode | (ss << 6) | dd); \
    uint16_t srcval; \
    if ((ss & 070) == 0) { \
        srcval = gprs[curgprx(ss)]; \
    } else { \
        uint16_t ssva = sendfetchdd (ss, true); \
        srcval = randbits (16); \
        sendword (ssva, srcval, psw >> 14, false, "src value"); \
    } \
    uint16_t dstval, result; \
    if ((dd & 070) == 0) { \
        dstval = readdst ? gprs[curgprx(dd)] : 0; \
        result = function; \
        if (writedst) gprs[curgprx(dd)] = result; \
    } else { \
        uint16_t ddva = sendfetchdd (dd, true); \
        dstval = randbits (16); \
        if (readdst)  sendword (ddva, dstval, psw >> 14, writedst, "dst value"); \
        result = function; \
        if (writedst) recvword (ddva, result, psw >> 14, "dst value"); \
    } \
    writepsw ((psw & 0177760) | ((result & 0100000) ? 010 : 0) | ((result == 0) ? 004 : 0) | ((pswv) ? 002 : 0) | ((pswc) ? 001 : 0));

#define DOUBLEBYTE(instr,opcode,readdst,writedst,function,pswv,pswc) \
    uint16_t ss = genranddd (false, 0); \
    uint16_t dd; \
    do dd = genranddd (false, ss); \
    while ((dd & 076) == 006); \
    printf ("%12llu0 : %s %s,%s\n", cyclectr, instr, ddstr (ssbuff, ss), ddstr (ddbuff, dd)); \
    sendfetch (opcode | (ss << 6) | dd); \
    uint8_t srcval; \
    if ((ss & 070) == 0) { \
        srcval = gprs[curgprx(ss)]; \
    } else { \
        uint16_t ssva = sendfetchdd (ss, false); \
        srcval = randbits (8); \
        sendbyte (ssva, srcval, psw >> 14, false, "src value"); \
    } \
    uint8_t dstval, result; \
    if ((dd & 070) == 0) { \
        uint16_t *dr = &gprs[curgprx(dd)]; \
        dstval = readdst ? *dr : 0; \
        result = function; \
        if (writedst) *dr = (*dr & 0177400) | result; \
    } else { \
        uint16_t ddva = sendfetchdd (dd, false); \
        dstval = randbits (16); \
        if (readdst)  sendbyte (ddva, dstval, psw >> 14, writedst, "dst value"); \
        result = function; \
        if (writedst) recvbyte (ddva, result, psw >> 14, "dst value"); \
    } \
    writepsw ((psw & 0177760) | ((result & 0000200) ? 010 : 0) | ((result == 0) ? 004 : 0) | ((pswv) ? 002 : 0) | ((pswc) ? 001 : 0));

// throw one of these when processor is about to trap
struct TrapThru {
    TrapThru (uint8_t v) { vector = v; }
    uint8_t vector;
};

char const *const brmnes[] = {
        "BN",  "BR",  "BNE", "BEQ",  "BGE", "BLT", "BGT", "BLE",
        "BPL", "BMI", "BHI", "BLOS", "BVC", "BVS", "BCC", "BCS" };

bool didsomething;
long long unsigned cyclectr;
uint16_t gprs[16], pars[16], pdrs[16];
uint16_t mmr0, mmr2, psw;
Vpdp1134 vp;

uint16_t gprx (uint16_t r, uint16_t mode) { return (r == 6) && (mode & 2) ? 016 : r; }
uint16_t curgprx (uint16_t r) { return gprx (r, psw >> 14); }

uint16_t genranddd (bool word, uint16_t ss);
void vfyintreg (uint16_t regva, uint16_t regda);
uint16_t sendfetchdd (uint16_t dd, bool word);
void dointreq (uint16_t brlev);
void trapthrough (uint16_t vector);
void restart ();
void writepsw (uint16_t newpsw);
void sendfetch (uint16_t data);
void sendword (uint16_t virtaddr, uint16_t data, uint16_t mode, bool rmw, char const *desc);
void sendbyte (uint16_t virtaddr, uint8_t data, uint16_t mode, bool rmw, char const *desc);
void recvword (uint16_t virtaddr, uint16_t data, uint16_t mode, char const *desc);
void recvbyte (uint16_t virtaddr, uint8_t data, uint16_t mode, char const *desc);
uint32_t virt2phys (uint16_t virtaddr, bool wrt, uint16_t mode);
void setpdrw (char ident, int index, int value);
void monintrd (uint32_t physaddr, uint16_t data, char const *desc);
void monintwr (uint32_t physaddr, uint16_t data, char const *desc);
void readword (uint32_t physaddr, uint16_t data);
void writeword (uint32_t physaddr, uint16_t data);
void kerchunk ();
uint16_t mulinstr (uint16_t regno, uint16_t srcval);
uint16_t divinstr (uint16_t regno, uint16_t srcval);
uint16_t ashinstr (uint16_t regno, uint16_t srcval);
uint16_t ashcinstr (uint16_t regno, uint16_t srcval);
void fatal (char const *fmt, ...);
void dumpstate ();
char *ddstr (char *ddbuff, uint16_t dd);
uint32_t randbits (int nbits);

int main ()
{
    setlinebuf (stdout);

    vp.bus_ac_lo_in_l   = 1;
    vp.bus_bbsy_in_l    = 1;
    vp.bus_br_in_l      = 15;
    vp.bus_dc_lo_in_l   = 1;
    vp.bus_intr_in_l    = 1;
    vp.bus_npr_in_l     = 1;
    vp.bus_sack_in_l    = 1;
    vp.halt_rqst_in_l   = 1;

    vp.bus_a_in_l    = 0777777;
    vp.bus_c_in_l    = 3;
    vp.bus_d_in_l    = 0177777;
    vp.bus_init_in_l = 1;
    vp.bus_msyn_in_l = 1;
    vp.bus_ssyn_in_l = 1;

    vp.CLOCK = 0;
    vp.RESET = 1;
    kerchunk ();
    vp.RESET = 0;
    kerchunk ();

    restart ();

    didsomething = true;

    while (true) {
        char ddbuff[8], ssbuff[8];

        if (didsomething) {

            // processor should be fetching
            for (int i = 0; vp.state != S_FETCH; i ++) {
                if (i > 100) fatal ("expecting state S_FETCH (%02u)\n", S_FETCH);
                kerchunk ();
            }
            if ((vp.r0 != gprs[0]) || (vp.r1 != gprs[1]) || (vp.r2 != gprs[2]) || (vp.r3 != gprs[3]) || (vp.r4 != gprs[4]) || (vp.r5 != gprs[5]) || (vp.r6 != gprs[6]) || (vp.r7 != gprs[7]) || (vp.ps != psw) || (vp.r16 != gprs[016])) {
                fatal ("register mismatch - expect         R0=%06o R1=%06o R2=%06o R3=%06o R4=%06o R5=%06o R6=%06o R7=%06o PS=%06o R16=%06o\n",
                        gprs[0] ,gprs[1] ,gprs[2] ,gprs[3] ,gprs[4] ,gprs[5] ,gprs[6] ,gprs[7], psw, gprs[016]);
            }
            if ((((psw & 0140000) != 0000000) && ((psw & 0140000) != 0140000)) ||   // current mode must be kernel or user
                (((psw & 0030000) != 0000000) && ((psw & 0030000) != 0030000)) ||   // previous mode must be kernel or user
                ((psw & 0140000) > ((psw & 0030000) << 2))) {                       // previous mode must be .ge. current mode
                fatal ("invalid psw %06o\n", psw);
            }

            printf ("R0=%06o R1=%06o R2=%06o R3=%06o R4=%06o R5=%06o R6=%06o R7=%06o PS=%06o R16=%06o\n",
                    gprs[0] ,gprs[1] ,gprs[2] ,gprs[3] ,gprs[4] ,gprs[5] ,gprs[6] ,gprs[7], psw, gprs[016]);
            printf ("  MMR0=%06o PAR7=%06o PDR7=%06o\n", mmr0, pars[7], pdrs[7]);
            printf ("- - - - - - - - - - - - - - - - - - - -\n");

            // maybe request interrupt
            uint16_t irq = randbits (8);
            if (irq < 16) {
                vp.bus_br_in_l = irq;
            }
        }

        // generate random opcode and send to processor
        if ((mmr0 & 0160000) == 0) mmr2 = gprs[7];
        bool didrtt = false;
        didsomething = false;
        uint8_t select = randbits (6);
        try {
            switch (select) {

                // HALT
                case  0: {
                    if (! (psw & 0160000)) {
                        printf ("%12llu0 : HALT\n", cyclectr);
                        sendfetch (0000000);
                        for (int i = 0; ! vp.halt_grant_out_h; i ++) {
                            if (i > 200) fatal ("did not halt\n");
                            kerchunk ();
                        }
                        vp.halt_rqst_in_l = 0;
                        kerchunk ();
                        kerchunk ();
                        vp.halt_rqst_in_l = 1;
                        for (int i = 0; vp.halt_grant_out_h; i ++) {
                            if (i > 200) fatal ("halt grant stuck on\n");
                            kerchunk ();
                        }
                    }
                    break;
                }

                // RTI/RTT
                case  1: {
                    didrtt = randbits (1);
                    uint16_t opcode = didrtt ? 0000006 : 0000002;
                    printf ("%12llu0 : %s\n", cyclectr, didrtt ? "RTT" : "RTI");
                    sendfetch (opcode);
                    uint16_t newpc;
                    do newpc = randbits (15) * 2;
                    while (newpc > 0157770);
                    uint16_t newps = randbits (4);          // NZVC
                    if (randbits (4) == 0) newps |= 020;    // Trace
                    newps |= randbits (3) << 5;             // priority
                    if (randbits (2) == 0) {
                        newps |= 0170000;                   // currmode = prevmode = USER
                    } else if (randbits (3) == 0) {
                        newps |= 0030000;                   // currmode = KERNEL; prevmode = USER
                    }
                    sendword (gprs[curgprx(6)] + 0, newpc, psw >> 14, false, "rti/rtt restored pc");
                    sendword (gprs[curgprx(6)] + 2, newps, psw >> 14, false, "rti/rtt restored ps");
                    gprs[curgprx(6)] += 4;
                    gprs[7]  = newpc;
                    if (psw & 0140000) {
                        writepsw ((psw & 0177740) | (newps & 037));
                    } else {
                        writepsw (newps);
                    }
                    break;
                }

                // BPT
                case  2: {
                    printf ("%12llu0 : BPT\n", cyclectr);
                    sendfetch (0000003);
                    trapthrough (0014);
                    break;
                }

                // IOT
                case  3: {
                    printf ("%12llu0 : IOT\n", cyclectr);
                    sendfetch (0000004);
                    trapthrough (0020);
                    break;
                }

                /*
        wire iWAIT  = (instreg == 1);
        wire iRESET = (instreg == 5);
                */

                // JMP
                case  7: {
                    uint16_t dd;
                    do dd = genranddd (true, 0);
                    while ((dd & 070) == 0);
                    printf ("%12llu0 : JMP %s\n", cyclectr, ddstr (ddbuff, dd));
                    sendfetch (0000100 | dd);
                    uint16_t ddva = sendfetchdd (dd, true);
                    gprs[7] = ddva;
                    break;
                }

                // SWAB
                case  8: {
                    SINGLEWORD ("SWAB ", 0000300, true, true, (dstval << 8) | (dstval >> 8), false, false);
                    writepsw ((psw & 0177760) | ((result & 0000200) ? 010 : 0) | (((result & 0000377) == 0) ? 004 : 0));
                    break;
                }

                // JSR
                case  9: {
                    uint16_t dd;
                    do dd = genranddd (true, 0);
                    while ((dd & 070) == 0);
                    uint16_t r = randbits (3);
                    printf ("%12llu0 : JSR R%o,%s\n", cyclectr, r, ddstr (ddbuff, dd));
                    sendfetch (0004000 | (r << 6) | dd);
                    uint16_t ddva = sendfetchdd (dd, true);
                    uint16_t oldreg = gprs[curgprx(r)];
                    gprs[curgprx(6)] -= 2;
                    recvword (gprs[curgprx(6)], oldreg, psw >> 14, "push old register");
                    gprs[curgprx(r)] = gprs[7];
                    gprs[7] = ddva;
                    break;
                }

                // CLR
                case 10: {
                    SINGLEWORD ("CLR ", 0005000, false, true, 0, false, false);
                    break;
                }

                // CLRB
                case 11: {
                    SINGLEBYTE ("CLRB ", 0105000, false, true, 0, false, false);
                    break;
                }

                // COM
                case 12: {
                    SINGLEWORD ("COM ", 0005100, true, true, ~ dstval, false, true);
                    break;
                }

                // COMB
                case 13: {
                    SINGLEBYTE ("COMB ", 0105100, true, true, ~ dstval, false, true);
                    break;
                }

                // INC
                case 14: {
                    SINGLEWORD ("INC ", 0005200, true, true, dstval + 1, dstval == 0077777, psw & 1);
                    break;
                }

                // INCB
                case 15: {
                    SINGLEBYTE ("INCB ", 0105200, true, true, dstval + 1, dstval == 0177, psw & 1);
                    break;
                }

                // DEC
                case 16: {
                    SINGLEWORD ("DEC ", 0005300, true, true, dstval - 1, dstval == 0100000, psw & 1);
                    break;
                }

                // DECB
                case 17: {
                    SINGLEBYTE ("DECB ", 0105300, true, true, dstval - 1, dstval == 0200, psw & 1);
                    break;
                }

                // NEG
                case 18: {
                    SINGLEWORD ("NEG ", 0005400, true, true, - dstval, result == 0100000, result != 0);
                    break;
                }

                // NEGB
                case 19: {
                    SINGLEBYTE ("NEGB ", 0105400, true, true, - dstval, result == 0000200, result != 0);
                    break;
                }

                // ADC
                case 20: {
                    SINGLEWORD ("ADC ", 0005500, true, true, dstval + (psw & 1), ~ dstval & result & 0100000, dstval & ~ result & 0100000);
                    break;
                }

                // ADCB
                case 21: {
                    SINGLEBYTE ("ADCB ", 0105500, true, true, dstval + (psw & 1), ~ dstval & result & 0000200, dstval & ~ result & 0000200);
                    break;
                }

                // SBC
                case 22: {
                    SINGLEWORD ("SBC ", 0005600, true, true, dstval - (psw & 1), dstval & ~ result & 0100000, ~ dstval & result & 0100000);
                    break;
                }

                // SBCB
                case 23: {
                    SINGLEBYTE ("SBCB ", 0105600, true, true, dstval - (psw & 1), dstval & ~ result & 0000200, ~ dstval & result & 0000200);
                    break;
                }

                // TST
                case 24: {
                    SINGLEWORD ("TST ", 0005700, true, false, dstval, false, false);
                    break;
                }

                // TSTB
                case 25: {
                    SINGLEBYTE ("TSTB ", 0105700, true, false, dstval, false, false);
                    break;
                }

                // ROR
                case 26: {
                    SINGLEWORD ("ROR ", 0006000, true, true, ((psw & 1) << 15) | (dstval >> 1),
                        ((result >> 15) ^ dstval) & 1, dstval & 1);
                    break;
                }

                // RORB
                case 27: {
                    SINGLEBYTE ("RORB ", 0106000, true, true, ((psw & 1) <<  7) | (dstval >> 1),
                        ((result >>  7) ^ dstval) & 1, dstval & 1);
                    break;
                }

                // ROL
                case 28: {
                    SINGLEWORD ("ROL ", 0006100, true, true, (dstval << 1) | (psw & 1),
                        ((result ^ dstval) >> 15) & 1, (dstval >> 15) & 1);
                    break;
                }

                // ROLB
                case 29: {
                    SINGLEBYTE ("ROLB ", 0106100, true, true, (dstval << 1) | (psw & 1),
                        ((result ^ dstval) >>  7) & 1, (dstval >>  7) & 1);
                    break;
                }

                // ASR
                case 30: {
                    SINGLEWORD ("ASR ", 0006200, true, true, (dstval & 0100000) | (dstval >> 1),
                        ((result >> 15) ^ dstval) & 1, dstval & 1);
                    break;
                }

                // ASRB
                case 31: {
                    SINGLEBYTE ("ASRB ", 0106200, true, true, (dstval & 0000200) | (dstval >> 1),
                        ((result >>  7) ^ dstval) & 1, dstval & 1);
                    break;
                }

                // ASL
                case 32: {
                    SINGLEWORD ("ASL ", 0006300, true, true, dstval << 1,
                        ((result ^ dstval) >> 15) & 1, (dstval >> 15) & 1);
                    break;
                }

                // ASLB
                case 33: {
                    SINGLEBYTE ("ASLB ", 0106300, true, true, dstval << 1,
                        ((result ^ dstval) >>  7) & 1, (dstval >>  7) & 1);
                    break;
                }

                // MARK (p 99)
                case 34: {
                    if (! (gprs[5] & 1)) {
                        uint16_t nn = randbits (6);
                        printf ("%12llu0 : MARK %02o\n", cyclectr, nn);
                        sendfetch (0006400 | nn);
                        uint16_t popfrom = gprs[7] + nn * 2;
                        gprs[curgprx(6)] = popfrom + 2;
                        gprs[7] = gprs[5];
                        uint16_t newr5 = randbits (16);
                        sendword (popfrom, newr5, psw >> 14, false, "pop old R5");
                        gprs[5] = newr5;
                    }
                    break;
                }

                // MTPS (p 64)
                case 35: {
                    uint16_t dd = genranddd (false, 0);
                    printf ("%12llu0 : MTPS %s\n", cyclectr, ddstr (ddbuff, dd));
                    sendfetch (0106400 | dd);
                    uint8_t newpsb;
                    if ((dd & 070) == 0) {
                        newpsb = gprs[curgprx(dd)];
                    } else {
                        uint16_t ddva = sendfetchdd (dd, false);
                        newpsb = randbits (3) << 5;
                        sendbyte (ddva, newpsb, psw >> 14, false, "psb value");
                    }
                    if ((psw & 0140000) == 0) writepsw ((psw & 0177437) | (newpsb & 0340));
                    writepsw ((psw & 0177761) | ((newpsb & 0200) ? 010 : 0) | ((newpsb == 0) ? 004 : 0));
                    break;
                }

                // MFPI (p 168)
                case 36: {
                    if (gprs[curgprx(6)] > 0410) {       // avoid yellow stack
                        uint16_t dd = genranddd (true, 0);
                        printf ("%12llu0 : MFPI %s\n", cyclectr, ddstr (ddbuff, dd));
                        sendfetch (0006500 | dd);
                        uint16_t value;
                        if ((dd & 070) == 0) {
                            value = gprs[gprx(dd,psw>>12)];
                        } else {
                            uint16_t ddva = sendfetchdd (dd, true);
                            value = randbits (16);
                            sendword (ddva, value, psw >> 12, false, "dst value");
                        }
                        uint16_t newsp = gprs[curgprx(6)] - 2;
                        gprs[curgprx(6)] = newsp;
                        recvword (newsp, value, psw >> 14, "push value");
                    }
                    break;
                }

                // MTPI (p 169)
                case 37: {
                    uint16_t dd;
                    do dd = genranddd (true, 0);
                    while (dd == 007);
                    printf ("%12llu0 : MTPI %s\n", cyclectr, ddstr (ddbuff, dd));
                    sendfetch (0006600 | dd);
                    uint16_t ddva = ((dd & 070) == 0) ? 0 : sendfetchdd (dd, true);
                    uint16_t oldsp = gprs[curgprx(6)];
                    gprs[curgprx(6)] = oldsp + 2;
                    uint16_t value = randbits (16);
                    if (dd == 006) value &= -2;
                    sendword (oldsp, value, psw >> 14, false, "pop stack value");
                    if ((dd & 070) == 0) {
                        gprs[gprx(dd,psw>>12)] = value;
                    } else {
                        recvword (ddva, value, psw >> 12, "dst value");
                    }
                    break;
                }

                // SXT (p 62)
                case 38: {
                    SINGLEWORD ("SXT ", 0006700, false, true, (psw & 010) ? -1 : 0, false, psw & 1);
                    break;
                }

                // MFPS (p 63)
                case 39: {
                    uint16_t dd;
                    do dd = genranddd (false, 0);
                    while ((dd & 076) == 006);
                    printf ("%12llu0 : MFPS %s\n", cyclectr, ddstr (ddbuff, dd));
                    sendfetch (0106700 | dd);
                    if ((dd & 070) == 0) {
                        gprs[curgprx(dd)] = (int16_t) (int8_t) psw;
                    } else {
                        uint16_t ddva = sendfetchdd (dd, false);
                        recvbyte (ddva, psw, psw >> 14, "psb value");
                    }
                    writepsw ((psw & 0177761) | ((psw & 0000200) ? 010 : 0) | (((int8_t) psw == 0) ? 004 : 0));
                    break;
                }

                // MOV
                case 40: {
                    DOUBLEWORD ("MOV", 0010000, false, true, srcval, false, psw & 1);
                    break;
                }

                // MOVB
                case 41: {
                    DOUBLEBYTE ("MOVB", 0110000, false, true, srcval, false, psw & 1);
                    if ((dd & 070) == 0) {
                        gprs[curgprx(dd)] = (int16_t) (int8_t) result;
                    }
                    break;
                }

                // CMP
                case 42: {
                    DOUBLEWORD ("CMP", 0020000, true, false, srcval - dstval,
                        (srcval ^ dstval) & (~ result ^ dstval) & 0100000,
                        srcval < dstval);
                    break;
                }

                // CMPB
                case 43: {
                    DOUBLEBYTE ("CMPB", 0120000, true, false, srcval - dstval,
                        (srcval ^ dstval) & (~ result ^ dstval) & 0000200,
                        srcval < dstval);
                    break;
                }

                // BIT
                case 44: {
                    DOUBLEWORD ("BIT", 0030000, true, false, srcval & dstval, false, psw & 1);
                    break;
                }

                // BITB
                case 45: {
                    DOUBLEBYTE ("BITB", 0130000, true, false, srcval & dstval, false, psw & 1);
                    break;
                }

                // BIC
                case 46: {
                    DOUBLEWORD ("BIC", 0040000, true, true, ~ srcval & dstval, false, psw & 1);
                    break;
                }

                // BICB
                case 47: {
                    DOUBLEBYTE ("BICB", 0140000, true, true, ~ srcval & dstval, false, psw & 1);
                    break;
                }

                // BIS
                case 48: {
                    DOUBLEWORD ("BIS", 0050000, true, true, srcval | dstval, false, psw & 1);
                    break;
                }

                // BISB
                case 49: {
                    DOUBLEBYTE ("BISB", 0150000, true, true, srcval | dstval, false, psw & 1);
                    break;
                }

                // ADD
                case 50: {
                    DOUBLEWORD ("ADD", 0060000, true, true, dstval + srcval,
                        (~ srcval ^ dstval) & (result ^ dstval) & 0100000,
                        ((uint32_t) srcval + (uint32_t) dstval) >> 16);
                    break;
                }

                // SUB (p 68)
                case 51: {
                    DOUBLEWORD ("SUB", 0160000, true, true, dstval - srcval,
                        (srcval ^ dstval) & (~ result ^ srcval) & 0100000,
                        dstval < srcval);
                    break;
                }

                // MUL (p 147)
                case 52: {
                    uint16_t regno; do regno = randbits (3); while ((regno & 6) == 6);
                    char xrbuff[8];
                    sprintf (xrbuff, "MUL R%o,", regno);
                    SINGLEWORD (xrbuff, 0070000 | (regno << 6), true, false, mulinstr (regno, dstval), psw & 2, psw & 1);
                    break;
                }

                // DIV (p 148)
                case 53: {
                    uint16_t regno; do regno = randbits (2) * 2; while (regno == 6);
                    char xrbuff[8];
                    sprintf (xrbuff, "DIV R%o,", regno);
                    uint16_t oldpsw = psw;
                    SINGLEWORD (xrbuff, 0071000 | (regno << 6), true, false, divinstr (regno, dstval), psw & 2, psw & 1);
                    if (psw & 002) writepsw ((psw & 0177763) | (oldpsw & 0000014));
                    break;
                }

                // ASH (p 149)
                case 54: {
                    uint16_t regno; do regno = randbits (2) * 2; while (regno == 6);
                    char xrbuff[8];
                    sprintf (xrbuff, "ASH R%o,", regno);
                    SINGLEWORD (xrbuff, 0072000 | (regno << 6), true, false, ashinstr (regno, dstval), psw & 2, psw & 1);
                    break;
                }

                // ASHC (p 150)
                case 55: {
                    uint16_t regno; do regno = randbits (2) * 2; while (regno == 6);
                    char xrbuff[9];
                    sprintf (xrbuff, "ASHC R%o,", regno);
                    SINGLEWORD (xrbuff, 0073000 | (regno << 6), true, false, ashcinstr (regno, dstval), psw & 2, psw & 1);
                    break;
                }

                // XOR (p 73)
                case 56: {
                    uint16_t srcreg = randbits (3);
                    char xrbuff[8];
                    sprintf (xrbuff, "XOR R%o,", srcreg);
                    SINGLEWORD (xrbuff, 0074000 | (srcreg << 6), true, true, gprs[curgprx(srcreg)] ^ dstval, false, psw & 1);
                    break;
                }

                // SOB (p 101)
                case 57: {
                    uint16_t regno;
                    do regno = randbits (3);
                    while ((regno & 6) == 6);
                    uint16_t nn = randbits (6);
                    printf ("%12llu0 : SOB R%o,%06o\n", cyclectr, regno, nn);
                    sendfetch (0077000 | (regno << 6) | nn);
                    if (-- gprs[curgprx(regno)] != 0) gprs[7] -= nn * 2;
                    break;
                }

                // EMT
                case 58: {
                    uint16_t code = randbits (8);
                    printf ("%12llu0 : EMT %03o\n", cyclectr, code);
                    sendfetch (0104000 | code);
                    trapthrough (0030);
                    break;
                }

                // TRAP
                case 59: {
                    uint16_t code = randbits (8);
                    printf ("%12llu0 : TRAP %03o\n", cyclectr, code);
                    sendfetch (0104400 | code);
                    trapthrough (0034);
                    break;
                }

                // Bxx (p 75..93)
                case 60: {
                    uint16_t brcode;
                    do brcode = randbits (4);
                    while (brcode == 0);

                    bool n = (psw & 8) != 0;
                    bool z = (psw & 4) != 0;
                    bool v = (psw & 2) != 0;
                    bool c = (psw & 1) != 0;
                    bool brtrue;
                    switch (brcode >> 1) {
                        case 0: brtrue = false; break;
                        case 1: brtrue = ! z; break;                // BNE
                        case 2: brtrue = ! (n ^ v); break;          // BGE
                        case 3: brtrue = ! (n ^ v) & ! z; break;    // BGT
                        case 4: brtrue = ! n; break;                // BPL
                        case 5: brtrue = ! c & ! z; break;          // BHI
                        case 6: brtrue = ! v; break;                // BVC
                        case 7: brtrue = ! c; break;                // BCC
                    }
                    brtrue ^= (brcode & 1) != 0;

                    uint16_t opcode = randbits (8) | ((brcode & 8) << 12) | ((brcode & 7) << 8);
                    uint16_t newpc  = gprs[7] + ((int16_t) (int8_t) opcode) * 2 + 2;
                    printf ("%12llu0 : %s %06o  (%s)\n", cyclectr, brmnes[brcode], newpc, brtrue ? "true" : "false");
                    sendfetch (opcode);
                    if (brtrue) gprs[7] = newpc;
                    break;
                }

                // CCS (p 114)
                case 61: {
                    uint16_t bits = randbits (5);
                    printf ("%12llu0 : CCS %02o\n", cyclectr, bits);
                    sendfetch (0000240 | bits);
                    if (bits & 020) writepsw (psw |    bits & 017);
                               else writepsw (psw & ~ (bits & 017));
                    break;
                }

                // modify mmu register
                // must be in kernel mode and PC must point to a usable address so we don't have to worry about faults
                case 63: {
                    if (((psw & 0140000) == 0) && (gprs[7] <= 0157777) && ((gprs[7] & 0017777) <= 0017370) && ((gprs[7] & 0017777) >= 0000200)) {
                        uint16_t n = randbits (6);
                        switch (n >> 4) {

                            // increment address register
                            case 0: {
                                uint16_t regva = ((n & 8) ? 0177640 : 0172340) + (n & 7) * 2;
                                if (n == 7) {   // leave kernel 160000 -> 760000 intact
                                    vfyintreg (regva, pars[7]);
                                } else {
                                    uint16_t incby = 1;
                                    uint16_t oldpar = pars[n&15];
                                    uint16_t newpar;
                                    if (pars[n&15] < 07377) {
                                        newpar = oldpar + 1;
                                        printf ("%12llu0 : INC @#%06o\n", cyclectr, regva);
                                        sendfetch (0005237);
                                    } else {
                                        incby = 00200 + randbits (5);
                                        newpar = oldpar + incby;
                                        printf ("%12llu0 : ADD #%06o,@#%06o\n", cyclectr, incby, regva);
                                        sendfetch (0062737);
                                        sendword (gprs[7], incby, psw >> 14, false, "par increment");
                                        gprs[7] += 2;
                                        psw &= ~ 001;
                                    }
                                    sendword (gprs[7], regva, psw >> 14, false, "par address");
                                    gprs[7] += 2;
                                    monintrd (regva | 0760000, oldpar, "old par contents");
                                    monintwr (regva | 0760000, newpar, "new par contents");
                                    setpdrw ('Z', n & 15, 0);          // clear W bit upon writing address register
                                    if (mmr0 & 1) setpdrw ('Y', 7, 1); // set IO page W bit
                                    psw = (psw & ~ 016) | ((newpar & 0100000) ? 010 :0) | ((newpar == 0) ? 004 : 0);
                                    pars[n&15]  = newpar & 0007777;
                                }
                                break;
                            }

                            // verify descriptor register contents
                            case 1: {
                                uint16_t regva = ((n & 8) ? 0177600 : 0172300) + (n & 7) * 2;
                                // for pdrs[7] or half the time, just verify contents by reading and comparing
                                if (((n & 15) == 7) || randbits (1)) {
                                    vfyintreg (regva, pdrs[n&15]);
                                }
                                // otherwise, use 1 of 4 descriptors
                                else {
                                    static uint16_t const cannedpdrs[] = {
                                        077406,     // full 4KW read/write access
                                        077402,     // full 4KW read-only access
                                        076006,     // missing highest 64W read/write
                                        000416      // missing lowest 32W read/write
                                    };
                                    uint16_t oldpdr = pdrs[n&15];
                                    uint16_t newpdr = cannedpdrs[randbits(2)];
                                    uint16_t addend = newpdr - oldpdr;
                                    printf ("%12llu0 : ADD #%06o,@#%06o\n", cyclectr, addend, regva);
                                    sendfetch (062737);
                                    sendword (gprs[7], addend, psw >> 14, false, "addto par value");
                                    gprs[7] += 2;
                                    sendword (gprs[7], regva, psw >> 14, false, "pdr address");
                                    gprs[7] += 2;
                                    monintrd (regva | 0760000, oldpdr, "old par contents");
                                    monintwr (regva | 0760000, newpdr, "new par contents");
                                    setpdrw ('Z', n & 15, 0);          // clear W bit upon writing descriptor register
                                    if (mmr0 & 1) setpdrw ('Y', 7, 1); // set IO page W bit
                                    psw = (psw & ~ 017) | ((newpdr & 0100000) ? 010 :0) | ((newpdr == 0) ? 004 : 0) | ((newpdr < oldpdr) ? 001 : 0);
                                    pdrs[n&15] = newpdr;
                                }
                                break;
                            }

                            // access MMR0, MMR2
                            case 2: {
                                // verify MMR0 contents
                                if (n == 040) vfyintreg (0177572, mmr0);

                                // flip MMR0 enable bit, clear abort bits
                                if (n == 041) {
                                    uint16_t newmmr0 = (mmr0 & 0157) ^ 1;
                                    printf ("%12llu0 : MOV #%06o,@#%06o\n", cyclectr, newmmr0, 0177572);
                                    sendfetch (0012737);
                                    sendword (gprs[7], newmmr0, psw >> 14, false, "new MMR0 value");
                                    gprs[7] += 2;
                                    sendword (gprs[7], 0177572, psw >> 14, false, "MMR0 address");
                                    gprs[7] += 2;
                                    monintwr (0777572, newmmr0, "writing MMR0");
                                    if (mmr0 & 1) setpdrw ('X', 7, 1);      // set IO page W bit
                                    mmr0 = newmmr0;
                                    psw  = (psw & 0177761) | ((newmmr0 & 0100000) ? 010 : 000) | ((newmmr0 == 0) ? 004 : 000);
                                }

                                // verify MMR2 contents
                                if (n == 042) vfyintreg (0177576, mmr2);
                                break;
                            }
                        }
                    }
                    break;
                }
            }
        } catch (TrapThru &tt) {
            trapthrough (tt.vector);
        }

        if (didsomething) {

            // end-of-instruction traps
            while (true) {
                     if (((psw & 0340) < 0340) && ! (vp.bus_br_in_l & 010)) dointreq (7);
                else if (((psw & 0340) < 0300) && ! (vp.bus_br_in_l & 004)) dointreq (6);
                else if (((psw & 0340) < 0240) && ! (vp.bus_br_in_l & 002)) dointreq (5);
                else if (((psw & 0340) < 0200) && ! (vp.bus_br_in_l & 001)) dointreq (4);
                else if (! didrtt && (psw & 020)) trapthrough (0014);
                else break;
            }
        }
    }
    return 0;
}

// generate random DD operand
// if memory address, make sure it is in range and for word, it is even
uint16_t genranddd (bool word, uint16_t ss)
{
    bool randok = (randbits (7) != 0);
    bool oddbad = word & randok;

    while (true) {

        // get a random dd field
        uint16_t dd = randbits (6);
        uint16_t dr = dd & 7;
        uint16_t dv = gprs[curgprx(dr)];

        // maybe ss field will modify the register
        uint16_t sinc = (word || ((ss & 6) == 6)) ? 2 : 1;
        if (ss == 020 + dr) dv += sinc;
        if (ss == 030 + dr) dv += 2;
        if (ss == 040 + dr) dv -= sinc;
        if (ss == 050 + dr) dv -= 2;

        // make sure we can use it
        switch (dd >> 3) {
            case 0: break;
            case 1: case 2: {
                if (dv > 0157777) continue;
                if (dv & (uint16_t) oddbad) continue;
                break;
            }
            case 3: {
                if (dv > 0157777) continue;
                if (dv & (uint16_t) randok) continue;
                break;
            }
            case 4: {
                uint16_t dinc = (word || ((dr & 6) == 6)) ? 2 : 1;
                dv -= dinc;
                if (dv > 0157777) continue;
                if (dv & oddbad) continue;
                if ((dd == 046) && (dv < 0410)) continue;   // avoid yellow stack
                break;
            }
            case 5: {
                dv -= 2;
                if (dv > 0157777) continue;
                if (dv & (uint16_t) randok) continue;
                break;
            }
            case 6: case 7: break;
        }
        return dd;
    }
}

// send any index word or indirect pointer to processor
// return resultant virtual address
uint16_t sendfetchdd (uint16_t dd, bool word)
{
    bool randok = (randbits (7) != 0);
    bool oddbad = word & randok;

    switch (dd >> 3) {
        case 1: {
            return gprs[curgprx(dd&7)];
        }
        case 2: {
            uint16_t x = gprs[curgprx(dd&7)];
            gprs[curgprx(dd&7)] = x + (word || ((dd & 6) == 6) ? 2 : 1);
            return x;
        }
        case 3: {
            uint16_t x = gprs[curgprx(dd&7)];
            gprs[curgprx(dd&7)] = x + 2;
            uint16_t y;
            do y = randbits (16);
            while ((y > 0157777) || (y & (uint16_t) oddbad));
            sendword (x, y, psw >> 14, false, "@(Rx)+ pointer");
            return y;
        }
        case 4: {
            uint16_t x = gprs[curgprx(dd&7)] - (word || ((dd & 6) == 6) ? 2 : 1);
            gprs[curgprx(dd&7)] = x;
            return x;
        }
        case 5: {
            uint16_t x = gprs[curgprx(dd&7)] - 2;
            gprs[curgprx(dd&7)] = x;
            uint16_t y;
            do y = randbits (16);
            while ((y > 0157777) || (y & (uint16_t) oddbad));
            sendword (x, y, psw >> 14, false, "@-(Rn) pointer");
            return y;
        }
        case 6: {
            uint16_t x = gprs[curgprx(dd&7)];
            if ((dd & 7) == 7) x += 2;
            uint16_t y, z;
            do {
                y = randbits (16);
                z = x + y;
            } while ((z > 0157777) || (z & (uint16_t) oddbad));
            gprs[7] += 2;
            sendword (gprs[7] - 2, y, psw >> 14, false, "index");
            return x + y;
        }
        case 7: {
            uint16_t x = gprs[curgprx(dd&7)];                   // register being indexed
            if ((dd & 7) == 7) x += 2;                          // if PC, index after word fetched
            uint16_t y, z;
            do {
                y = randbits (16);                              // generate random index value
                z = x + y;                                      // compute effective address
            } while ((z > 0157777) || (z & (uint16_t) randok)); // repeat if not in range or odd
            uint16_t p;                                         // generate random pointer
            do p = randbits (16);
            while ((p > 0157777) || (p & (uint16_t) oddbad));   // repeat if not in range
            gprs[7] += 2;                                       // increment before read in case of exception
            sendword (gprs[7] - 2, y, psw >> 14, false, "index");  // send random index
            sendword (z, p, psw >> 14, false, "@x(Rn) pointer");   // send random pointer
            return p;                                           // return pointer
        }
        default: abort ();
    }
}

void dointreq (uint16_t brlev)
{
    printf ("dointreq: br level %o\n", brlev);
    for (int i = 0; ! (vp.bus_bg_out_h & (1 << (brlev - 4))); i ++) {
        if (i > 200) fatal ("dointreq: did not see BG%o\n", brlev);
        kerchunk ();
    }

    uint16_t vector;
    do vector = randbits (4) * 4;
    while (vector == 0);

    vp.bus_bbsy_in_l    = 0;
    vp.bus_br_in_l     |= 1 << (brlev - 4);
    vp.bus_d_in_l    = ~ vector;
    vp.bus_intr_in_l    = 0;
    vp.bus_msyn_in_l = 0;
    vp.bus_sack_in_l    = 0;
    for (int i = 0; vp.bus_ssyn_out_l; i ++) {
        if (i > 200) fatal ("dointreq: did not see SSYN\n");
        kerchunk ();
    }

    vp.bus_bbsy_in_l    = 1;
    vp.bus_d_in_l    = 0177777;
    vp.bus_intr_in_l    = 1;
    vp.bus_msyn_in_l = 1;
    vp.bus_sack_in_l    = 1;
    for (int i = 0; ! vp.bus_ssyn_out_l; i ++) {
        if (i > 200) fatal ("dointreq: SSYN stuck on\n");
        kerchunk ();
    }

    trapthrough (vector);
}

// verify the processor doing a trap sequence through the given vector
// supply random vector contents
void trapthrough (uint16_t vector)
{
    printf ("trapthrough: vector %03o\n", vector);
dotrap:;
    bool allowdouble = false;
    try {
        uint16_t newpc;
        do newpc = randbits (15) * 2;
        while (newpc > 0157777);

        uint16_t newps = randbits (8) & 0357;       // no T bit
        if (randbits (3) == 0) {
            newps |= psw & 0140000;                 // sometimes trap to user mode
        }
        newps |= ((newps | psw) >> 2) & 0030000;    // newps[prevmode] = newps[currmode] | oldps[currmode]

        uint16_t newsp = gprs[gprx(6,newps>>14)];   // stack pointer in new processor mode

        sendword (vector,     newpc,  0, false, "trap vec pc");
        sendword (vector | 2, newps,  0, false, "trap vec ps");

        if (newps & 0140000) allowdouble = true;

        recvword (newsp - 2, psw,     newps >> 14, "push trap ps");
        recvword (newsp - 4, gprs[7], newps >> 14, "push trap pc");
        gprs[gprx(6,newps>>14)] = newsp - 4;

        gprs[7] = newpc;
        writepsw (newps);
    } catch (TrapThru tt) {

        printf ("trapthrough: double trap %03o\n", tt.vector);
        if (allowdouble) {
            vector = tt.vector;
            goto dotrap;
        }

        for (int i = 0; ! vp.halt_grant_out_h; i ++) {
            if (i > 200) fatal ("trapthrough: failed to halt for double trap\n");
            kerchunk ();
        }
        restart ();
    }
}

// processor is halted, initialize registers and start it up
void restart ()
{
    printf ("restart: apply reset pulse\n");
    vp.halt_rqst_in_l = 0;
    vp.RESET = 1;
    kerchunk ();
    kerchunk ();
    vp.RESET = 0;
    kerchunk ();
    kerchunk ();
    if (! vp.halt_grant_out_h) fatal ("restart: halt not granted\n");

    mmr0 = 0;
    psw  = 0;

    printf ("restart: initialize gp registers\n");
    for (int i = 0; i < 16; i ++) {
        uint16_t r = randbits (16);
        if ((i == 6) || (i == 7) || (i == 14)) {
            r &= 007776;    // make sure SP,PC are decent
            r |= 004000;    // ...so we don't get odd addr / timeout traps constantly
        }
        writeword (0777700 + i, r);
        gprs[i] = r;
    }

    printf ("restart: initialize mm registers\n");
    setpdrw ('W', 7, 0);
    for (int i = 0; i < 16; i ++) {
        pars[i] = i * 0123 + 5;
        pdrs[i] = 0077406;
    }
    pars[7] = 007600;   // always map kernel 160000->760000

    for (int i = 0; i < 8; i ++) {
        writeword (0772300 + i * 2, pdrs[i+0]);     // kernel descriptor
        writeword (0772340 + i * 2, pars[i+0]);     // kernel address
        writeword (0777600 + i * 2, pdrs[i+8]);     // user descriptor
        writeword (0777640 + i * 2, pars[i+8]);     // user address
    }

    printf ("restart: negating halt_rqst_l\n");
    vp.halt_rqst_in_l = 1;
    for (int i = 0; vp.halt_grant_out_h; i ++) {
        if (i > 200) fatal ("restart: halt grant stuck on\n");
        kerchunk ();
    }
}

void writepsw (uint16_t newpsw)
{
    psw = newpsw;
}

// verify internal register's contents
void vfyintreg (uint16_t regva, uint16_t regda)
{
    printf ("%12llu0 : TST @#%06o\n", cyclectr, regva);
    sendfetch (0005737);
    sendword (gprs[7], regva, psw >> 14, false, "address");
    gprs[7] += 2;
    monintrd (regva | 0760000, regda, "contents");
    psw = (psw & ~ 017) | ((regda & 0100000) ? 010 : 0) | ((regda == 0) ? 004 : 0);
}

// send opcode word to processor and increment local copy of PC
void sendfetch (uint16_t data)
{
    sendword (gprs[7], data, psw >> 14, false, "opcode");
    gprs[7] += 2;
}

// PDP is reading a word from memory
// send the value we want it to get
//  input:
//   virtaddr = virtual address PDP is supposedly reading from
//   data = data value to send to PDP
//   mode = processor mode used to translate virtaddr to physical address
//   rmw = false: DATI cycle; true: DATIP cycle
//  output:
//   processor stepped to end of cycle
//   data sent to processor
void sendword (uint16_t virtaddr, uint16_t data, uint16_t mode, bool rmw, char const *desc)
{
    printf ("- sendword %06o %06o  %s\n", virtaddr, data, desc);

    if (virtaddr & 1) {
        printf ("sendword: odd address %06o\n", virtaddr);
        throw TrapThru (004);
    }

    uint32_t physaddr = virt2phys (virtaddr, rmw, mode);

    for (int i = 0; vp.bus_msyn_out_l; i ++) {
        if (i > 200) fatal ("sendword: vp.bus_MSYN did not assert\n");
        kerchunk ();
    }

    if ((vp.bus_a_out_l ^ 0777777) != physaddr) fatal ("sendword: expected BUS_A %06o got %06o\n", physaddr, vp.bus_a_out_l ^ 0777777);
    uint16_t cexpect = rmw ? 1 : 0;
    if ((vp.bus_c_out_l ^ 3) != cexpect) fatal ("sendword: expected BUS_C %o was %o\n", cexpect, vp.bus_c_out_l ^ 3);

    vp.bus_d_in_l = data ^ 0177777;
    vp.bus_ssyn_in_l = 0;
    for (int i = 0; ! vp.bus_msyn_out_l; i ++) {
        if (i > 200) fatal ("sendword: vp.bus_MSYN did not negate\n");
        kerchunk ();
    }

    vp.bus_ssyn_in_l = 1;
    vp.bus_d_in_l = 0177777;
}

// PDP is reading a byte from memory
// send the value we want it to get
void sendbyte (uint16_t virtaddr, uint8_t data, uint16_t mode, bool rmw, char const *desc)
{
    printf ("- sendbyte %06o %03o  %s\n", virtaddr, data, desc);

    uint32_t physaddr = virt2phys (virtaddr, rmw, mode);

    for (int i = 0; vp.bus_msyn_out_l; i ++) {
        if (i > 200) fatal ("sendbyte: vp.bus_MSYN did not assert\n");
        kerchunk ();
    }

    if ((vp.bus_a_out_l ^ 0777777) != physaddr) fatal ("sendbyte: expected BUS_A %06o got %06o\n", physaddr, vp.bus_a_out_l ^ 0777777);
    uint16_t cexpect = rmw ? 1 : 0;
    if ((vp.bus_c_out_l ^ 3) != cexpect) fatal ("sendword: expected BUS_C %o was %o\n", cexpect, vp.bus_c_out_l ^ 3);

    vp.bus_d_in_l = (((uint16_t) data) << ((physaddr & 1) * 8)) ^ 0177777;
    vp.bus_ssyn_in_l = 0;
    for (int i = 0; ! vp.bus_msyn_out_l; i ++) {
        if (i > 200) fatal ("sendbyte: vp.bus_MSYN did not negate\n");
        kerchunk ();
    }

    vp.bus_ssyn_in_l = 1;
    vp.bus_d_in_l = 0177777;
}

// receive a word over unibus from the pdp and check its value
void recvword (uint16_t virtaddr, uint16_t data, uint16_t mode, char const *desc)
{
    printf ("- recvword %06o %06o  %s\n", virtaddr, data, desc);

    if (virtaddr & 1) {
        printf ("recvword: odd address %06o\n", virtaddr);
        throw TrapThru (004);
    }

    uint32_t physaddr = virt2phys (virtaddr, true, mode);

    for (int i = 0; vp.bus_msyn_out_l; i ++) {
        if (i > 200) fatal ("recvword: vp.bus_MSYN did not assert\n");
        kerchunk ();
    }
    if ((vp.bus_a_out_l ^ 0777777) != physaddr) fatal ("recvword: expected BUS_A %06o got %06o\n", physaddr, vp.bus_a_out_l ^ 0777777);
    if ((vp.bus_c_out_l ^ 3) != 2) fatal ("recvword: expected BUS_C 2 was %o\n", vp.bus_c_out_l ^ 3);
    uint16_t got = vp.bus_d_out_l ^ 0177777;
    if (got != data) fatal ("recvword: expected BUS_D %06o got %06o\n", data, got);
    vp.bus_ssyn_in_l = 0;
    for (int i = 0; ! vp.bus_msyn_out_l; i ++) {
        if (i > 200) fatal ("recvword: vp.bus_MSYN did not negate\n");
        kerchunk ();
    }
    vp.bus_ssyn_in_l = 1;
}

// receive a byte over unibus from the pdp and check its value
void recvbyte (uint16_t virtaddr, uint8_t data, uint16_t mode, char const *desc)
{
    printf ("- recvbyte %06o %03o  %s\n", virtaddr, data, desc);

    uint32_t physaddr = virt2phys (virtaddr, true, mode);

    for (int i = 0; vp.bus_msyn_out_l; i ++) {
        if (i > 200) fatal ("recvbyte: vp.bus_MSYN did not assert\n");
        kerchunk ();
    }
    if ((vp.bus_a_out_l ^ 0777777) != physaddr) fatal ("recvbyte: expected BUS_A %06o got %06o\n", physaddr, vp.bus_a_out_l ^ 0777777);
    if ((vp.bus_c_out_l ^ 3) != 3) fatal ("recvbyte: expected BUS_C 3 was %o\n", vp.bus_c_out_l ^ 3);
    uint8_t got = (vp.bus_d_out_l ^ 0177777) >> ((physaddr & 1) * 8);
    if (got != data) fatal ("recvbyte: expected BUS_D %03o got %03o\n", data, got);
    vp.bus_ssyn_in_l = 0;
    for (int i = 0; ! vp.bus_msyn_out_l; i ++) {
        if (i > 200) fatal ("recvbyte: vp.bus_MSYN did not negate\n");
        kerchunk ();
    }
    vp.bus_ssyn_in_l = 1;
}

// convert 16-bit virtual address to 18-bit physical address
uint32_t virt2phys (uint16_t virtaddr, bool wrt, uint16_t mode)
{
    uint32_t pa;

    if (mmr0 & 1) {
        uint16_t i = ((mode & 1) << 3) | (virtaddr >> 13);                      // par/pdr index
        uint16_t d = pdrs[i];                                                   // descriptor
        uint16_t a = 0;                                                         // aborts
        if (! (d & 2)) a |= 1 << 15;                                            // no-access page check
        else {
            if (d & 8) {
                if (((virtaddr >> 6) & 0177) < ((d >> 8) & 0177)) a |= 1 << 14; // expand downward
            } else {
                if (((virtaddr >> 6) & 0177) > ((d >> 8) & 0177)) a |= 1 << 14; // expand upward
            }
        }
        if ((a == 0) && wrt && ! (d & 4)) a |= 1 << 13;                         // write-protect
        if (a != 0) {
            if ((mmr0 & 0160000) == 0) {
                mmr0 = a | ((mode & 3) << 5) | ((virtaddr >> 13) << 1) | 1;
            }
            throw TrapThru (0250);
        }
        if (wrt) setpdrw ('V', i, 1);
        pa = ((uint32_t) pars[i] << 6) + (virtaddr & 0017777);
    } else {
        pa = (virtaddr > 0157777) ? virtaddr | 0600000 : virtaddr;
    }

    // random instructions are hands-off IO page
    if (pa > 0757777) {
        printf ("virt2phys: bus timeout %06o\n", pa);
        for (int i = 0; vp.bus_msyn_out_l; i ++) {
            if (i > 200) fatal ("virt2phys: vp.bus_MSYN did not assert\n");
            kerchunk ();
        }
        for (int i = 0; ! vp.bus_msyn_out_l; i ++) {
            if (i > 20000) fatal ("virt2phys: vp.bus_MSYN did not negate\n");
            kerchunk ();
        }
        throw TrapThru (004);
    }

    return pa;
}

void setpdrw (char ident, int index, int value)
{
    pdrs[index] = (pdrs[index] & ~ 0100) | (value << 6);
}

// just stuffed an instruction into processor that causes it to read from an internal register
// clock the processor through that read and make sure the address, function and data match what we expect
void monintrd (uint32_t physaddr, uint16_t data, char const *desc)
{
    printf ("- monintrd %06o %06o  %s\n", physaddr, data, desc);

    // wait for processor to place address, function out on unibus
    // meanwhile pass address, function back into processor so internal register can decode them
    for (int i = 0; vp.bus_msyn_out_l; i ++) {
        if (i > 200) fatal ("monintrd: vp.bus_MSYN did not assert\n");
        vp.bus_a_in_l = vp.bus_a_out_l;
        vp.bus_c_in_l = vp.bus_c_out_l;
        vp.bus_d_in_l = vp.bus_d_out_l;
        kerchunk ();
    }

    // verify the values match what we expect
    if ((vp.bus_a_out_l ^ 0777777) != physaddr) fatal ("monintrd: expected BUS_A %06o got %06o\n", physaddr, vp.bus_a_out_l ^ 0777777);
    if ((vp.bus_c_out_l & 2) == 0) fatal ("monintrd: expected BUS_C[1] 1 was 0\n");

    // pass MSYN back into processor so internal register will finialize decoding
    vp.bus_msyn_in_l = 0;

    // wait for the internal registers to complete the read by ass_serting ssyn
    for (int i = 0; vp.bus_ssyn_out_l; i ++) {
        if (vp.bus_msyn_out_l) fatal ("monintrd: vp.bus_MSYN negated while waiting for SSYN\n");
        if (i > 200) fatal ("monintrd: vp.bus_SSYN did not assert\n");
        kerchunk ();
    }

    // verify internal register has contents we expect
    uint16_t got = vp.bus_d_out_l ^ 0177777;
    if (got != data) fatal ("monintrd: expected BUS_D %06o got %06o\n", data, got);

    // wait for both MSYN and SSYN to be negated
    // meanwhile forward data from internal register to the processor
    for (int i = 0; ! vp.bus_msyn_out_l || ! vp.bus_ssyn_out_l; i ++) {
        if (i > 200) fatal ("monintrd: vp.bus_MSYN or vp.bus_SSYN did not negate\n");
        vp.bus_d_in_l    = vp.bus_d_out_l;
        vp.bus_msyn_in_l = vp.bus_msyn_out_l;
        vp.bus_ssyn_in_l = vp.bus_ssyn_out_l;
        kerchunk ();
    }
    vp.bus_msyn_in_l = 1;
    vp.bus_ssyn_in_l = 1;

    // stop forwarding address, function, data
    vp.bus_a_in_l = 0777777;
    vp.bus_c_in_l = 3;
    vp.bus_d_in_l = 0177777;
}

// just stuffed an instruction into processor that causes it to write to an internal register
// clock the processor through that write and make sure the address, function and data match what we expect
void monintwr (uint32_t physaddr, uint16_t data, char const *desc)
{
    printf ("- monintwr %06o %06o  %s\n", physaddr, data, desc);

    // wait for processor to place address, function, data out on unibus
    // meanwhile pass address, function, data back into processor so internal register can decode them
    for (int i = 0; vp.bus_msyn_out_l; i ++) {
        if (i > 200) fatal ("monintwr: vp.bus_MSYN did not assert\n");
        vp.bus_a_in_l = vp.bus_a_out_l;
        vp.bus_c_in_l = vp.bus_c_out_l;
        vp.bus_d_in_l = vp.bus_d_out_l;
        kerchunk ();
    }

    // verify the values match what we expect
    if ((vp.bus_a_out_l ^ 0777777) != physaddr) fatal ("monintwr: expected BUS_A %06o got %06o\n", physaddr, vp.bus_a_out_l ^ 0777777);
    if ((vp.bus_c_out_l ^ 3) != 2) fatal ("monintwr: expected BUS_C 2 was %o\n", vp.bus_c_out_l ^ 3);
    uint16_t got = vp.bus_d_out_l ^ 0177777;
    if (got != data) fatal ("monintwr: expected BUS_D %06o got %06o\n", data, got);

    // pass MSYN back into processor so internal register will finialize decoding
    vp.bus_msyn_in_l = 0;

    // wait for the internal registers to complete the write by asserting ssyn
    for (int i = 0; vp.bus_ssyn_out_l; i ++) {
        if (vp.bus_msyn_out_l) fatal ("monintwr: vp.bus_MSYN negated while waiting for SSYN\n");
        if (i > 200) fatal ("monintwr: vp.bus_SSYN did not assert\n");
        kerchunk ();
    }

    // wait for both MSYN and SSYN to be negated
    for (int i = 0; ! vp.bus_msyn_out_l || ! vp.bus_ssyn_out_l; i ++) {
        if (i > 200) fatal ("monintwr: vp.bus_MSYN or vp.bus_SSYN did not negate\n");
        vp.bus_msyn_in_l = vp.bus_msyn_out_l;
        vp.bus_ssyn_in_l = vp.bus_ssyn_out_l;
        kerchunk ();
    }
    vp.bus_msyn_in_l = 1;
    vp.bus_ssyn_in_l = 1;

    // stop forwarding address, function, data
    vp.bus_a_in_l = 0777777;
    vp.bus_c_in_l = 3;
    vp.bus_d_in_l = 0177777;
}

// read word from unibus via dma
void readword (uint32_t physaddr, uint16_t data)
{
    printf ("- readword %06o %06o\n", physaddr, data);

    // tell pdp we want to do dma cycle
    vp.bus_npr_in_l = 0;

    // wait for pdp to say it's ok
    for (int i = 0; ! vp.bus_npg_out_h; i ++) {
        if (i > 200) fatal ("readword: vp.bus_NPG did not assert\n");
        kerchunk ();
    }

    // acknowledge selection; drop request; output address and function
    vp.bus_bbsy_in_l = 0;
    vp.bus_a_in_l = physaddr ^ 0777777;
    vp.bus_c_in_l = 3;
    vp.bus_npr_in_l  = 1;
    vp.bus_sack_in_l = 0;

    // let address and function soak into bus and decoders
    for (int i = 0; i < 15; i ++) kerchunk ();

    // assert MSYN so slaves know address and function are valid
    vp.bus_msyn_in_l = 0;

    // wait for slave to reply with data
    for (int i = 0; vp.bus_ssyn_out_l; i ++) {
        if (i > 1000) fatal ("readword: vp.bus_SSYN did not assert\n");
        kerchunk ();
    }

    // let data soak into bus
    for (int i = 0; i < 8; i ++) kerchunk ();

    // check data is what we expect
    if ((vp.bus_d_out_l ^ 0177777) != data) fatal ("readword: expected BUS_D %06o got %06o\n", data, vp.bus_d_out_l ^ 0177777);

    // tell slave we got the data
    vp.bus_msyn_in_l = 1;
    vp.bus_sack_in_l = 1;

    // let msyn soak into bus
    for (int i = 0; i < 8; i ++) kerchunk ();

    // wait for slave to finish up
    for (int i = 0; ! vp.bus_ssyn_out_l; i ++) {
        if (i > 1000) fatal ("readword: vp.bus_SSYN did not negate\n");
        kerchunk ();
    }

    // drop everything else
    vp.bus_a_in_l = 0777777;
    vp.bus_bbsy_in_l = 1;
}

void writeword (uint32_t physaddr, uint16_t data)
{
    printf ("- writeword %06o %06o\n", physaddr, data);

    if (! vp.halt_grant_out_h) {

        // tell pdp we want to do dma cycle
        vp.bus_npr_in_l = 0;

        // wait for pdp to say it's ok
        for (int i = 0; ! vp.bus_npg_out_h; i ++) {
            if (i > 200) fatal ("writeword: vp.bus_NPG did not assert\n");
            kerchunk ();
        }
    }

    // acknowledge selection; drop request; output address, data and function
    vp.bus_bbsy_in_l = 0;
    vp.bus_a_in_l = physaddr ^ 0777777;
    vp.bus_c_in_l = 1;
    vp.bus_d_in_l = data ^ 0177777;
    vp.bus_npr_in_l  = 1;
    vp.bus_sack_in_l = 0;

    // let address and function soak into bus and decoders
    for (int i = 0; i < 15; i ++) kerchunk ();

    // assert MSYN so slaves know address and function are valid
    vp.bus_msyn_in_l = 0;

    // wait for slave to accept the data
    for (int i = 0; vp.bus_ssyn_out_l; i ++) {
        if (i > 1000) fatal ("writeword: vp.bus_SSYN did not assert\n");
        kerchunk ();
    }

    // tell slave we are removing address and data
    vp.bus_msyn_in_l = 1;
    vp.bus_sack_in_l = 1;

    // let msyn soak into bus
    for (int i = 0; i < 8; i ++) kerchunk ();

    // wait for slave to finish up
    for (int i = 0; ! vp.bus_ssyn_out_l; i ++) {
        if (i > 1000) fatal ("writeword: vp.bus_SSYN did not negate\n");
        kerchunk ();
    }

    // drop everything else
    vp.bus_a_in_l = 0777777;
    vp.bus_c_in_l = 3;
    vp.bus_d_in_l = 0177777;
    vp.bus_bbsy_in_l = 1;
}

// call with clock still low and input signals just changed
// returns with output signals updated and clock just set low
void kerchunk ()
{
    vp.eval ();     // let input signals soak in
    vp.CLOCK = 1;   // clock the state
    vp.eval ();     // let new state settle in
    vp.CLOCK = 0;   // get ready for more input changes

    ++ cyclectr;
    didsomething = true;
}

// MUL (p 147)
uint16_t mulinstr (uint16_t regno, uint16_t srcval)
{
    int16_t x = (int16_t) gprs[curgprx(regno)];
    int16_t y = (int16_t) srcval;
    int32_t prod = (int32_t) x * (int32_t) y;
    gprs[curgprx(regno)] = prod >> 16;
    gprs[curgprx(regno|1)] = prod;
    writepsw ((psw & 0177761) | ((int16_t) prod != prod));
    return (prod < 0) ? 0100000 : (prod != 0);
}

// DIV (p 148)
uint16_t divinstr (uint16_t regno, uint16_t srcval)
{
    bool overflow = true;
    int32_t quotient = 0;
    if (srcval != 0) {
        int32_t dividend = (gprs[curgprx(regno)] << 16) | gprs[curgprx(regno|1)];
        int16_t divisor  = srcval;
        bool divdneg = (dividend < 0);
        bool signbit = divdneg ^ (divisor < 0);
        if (dividend < 0) dividend = - dividend;
        if (divisor  < 0) divisor  = - divisor;
        quotient = (int32_t) ((uint32_t) dividend / (uint32_t) (uint16_t) divisor);
        int32_t remaindr = (int32_t) ((uint32_t) dividend % (uint32_t) (uint16_t) divisor);
        overflow = (dividend < 0) || (quotient > 32767);
        if (! overflow) {
            if (signbit) quotient  = - quotient;
            if (divdneg) remaindr  = - remaindr;
            gprs[curgprx(regno)]   = quotient;
            gprs[curgprx(regno|1)] = remaindr;
        }
    }
    writepsw ((psw & 0177774) | (overflow ? 002 : 0) | (srcval == 0));
    return quotient;
}

// ASH (p 149)
uint16_t ashinstr (uint16_t regno, uint16_t srcval)
{
    int16_t value = gprs[curgprx(regno)];
    int16_t count = ((srcval & 077) ^ 040) - 040;

    writepsw (psw & 0177775);

    if (count < 0) {
        do {
            writepsw ((psw & 0177776) | (value & 1));
            value >>= 1;
        } while (++ count < 0);
    }
    if (count > 0) {
        do {
            writepsw ((psw & 0177776) | ((value >> 15) & 1));
            int16_t newval = value << 1;
            if ((newval < 0) ^ (value < 0)) writepsw (psw | 2);
            value = newval;
        } while (-- count > 0);
    }

    gprs[curgprx(regno)] = value;

    return (value < 0) ? 0100000 : (value != 0);
}

// ASHC (p 150)
uint16_t ashcinstr (uint16_t regno, uint16_t srcval)
{
    int32_t value = (gprs[curgprx(regno)] << 16) | gprs[curgprx(regno|1)];
    int16_t count = ((srcval & 077) ^ 040) - 040;

    writepsw (psw & 0177775);

    if (count < 0) {
        do {
            writepsw ((psw & 0177776) | (value & 1));
            value >>= 1;
        } while (++ count < 0);
    }
    if (count > 0) {
        do {
            writepsw ((psw & 0177776) | ((value >> 31) & 1));
            int32_t newval = value << 1;
            if ((newval < 0) ^ (value < 0)) writepsw (psw | 2);
            value = newval;
        } while (-- count > 0);
    }

    gprs[curgprx(regno)] = value >> 16;
    gprs[curgprx(regno|1)] = value;

    return (value < 0) ? 0100000 : (value != 0);
}

// print out error message, dump fpga registers, then abort
void fatal (char const *fmt, ...)
{
    va_list ap;
    va_start (ap, fmt);
    vprintf (fmt, ap);
    va_end (ap);
    dumpstate ();
    abort ();
}

// print out fpga state
void dumpstate ()
{
    printf ("%12llu0:  RESET=%o  state=%02d  R0=%06o R1=%06o R2=%06o R3=%06o R4=%06o R5=%06o R6=%06o R7=%06o PS=%06o R16=%06o\n"
        "   bus_ac_lo_l=%o bus_bbsy_l=%o bus_br_l=%o%o%o%o bus_dc_lo_l=%o bus_intr_l=%o bus_npr_l=%o bus_sack_l=%o halt_rqst_l=%o\n"
        "   bus_a_in_l=%06o bus_c_in_l=%o%o bus_d_in_l=%06o bus_init_in_l=%o bus_msyn_in_l=%o bus_ssyn_in_l=%o\n"
        "   bus_a_out_l=%06o bus_c_out_l=%o%o bus_d_out_l=%06o bus_init_out_l=%o bus_msyn_out_l=%o bus_ssyn_out_l=%o bus_bg_out_h=%o%o%o%o bus_npg_out_h=%o\n"
        "   halt_grant_out_h=%o\n\n",

                cyclectr,
                vp.RESET,
                vp.state,
                vp.r0,
                vp.r1,
                vp.r2,
                vp.r3,
                vp.r4,
                vp.r5,
                vp.r6,
                vp.r7,
                vp.ps,
                vp.r16,

                vp.bus_ac_lo_in_l,
                vp.bus_bbsy_in_l,
                (vp.bus_br_in_l >> 3) & 1, (vp.bus_br_in_l >> 2) & 1, (vp.bus_br_in_l >> 1) & 1, (vp.bus_br_in_l >> 0) & 1,
                vp.bus_dc_lo_in_l,
                vp.bus_intr_in_l,
                vp.bus_npr_in_l,
                vp.bus_sack_in_l,
                vp.halt_rqst_in_l,

                vp.bus_a_in_l,
                (vp.bus_c_in_l >> 1) & 1, (vp.bus_c_in_l >> 0) & 1,
                vp.bus_d_in_l,
                vp.bus_init_in_l,
                vp.bus_msyn_in_l,
                vp.bus_ssyn_in_l,

                vp.bus_a_out_l,
                (vp.bus_c_out_l >> 1) & 1, (vp.bus_c_out_l >> 0) & 1,
                vp.bus_d_out_l,
                vp.bus_init_out_l,
                vp.bus_msyn_out_l,
                vp.bus_ssyn_out_l,

                (vp.bus_bg_out_h >> 3) & 1, (vp.bus_bg_out_h >> 2) & 1, (vp.bus_bg_out_h >> 1) & 1, (vp.bus_bg_out_h >> 0) & 1,
                vp.bus_npg_out_h,
                vp.halt_grant_out_h);
}

// format the dd field to a string
// assumes ddbuff assumed to be at least 8 chars long
char *ddstr (char *ddbuff, uint16_t dd)
{
    char *p = ddbuff;
    if (dd & 8) *(p ++) = '@';
    switch (dd >> 4) {
        case 0: {
            *(p ++) = 'R';
            *(p ++) = '0' + (dd & 7);
            break;
        }
        case 1: {
            *(p ++) = '(';
            *(p ++) = 'R';
            *(p ++) = '0' + (dd & 7);
            *(p ++) = ')';
            *(p ++) = '+';
            break;
        }
        case 2: {
            *(p ++) = '-';
            *(p ++) = '(';
            *(p ++) = 'R';
            *(p ++) = '0' + (dd & 7);
            *(p ++) = ')';
            break;
        }
        case 3: {
            *(p ++) = 'x';
            *(p ++) = '(';
            *(p ++) = 'R';
            *(p ++) = '0' + (dd & 7);
            *(p ++) = ')';
            break;
        }
        default: abort ();
    }
    *p = 0;
    return ddbuff;
}

// generate a random number
uint32_t randbits (int nbits)
{
    static uint64_t seed = 0x123456789ABCDEF0ULL;

    uint16_t randval = 0;

    while (-- nbits >= 0) {

        // https://www.xilinx.com/support/documentation/application_notes/xapp052.pdf
        uint64_t xnor = ~ ((seed >> 63) ^ (seed >> 62) ^ (seed >> 60) ^ (seed >> 59));
        seed = (seed << 1) | (xnor & 1);

        randval += randval + (seed & 1);
    }

    return randval;
}
