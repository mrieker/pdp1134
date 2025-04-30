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
        result = function; \
        if (readdst)  sendword (ddva, dstval, "dst value"); \
        if (writedst) recvword (ddva, result, "dst value"); \
    } \
    psw = (psw & 0177760) | ((result & 0100000) ? 010 : 0) | ((result == 0) ? 004 : 0) | ((pswv) ? 002 : 0) | ((pswc) ? 001 : 0);

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
        result = function; \
        if (readdst)  sendbyte (ddva, dstval, "dst value"); \
        if (writedst) recvbyte (ddva, result, "dst value"); \
    } \
    psw = (psw & 0177760) | ((result & 0000200) ? 010 : 0) | ((result == 0) ? 004 : 0) | ((pswv) ? 002 : 0) | ((pswc) ? 001 : 0);

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
        sendword (ssva, srcval, "src value"); \
    } \
    uint16_t dstval, result; \
    if ((dd & 070) == 0) { \
        dstval = readdst ? gprs[curgprx(dd)] : 0; \
        result = function; \
        if (writedst) gprs[curgprx(dd)] = result; \
    } else { \
        uint16_t ddva = sendfetchdd (dd, true); \
        dstval = randbits (16); \
        result = function; \
        if (readdst)  sendword (ddva, dstval, "dst value"); \
        if (writedst) recvword (ddva, result, "dst value"); \
    } \
    psw = (psw & 0177760) | ((result & 0100000) ? 010 : 0) | ((result == 0) ? 004 : 0) | ((pswv) ? 002 : 0) | ((pswc) ? 001 : 0);

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
        sendbyte (ssva, srcval, "src value"); \
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
        result = function; \
        if (readdst)  sendbyte (ddva, dstval, "dst value"); \
        if (writedst) recvbyte (ddva, result, "dst value"); \
    } \
    psw = (psw & 0177760) | ((result & 0000200) ? 010 : 0) | ((result == 0) ? 004 : 0) | ((pswv) ? 002 : 0) | ((pswc) ? 001 : 0);


char const *const brmnes[] = {
        "BN",  "BR",  "BNE", "BEQ",  "BGE", "BLT", "BGT", "BLE",
        "BPL", "BMI", "BHI", "BLOS", "BVC", "BVS", "BCC", "BCS" };

bool didsomething;
long long unsigned cyclectr;
uint16_t gprs[16];
uint16_t psw;
Vpdp1134 vp;

uint16_t gprx (uint16_t r, uint16_t mode) { return (r == 6) && (mode & 2) ? 016 : r; }
uint16_t curgprx (uint16_t r) { return gprx (r, psw >> 14); }

