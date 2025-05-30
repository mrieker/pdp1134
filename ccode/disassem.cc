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

#include "disassem.h"
#include "strprintf.h"

static void getopaddr (std::string *strbuf, uint16_t mr, int *used_r, uint16_t operand1, uint16_t operand2);

// disassemble PDP-11 instruction
//  input:
//   strbuf   = where to append output to
//   instreg  = opcode being disassembled
//   operand1 = first word following instreg
//   operand2 = second word following instreg
//  output:
//   returns:
//    0 : illegal instruction
//    1 : just used 1 word (instreg)
//    2 : used 2 words (instreg, operand1)
//    3 : used 3 words (instreg, operand1, operand2)
//   disassembly appended to *strbuf
int disassem (std::string *strbuf, uint16_t instreg, uint16_t operand1, uint16_t operand2)
{
    int used = 1;

    bool byte = (instreg >> 15) & 1;
    switch ((instreg >> 12) & 7) {

        case 0: {
            // x 000 xxx xxx xxx xxx
            switch ((instreg >> 6) & 077) {
                case 001: { // JMP
                    if (! byte) {
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
                    if (! byte) {   // SWAB
                        strbuf->append (" SWAB");
                        goto s_endsingle;
                    }
                    break;
                }
                case 040 ... 047: {   // JSR
                    if (! byte) {
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
                    strprintf (strbuf, " CLR%c", byte ? 'B' : ' ');
                    goto s_endsingle;
                }
                case 051: { // COMb
                    strprintf (strbuf, " COM%c", byte ? 'B' : ' ');
                    goto s_endsingle;
                }
                case 052: { // INCb
                    strprintf (strbuf, " INC%c", byte ? 'B' : ' ');
                    goto s_endsingle;
                }
                case 053: { // DECb
                    strprintf (strbuf, " DEC%c", byte ? 'B' : ' ');
                    goto s_endsingle;
                }
                case 054: { // NEGb
                    strprintf (strbuf, " NEG%c", byte ? 'B' : ' ');
                    goto s_endsingle;
                }
                case 055: { // ADCb
                    strprintf (strbuf, " ADC%c", byte ? 'B' : ' ');
                    goto s_endsingle;
                }
                case 056: { // SBCb
                    strprintf (strbuf, " SBC%c", byte ? 'B' : ' ');
                    goto s_endsingle;
                }
                case 057: { // TSTb
                    strprintf (strbuf, " TST%c", byte ? 'B' : ' ');
                    goto s_endsingle;
                }
                case 060: { // RORb
                    strprintf (strbuf, " ROR%c", byte ? 'B' : ' ');
                    goto s_endsingle;
                }
                case 061: { // ROLb
                    strprintf (strbuf, " ROL%c", byte ? 'B' : ' ');
                    goto s_endsingle;
                }
                case 062: { // ASRb
                    strprintf (strbuf, " ASR%c", byte ? 'B' : ' ');
                    goto s_endsingle;
                }
                case 063: { // ASLb
                    strprintf (strbuf, " ASL%c", byte ? 'B' : ' ');
                    goto s_endsingle;
                }
                case 064: {
                    if (byte) {     // MTPS
                        strbuf->append (" MTPS");
                        goto s_endsingle;
                    }
                    strprintf (strbuf, " MARK %02o", instreg & 077);
                    goto s_endinst;
                }
                case 065: {         // MFPI/D
                    strbuf->append (" MFPI");
                    goto s_endsingle;
                }
                case 066: {         // MTPI/D
                    strbuf->append (" MTPI");
                    goto s_endsingle;
                }
                case 067: {
                    if (byte) {     // MFPS
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
                            strbuf->append (" RTI");
                            goto s_endinst;
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
                            strbuf->append (" RTT");
                            goto s_endinst;
                        }
                    }
                    break;
                }
            }
            break;
        }

        case 1: {   // MOVb
            strprintf (strbuf, " MOV%c", byte ? 'B' : ' ');
            goto s_enddouble;
        }
        case 2: {   // CMPb
            strprintf (strbuf, " CMP%c", byte ? 'B' : ' ');
            goto s_enddouble;
        }
        case 3: {   // BITb
            strprintf (strbuf, " BIT%c", byte ? 'B' : ' ');
            goto s_enddouble;
        }
        case 4: {   // BICb
            strprintf (strbuf, " BIC%c", byte ? 'B' : ' ');
            goto s_enddouble;
        }
        case 5: {   // BISb
            strprintf (strbuf, " BIS%c", byte ? 'B' : ' ');
            goto s_enddouble;
        }
        case 6: {   // ADD/SUB
            strbuf->append (byte ? " SUB " : " ADD ");
            goto s_enddouble;
        }

        case 7: {
            // x 111 xxx xxx xxx xxx
            if (! byte) {
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

s_endextarith:;
    getopaddr (strbuf, instreg, &used, operand1, operand2);
    strprintf (strbuf, ", R%o", (instreg >> 6) & 7);
    goto s_endinst;
s_enddouble:;
    getopaddr (strbuf, instreg >> 6, &used, operand1, operand2);
    strbuf->push_back (',');
s_endsingle:;
    getopaddr (strbuf, instreg, &used, operand1, operand2);

s_endinst:;
    return used;
}

// determine address of source or destination operand
static void getopaddr (std::string *strbuf, uint16_t mr, int *used_r, uint16_t operand1, uint16_t operand2)
{
    uint16_t op;
    switch (*used_r) {
        case 1: op = operand1; break;
        case 2: op = operand2; break;
        default: abort ();
    }
    switch ((mr >> 3) & 7) {
        case 0: {
            strprintf (strbuf, " R%o", mr & 7);
            break;
        }
        case 1: {
            strprintf (strbuf, " @R%o", mr & 7);
            break;
        }
        case 2: {
            if ((mr & 7) == 7) {
                strprintf (strbuf, " #%06o", op);
                (*used_r) ++;
            } else {
                strprintf (strbuf, " (R%o)+", mr & 7);
            }
            break;
        }
        case 3: {
            if ((mr & 7) == 7) {
                strprintf (strbuf, " @#%06o", op);
                (*used_r) ++;
            } else {
                strprintf (strbuf, " @(R%o)+", mr & 7);
            }
            break;
        }
        case 4: {
            strprintf (strbuf, " -(R%o)", mr & 7);
            break;
        }
        case 5: {
            strprintf (strbuf, " @-(R%o)", mr & 7);
            break;
        }
        case 6: {
            strprintf (strbuf, " %06o(R%o)", op, mr & 7);
            (*used_r) ++;
            break;
        }
        case 7: {
            strprintf (strbuf, " @%06o(R%o)", op, mr & 7);
            (*used_r) ++;
            break;
        }
        default: abort ();
    }
}
