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

#include <stdint.h>
#include <string>
#include <string.h>

#include "disassem.h"
#include "strprintf.h"

#define PA disassem_PA

struct DisassemCtx {
    std::string *strbuf;
    uint16_t instreg;
    uint16_t operand1;
    uint16_t operand2;
    bool (*readword) (void *param, uint32_t addr, uint16_t *data_r);
    void *param;

    bool byte;
    bool gproks[8];
    int pcinc;
    uint16_t gprs[8];

    void getop  (uint16_t mr, bool prev = false);
    bool getgpr (uint16_t r, uint16_t *data, bool prev = false);
    bool rdsize (uint16_t addr, uint16_t *data);
    bool rdbyte (uint16_t addr, uint16_t *data);
    bool rdword (uint16_t addr, uint16_t *data);
};

static bool readnull (void *param, uint32_t addr, uint16_t *data_r)
{
    return false;
}

// disassemble PDP-11 instruction
//  input:
//   strbuf   = where to append output to
//   instreg  = opcode being disassembled
//   operand1 = first word following instreg
//   operand2 = second word following instreg
//   readword = called to read word from bus (or NULL)
//  output:
//   returns:
//    0 : illegal instruction
//    1 : just used 1 word (instreg)
//    2 : used 2 words (instreg, operand1)
//    3 : used 3 words (instreg, operand1, operand2)
//   disassembly appended to *strbuf
int disassem (std::string *strbuf, uint16_t instreg, uint16_t operand1, uint16_t operand2,
        bool (*readword) (void *param, uint32_t addr, uint16_t *data_r), void *param)
{
    if (readword == NULL) readword = readnull;

    DisassemCtx ctx;
    ctx.strbuf   = strbuf;
    ctx.instreg  = instreg;
    ctx.operand1 = operand1;
    ctx.operand2 = operand2;
    ctx.readword = readword;
    ctx.param    = param;

    memset (ctx.gproks, 0, sizeof ctx.gproks);
    ctx.pcinc = 1;
    ctx.byte = (instreg >> 15) & 1;

    strprintf (strbuf, "%06o", instreg);

    switch ((instreg >> 12) & 7) {

        case 0: {
            // x 000 xxx xxx xxx xxx
            switch ((instreg >> 6) & 077) {
                case 001: { // JMP
                    if (! ctx.byte) {
                        strbuf->append (" JMP ");
                        goto s_endsingle;
                    }
                    break;
                }
                case 002: {
                    if ((instreg & 0177770) == 0000200) {   // RTS
                        strprintf (strbuf, " RTS  R%o", instreg & 7);
                        goto s_endinst;
                    }
                    if ((instreg & 0177740) == 0000240) {   // CCS
                        uint16_t bit = (instreg >> 4) & 1;
                        strprintf (strbuf, " %c%c%c%c%c",
                            bit ? 'S' : 'C',
                            instreg & 010 ? 'N' : '-',
                            instreg & 004 ? 'Z' : '-',
                            instreg & 002 ? 'V' : '-',
                            instreg & 001 ? 'C' : '-');
                        goto s_endinst;
                    }
                    break;
                }
                case 003: {
                    if (! ctx.byte) {   // SWAB
                        strbuf->append (" SWAB");
                        goto s_endsingle;
                    }
                    break;
                }
                case 040 ... 047: {   // JSR
                    if (! ctx.byte) {
                        strprintf (strbuf, " JSR  R%o,", (instreg >> 6) & 7);
                        goto s_endsingle;
                    }
                    if ((instreg & 0177400) == 0104000) {   // EMT
                        strprintf (strbuf, " EMT  %03o", instreg & 0377);
                        goto s_endinst;
                    }
                    if ((instreg & 0177400) == 0104400) {   // TRAP
                        strprintf (strbuf, " TRAP %03o", instreg & 0377);
                        goto s_endinst;
                    }
                    break;
                }
                case 050: { // CLRb
                    strprintf (strbuf, " CLR%c", ctx.byte ? 'B' : ' ');
                    goto s_endsingle;
                }
                case 051: { // COMb
                    strprintf (strbuf, " COM%c", ctx.byte ? 'B' : ' ');
                    goto s_endsingle;
                }
                case 052: { // INCb
                    strprintf (strbuf, " INC%c", ctx.byte ? 'B' : ' ');
                    goto s_endsingle;
                }
                case 053: { // DECb
                    strprintf (strbuf, " DEC%c", ctx.byte ? 'B' : ' ');
                    goto s_endsingle;
                }
                case 054: { // NEGb
                    strprintf (strbuf, " NEG%c", ctx.byte ? 'B' : ' ');
                    goto s_endsingle;
                }
                case 055: { // ADCb
                    strprintf (strbuf, " ADC%c", ctx.byte ? 'B' : ' ');
                    goto s_endsingle;
                }
                case 056: { // SBCb
                    strprintf (strbuf, " SBC%c", ctx.byte ? 'B' : ' ');
                    goto s_endsingle;
                }
                case 057: { // TSTb
                    strprintf (strbuf, " TST%c", ctx.byte ? 'B' : ' ');
                    goto s_endsingle;
                }
                case 060: { // RORb
                    strprintf (strbuf, " ROR%c", ctx.byte ? 'B' : ' ');
                    goto s_endsingle;
                }
                case 061: { // ROLb
                    strprintf (strbuf, " ROL%c", ctx.byte ? 'B' : ' ');
                    goto s_endsingle;
                }
                case 062: { // ASRb
                    strprintf (strbuf, " ASR%c", ctx.byte ? 'B' : ' ');
                    goto s_endsingle;
                }
                case 063: { // ASLb
                    strprintf (strbuf, " ASL%c", ctx.byte ? 'B' : ' ');
                    goto s_endsingle;
                }
                case 064: {
                    if (ctx.byte) { // MTPS
                        strbuf->append (" MTPS");
                        ctx.byte = true;
                        goto s_endsingle;
                    }
                    strprintf (strbuf, " MARK %02o", instreg & 077);
                    goto s_endinst;
                }
                case 065: {         // MFPI/D
                    strbuf->append (" MFPI");
                    ctx.byte = false;
                    if (instreg & 070) goto s_endsingle;
                    strprintf (strbuf, " R%o", instreg & 7);
                    uint16_t data;
                    if (ctx.getgpr (instreg, &data, true)) {
                        strprintf (strbuf, " [%06o]", data);
                    }
                    goto s_endinst;
                }
                case 066: {         // MTPI/D
                    strbuf->append (" MTPI");
                    ctx.byte = false;
                    if (instreg & 070) goto s_endsingle;
                    strprintf (strbuf, " R%o", instreg & 7);
                    uint16_t data;
                    if (ctx.getgpr (instreg, &data, true)) {
                        strprintf (strbuf, " [%06o]", data);
                    }
                    goto s_endinst;
                }
                case 067: {
                    if (ctx.byte) { // MFPS
                        strbuf->append (" MFPS");
                    }
                    else {          // SEXT
                        strbuf->append (" SEXT");
                    }
                    goto s_endsingle;
                }
            }

            // x 000 xxx xxx xxx xxx
            uint16_t disp = instreg & 0377;
            switch (((instreg >> 12) & 010) | ((instreg >> 8) & 007)) {
                case 001: strprintf (strbuf, " BR   %03o", disp); goto s_endinst;
                case 002: strprintf (strbuf, " BNE  %03o", disp); goto s_endinst;
                case 003: strprintf (strbuf, " BEQ  %03o", disp); goto s_endinst;
                case 004: strprintf (strbuf, " BGE  %03o", disp); goto s_endinst;
                case 005: strprintf (strbuf, " BLT  %03o", disp); goto s_endinst;
                case 006: strprintf (strbuf, " BGT  %03o", disp); goto s_endinst;
                case 007: strprintf (strbuf, " BLE  %03o", disp); goto s_endinst;
                case 010: strprintf (strbuf, " BPL  %03o", disp); goto s_endinst;
                case 011: strprintf (strbuf, " BMI  %03o", disp); goto s_endinst;
                case 012: strprintf (strbuf, " BHI  %03o", disp); goto s_endinst;
                case 013: strprintf (strbuf, " BLOS %03o", disp); goto s_endinst;
                case 014: strprintf (strbuf, " BVC  %03o", disp); goto s_endinst;
                case 015: strprintf (strbuf, " BVS  %03o", disp); goto s_endinst;
                case 016: strprintf (strbuf, " BCC  %03o", disp); goto s_endinst;
                case 017: strprintf (strbuf, " BCS  %03o", disp); goto s_endinst;

                // 0 000 x00 0xx xxx xxx
                case 0: {
                    switch (instreg) {
                        case 0: {
                            strbuf->append (" HALT");
                            goto s_endinst;
                        }
                        case 1: {
                            strbuf->append (" WAIT");
                            goto s_endinst;
                        }
                        case 2: {
                            strbuf->append (" RTI ");
                            goto s_endrtirtt;
                        }
                        case 3: {
                            strbuf->append (" BPT");
                            goto s_endinst;
                        }
                        case 4: {
                            strbuf->append (" IOT");
                            goto s_endinst;
                        }
                        case 5: {
                            strbuf->append (" RESET");
                            goto s_endinst;
                        }
                        case 6: {
                            strbuf->append (" RTT ");
                            goto s_endrtirtt;
                        }
                    }
                    break;
                }
            }
            break;
        }

        case 1: {   // MOVb
            strprintf (strbuf, " MOV%c", ctx.byte ? 'B' : ' ');
            goto s_enddouble;
        }
        case 2: {   // CMPb
            strprintf (strbuf, " CMP%c", ctx.byte ? 'B' : ' ');
            goto s_enddouble;
        }
        case 3: {   // BITb
            strprintf (strbuf, " BIT%c", ctx.byte ? 'B' : ' ');
            goto s_enddouble;
        }
        case 4: {   // BICb
            strprintf (strbuf, " BIC%c", ctx.byte ? 'B' : ' ');
            goto s_enddouble;
        }
        case 5: {   // BISb
            strprintf (strbuf, " BIS%c", ctx.byte ? 'B' : ' ');
            goto s_enddouble;
        }
        case 6: {   // ADD/SUB
            strbuf->append (ctx.byte ? " SUB " : " ADD ");
            ctx.byte = false;
            goto s_enddouble;
        }

        case 7: {
            // x 111 xxx xxx xxx xxx
            if (! ctx.byte) {
                // 0 111 xxx xxx xxx xxx
                switch ((instreg >> 9) & 7) {
                    case 0: {   // MUL
                        strbuf->append (" MUL ");
                        goto s_endextarith;
                    }
                    case 1: {   // DIV
                        strbuf->append (" DIV ");
                        goto s_endextarith;
                    }
                    case 2: {   // ASH
                        strbuf->append (" ASH ");
                        goto s_endextarith;
                    }
                    case 3: {   // ASHC
                        strbuf->append (" ASHC");
                        goto s_endextarith;
                    }
                    case 4: {   // XOR
                        strprintf (strbuf, " XOR  R%o,", (instreg >> 6) & 7);
                        goto s_endsingle;
                    }
                    case 7: {   // SOB
                        strprintf (strbuf, " SOB  R%o, %02o", (instreg >> 6) & 7, instreg & 077);
                        goto s_endinst;
                    }
                }
            }
            break;
        }
    }

    // illegal instruction
    return 0;

s_endrtirtt:;
    {
        uint16_t cursp, newpc, newps;
        if (ctx.getgpr (6, &cursp) && ctx.rdword (cursp, &newpc) && ctx.rdword (cursp + 2, &newps)) {
            strprintf (strbuf, " [%06o/%06o %06o]", cursp, newpc, newps);
        }
    }
    goto s_endinst;

s_endextarith:;
    ctx.byte = false;
    ctx.getop (instreg);
    strprintf (strbuf, ", R%o", (instreg >> 6) & 7);
    goto s_endinst;

s_enddouble:;
    ctx.getop (instreg >> 6);
    strbuf->push_back (',');
s_endsingle:;
    ctx.getop (instreg);

s_endinst:;
    return ctx.pcinc;
}