uint16_t genranddd (bool word, uint16_t ss);
uint16_t sendfetchdd (uint16_t dd, bool word);
void sendfetch (uint16_t data);
void sendword (uint16_t virtaddr, uint16_t data, char const *desc);
void sendbyte (uint16_t virtaddr, uint8_t data, char const *desc);
void recvword (uint16_t virtaddr, uint16_t data, char const *desc);
void recvbyte (uint16_t virtaddr, uint8_t data, char const *desc);
uint32_t virt2phys (uint16_t virtaddr, bool wrt);
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

    vp.bus_ac_lo_l   = 1;
    vp.bus_bbsy_l    = 1;
    vp.bus_br_l      = 15;
    vp.bus_dc_lo_l   = 1;
    vp.bus_intr_l    = 1;
    vp.bus_npr_l     = 1;
    vp.bus_pa_l      = 1;
    vp.bus_pb_l      = 1;
    vp.bus_sack_l    = 1;
    vp.halt_rqst_l   = 1;

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

    printf ("asserting halt_rqst_l\n");
    vp.halt_rqst_l = 0;
    kerchunk ();
    kerchunk ();
    if (! vp.halt_grant_h) fatal ("halt not granted\n");

    printf ("initialize registers\n");
    for (int i = 0; i < 16; i ++) {
        uint16_t r = randbits (16);
        if ((i == 6) || (i == 7) || (i == 14)) {
            r &= 007776;    // make sure SP,PC are decent
            r |= 004000;    // ...so we don't get odd addr / timeout traps constantly
        }
        writeword (0777700 + i, r);
        gprs[i] = r;
    }

    printf ("negating halt_rqst_l\n");
    vp.halt_rqst_l = 1;
    for (int i = 0; vp.halt_grant_h; i ++) {
        if (i > 5) fatal ("halt grant stuck on\n");
        kerchunk ();
    }

    didsomething = true;

    while (true) {
        char ddbuff[8], ssbuff[8];

        if (didsomething) {

            // processor should be fetching
            for (int i = 0; vp.state != S_FETCH; i ++) {
                if (i > 100) fatal ("expecting state S_FETCH (%02u)\n", S_FETCH);
                kerchunk ();
            }
            if ((vp.r0 != gprs[0]) || (vp.r1 != gprs[1]) || (vp.r2 != gprs[2]) || (vp.r3 != gprs[3]) || (vp.r4 != gprs[4]) || (vp.r5 != gprs[5]) || (vp.r6 != gprs[6]) || (vp.r7 != gprs[7]) || (vp.ps != psw)) {
                fatal ("register mismatch - expect         R0=%06o R1=%06o R2=%06o R3=%06o R4=%06o R5=%06o R6=%06o R7=%06o PS=%06o\n",
                        gprs[0] ,gprs[1] ,gprs[2] ,gprs[3] ,gprs[4] ,gprs[5] ,gprs[6] ,gprs[7], psw);
            }

            printf ("R0=%06o R1=%06o R2=%06o R3=%06o R4=%06o R5=%06o R6=%06o R7=%06o PS=%06o\n",
                    gprs[0] ,gprs[1] ,gprs[2] ,gprs[3] ,gprs[4] ,gprs[5] ,gprs[6] ,gprs[7], psw);
            printf ("- - - - - - - - - - - - - - - - - - - -\n");
        }

        // generate random opcode and send to processor
        didsomething = false;
        uint8_t select = randbits (6);
        switch (select) {

            // HALT
            case  0: {
                printf ("%12llu0 : HALT\n", cyclectr);
                sendfetch (0000000);
                for (int i = 0; ! vp.halt_grant_h; i ++) {
                    if (i > 5) fatal ("did not halt\n");
                    kerchunk ();
                }
                vp.halt_rqst_l = 0;
                kerchunk ();
                kerchunk ();
                vp.halt_rqst_l = 1;
                for (int i = 0; vp.halt_grant_h; i ++) {
                    if (i > 5) fatal ("halt grant stuck on\n");
                    kerchunk ();
                }
                break;
            }

            /*
    wire iWAIT  = (instreg == 1);
    wire iRTI   = (instreg == 2);
    wire iBPT   = (instreg == 3);
    wire iIOT   = (instreg == 4);
    wire iRESET = (instreg == 5);
    wire iRTT   = (instreg == 6);
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
                psw = (psw & 0177760) | ((result & 0000200) ? 010 : 0) | (((result & 0000377) == 0) ? 004 : 0);
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
                recvword (gprs[curgprx(6)], oldreg, "push old register");
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
                    uint16_t newsp = gprs[7] + nn * 2;
                    gprs[7] = gprs[5];
                    uint16_t newr5 = randbits (16);
                    sendword (newsp, newr5, "push old R5");
                    gprs[5] = newr5;
                    gprs[curgprx(6)] = newsp + 2;
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
                    sendbyte (ddva, newpsb, "psb value");
                }
                if ((psw & 0140000) == 0) psw = (psw & 0177437) | (newpsb & 0340);
                psw = (psw & 0177761) | ((newpsb & 0200) ? 010 : 0) | ((newpsb == 0) ? 004 : 0);
                break;
            }

            // MFPI (p 168)
            case 36: {
                if (gprs[6] > 0410) {       // avoid yellow stack
                    uint16_t dd = genranddd (true, 0);
                    printf ("%12llu0 : MFPI %s\n", cyclectr, ddstr (ddbuff, dd));
                    sendfetch (0006500 | dd);
                    uint16_t value;
                    if ((dd & 070) == 0) {
                        value = gprs[gprx(dd,psw>>12)];
                    } else {
                        uint16_t ddva = sendfetchdd (dd, true);
                        value = randbits (16);
                        sendword (ddva, value, "dst value");
                    }
                    uint16_t newsp = gprs[curgprx(6)] - 2;
                    gprs[curgprx(6)] = newsp;
                    recvword (newsp, value, "push value");
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
                sendword (oldsp, value, "pop stack value");
                if ((dd & 070) == 0) {
                    gprs[gprx(dd,psw>>12)] = value;
                } else {
                    recvword (ddva, value, "dst value");
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
                    recvbyte (ddva, psw, "psb value");
                }
                psw = (psw & 0177761) | ((psw & 0000200) ? 010 : 0) | (((int8_t) psw == 0) ? 004 : 0);
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

/***
            // DIV (p 148)
            case 53: {
                uint16_t regno; do regno = randbits (2) * 2; while (regno == 6);
                char xrbuff[8];
                sprintf (xrbuff, "DIV R%o,", regno);
                SINGLEWORD (xrbuff, 0071000 | (regno << 6), true, false, divinstr (regno, dstval), psw & 2, psw & 1);
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
***/

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

/***
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
***/

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
                if (bits & 020) psw |=    bits & 017;
                           else psw &= ~ (bits & 017);
                break;
            }
        }
    }
    return 0;
}

