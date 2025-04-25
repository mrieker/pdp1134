
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "obj_dir/Vpdp1134.h"

// single word-sized operand instruction
//  opcode   = 16-bit opcode with 00 for the dd field
//  readdst  = true to read old dst value
//  writedst = true to write new dst value
//  function = computation function (dstval = old dst value)
//  pswv     = true to set V bit; false to clear V bit
//  pswc     = true to set C bit; false to clear C bit
#define SINGLEWORD(instr,opcode,readdst,writedst,function,pswv,pswc) do { \
    uint16_t dd = genranddd (true); \
    printf ("= %s %s\n", instr, ddstr (ddbuff, dd)); \
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
        if (readdst)  sendword (ddva, dstval); \
        if (writedst) recvword (ddva, result); \
    } \
    psw = (psw & 0177760) | ((result & 0100000) ? 010 : 0) | ((result == 0) ? 004 : 0) | ((pswv) ? 002 : 0) | ((pswc) ? 001 : 0); \
} while (false)

#define SINGLEBYTE(instr,opcode,readdst,writedst,function,pswv,pswc) do { \
    uint16_t dd = genranddd (false); \
    printf ("= %s %s\n", instr, ddstr (ddbuff, dd)); \
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
        if (readdst)  sendbyte (ddva, dstval); \
        if (writedst) recvbyte (ddva, result); \
    } \
    psw = (psw & 0177760) | ((result & 0000200) ? 010 : 0) | ((result == 0) ? 004 : 0) | ((pswv) ? 002 : 0) | ((pswc) ? 001 : 0); \
} while (false)

#define DOUBLEWORD(instr,opcode,readdst,writedst,function,pswv,pswc) do { \
    uint16_t ss = genranddd (true); \
    uint16_t dd = genranddd (true); \
    printf ("= %s %s,%s\n", instr, ddstr (ssbuff, ss), ddstr (ddbuff, dd)); \
    sendfetch (opcode | (ss << 6) | dd); \
    uint16_t srcval; \
    if ((ss & 070) == 0) { \
        srcval = gprs[curgprx(ss)]; \
    } else { \
        uint16_t ssva = sendfetchdd (ss, true); \
        srcval = randbits (16); \
        sendword (ssva, srcval); \
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
        if (readdst)  sendword (ddva, dstval); \
        if (writedst) recvword (ddva, result); \
    } \
    psw = (psw & 0177760) | ((result & 0100000) ? 010 : 0) | ((result == 0) ? 004 : 0) | ((pswv) ? 002 : 0) | ((pswc) ? 001 : 0); \
} while (false)

#define DOUBLEBYTE(instr,opcode,readdst,writedst,function,pswv,pswc) do { \
    uint16_t ss = genranddd (false); \
    uint16_t dd = genranddd (false); \
    printf ("= %s %s,%s\n", instr, ddstr (ssbuff, ss), ddstr (ddbuff, dd)); \
    sendfetch (opcode | (ss << 6) | dd); \
    uint8_t srcval; \
    if ((ss & 070) == 0) { \
        srcval = gprs[curgprx(ss)]; \
    } else { \
        uint16_t ssva = sendfetchdd (ss, false); \
        srcval = randbits (8); \
        sendbyte (ssva, srcval); \
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
        if (readdst)  sendbyte (ddva, dstval); \
        if (writedst) recvbyte (ddva, result); \
    } \
    psw = (psw & 0177760) | ((result & 0000200) ? 010 : 0) | ((result == 0) ? 004 : 0) | ((pswv) ? 002 : 0) | ((pswc) ? 001 : 0); \
} while (false)

uint16_t gprs[16];
uint16_t psw;
uint32_t cyclectr;
Vpdp1134 vp;

uint16_t gprx (uint16_t r, uint16_t mode) { return (r == 6) && (mode & 2) ? 016 : r; }
uint16_t curgprx (uint16_t r) { return gprx (r, psw >> 14); }