// disassemble operand then try to print address and value
void DisassemCtx::getop (uint16_t mr, bool prev)
{
    // get possible operand that follows opcode for modes 27,37,6x,7x
    uint16_t op;
    switch (pcinc) {
        case 1: op = operand1; break;
        case 2: op = operand2; break;
        default: abort ();
    }

    // get register and data width
    uint16_t r = mr & 7;
    int byte36 = byte ? 3 : 6;
    uint16_t incr = (byte && (r != 6) && (r != 7)) ? 1 : 2;

    uint16_t addr, addr2;

    // decode addressing mode
    switch ((mr >> 3) & 7) {

        // register holds the value
        case 0: {
            strprintf (strbuf, " R%o", r);
            uint16_t data;
            if (getgpr (r, &data)) {
                if (byte) data &= 0377;
                strprintf (strbuf, " [%0*o]", byte36, data);
            }
            break;
        }

        // register holds address of value
        case 1: {
            strprintf (strbuf, " @R%o", r);
            if (getgpr (r, &addr)) goto praddr;
            break;
        }

        // register holds address of value, then increment by size
        case 2: {
            if (r == 7) {
                if (byte) op &= 0377;
                strprintf (strbuf, " #%0*o", byte36, op);
                pcinc ++;
            } else {
                strprintf (strbuf, " (R%o)+", r);
                bool ok = getgpr (r, &addr);
                gprs[r] += incr;
                if (ok) goto praddr;
            }
            break;
        }

        // register hold address of address of value, then increment register by 2
        case 3: {
            if (r == 7) {
                strprintf (strbuf, " @#%06o", op);
                pcinc ++;
                addr = op;
                goto praddr;
            } else {
                strprintf (strbuf, " @(R%o)+", r);
                bool ok = getgpr (r, &addr2);
                gprs[r] += 2;
                if (ok) goto praddr2;
            }
            break;
        }

        // decrement register then it contains address of value
        case 4: {
            strprintf (strbuf, " -(R%o)", r);
            if (getgpr (r, &addr)) {
                gprs[r] = addr -= incr;
                goto praddr;
            }
            break;
        }

        // decrement register then it contains address of address of value
        case 5: {
            strprintf (strbuf, " @-(R%o)", r);
            if (getgpr (r, &addr2)) {
                gprs[r] = addr2 -= 2;
                goto praddr2;
            }
            break;
        }

        // register plus next operand word make address of value
        case 6: {
            pcinc ++;
            if ((r == 7) && getgpr (7, &addr)) {
                addr += op;
                strprintf (strbuf, " %06o", addr);
                goto praddr;
            } else {
                strprintf (strbuf, " %06o(R%o)", op, r);
                if (getgpr (mr, &addr)) {
                    addr += op;
                    goto praddr;
                }
            }
            break;
        }

        // register plus next operand word make address of address of value
        case 7: {
            pcinc ++;
            if ((r == 7) && getgpr (7, &addr2)) {
                addr2 += op;
                strprintf (strbuf, " @%06o", addr2);
                goto praddr2;
            } else {
                strprintf (strbuf, " @%06o(R%o)", op, r);
                if (getgpr (r, &addr2)) {
                    addr2 += op;
                    goto praddr2;
                }
            }
            break;
        }
        default: abort ();
    }
    return;

praddr2:;
    strprintf (strbuf, " [%06o/", addr2);
    if (!rdword (addr2, &addr)) {
        strbuf->append ("------]");
        return;
    }
    strprintf (strbuf, "%06o/", addr);
    goto prdata;
praddr:;
    strprintf (strbuf, " [%06o/", addr);
prdata:;
    uint16_t data;
    if (rdsize (addr, &data)) {
        strprintf (strbuf, "%0*o]", byte36, data);
    } else {
        strbuf->append (byte ? "---]" : "------]");
    }
}

// try to get the register contents from the processor
bool DisassemCtx::getgpr (uint16_t r, uint16_t *data, bool prev)
{
    r &= 7;
    if (! gproks[r]) {
        uint16_t rr = r;
        if (rr == 6) {
            uint16_t psw;
            if (! readword (param, PA | 0777776, &psw)) return false;
            if (psw & (prev ? 0030000 : 0140000)) rr += 8;
        }
        gproks[r] = readword (param, PA | (0777700 + rr), &gprs[r]);
    }
    *data = gprs[r];
    if (r == 7) *data += pcinc * 2;
    return gproks[r];
}

bool DisassemCtx::rdsize (uint16_t addr, uint16_t *data)
{
    return byte ? rdbyte (addr, data) : rdword (addr, data);
}

bool DisassemCtx::rdbyte (uint16_t addr, uint16_t *data)
{
    uint16_t temp;
    if (! rdword (addr & 0177776, &temp)) return false;
    *data = (addr & 1) ? (temp >> 8) : (temp & 0377);
    return true;
}

bool DisassemCtx::rdword (uint16_t addr, uint16_t *data)
{
    return (! (addr & 1)) && readword (param, addr, data);
}