// generate random DD operand
// if memory address, make sure it is in range and for word, it is even
uint16_t genranddd (bool word, uint16_t ss)
{
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
                if (dv & (uint16_t) word) continue;
                break;
            }
            case 3: {
                if (dv > 0157777) continue;
                if (dv & 1) continue;
                break;
            }
            case 4: {
                uint16_t dinc = (word || ((dr & 6) == 6)) ? 2 : 1;
                dv -= dinc;
                if (dv > 0157777) continue;
                if (dv & 1) continue;
                if ((dd == 046) && (dv < 0410)) continue;   // avoid yellow stack
                break;
            }
            case 5: {
                dv -= 2;
                if (dv > 0157777) continue;
                if (dv & 1) continue;
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
            uint16_t y;
            do y = randbits (16);
            while ((y > 0157777) || (y & (uint16_t) word));
            sendword (x, y, "@(Rx)+ pointer");
            gprs[curgprx(dd&7)] = x + 2;
            return y;
        }
        case 4: {
            uint16_t x = gprs[curgprx(dd&7)] - (word || ((dd & 6) == 6) ? 2 : 1);
            gprs[curgprx(dd&7)] = x;
            return x;
        }
        case 5: {
            uint16_t x = gprs[curgprx(dd&7)] - 2;
            uint16_t y;
            do y = randbits (16);
            while ((y > 0157777) || (y & (uint16_t) word));
            sendword (x, y, "@-(Rn) pointer");
            gprs[curgprx(dd&7)] = x;
            return y;
        }
        case 6: {
            uint16_t x = gprs[curgprx(dd&7)];
            if ((dd & 7) == 7) x += 2;
            uint16_t y, z;
            do {
                y = randbits (16);
                z = x + y;
            } while ((z > 0157777) || (z & (uint16_t) word));
            sendword (gprs[7], y, "index");
            gprs[7] += 2;
            return x + y;
        }
        case 7: {
            uint16_t x = gprs[curgprx(dd&7)];                   // register being indexed
            if ((dd & 7) == 7) x += 2;                          // if PC, index after word fetched
            uint16_t y, z;
            do {
                y = randbits (16);                              // generate random index value
                z = x + y;                                      // compute effective address
            } while ((z > 0157777) || (z & 1));                 // repeat if not in range or odd
            uint16_t p;                                         // generate random pointer
            do p = randbits (16);
            while ((p > 0157777) || (p & (uint16_t) word));     // repeat if not in range
            sendword (gprs[7], y, "index");                     // send random index
            gprs[7] += 2;
            sendword (z, p, "@x(Rn) pointer");                  // send random pointer
            return p;                                           // return pointer
        }
        default: abort ();
    }
}

// send opcode word to processor and increment local copy of PC
void sendfetch (uint16_t data)
{
    sendword (gprs[7], data, "opcode");
    gprs[7] += 2;
}