uint16_t genranddd (bool word);
uint16_t sendfetchdd (uint16_t dd, bool word);
void sendfetch (uint16_t data);
void sendword (uint32_t physaddr, uint16_t data);
void sendbyte (uint32_t physaddr, uint8_t data);
void recvword (uint32_t physaddr, uint16_t data);
void recvbyte (uint32_t physaddr, uint8_t data);
void readword (uint32_t physaddr, uint16_t data);
void writeword (uint32_t physaddr, uint16_t data);
void kerchunk ();
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

    while (true) {
        char ddbuff[8], ssbuff[8];

        // processor should be fetching

        // generate random opcode and send to processor
        uint8_t select = randbits (8);
        switch (select) {

            // HALT
            case  0: {
                printf ("= HALT\n");
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
                do dd = genranddd (true);
                while ((dd & 070) == 0);
                printf ("= JMP %s\n", ddstr (ddbuff, dd));
                sendfetch (0000100 | dd);
                uint16_t ddva = sendfetchdd (dd, true);
                gprs[7] = ddva;
                break;
            }

            // SWAB
            case  8: {
                SINGLEWORD ("SWAB", 0000300, true, true, (dstval << 8) | (dstval >> 8), false, false);
                break;
            }

            // JSR
            case  9: {
                uint16_t dd;
                do dd = genranddd (true);
                while ((dd & 070) == 0);
                uint16_t r = randbits (3);
                printf ("= JSR R%o,%s\n", r, ddstr (ddbuff, dd));
                sendfetch (0000100 | (r << 6) | dd);
                uint16_t ddva = sendfetchdd (dd, true);
                gprs[curgprx(6)] -= 2;
                recvword (gprs[curgprx(6)], gprs[curgprx(r)]);
                gprs[curgprx(r)] = gprs[7];
                gprs[7] = ddva;
                break;
            }

            // CLR
            case 10: {
                SINGLEWORD ("CLR", 0005000, false, true, 0, false, false);
                break;
            }

            // CLRB
            case 11: {
                SINGLEBYTE ("", 0105000, false, true, 0, false, false);
                break;
            }

            // COM
            case 12: {
                SINGLEWORD ("", 0005100, true, true, ~ dstval, false, true);
                break;
            }

            // COMB
            case 13: {
                SINGLEWORD ("", 0105100, true, true, ~ dstval, false, true);
                break;
            }

            // INC
            case 14: {
                SINGLEWORD ("", 0005200, true, true, dstval + 1, dstval & ~ result & 0100000, psw & 1);
                break;
            }

            // INCB
            case 15: {
                SINGLEBYTE ("", 0105200, true, true, dstval + 1, dstval & ~ result & 0000200, psw & 1);
                break;
            }

            // DEC
            case 16: {
                SINGLEWORD ("", 0005300, true, true, dstval - 1, ~ dstval & result & 0100000, psw & 1);
                break;
            }

            // DECB
            case 17: {
                SINGLEBYTE ("", 0105300, true, true, dstval - 1, ~ dstval & result & 0000200, psw & 1);
                break;
            }

            // NEG
            case 18: {
                SINGLEWORD ("", 0005400, true, true, - dstval, result == 0100000, result != 0);
                break;
            }

            // NEGB
            case 19: {
                SINGLEBYTE ("", 0105400, true, true, - dstval, result == 0000200, result != 0);
                break;
            }

            // ADC
            case 20: {
                SINGLEWORD ("", 0005500, true, true, dstval + (psw & 1), ~ dstval & result & 0100000, ~ dstval & result & 0100000);
                break;
            }

            // ADCB
            case 21: {
                SINGLEBYTE ("", 0105500, true, true, dstval + (psw & 1), ~ dstval & result & 0000200, ~ dstval & result & 0000200);
                break;
            }

            // SBC
            case 22: {
                SINGLEWORD ("", 0005600, true, true, dstval - (psw & 1), dstval & ~ result & 0100000, dstval & ~ result & 0100000);
                break;
            }

            // SBCB
            case 23: {
                SINGLEWORD ("", 0105600, true, true, dstval - (psw & 1), dstval & ~ result & 0000200, dstval & ~ result & 0000200);
                break;
            }

            // TST
            case 24: {
                SINGLEWORD ("", 0005700, true, false, dstval, false, false);
                break;
            }

            // TSTB
            case 25: {
                SINGLEBYTE ("", 0105700, true, false, dstval, false, false);
                break;
            }

            // ROR
            case 26: {
                SINGLEWORD ("", 0006000, true, true, ((psw & 1) << 15) | (dstval >> 1),
                    ((result >> 15) ^ dstval) & 1, dstval & 1);
                break;
            }

            // RORB
            case 27: {
                SINGLEBYTE ("", 0106000, true, true, ((psw & 1) <<  7) | (dstval >> 1),
                    ((result >>  7) ^ dstval) & 1, dstval & 1);
                break;
            }

            // ROL
            case 28: {
                SINGLEWORD ("", 0006100, true, true, (dstval << 1) | (psw & 1),
                    ((result ^ dstval) >> 15) & 1, (dstval >> 15) & 1);
                break;
            }

            // ROLB
            case 29: {
                SINGLEWORD ("", 0106100, true, true, (dstval << 1) | (psw & 1),
                    ((result ^ dstval) >>  7) & 1, (dstval >>  7) & 1);
                break;
            }

            // ASR
            case 30: {
                SINGLEWORD ("", 0006200, true, true, (dstval & 0100000) | (dstval >> 1),
                    ((result >> 15) ^ dstval) & 1, dstval & 1);
                break;
            }

            // ASRB
            case 31: {
                SINGLEBYTE ("", 0106200, true, true, (dstval & 0000200) | (dstval >> 1),
                    ((result >>  7) ^ dstval) & 1, dstval & 1);
                break;
            }

            // ASL
            case 32: {
                SINGLEWORD ("", 0006300, true, true, dstval << 1,
                    ((result ^ dstval) >> 15) & 1, (dstval >> 15) & 1);
                break;
            }

            // ASLB
            case 33: {
                SINGLEWORD ("", 0106300, true, true, dstval << 1,
                    ((result ^ dstval) >>  7) & 1, (dstval >>  7) & 1);
                break;
            }

            // MARK (p 99)
            case 34: {
                uint16_t nn = randbits (6);
                sendfetch (0006400 | nn);
                uint16_t newsp = gprs[7] + nn * 2;
                gprs[7] = gprs[5];
                uint16_t newr5 = randbits (16);
                sendword (newsp, newr5);
                gprs[5] = newr5;
                gprs[curgprx(6)] = newsp + 2;
                break;
            }

            // MTPS (p 64)
            case 35: {
                uint16_t dd = genranddd (false);
                sendfetch (0106400 | dd);
                uint8_t newpsb;
                if ((dd & 070) == 0) {
                    newpsb = gprs[curgprx(dd)];
                } else {
                    uint16_t ddva = sendfetchdd (dd, true);
                    uint8_t newpsb = randbits (3) << 5;
                    sendbyte (ddva, newpsb);
                }
                if ((psw & 0140000) == 0) psw = (psw & 0177437) | (newpsb & 0000340);
                psw = (psw & 0177761) | ((newpsb & 0000200) ? 010 : 0) | ((newpsb == 0) ? 004 : 0);
                break;
            }

            // MFPI (p 168)
            case 36: {
                uint16_t dd = genranddd (true);
                sendfetch (0006500 | dd);
                uint16_t value;
                if ((dd & 070) == 0) {
                    value = gprs[gprx(dd,psw>>12)];
                } else {
                    uint16_t ddva = sendfetchdd (dd, true);
                    value = randbits (16);
                    sendword (ddva, value);
                }
                uint16_t newsp = gprs[curgprx(6)] - 2;
                gprs[curgprx(6)] = newsp;
                recvword (newsp, value);
                break;
            }

            // MTPI (p 169)
            case 37: {
                uint16_t dd = genranddd (true);
                sendfetch (0006600 | dd);
                uint16_t oldsp = gprs[curgprx(6)];
                gprs[curgprx(6)] = oldsp + 2;
                uint16_t value = randbits (16);
                sendword (oldsp, value);
                if ((dd & 070) == 0) {
                    gprs[gprx(dd,psw>>12)] = value;
                } else {
                    uint16_t ddva = sendfetchdd (dd, true);
                    recvword (ddva, value);
                }
                break;
            }

            // SXT (p 62)
            case 38: {
                SINGLEWORD ("", 0006700, false, true, (psw & 010) ? -1 : 0, false, psw & 1);
                break;
            }

            // MFPS (p 63)
            case 39: {
                uint16_t dd = genranddd (false);
                sendfetch (0106700 | dd);
                if ((dd & 070) == 0) {
                    gprs[curgprx(dd)] = (int16_t) (int8_t) psw;
                } else {
                    uint16_t ddva = sendfetchdd (dd, false);
                    recvbyte (ddva, psw);
                }
                psw = (psw & 0177761) | ((psw & 0000200) ? 010 : 0) | (((int8_t) psw == 0) ? 004 : 0);
                break;
            }

            // MOV
            case 40: {
                DOUBLEWORD ("", 0010000, false, true, srcval, false, psw & 1);
                break;
            }

            // MOVB
            case 41: {
                DOUBLEBYTE ("", 0110000, false, true, srcval, false, psw & 1);
                break;
            }

            // CMP
            case 42: {
                DOUBLEWORD ("", 0020000, true, false, srcval - dstval,
                    (srcval ^ dstval) & (~ result ^ dstval) & 0100000,
                    srcval < dstval);
                break;
            }

            // CMPB
            case 43: {
                DOUBLEBYTE ("", 0120000, true, false, srcval - dstval,
                    (srcval ^ dstval) & (~ result ^ dstval) & 0000200,
                    srcval < dstval);
                break;
            }

            // BIT
            case 44: {
                DOUBLEWORD ("", 0030000, true, false, srcval & dstval, false, psw & 1);
                break;
            }

            // BITB
            case 45: {
                DOUBLEBYTE ("", 0130000, true, false, srcval & dstval, false, psw & 1);
                break;
            }

            // BIC
            case 46: {
                DOUBLEWORD ("", 0040000, true, true, ~ srcval & dstval, false, psw & 1);
                break;
            }

            // BICB
            case 47: {
                DOUBLEBYTE ("", 0140000, true, true, ~ srcval & dstval, false, psw & 1);
                break;
            }

            // BIS
            case 48: {
                DOUBLEWORD ("", 0040000, true, true, srcval | dstval, false, psw & 1);
                break;
            }

            // BISB
            case 49: {
                DOUBLEBYTE ("", 0140000, true, true, srcval | dstval, false, psw & 1);
                break;
            }

            // ADD
            case 50: {
                DOUBLEWORD ("", 0020000, true, false, dstval + srcval,
                    (~ srcval ^ dstval) & (result ^ dstval) & 0100000,
                    ((uint32_t) srcval + (uint32_t) dstval) >> 16);
                break;
            }

            // SUB
            case 51: {
                DOUBLEWORD ("", 0020000, true, true, dstval - srcval,
                    (srcval ^ dstval) & (~ result ^ srcval) & 0100000,
                    dstval < srcval);
                break;
            }

/***
            // MUL (p 147)
            case 52: {
                uint16_t regno = randbits (3);
                SINGLEWORD ("", 0070000 | (regno << 6), true, false, mulinstr (regno, dstval), psw & 2, psw & 1);
                break;
            }

            // DIV (p 148)
            case 53: {
                uint16_t regno = randbits (3);
                SINGLEWORD ("", 0071000 | (regno << 6), true, false, divinstr (regno, dstval), psw & 2, psw & 1);
                break;
            }

            // ASH (p 149)
            case 54: {
                uint16_t regno = randbits (3);
                SINGLEWORD ("", 0072000 | (regno << 6), true, false, ashinstr (regno, dstval), psw & 2, psw & 1);
                break;
            }

            // ASHC (p 150)
            case 55: {
                uint16_t regno = randbits (3);
                SINGLEWORD ("", 0073000 | (regno << 6), true, false, ashcinstr (regno, dstval), psw & 2, psw & 1);
                break;
            }
***/

            // XOR (p 73)
            case 56: {
                uint16_t srcreg = randbits (3);
                uint16_t srcval = gprs[curgprx(srcreg)];
                SINGLEWORD ("", 0074000 | (srcreg << 6), true, true, srcval ^ dstval, false, psw & 1);
                break;
            }

            // SOB (p 101)
            case 57: {
                uint16_t regno = randbits (3);
                uint16_t nn = randbits (6);
                sendfetch (0077000 | (regno << 6) | nn);
                if (-- gprs[curgprx(regno)] != 0) gprs[7] -= nn * 2;
                break;
            }

/***
            // EMT
            case 58: {
                uint16_t code = randbits (8);
                sendfetch (0104000 | code);
                trapthrough (0030);
                break;
            }

            // TRAP
            case 59: {
                uint16_t code = randbits (8);
                sendfetch (0104400 | code);
                trapthrough (0034);
                break;
            }
***/

            // Bxx (p 75..93)
            case 60: {
                uint16_t opcode;
                do opcode = randbits (16) & 0103777;
                while ((opcode & 0103400) == 0);
                bool n = (psw & 8) != 0;
                bool z = (psw & 4) != 0;
                bool v = (psw & 2) != 0;
                bool c = (psw & 1) != 0;
                bool brtrue;
                switch (((opcode >> 13) & 4) | ((opcode >> 9) & 3)) {
                    case 0: brtrue = false; break;
                    case 1: brtrue = ! z; break;                // BNE
                    case 2: brtrue = ! (n ^ v); break;          // BGE
                    case 3: brtrue = ! (n ^ v) & ! z; break;    // BGT
                    case 4: brtrue = ! n; break;                // BPL
                    case 5: brtrue = ! c & ! z; break;          // BHI
                    case 6: brtrue = ! v; break;                // BVC
                    case 7: brtrue = ! c; break;                // BCC
                }
                brtrue ^= (opcode & 0000400) != 0;
                break;
            }

            // CCS (p 114)
            case 61: {
                uint16_t bits = randbits (5);
                sendfetch (0000240 | bits);
                if (bits & 020) psw |= bits & 017;
                           else psw &= bits ^ 017;
                break;
            }
        }
    }
    return 0;
}