// PDP is reading a word from memory
// send the value we want it to get
void sendword (uint16_t virtaddr, uint16_t data, char const *desc)
{
    printf ("- sendword %06o %06o  %s\n", virtaddr, data, desc);

    uint32_t physaddr = virt2phys (virtaddr, false);

    for (int i = 0; vp.bus_msyn_out_l; i = i + 1) {
        if (i > 200) fatal ("sendword: vp.bus_MSYN did not assert\n");
        kerchunk ();
    }

    if ((vp.bus_a_out_l ^ 0777777) != physaddr) fatal ("sendword: expected BUS_A %06o got %06o\n", physaddr, vp.bus_a_out_l ^ 0777777);
    if (! (vp.bus_c_out_l & 2)) fatal ("sendword: expected BUS_C[1] 0 was 1\n");

    vp.bus_d_in_l = data ^ 0177777;
    vp.bus_ssyn_in_l = 0;
    for (int i = 0; ! vp.bus_msyn_out_l; i = i + 1) {
        if (i > 200) fatal ("sendword: vp.bus_MSYN did not negate\n");
        kerchunk ();
    }

    vp.bus_ssyn_in_l = 1;
    vp.bus_d_in_l = 0177777;
}

// PDP is reading a byte from memory
// send the value we want it to get
void sendbyte (uint16_t virtaddr, uint8_t data, char const *desc)
{
    printf ("- sendbyte %06o %03o  %s\n", virtaddr, data, desc);

    uint32_t physaddr = virt2phys (virtaddr, false);

    for (int i = 0; vp.bus_msyn_out_l; i = i + 1) {
        if (i > 200) fatal ("sendbyte: vp.bus_MSYN did not assert\n");
        kerchunk ();
    }

    if ((vp.bus_a_out_l ^ 0777777) != physaddr) fatal ("sendbyte: expected BUS_A %06o got %06o\n", physaddr, vp.bus_a_out_l ^ 0777777);
    if (! (vp.bus_c_out_l & 2)) fatal ("sendbyte: expected BUS_C[1] 0 was 1\n");

    vp.bus_d_in_l = (((uint16_t) data) << ((physaddr & 1) * 8)) ^ 0177777;
    vp.bus_ssyn_in_l = 0;
    for (int i = 0; ! vp.bus_msyn_out_l; i = i + 1) {
        if (i > 200) fatal ("sendbyte: vp.bus_MSYN did not negate\n");
        kerchunk ();
    }

    vp.bus_ssyn_in_l = 1;
    vp.bus_d_in_l = 0177777;
}

// receive a word over unibus from the pdp and check its value
void recvword (uint16_t virtaddr, uint16_t data, char const *desc)
{
    printf ("- recvword %06o %06o  %s\n", virtaddr, data, desc);

    uint32_t physaddr = virt2phys (virtaddr, true);

    for (int i = 0; vp.bus_msyn_out_l; i = i + 1) {
        if (i > 200) fatal ("recvword: vp.bus_MSYN did not assert\n");
        kerchunk ();
    }
    if ((vp.bus_a_out_l ^ 0777777) != physaddr) fatal ("recvword: expected BUS_A %06o got %06o\n", physaddr, vp.bus_a_out_l ^ 0777777);
    if ((vp.bus_c_out_l ^ 3) != 2) fatal ("recvword: expected BUS_C 2 was %o\n", vp.bus_c_out_l ^ 3);
    uint16_t got = vp.bus_d_out_l ^ 0177777;
    if (got != data) fatal ("recvword: expected BUS_D %06o got %06o\n", data, got);
    vp.bus_ssyn_in_l = 0;
    for (int i = 0; ! vp.bus_msyn_out_l; i = i + 1) {
        if (i > 200) fatal ("recvword: vp.bus_MSYN did not negate\n");
        kerchunk ();
    }
    vp.bus_ssyn_in_l = 1;
}

// receive a byte over unibus from the pdp and check its value
void recvbyte (uint16_t virtaddr, uint8_t data, char const *desc)
{
    printf ("- recvbyte %06o %03o  %s\n", virtaddr, data, desc);

    uint32_t physaddr = virt2phys (virtaddr, true);

    for (int i = 0; vp.bus_msyn_out_l; i = i + 1) {
        if (i > 200) fatal ("recvbyte: vp.bus_MSYN did not assert\n");
        kerchunk ();
    }
    if ((vp.bus_a_out_l ^ 0777777) != physaddr) fatal ("recvbyte: expected BUS_A %06o got %06o\n", physaddr, vp.bus_a_out_l ^ 0777777);
    if ((vp.bus_c_out_l ^ 3) != 3) fatal ("recvbyte: expected BUS_C 3 was %o\n", vp.bus_c_out_l ^ 3);
    uint8_t got = (vp.bus_d_out_l ^ 0177777) >> ((physaddr & 1) * 8);
    if (got != data) fatal ("recvbyte: expected BUS_D %03o got %03o\n", data, got);
    vp.bus_ssyn_in_l = 0;
    for (int i = 0; ! vp.bus_msyn_out_l; i = i + 1) {
        if (i > 200) fatal ("recvbyte: vp.bus_MSYN did not negate\n");
        kerchunk ();
    }
    vp.bus_ssyn_in_l = 1;
}

uint32_t virt2phys (uint16_t virtaddr, bool wrt)
{
    return (virtaddr > 0157777) ? virtaddr | 0600000 : virtaddr;
}

// read word from unibus via dma
void readword (uint32_t physaddr, uint16_t data)
{
    printf ("- readword %06o %06o\n", physaddr, data);

    // tell pdp we want to do dma cycle
    vp.bus_npr_l = 0;

    // wait for pdp to say it's ok
    for (int i = 0; ! vp.bus_npg_h; i = i + 1) {
        if (i > 200) fatal ("readword: vp.bus_NPG did not assert\n");
        kerchunk ();
    }

    // acknowledge selection; drop request; output address and function
    vp.bus_bbsy_l = 0;
    vp.bus_a_in_l = physaddr ^ 0777777;
    vp.bus_c_in_l = 3;
    vp.bus_npr_l  = 1;
    vp.bus_sack_l = 0;

    // let address and function soak into bus and decoders
    for (int i = 0; i < 15; i = i + 1) kerchunk ();

    // assert MSYN so slaves know address and function are valid
    vp.bus_msyn_in_l = 0;

    // wait for slave to reply with data
    for (int i = 0; vp.bus_ssyn_out_l; i = i + 1) {
        if (i > 1000) fatal ("readword: vp.bus_SSYN did not assert\n");
        kerchunk ();
    }

    // let data soak into bus
    for (int i = 0; i < 8; i = i + 1) kerchunk ();

    // check data is what we expect
    if ((vp.bus_d_out_l ^ 0177777) != data) fatal ("readword: expected BUS_D %06o got %06o\n", data, vp.bus_d_out_l ^ 0177777);

    // tell slave we got the data
    vp.bus_msyn_in_l = 1;
    vp.bus_sack_l = 1;

    // let msyn soak into bus
    for (int i = 0; i < 8; i = i + 1) kerchunk ();

    // wait for slave to finish up
    for (int i = 0; ! vp.bus_ssyn_out_l; i = i + 1) {
        if (i > 1000) fatal ("readword: vp.bus_SSYN did not negate\n");
        kerchunk ();
    }

    // drop everything else
    vp.bus_a_in_l = 0777777;
    vp.bus_bbsy_l = 1;
}

void writeword (uint32_t physaddr, uint16_t data)
{
    printf ("- writeword %06o %06o\n", physaddr, data);

    // tell pdp we want to do dma cycle
    vp.bus_npr_l = 0;

    // wait for pdp to say it's ok
    for (int i = 0; ! vp.bus_npg_h; i = i + 1) {
        if (i > 200) fatal ("writeword: vp.bus_NPG did not assert\n");
        kerchunk ();
    }

    // acknowledge selection; drop request; output address, data and function
    vp.bus_bbsy_l = 0;
    vp.bus_a_in_l = physaddr ^ 0777777;
    vp.bus_c_in_l = 1;
    vp.bus_d_in_l = data ^ 0177777;
    vp.bus_npr_l  = 1;
    vp.bus_sack_l = 0;

    // let address and function soak into bus and decoders
    for (int i = 0; i < 15; i = i + 1) kerchunk ();

    // assert MSYN so slaves know address and function are valid
    vp.bus_msyn_in_l = 0;

    // wait for slave to accept the data
    for (int i = 0; vp.bus_ssyn_out_l; i = i + 1) {
        if (i > 1000) fatal ("writeword: vp.bus_SSYN did not assert\n");
        kerchunk ();
    }

    // tell slave we are removing address and data
    vp.bus_msyn_in_l = 1;
    vp.bus_sack_l = 1;

    // let msyn soak into bus
    for (int i = 0; i < 8; i = i + 1) kerchunk ();

    // wait for slave to finish up
    for (int i = 0; ! vp.bus_ssyn_out_l; i = i + 1) {
        if (i > 1000) fatal ("writeword: vp.bus_SSYN did not negate\n");
        kerchunk ();
    }

    // drop everything else
    vp.bus_a_in_l = 0777777;
    vp.bus_c_in_l = 3;
    vp.bus_d_in_l = 0177777;
    vp.bus_bbsy_l = 1;
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
    psw = (psw & 0177761) | ((int16_t) prod != prod);
    return (prod < 0) ? 0100000 : (prod != 0);
}