// generate random DD operand
// make sure address is a memory address
uint16_t genranddd (bool word)
{
    while (true) {
        uint16_t dd = randbits (6);
        switch (dd >> 3) {
            case 0: break;
            case 1: case 2: {
                uint16_t x = gprs[curgprx(dd&7)];
                if (x > 0157777) continue;
                if (x & (uint16_t) word) continue;
                break;
            }
            case 3: {
                uint16_t x = gprs[curgprx(dd&7)];
                if (x > 0157777) continue;
                if (x & 1) continue;
                break;
            }
            case 4: {
                uint16_t x = gprs[curgprx(dd&7)] - (word ? 2 : 1);
                if (x > 0157777) continue;
                if (x & 1) continue;
                break;
            }
            case 5: {
                uint16_t x = gprs[curgprx(dd&7)] - 2;
                if (x > 0157777) continue;
                if (x & 1) continue;
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
            sendword (x, y);
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
            sendword (x, y);
            gprs[curgprx(dd&7)] = x;
            return y;
        }
        case 6: {
            uint16_t x = gprs[curgprx(dd&7)];
            if ((dd & 7) == 7) x += 2;
            uint16_t y;
            do y = randbits (16);
            while ((x + y > 0157777) || ((x + y) & (uint16_t) word));
            sendfetch (y);
            return x + y;
        }
        case 7: {
            uint16_t x = gprs[curgprx(dd&7)];
            if ((dd & 7) == 7) x += 2;
            uint16_t y;
            do y = randbits (16);
            while ((x + y > 0157777) || ((x + y) & 1));
            sendfetch (y);
            uint16_t z;
            do z = randbits (16);
            while ((z > 0157777) || (z & (uint16_t) word));
            sendword (x + y, z);
            return z;
        }
        default: abort ();
    }
}

// send opcode word to processor and increment local copy of PC
void sendfetch (uint16_t data)
{
    sendword (gprs[7], data);
    gprs[7] += 2;
}

// PDP is reading a word from memory
// send the value we want it to get
void sendword (uint32_t physaddr, uint16_t data)
{
    printf ("- sendword %06o %06o\n", physaddr, data);

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
void sendbyte (uint32_t physaddr, uint8_t data)
{
    printf ("- sendbyte %06o %03o\n", physaddr, data);

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
void recvword (uint32_t physaddr, uint16_t data)
{
    printf ("- recvword %06o %06o\n", physaddr, data);
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
void recvbyte (uint32_t physaddr, uint8_t data)
{
    printf ("- recvbyte %06o %03o\n", physaddr, data);
    for (int i = 0; vp.bus_msyn_out_l; i = i + 1) {
        if (i > 200) fatal ("recvbyte: vp.bus_MSYN did not assert\n");
        kerchunk ();
    }
    if ((vp.bus_a_out_l ^ 0777777) != physaddr) fatal ("recvbyte: expected BUS_A %06o got %06o\n", physaddr, vp.bus_a_out_l ^ 0777777);
    if ((vp.bus_c_out_l ^ 3) != 2) fatal ("recvbyte: expected BUS_C 2 was %o\n", vp.bus_c_out_l ^ 3);
    uint8_t got = (vp.bus_d_out_l ^ 0177777) >> ((physaddr & 1) * 8);
    if (got != data) fatal ("recvbyte: expected BUS_D %03o got %03o\n", data, got);
    vp.bus_ssyn_in_l = 0;
    for (int i = 0; ! vp.bus_msyn_out_l; i = i + 1) {
        if (i > 200) fatal ("recvbyte: vp.bus_MSYN did not negate\n");
        kerchunk ();
    }
    vp.bus_ssyn_in_l = 1;
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
}

void fatal (char const *fmt, ...)
{
    va_list ap;
    va_start (ap, fmt);
    vprintf (fmt, ap);
    va_end (ap);
    dumpstate ();
    abort ();
}

void dumpstate ()
{
    printf ("%9u0:  RESET=%o  state=%02d pc=%06o\n"
        "   bus_ac_lo_l=%o bus_bbsy_l=%o bus_br_l=%o%o%o%o bus_dc_lo_l=%o bus_intr_l=%o bus_npr_l=%o bus_pa_l=%o bus_pb_l=%o bus_sack_l=%o halt_rqst_l=%o\n"
        "   bus_a_in_l=%06o bus_c_in_l=%o%o bus_d_in_l=%06o bus_init_in_l=%o bus_msyn_in_l=%o bus_ssyn_in_l=%o\n"
        "   bus_a_out_l=%06o bus_c_out_l=%o%o bus_d_out_l=%06o bus_init_out_l=%o bus_msyn_out_l=%o bus_ssyn_out_l=%o bus_bg_h=%o%o%o%o bus_npg_h=%o\n"
        "   halt_grant_h=%o\n\n",

                cyclectr,
                vp.RESET,
                vp.state,
                vp.pc,

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