// DIV (p 148)
uint16_t divinstr (uint16_t regno, uint16_t srcval)
{
    bool overflow = true;
    int32_t dividend = 0;
    int32_t quotient = 0;
    int32_t remaindr = 0;
    if (srcval != 0) {
        dividend = (gprs[curgprx(regno)] << 16) | gprs[curgprx(regno|1)];
        quotient = dividend / (int16_t) srcval;
        remaindr = dividend % (int16_t) srcval;
        gprs[curgprx(regno)]   = quotient;
        gprs[curgprx(regno|1)] = remaindr;
        overflow = ((int16_t) quotient != quotient);
    }
    psw = (psw & 0177760) | (overflow ? 002 : 0) | (srcval == 0);
    return (quotient < 0) ? 0100000 : (quotient != 0);
}

// ASH (p 149)
uint16_t ashinstr (uint16_t regno, uint16_t srcval)
{
    int16_t value = gprs[curgprx(regno)];
    int16_t count = ((srcval & 077) ^ 040) - 040;

    psw &= 0177775;

    if (count < 0) {
        do {
            psw = (psw & 0177776) | ((value >> 15) & 1);
            value >>= 1;
        } while (++ count < 0);
    }
    if (count > 0) {
        do {
            psw = (psw & 0177776) | (value & 1);
            int16_t newval = value << 1;
            if ((newval < 0) ^ (value < 0)) psw |= 2;
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

    psw &= 0177775;

    if (count < 0) {
        do {
            psw = (psw & 0177776) | ((value >> 31) & 1);
            value >>= 1;
        } while (++ count < 0);
    }
    if (count > 0) {
        do {
            psw = (psw & 0177776) | (value & 1);
            int32_t newval = value << 1;
            if ((newval < 0) ^ (value < 0)) psw |= 2;
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
    printf ("%12llu0:  RESET=%o  state=%02d  R0=%06o R1=%06o R2=%06o R3=%06o R4=%06o R5=%06o R6=%06o R7=%06o PS=%06o\n"
        "   bus_ac_lo_l=%o bus_bbsy_l=%o bus_br_l=%o%o%o%o bus_dc_lo_l=%o bus_intr_l=%o bus_npr_l=%o bus_pa_l=%o bus_pb_l=%o bus_sack_l=%o halt_rqst_l=%o\n"
        "   bus_a_in_l=%06o bus_c_in_l=%o%o bus_d_in_l=%06o bus_init_in_l=%o bus_msyn_in_l=%o bus_ssyn_in_l=%o\n"
        "   bus_a_out_l=%06o bus_c_out_l=%o%o bus_d_out_l=%06o bus_init_out_l=%o bus_msyn_out_l=%o bus_ssyn_out_l=%o bus_bg_h=%o%o%o%o bus_npg_h=%o\n"
        "   halt_grant_h=%o\n\n",

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

                vp.bus_ac_lo_l,
                vp.bus_bbsy_l,
                (vp.bus_br_l >> 3) & 1, (vp.bus_br_l >> 2) & 1, (vp.bus_br_l >> 1) & 1, (vp.bus_br_l >> 0) & 1,
                vp.bus_dc_lo_l,
                vp.bus_intr_l,
                vp.bus_npr_l,
                vp.bus_pa_l,
                vp.bus_pb_l,
                vp.bus_sack_l,
                vp.halt_rqst_l,

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

                (vp.bus_bg_h >> 3) & 1, (vp.bus_bg_h >> 2) & 1, (vp.bus_bg_h >> 1) & 1, (vp.bus_bg_h >> 0) & 1,
                vp.bus_npg_h,
                vp.halt_grant_h);
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
