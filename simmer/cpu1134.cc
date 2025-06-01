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

#include <stdio.h>
#include <stdlib.h>

#include "cpu1134.h"
#include "swlight.h"
#include "../ccode/disassem.h"
#include "../ccode/z11defs.h"

#define FIELD(r,m) (((r) & (m)) / ((m) & - (m)))
#define HALTOP 0
#define SEXTBW(byte) ((((byte) & 0377) ^ 0200) - 0200)
#define YELLOWSTK 0400  // proc hndbk 1976 p4-67

#define P_NOTFPSIM 1
#define P_POWEROFF 2
#define P_POWERON  3
#define P_HALTBYSW 4
#define P_JAMMEDUP 5
#define P_RUNNING  6

#define T_BPTRACE 0014
#define T_EMT     0030
#define T_ILLINST 0010
#define T_ILLJMPM 0004
#define T_ILLPSWM 0004
#define T_IOT     0020
#define T_MMABORT 0250
#define T_ODDADDR 0004
#define T_TIMEOUT 0004
#define T_TRAP    0034
#define T_YELOSTK 0004

CPU1134 *CPU1134::singleton;

struct CPU1134Trap : std::exception {
    uint8_t vector;

    CPU1134Trap (uint8_t v);
};

bool CPU1134::cpuhaltins ()
{
    return singleton->jammedup ();
}

// constructor
CPU1134::CPU1134 ()
{
    singleton = this;
    logit = NULL;

    // declare what unibus addresses we respond to
    for (int i = 0; i < 8; i ++) {
        unidevtable[017700/2+i] = this; // gprs
        unidevtable[012300/2+i] = this; // knlpdrs
        unidevtable[012340/2+i] = this; // knlpars
        unidevtable[017600/2+i] = this; // usrpdrs
        unidevtable[017640/2+i] = this; // usrpars
    }
    unidevtable[017572/2] = this;       // mmr0
    unidevtable[017576/2] = this;       // mmr2
    unidevtable[017776/2] = this;       // psw

    // debug prints
    char const *dbgenv = getenv ("cpu1134_debug");
    int dbgint = (dbgenv == NULL) ? 0 : atoi (dbgenv);
    dbg1 = dbgint >= 1;
    dbg2 = dbgint >= 2;

    // fpga reset
    regctla =
        (FM_OFF << 30) |    // FM_OFF disconnect from bus
        (    1U << 21);     // man_npg_out_l
    regctlb =
        (   15U << 24);     // man_bg_out_l
    regctli = 0;
    lastpoweron = false;
    lastprint = 0;
}

// respond to axibus requests
uint32_t CPU1134::axirdslv (uint32_t index)
{
    switch (index) {
        case  0: return 0x31314017; // "11"; size; version
        case  1: return regctla;
        case  2: return regctlb;
        case  3: return 0;
        case  4: return
            ((jammedup () || SWLight::swlhaltreq ())  << 28);    // dev_hltgr_l
        case  5: return 0;
        case  6: return 0;
        case  7: return 0;
        case  8: return 0;
        case  9: return regctli;
        case 10: return ((uint32_t) psw << 16) | gprs[7];
        case 11: return (lastpoweron << 1) | jammedup ();
        case 28: return 0;
        case 29: return 0;
        case 30: return 0;
        case 31: return 0;
    }
    return 0xDEADBEEF;
}

void CPU1134::axiwrslv (uint32_t index, uint32_t data)
{
    switch (index) {
        case 1: regctla = data; break;
        case 2: regctlb = data; break;
    }
}

// don't do anything to CPU state for RESET instruction
void CPU1134::resetslave ()
{ }

// CPU itself does not request any interrupt
uint8_t CPU1134::getintslave (uint16_t level)
{
    return 0;
}

// something on unibus is attempting to read a cpu register
bool CPU1134::rdslave (uint32_t physaddr, uint16_t *data)
{
    if ((physaddr & 0777760) == 0777700) {
        *data = gprs[physaddr&017];
        return true;
    }

    if ((physaddr & 0777760) == 0772300) {
        *data = knlpdrs[(physaddr&016)>>1];
        return true;
    }

    if ((physaddr & 0777760) == 0772340) {
        *data = knlpars[(physaddr&016)>>1];
        return true;
    }

    if (physaddr == 0777572) {
        *data = mmr0;
        return true;
    }

    if (physaddr == 0777576) {
        *data = mmr2;
        return true;
    }

    if ((physaddr & 0777760) == 0777600) {
        *data = usrpdrs[(physaddr&016)>>1];
        return true;
    }

    if ((physaddr & 0777760) == 0777640) {
        *data = usrpars[(physaddr&016)>>1];
        return true;
    }

    if (physaddr == 0777776) {
        *data = psw;
        return true;
    }

    return false;
}

// something on unibus is attempting to write a cpu register
bool CPU1134::wrslave (uint32_t physaddr, uint16_t data, bool byte)
{
    if (byte) {
        uint16_t word;
        if (! rdslave (physaddr & 0777776, &word)) return false;
        if (physaddr & 1) data = (data << 8) | (word & 0377);
                else data = (word & 0177400) | (data & 0377);
    }

    if ((physaddr & 0777760) == 0777700) {
        gprs[physaddr&017] = data;
        return true;
    }

    if ((physaddr & 0777760) == 0772300) {
        uint16_t page  = (physaddr & 016) >> 1;
        uint16_t npdr  = data & 077416;
        knlpdrs[page]  = npdr;
        return true;
    }

    if ((physaddr & 0777760) == 0772340) {
        uint16_t page  = (physaddr & 016) >> 1;
        uint16_t npar  = data & 007777;
        knlpars[page]  = npar;
        knlpdrs[page] &= 077416;
        return true;
    }

    if (physaddr == 0777572) {
        mmr0 = data & 0160557;
        return true;
    }

    if (physaddr == 0777576) {
        return true;
    }

    if ((physaddr & 0777760) == 0777600) {
        usrpdrs[(physaddr&016)>>1] = data & 077416;
        return true;
    }

    if ((physaddr & 0777760) == 0777640) {
        usrpars[(physaddr&016)>>1]  = data & 07777;
        usrpdrs[(physaddr&016)>>1] &= 077416;
        return true;
    }

    if (physaddr == 0777776) {
        psw = (psw & 020) | (data & 0170357);
        return true;
    }

    return false;
}

// step processor state (one interrupt or instruction)
void CPU1134::stepit ()
{
    // don't do anything until enabled
    if (FIELD (regctla, a_fpgamode) != FM_SIM) {
        if (dbg1 && (lastprint != P_NOTFPSIM)) {
            printf ("CPU1134::stepit*: not FM_SIM\n");
            lastprint = P_NOTFPSIM;
        }
        lastpoweron = false;
        return;
    }

    // power must be on
    bool thispoweron = ! FIELD (regctla, a_man_dc_lo_out_h);
    if (! thispoweron) {
        if (dbg1 && (lastprint != P_POWEROFF)) {
            printf ("CPU1134::stepit*: power off\n");
            lastprint = P_POWEROFF;
        }
        if (lastpoweron) resetmaster ();
        lastpoweron = false;
        return;
    }

    // if power just came on, do a power-on reset (counts as a step)
    if (! lastpoweron) {
        if (dbg1) {
            lastprint = P_POWERON;
            printf ("CPU1134::stepit*: powering up\n");
        }
        resetmaster ();
        instreg     = HALTOP;
        lastpoweron = true;
        mmr0        = 0;
        psw         = 0;
        regctli     = 1;
        try {
            gprs[7] = rdwordphys (024);
            psw     = rdwordphys (026);
            instreg = ~ HALTOP;
            if (dbg1) printf ("CPU1134::stepit*: powered up to PC=%06o PS=%06o\n", gprs[7], psw);
        } catch (CPU1134Trap &t) {
            fprintf (stderr, "CPU1134::stepit: trap %03o reading power-on vector\n", t.vector);
        }
        return;
    }

    // don't do anything if console has us halted
    if (! SWLight::swlstepreq ()) {
        if (dbg1 && (lastprint != P_HALTBYSW)) {
            printf ("CPU1134::stepit*: halted by switches at PC=%06o PS=%06o\n", gprs[7], psw);
            lastprint = P_HALTBYSW;
        }
        return;
    }

    // don't do anything if executed an HALT instruction or had double-fault, etc
    // - must be power cycled with a_man_dc_lo_out_h to recover
    if (jammedup ()) {
        if (dbg1 && (lastprint != P_JAMMEDUP)) {
            printf ("CPU1134::stepit*: jammed up at PC=%06o PS=%06o\n", gprs[7], psw);
            lastprint = P_JAMMEDUP;
        }
        return;
    }

    if (dbg1 && (lastprint != P_RUNNING)) {
        printf ("CPU1134::stepit*: running at PC=%06o PS=%06o\n", gprs[7], psw);
        lastprint = P_RUNNING;
    }

    regctli ++;

    try {
        if (dbg2) printf ("CPU1134::stepit*: fetch");

        // check interrupts
        uint16_t prio = (psw >> 5) & 7;
        for (uint16_t level = 8; (-- level >= 4) && (level > prio);) {
            uint8_t vector = getintmaster (level);
            if (vector != 0) {
                if (dbg2) printf (" interrupt");
                throw CPU1134Trap (vector);
            }
        }

        // fetch next instruction
        instreg = rdwordvirt (gprs[7], psw >> 14);
        if (! (mmr0 & 0160000)) mmr2 = gprs[7];

        // maybe log it
        if (logit != NULL) {
            uint16_t op1 = 0xDEAD;
            uint16_t op2 = 0xBEEF;
            disread (this, gprs[7] + 2, &op1);
            disread (this, gprs[7] + 4, &op2);
            std::string strbuf;
            disassem (&strbuf, instreg, op1, op2, disread, this);
            fprintf (logit, "%06o.%06o %s\n", gprs[7], psw, strbuf.c_str ());
        }

        // increment program counter
        gprs[7] += 2;

        // decode and execute
        havedstaddr = false;
        yellowstkck = false;
        bool byte = (instreg & 0100000) != 0;
        switch ((instreg >> 12) & 7) {

            case 0: {
                // x 000 xxx xxx xxx xxx
                switch ((instreg >> 6) & 077) {
                    case 001: { // JMP
                        if (! byte) {
                            if (dbg2) printf (" JMP");
                            if ((instreg & 070) == 0) throw CPU1134Trap (T_ILLJMPM);
                            gprs[7] = getopaddr (instreg, false);
                            if (dbg2) printf (" => %06o", gprs[7]);
                            goto s_endinst;
                        }
                        break;
                    }
                    case 002: {
                        if ((instreg & 0177770) == 0000200) {   // RTS
                            if (dbg2) printf (" RTS R%o", instreg & 7);
                            uint16_t spgprx = gprx (6, psw >> 14);
                            uint16_t rgprx = gprx (instreg, psw >> 14);
                            gprs[7] = gprs[rgprx];
                            gprs[rgprx] = rdwordvirt (gprs[spgprx], psw >> 14);
                            if (dbg2) printf (" => PC=%06o Rn=%06o", gprs[7], gprs[rgprx]);
                            gprs[spgprx] += 2;
                            goto s_endinst;
                        }
                        if ((instreg & 0177740) == 0000240) {   // CCS
                            uint16_t bit = (instreg >> 4) & 1;
                            if (instreg & 001) psw = (psw & ~ 001) | (bit << 0);
                            if (instreg & 002) psw = (psw & ~ 002) | (bit << 1);
                            if (instreg & 004) psw = (psw & ~ 004) | (bit << 2);
                            if (instreg & 010) psw = (psw & ~ 010) | (bit << 3);
                            if (dbg2) printf (" %c%c%c%c%c => %06o",
                                bit ? 'S' : 'C',
                                instreg & 010 ? 'N' : '-',
                                instreg & 004 ? 'Z' : '-',
                                instreg & 002 ? 'V' : '-',
                                instreg & 001 ? 'C' : '-', psw);
                            goto s_endinst;
                        }
                        break;
                    }
                    case 003: {
                        if (! byte) {   // SWAB
                            if (dbg2) printf (" SWAB");
                            uint16_t dstval = readdst (false);
                            uint16_t result = (dstval << 8) | (dstval >> 8);
                            if (dbg2) printf (" => %06o", result);
                            updnzvc (result, true, 0, 0);
                            writedst (result, false);
                            goto s_endinst;
                        }
                        break;
                    }
                    case 040 ... 047: {   // JSR
                        if (! byte) {
                            if (dbg2) printf (" JSR R%o", (instreg >> 6) & 7);
                            if ((instreg & 070) == 0) throw CPU1134Trap (T_ILLJMPM);
                            uint16_t jmpaddr = getopaddr (instreg, false);
                            if (dbg2) printf (" => %06o", jmpaddr);
                            yellowstkck = true;
                            uint16_t spgprx = gprx (6, psw >> 14);
                            uint16_t rgprx = gprx (instreg >> 6, psw >> 14);
                            gprs[spgprx] -= 2;
                            wrwordvirt (gprs[spgprx], gprs[rgprx], psw >> 14);
                            gprs[rgprx] = gprs[7];
                            gprs[7] = jmpaddr;
                            goto s_endinst;
                        }
                        if ((instreg & 0177400) == 0104000) {   // EMT
                            if (dbg2) printf (" EMT %03o", instreg & 0377);
                            throw CPU1134Trap (T_EMT);
                        }
                        if ((instreg & 0177400) == 0104400) {   // TRAP
                            if (dbg2) printf (" TRAP %03o", instreg & 0377);
                            throw CPU1134Trap (T_TRAP);
                        }
                        break;
                    }
                    case 050: { // CLRb
                        if (dbg2) printf (" CLR%c", byte ? 'B' : ' ');
                        updnzvc (0, byte, 0, 0);
                        writedst (0, byte);
                        goto s_endinst;
                    }
                    case 051: { // COMb
                        if (dbg2) printf (" COM%c", byte ? 'B' : ' ');
                        uint16_t dstval = readdst (byte);
                        uint16_t result = ~ dstval;
                        updnzvc (result, byte, 0, 1);
                        writedst (result, byte);
                        goto s_endinst;
                    }
                    case 052: { // INCb
                        if (dbg2) printf (" INC%c", byte ? 'B' : ' ');
                        uint16_t dstval = readdst (byte);
                        uint16_t result = dstval + 1;
                        updnzvc (result, byte, addvbit (dstval, 1, byte), psw & 1);
                        writedst (result, byte);
                        goto s_endinst;
                    }
                    case 053: { // DECb
                        if (dbg2) printf (" DEC%c", byte ? 'B' : ' ');
                        uint16_t dstval = readdst (byte);
                        uint16_t result = dstval - 1;
                        updnzvc (result, byte, subvbit (dstval, 1, byte), psw & 1);
                        writedst (result, byte);
                        goto s_endinst;
                    }
                    case 054: { // NEGb
                        if (dbg2) printf (" NEG%c", byte ? 'B' : ' ');
                        uint16_t dstval = readdst (byte);
                        uint16_t result = - dstval;
                        updnzvc (result, byte, subvbit (0, dstval, byte), subcbit (0, dstval, byte));
                        writedst (result, byte);
                        goto s_endinst;
                    }
                    case 055: { // ADCb
                        if (dbg2) printf (" ADC%c", byte ? 'B' : ' ');
                        uint16_t dstval = readdst (byte);
                        uint16_t result = dstval + (psw & 1);
                        updnzvc (result, byte, addvbit (dstval, psw & 1, byte), addcbit (dstval, psw & 1, byte));
                        writedst (result, byte);
                        goto s_endinst;
                    }
                    case 056: { // SBCb
                        if (dbg2) printf (" SBC%c", byte ? 'B' : ' ');
                        uint16_t dstval = readdst (byte);
                        uint16_t result = dstval - (psw & 1);
                        updnzvc (result, byte, subvbit (dstval, psw & 1, byte), subcbit (dstval, psw & 1, byte));
                        writedst (result, byte);
                        goto s_endinst;
                    }
                    case 057: { // TSTb
                        if (dbg2) printf (" TST%c", byte ? 'B' : ' ');
                        uint16_t dstval = readdst (byte);
                        updnzvc (dstval, byte, 0, 0);
                        goto s_endinst;
                    }
                    case 060: { // RORb
                        if (dbg2) printf (" ROR%c", byte ? 'B' : ' ');
                        uint16_t dstval = readdst (byte);
                        uint16_t result = ((byte ? dstval & 0xFFU : dstval) >> 1) | ((psw & 1) << (byte ? 7 : 15));
                        updnzvc (result, byte, asrvbit (dstval, result, byte), dstval & 1);
                        writedst (result, byte);
                        goto s_endinst;
                    }
                    case 061: { // ROLb
                        if (dbg2) printf (" ROL%c", byte ? 'B' : ' ');
                        uint16_t dstval = readdst (byte);
                        uint16_t result = (dstval << 1) | (psw & 1);
                        updnzvc (result, byte, aslvbit (dstval, result, byte), (dstval >> (byte ? 7 : 15)) & 1);
                        writedst (result, byte);
                        goto s_endinst;
                    }
                    case 062: { // ASRb
                        if (dbg2) printf (" ASR%c", byte ? 'B' : ' ');
                        uint16_t dstval = readdst (byte);
                        uint16_t result = ((int16_t) (byte ? SEXTBW(dstval) : dstval)) >> 1;
                        updnzvc (result, byte, asrvbit (dstval, result, byte), dstval & 1);
                        writedst (result, byte);
                        goto s_endinst;
                    }
                    case 063: { // ASLb
                        if (dbg2) printf (" ASL%c", byte ? 'B' : ' ');
                        uint16_t dstval = readdst (byte);
                        uint16_t result = dstval << 1;
                        updnzvc (result, byte, aslvbit (dstval, result, byte), (dstval >> (byte ? 7 : 15)) & 1);
                        writedst (result, byte);
                        goto s_endinst;
                    }
                    case 064: {
                        if (byte) {     // MTPS
                            if (dbg2) printf (" MTPS");
                            uint16_t dstval = readdst (true);
                            if ((psw & 0140000) == 0) {
                                psw = (psw & ~ 0340) | (dstval & 0340);
                            }
                            psw = (psw & ~ 0017) | (dstval & 0017);
                        }
                        else {          // MARK
                            if (dbg2) printf (" MARK %02o", instreg & 077);
                            uint16_t spgprx = gprx (6, psw >> 14);
                            gprs[spgprx] = gprs[7] + 2 * (instreg & 077);
                            gprs[7] = gprs[5];
                            gprs[5] = rdwordvirt (gprs[spgprx], psw >> 14);
                            gprs[spgprx] += 2;
                            if (dbg2) printf (" => R5=%06o SP=%06o PC=%06o", gprs[5], gprs[spgprx], gprs[7]);
                        }
                        goto s_endinst;
                    }
                    case 065: {         // MFPI/D
                        if (dbg2) printf (" MFPI");
                        uint16_t srcval;
                        if ((instreg & 070) == 0) {
                            if (dbg2) printf (" R%o", instreg & 7);
                            uint16_t srcgprx = gprx (instreg & 7, psw >> 12);
                            srcval = gprs[srcgprx];
                        } else {
                            uint16_t srcadr = getopaddr (instreg, false);
                            srcval = rdwordvirt (srcadr, psw >> 12);
                        }
                        if (dbg2) printf (" => %06o", srcval);

                        uint16_t spgprx = gprx (6, psw >> 14);
                        yellowstkck = true;
                        gprs[spgprx] -= 2;
                        updnzvc (srcval, false, 0, psw & 1);
                        wrwordvirt (gprs[spgprx], srcval, psw >> 14);
                        goto s_endinst;
                    }
                    case 066: {         // MTPI/D
                        if (dbg2) printf (" MTPI");
                        uint16_t spgprx = gprx (6, psw >> 14);
                        uint16_t srcval = rdwordvirt (gprs[spgprx], psw >> 14);
                        gprs[spgprx] += 2;

                        if ((instreg & 070) == 0) {
                            if (dbg2) printf (" R%o", instreg & 7);
                            uint16_t srcgprx = gprx (instreg & 7, psw >> 12);
                            updnzvc (srcval, false, 0, psw & 1);
                            gprs[srcgprx] = srcval;
                            if (dbg2) printf (" [R%o<=%06o]", instreg & 7, srcval);
                        } else {
                            uint16_t srcadr = getopaddr (instreg, false);
                            updnzvc (srcval, false, 0, psw & 1);
                            wrwordvirt (srcadr, srcval, psw >> 12);
                        }
                        goto s_endinst;
                    }
                    case 067: {
                        if (byte) {     // MFPS
                            if (dbg2) printf (" MFPS");
                            uint16_t result = (int16_t) (int8_t) psw;
                            updnzvc (result, true, 0, psw & 1);
                            writedst (result, (instreg & 070) != 0);
                        }
                        else {          // SEXT
                            if (dbg2) printf (" SEXT");
                            uint16_t result = (psw & 010) ? -1 : 0;
                            updnzvc (result, false, 0, psw & 1);
                            writedst (result, false);
                        }
                        goto s_endinst;
                    }
                }

                // x 000 xxx xxx xxx xxx
                bool blt = ((psw >> 3) ^ (psw >> 1)) & 1;
                bool ble = ((psw >> 2) & 1) | blt;
                uint16_t newpc = gprs[7] + SEXTBW(instreg) * 2;
                switch (((instreg >> 12) & 010) | ((instreg >> 8) & 007)) {
                    case 001: if (dbg2) printf (" BR   %03o", instreg & 0377);                    { gprs[7] = newpc; if (dbg2) printf (" => %06o", newpc); } goto s_endinst; // BR
                    case 002: if (dbg2) printf (" BNE  %03o", instreg & 0377); if (! (psw & 004)) { gprs[7] = newpc; if (dbg2) printf (" => %06o", newpc); } goto s_endinst; // BNE
                    case 003: if (dbg2) printf (" BEQ  %03o", instreg & 0377); if (  (psw & 004)) { gprs[7] = newpc; if (dbg2) printf (" => %06o", newpc); } goto s_endinst; // BEQ
                    case 004: if (dbg2) printf (" BGE  %03o", instreg & 0377); if (!  blt       ) { gprs[7] = newpc; if (dbg2) printf (" => %06o", newpc); } goto s_endinst; // BGE
                    case 005: if (dbg2) printf (" BLT  %03o", instreg & 0377); if (   blt       ) { gprs[7] = newpc; if (dbg2) printf (" => %06o", newpc); } goto s_endinst; // BLT
                    case 006: if (dbg2) printf (" BGT  %03o", instreg & 0377); if (!  ble       ) { gprs[7] = newpc; if (dbg2) printf (" => %06o", newpc); } goto s_endinst; // BGT
                    case 007: if (dbg2) printf (" BLE  %03o", instreg & 0377); if (   ble       ) { gprs[7] = newpc; if (dbg2) printf (" => %06o", newpc); } goto s_endinst; // BLE
                    case 010: if (dbg2) printf (" BPL  %03o", instreg & 0377); if (! (psw & 010)) { gprs[7] = newpc; if (dbg2) printf (" => %06o", newpc); } goto s_endinst; // BPL
                    case 011: if (dbg2) printf (" BMI  %03o", instreg & 0377); if (  (psw & 010)) { gprs[7] = newpc; if (dbg2) printf (" => %06o", newpc); } goto s_endinst; // BMI
                    case 012: if (dbg2) printf (" BHI  %03o", instreg & 0377); if (! (psw & 005)) { gprs[7] = newpc; if (dbg2) printf (" => %06o", newpc); } goto s_endinst; // BHI
                    case 013: if (dbg2) printf (" BLOS %03o", instreg & 0377); if (  (psw & 005)) { gprs[7] = newpc; if (dbg2) printf (" => %06o", newpc); } goto s_endinst; // BLOS
                    case 014: if (dbg2) printf (" BVC  %03o", instreg & 0377); if (! (psw & 002)) { gprs[7] = newpc; if (dbg2) printf (" => %06o", newpc); } goto s_endinst; // BVC
                    case 015: if (dbg2) printf (" BVS  %03o", instreg & 0377); if (  (psw & 002)) { gprs[7] = newpc; if (dbg2) printf (" => %06o", newpc); } goto s_endinst; // BVS
                    case 016: if (dbg2) printf (" BCC  %03o", instreg & 0377); if (! (psw & 001)) { gprs[7] = newpc; if (dbg2) printf (" => %06o", newpc); } goto s_endinst; // BCC
                    case 017: if (dbg2) printf (" BCS  %03o", instreg & 0377); if (  (psw & 001)) { gprs[7] = newpc; if (dbg2) printf (" => %06o", newpc); } goto s_endinst; // BCS

                    // 0 000 x00 0xx xxx xxx
                    case 0: {
                        switch (instreg) {
                            case 0: {
                                if (dbg2) printf (" HALT");
                                if ((mmr0 & 1) && (psw & 0160000)) throw CPU1134Trap (T_ILLINST);
                                if (dbg2) printf ("\n");
                                if (dbg1) printf ("CPU1134::stepit*: HALT instr at PC=%06o PS=%06o\n", gprs[7], psw);
                                return;                                 // HALT
                            }
                            case 1: {
                                if (dbg2) printf (" WAIT");
                                goto s_endinst;                         // WAIT
                            }
                            case 2: {                                   // RTI
                                if (dbg2) printf (" RTI");
                                uint16_t spgprx = gprx (6, psw >> 14);
                                uint16_t newpc = rdwordvirt (gprs[spgprx], psw >> 14);
                                gprs[spgprx] += 2;
                                uint16_t newps = rdwordvirt (gprs[spgprx], psw >> 14);
                                gprs[spgprx] += 2;
                                gprs[7] = newpc;
                                if (psw & 0140000) {
                                    psw = (psw & 0170340) | (newps & 037);
                                } else {
                                    psw = newps & 0170377;
                                }
                                if (dbg2) printf (" => PC=%06o PS=%06o", gprs[7], psw);
                                goto s_endinst;
                            }
                            case 3: {
                                if (dbg2) printf (" BPT");
                                throw CPU1134Trap (T_BPTRACE);              // BPT
                            }
                            case 4: {
                                if (dbg2) printf (" IOT");
                                throw CPU1134Trap (T_IOT);                  // IOT
                            }
                            case 5: {
                                if (dbg2) printf (" RESET");
                                if ((psw & 0140000) == 0) resetmaster ();   // RESET
                                goto s_endinst;
                            }
                            case 6: {                                       // RTT
                                if (dbg2) printf (" RTT");
                                uint16_t spgprx = gprx (6, psw >> 14);
                                uint16_t newpc = rdwordvirt (gprs[spgprx], psw >> 14);
                                gprs[spgprx] += 2;
                                uint16_t newps = rdwordvirt (gprs[spgprx], psw >> 14);
                                gprs[spgprx] += 2;
                                gprs[7] = newpc;
                                if (psw & 0140000) {
                                    psw = (psw & 0170340) | (newps & 037);
                                } else {
                                    psw = newps & 0170377;
                                }
                                if (dbg2) printf (" => PC=%06o PS=%06o", gprs[7], psw);
                                goto s_endrtt;
                            }
                        }
                        break;
                    }
                }
                break;
            }

            case 1: {   // MOVb
                if (dbg2) printf (" MOV%c", byte ? 'B' : ' ');
                uint16_t srcval = readsrc (byte);
                if (byte && ((instreg & 070) == 0)) {
                    srcval = ((srcval & 0377) ^ 0200) - 0200;
                    byte   = false;
                }
                updnzvc (srcval, byte, 0, psw & 1);
                writedst (srcval, byte);
                goto s_endinst;
            }
            case 2: {   // CMPb
                if (dbg2) printf (" CMP%c", byte ? 'B' : ' ');
                uint16_t srcval = readsrc (byte);
                uint16_t dstval = readdst (byte);
                uint16_t result = srcval - dstval;
                updnzvc (result, byte, subvbit (srcval, dstval, byte), subcbit (srcval, dstval, byte));
                goto s_endinst;
            }
            case 3: {   // BITb
                if (dbg2) printf (" BIT%c", byte ? 'B' : ' ');
                uint16_t srcval = readsrc (byte);
                uint16_t dstval = readdst (byte);
                updnzvc (srcval & dstval, byte, 0, psw & 1);
                goto s_endinst;
            }
            case 4: {   // BICb
                if (dbg2) printf (" BIC%c", byte ? 'B' : ' ');
                uint16_t srcval = readsrc (byte);
                uint16_t dstval = readdst (byte);
                uint16_t result = dstval & ~ srcval;
                updnzvc (result, byte, 0, psw & 1);
                writedst (result, byte);
                goto s_endinst;
            }
            case 5: {   // BISb
                if (dbg2) printf (" BIS%c", byte ? 'B' : ' ');
                uint16_t srcval = readsrc (byte);
                uint16_t dstval = readdst (byte);
                uint16_t result = dstval | srcval;
                updnzvc (result, byte, 0, psw & 1);
                writedst (result, byte);
                goto s_endinst;
            }
            case 6: {   // ADD/SUB
                if (dbg2) printf (" %s ", byte ? "SUB" : "ADD");
                uint16_t srcval = readsrc (false);
                uint16_t dstval = readdst (false);
                if (byte) {
                    uint16_t result = dstval - srcval;
                    updnzvc (result, false, subvbit (dstval, srcval, false), subcbit (dstval, srcval, false));
                    writedst (result, false);
                } else {
                    uint16_t result = dstval + srcval;
                    updnzvc (result, false, addvbit (dstval, srcval, false), addcbit (dstval, srcval, false));
                    writedst (result, false);
                }
                goto s_endinst;
            }

            case 7: {
                // x 111 xxx xxx xxx xxx
                if (! byte) {
                    // 0 111 xxx xxx xxx xxx
                    switch ((instreg >> 9) & 7) {
                        case 0: {   // MUL
                            if (dbg2) printf (" MUL");
                            int16_t srcval   = readdst (false);
                            if (dbg2) printf (", R%o", (instreg >> 6) & 7);
                            uint16_t dstgprx = gprx (instreg >> 6, psw >> 14);
                            int16_t dstval   = gprs[dstgprx];
                            int32_t prod     = srcval * dstval;
                            gprs[dstgprx]    = prod >> 16;
                            gprs[dstgprx|1]  = prod;
                            updnzvc (prod >> 16, false, 0, (psw & 1) || (prod != (int32_t) (int16_t) prod));
                            goto s_endinst;
                        }
                        case 1: {   // DIV
                            if (dbg2) printf (" DIV");
                            int16_t srcval = readdst (false);
                            if (dbg2) printf (", R%o", (instreg >> 6) & 7);
                            if (srcval == 0) {
                                updnzvc (0, false, 1, 1);
                            } else {
                                uint16_t dstgprx = gprx (instreg >> 6, psw >> 14);
                                int32_t dividend = (((uint32_t) gprs[dstgprx]) << 16) | (uint32_t) gprs[dstgprx|1];
                                int32_t quotient = dividend / srcval;
                                int32_t remaindr = dividend % srcval;
                                gprs[dstgprx]    = quotient;
                                gprs[dstgprx|1]  = remaindr;
                                updnzvc (quotient, false, quotient == (int32_t) (int16_t) quotient, 0);
                            }
                            goto s_endinst;
                        }
                        case 2: {   // ASH
                            if (dbg2) printf (" ASH");
                            int16_t srcval   = readdst (false);
                            if (dbg2) printf (", R%o", (instreg >> 6) & 7);
                            uint16_t dstgprx = gprx (instreg >> 6, psw >> 14);
                            int16_t dividend = gprs[dstgprx];
                            bool vbit = false;
                            bool cbit = psw & 1;
                            if (srcval & 040) {
                                do {
                                    cbit  = dividend & 1;
                                    dividend >>= 1;
                                } while (++ srcval & 037);
                            } else if (srcval & 037) {
                                do {
                                    vbit |= ((dividend ^ (dividend * 2)) < 0);
                                    cbit  = (dividend < 0);
                                    dividend <<= 1;
                                } while (-- srcval & 037);
                            }
                            gprs[dstgprx] = dividend;
                            updnzvc (dividend, false, vbit, cbit);
                            goto s_endinst;
                        }
                        case 3: {   // ASHC
                            if (dbg2) printf (", ASHC");
                            int16_t srcval   = readdst (false);
                            if (dbg2) printf (" R%o", (instreg >> 6) & 7);
                            uint16_t dstgprx = gprx (instreg >> 6, psw >> 14);
                            int32_t dividend = (((uint32_t) gprs[dstgprx]) << 16) | (uint32_t) gprs[dstgprx|1];
                            bool vbit = false;
                            bool cbit = psw & 1;
                            if (srcval & 040) {
                                do {
                                    cbit  = dividend & 1;
                                    dividend >>= 1;
                                } while (++ srcval & 037);
                            } else if (srcval & 037) {
                                do {
                                    vbit |= ((dividend ^ (dividend * 2)) < 0);
                                    cbit  = (dividend < 0);
                                    dividend <<= 1;
                                } while (-- srcval & 037);
                            }
                            gprs[dstgprx]   = dividend >> 16;
                            gprs[dstgprx|1] = dividend;
                            updnzvc (dividend >> 16, false, vbit, cbit);
                            goto s_endinst;
                        }
                        case 4: {   // XOR
                            if (dbg2) printf (" XOR R%o,", (instreg >> 6) & 7);
                            uint16_t srcgprx = gprx (instreg >> 6, psw >> 14);
                            uint16_t srcval  = gprs[srcgprx];
                            uint16_t dstval  = readdst (byte);
                            uint16_t result  = dstval ^ srcval;
                            updnzvc (result, false, 0, psw & 1);
                            writedst (result, false);
                            goto s_endinst;
                        }
                        case 7: {   // SOB
                            if (dbg2) printf (" SOB  R%o, %02o", (instreg >> 6) & 7, instreg & 077);
                            uint16_t srcgprx = gprx (instreg >> 6, psw >> 14);
                            uint16_t result = -- gprs[srcgprx];
                            if (dbg2) printf (" [R%o<=%06o]", (instreg >> 6) & 7, result);
                            if (result != 0) {
                                gprs[7] -= 2 * (instreg & 077);
                                if (dbg2) printf (" => %06o", gprs[7]);
                            }
                            goto s_endinst;
                        }
                    }
                }
            }
        }

        // illegal opcode
        if (dbg2) printf (" illinst");
        throw CPU1134Trap (T_ILLINST);

    s_endinst:;
        if (yellowstkck && ! (psw & 0140000) && (gprs[6] < YELLOWSTK)) {
            if (dbg2) printf (" yellow stack");
            throw CPU1134Trap (T_YELOSTK);
        }
        if (psw & 020) {
            if (dbg2) printf (" trace bit");
            throw CPU1134Trap (T_BPTRACE);
        }
    s_endrtt:;
    }

    catch (CPU1134Trap &t) {
        try {
            uint8_t vec = t.vector;
            while (true) {
                if (dbg2) printf (" => trap %03o", vec);
                if (vec & 3) throw (T_ODDADDR);
                uint16_t newpc = rdwordvirt (vec,     0);
                uint16_t newps = rdwordvirt (vec | 2, 0);

                if (logit != NULL) {
                    fprintf (logit, "%06o.%06o trap %03o %06o %06o\n", gprs[7], psw, vec, newpc, newps);
                }

                uint16_t nspgprx = gprx (6, newps >> 14);

                gprs[nspgprx] -= 2;
                wrwordvirt (gprs[nspgprx], psw,     newps >> 14);
                gprs[nspgprx] -= 2;
                wrwordvirt (gprs[nspgprx], gprs[7], newps >> 14);

                gprs[7] = newpc;
                psw     = (newps & 0140377) | ((psw >> 2) & 0030000);

                if ((vec == T_YELOSTK) || ((psw & 0140000) != 0) || (gprs[6] >= YELLOWSTK)) break;
                if (dbg2) printf (" yellow stack2");
                vec = T_YELOSTK;
            }
        } catch (CPU1134Trap &t2) {
            if (dbg2) printf ("\n");
            fprintf (stderr, "CPU1134::stepit: trap %03o got double fault %03o\n", t.vector, t2.vector);
            instreg = HALTOP;
            return;
        }
    }
    if (dbg2) printf ("\n");
}

// see if cpu executed an HALT instruction or has double-faulted, etc
// it requires a power-cycle with a_man_dc_lo_out_h to recover
// hardware equivalent of this is having an HALT in the IR, it jams the HLTRQ_L line low
bool CPU1134::jammedup ()
{
    if (! singleton->lastpoweron) return true;
    if (singleton->instreg != HALTOP) return false;
    if ((singleton->mmr0 & 1) && (singleton->psw & 0140000)) return false;
    return true;
}

// determine address of source operand then read operand value
uint16_t CPU1134::readsrc (bool byte)
{
    uint16_t data;
    if ((instreg & 07000) == 0) {
        uint16_t srcgprx = gprx (instreg >> 6, psw >> 14);
        data = gprs[srcgprx];
        if (dbg2) printf (" R%o [R%o=>%06o]", (instreg >> 6) & 7, (instreg >> 6) & 7, data);
    } else {
        uint16_t srcaddr = getopaddr (instreg >> 6, byte);
        data = byte ? rdbytevirt (srcaddr, psw >> 14) : rdwordvirt (srcaddr, psw >> 14);
    }
    if (dbg2) printf (",");
    return data;
}

// determine address of destination operand then read operand value
uint16_t CPU1134::readdst (bool byte)
{
    havedstaddr = true;
    if ((instreg & 070) == 0) {
        uint16_t dstgprx = gprx (instreg, psw >> 14);
        uint16_t data = gprs[dstgprx];
        if (dbg2) printf (" R%o [R%o=>%06o]", instreg & 7, instreg & 7, data);
        return data;
    }
    dstaddr = getopaddr (instreg, byte);
    return byte ? rdbytevirt (dstaddr, psw >> 14) : rdwordvirt (dstaddr, psw >> 14);
}

// determine address of destination operand (if not already) then write operand value
void CPU1134::writedst (uint16_t data, bool byte)
{
    if ((instreg & 070) == 0) {
        uint16_t dstgprx = gprx (instreg, psw >> 14);
        if (byte) data = (gprs[dstgprx] & ~ 0377) | (data & 0377);
        gprs[dstgprx] = data;
        if (dbg2) {
            if (! havedstaddr) printf (" R%o", instreg & 7);
            printf (" [R%o<=%0*o]", instreg & 7, byte ? 3 : 6, byte ? data & 0377 : data);
        }
    } else {
        if (! havedstaddr) {
            dstaddr = getopaddr (instreg, byte);
            havedstaddr = true;
        }
        if (byte) wrbytevirt (dstaddr, data, psw >> 14);
             else wrwordvirt (dstaddr, data, psw >> 14);
    }
}

// determine address of source or destination operand
uint16_t CPU1134::getopaddr (uint16_t mr, bool byte)
{
    uint16_t r = gprx (mr, psw >> 14);
    if ((r == 6) && (psw & 0140000)) r = 016;
    uint16_t i = (byte && ((mr & 6) != 6)) ? 1 : 2;
    switch ((mr >> 3) & 7) {
        case 1: {
            if (dbg2) printf (" @R%o", mr & 7);
            return gprs[r];
        }
        case 2: {
            if (dbg2) printf (" (R%o)+", mr & 7);
            uint16_t a = gprs[r];
            gprs[r] += i;
            return a;
        }
        case 3: {
            if (dbg2) printf (" @(R%o)+", mr & 7);
            uint16_t a = gprs[r];
            gprs[r] += 2;
            return rdwordvirt (a, psw >> 14);
        }
        case 4: {
            if (dbg2) printf (" -(R%o)", mr & 7);
            yellowstkck |= (r == 6);
            gprs[r] -= i;
            return gprs[r];
        }
        case 5: {
            if (dbg2) printf (" @-(R%o)", mr & 7);
            yellowstkck |= (r == 6);
            gprs[r] -= 2;
            return rdwordvirt (gprs[r], psw >> 14);
        }
        case 6: {
            uint16_t x = rdwordvirt (gprs[7], psw >> 14);
            if (dbg2) printf (" %06o(R%o)", x, mr & 7);
            gprs[7] += 2;
            return gprs[r] + x;
        }
        case 7: {
            uint16_t x = rdwordvirt (gprs[7], psw >> 14);
            if (dbg2) printf (" @%06o(R%o)", x, mr & 7);
            gprs[7] += 2;
            return rdwordvirt (gprs[r] + x, psw >> 14);
        }
        default: abort ();
    }
}

// determine gpr index
uint16_t CPU1134::gprx (uint16_t r, uint16_t mode)
{
    r &= 7;
    if (r == 6) {
        switch (mode & 3) {
            case 0: break;
            case 3: r = 14; break;
            default: throw CPU1134Trap (T_ILLPSWM);
        }
    }
    return r;
}

// compute overflow bit for addition
bool CPU1134::addvbit (uint16_t a, uint16_t b, bool byte)
{
    if (byte) {
        int32_t sum = (int32_t) (int8_t) a + (int32_t) (int8_t) b;
        return sum != (int32_t) (int8_t) sum;
    }
    int32_t sum = (int32_t) (int16_t) a + (int32_t) (int16_t) b;
    return sum != (int32_t) (int16_t) sum;
}

// compute carry bit for addition
bool CPU1134::addcbit (uint16_t a, uint16_t b, bool byte)
{
    if (byte) {
        uint32_t sum = (uint32_t) (uint8_t) a + (uint32_t) (uint8_t) b;
        return sum != (uint32_t) (uint8_t) sum;
    }
    uint32_t sum = (uint32_t) (uint16_t) a + (uint32_t) (uint16_t) b;
    return sum != (uint32_t) (uint16_t) sum;
}

// compute overflow bit for subtraction
bool CPU1134::subvbit (uint16_t a, uint16_t b, bool byte)
{
    if (byte) {
        int32_t dif = (int32_t) (int8_t) a - (int32_t) (int8_t) b;
        return dif != (int32_t) (int8_t) dif;
    }
    int32_t dif = (int32_t) (int16_t) a - (int32_t) (int16_t) b;
    return dif != (int32_t) (int16_t) dif;
}

// compute carry bit for subtraction
bool CPU1134::subcbit (uint16_t a, uint16_t b, bool byte)
{
    if (byte) {
        return (uint8_t) a < (uint8_t) b;
    }
    return a < b;
}

// compute overflow bit for shift left
bool CPU1134::aslvbit (uint16_t dstval, uint16_t result, bool byte)
{
    bool cbit = (dstval >> (byte ? 7 : 15)) & 1;
    bool nbit = (result >> (byte ? 7 : 15)) & 1;
    return nbit ^ cbit;
}

// compute overflow bit for shift right
bool CPU1134::asrvbit (uint16_t dstval, uint16_t result, bool byte)
{
    bool cbit = dstval & 1;
    bool nbit = (result >> (byte ? 7 : 15)) & 1;
    return nbit ^ cbit;
}

// update psw's n,z,v,c bits
void CPU1134::updnzvc (uint16_t result, bool byte, uint16_t vbit, uint16_t cbit)
{
    psw &= ~ 017;
    if (byte) {
        if (result & 0200) psw |= 010;
        if ((result & 0377) == 0) psw |= 004;
    } else {
        if (result & 0100000) psw |= 010;
        if (result == 0) psw |= 004;
    }
    if (vbit & 1) psw |= 002;
    if (cbit & 1) psw |= 001;
    if (dbg2) printf (" [%c%c%c%c]", psw & 010 ? 'N' : '-', psw & 004 ? 'Z' : '-', psw & 002 ? 'V' : '-', psw & 001 ? 'C' : '-');
}

// read word for disassembly - return error instead of throwing exception
bool CPU1134::disread (void *vhis, uint32_t addr, uint16_t *data_r)
{
    CPU1134 *zhis = (CPU1134 *) vhis;

    // disassembler passing a physical address
    if (addr & disassem_PA) {
        addr &= 0777777;
    } else {
        // virtual address, check for MMU enabled
        addr &= 0177777;
        if (! (zhis->mmr0 & 1)) {
            // MMU disabled, pad bits <17:16>
            if (addr >= 0160000) addr |= 0760000;
        } else {
            // MMU enabled, read PDR
            bool user = (zhis->psw & 0140000) != 0;
            uint16_t page = addr >> 13;
            uint16_t pdr  = (user ? zhis->usrpdrs : zhis->knlpdrs)[page];
            // check page enabled
            if (! (pdr & 2)) return false;
            // check page length
            uint16_t blok = (addr >> 6) & 127;
            uint16_t len  = (pdr >> 8) & 127;
            if (pdr & 8) {
                if (blok < len) return false;
            } else {
                if (blok > len) return false;
            }
            // relocate address using PAR
            uint16_t par  = (user ? zhis->usrpars : zhis->knlpars)[page];
            addr = (((uint32_t) par & 4095) << 6) + (addr & 8191);
        }
    }
    // don't read misc i/o registers because of side effects
    if (addr >= 0760000) {
        if ((addr >= 0777700) && (addr <= 0777717)) goto good;  // registers
        if ((addr >= 0772300) && (addr <= 0772377)) goto good;  // knl pdrs, pars
        if ((addr >= 0777570) && (addr <= 0777577)) goto good;  // swlight, mmr0, mmr2
        if ((addr >= 0777600) && (addr <= 0777677)) goto good;  // usr pdrs, pars
        if (addr == 0777776) goto good;                         // psw
        return false;                                           // other i/o registers
    }
good:;
    return rdmaster (addr, data_r);
}

// read word given virtual address
uint16_t CPU1134::rdwordvirt (uint16_t vaddr, uint16_t mode)
{
    if (vaddr & 1) throw CPU1134Trap (T_ODDADDR);
    uint32_t paddr = getphysaddr (vaddr, false, mode);
    uint16_t data = rdwordphys (paddr);
    if (dbg2) printf (" [%06o=>%06o]", vaddr, data);
    return data;
}

// read byte given virtual address
uint8_t CPU1134::rdbytevirt (uint16_t vaddr, uint16_t mode)
{
    uint32_t paddr = getphysaddr (vaddr, false, mode);
    uint16_t word = rdwordphys (paddr & ~1);
    uint8_t data = (paddr & 1) ? (word >> 8) : word;
    if (dbg2) printf (" [%06o=>%03o]", vaddr, data);
    return data;
}

// write word given virtual address
void CPU1134::wrwordvirt (uint16_t vaddr, uint16_t data, uint16_t mode)
{
    if (vaddr & 1) throw CPU1134Trap (T_ODDADDR);
    uint32_t paddr = getphysaddr (vaddr, true, mode);
    wrwordphys (paddr, data);
    if (dbg2) printf (" [%06o<=%06o]", vaddr, data);
}

// write byte given virtual address
void CPU1134::wrbytevirt (uint16_t vaddr, uint8_t data, uint16_t mode)
{
    uint32_t paddr = getphysaddr (vaddr, true, mode);
    wrbytephys (paddr, data);
    if (dbg2) printf (" [%06o<=%03o]", vaddr, data);
}

// translate virtual address to physical address
uint32_t CPU1134::getphysaddr (uint16_t vaddr, bool write, uint16_t mode)
{
    if (! (mmr0 & 1)) {
        if (vaddr < 0160000) return vaddr;
        return 0760000 | vaddr;
    }

    uint16_t *pars = NULL;
    uint16_t *pdrs = NULL;
    switch (mode & 3) {
        case 0: { pars = knlpars; pdrs = knlpdrs; break; }
        case 3: { pars = usrpars; pdrs = usrpdrs; break; }
        default: throw CPU1134Trap (T_ILLPSWM);
    }

    uint16_t page = vaddr >> 13;
    pars += page;
    pdrs += page;

    if (! (mmr0 & 0160000)) {
        mmr0 = (mmr0 & ~ 01560) | ((mode & 3) << 5) | (page << 1);
    }

    if (! (*pdrs & 2)) {
        mmr0 |= 0100000;        // abort non-resident
        throw CPU1134Trap (T_MMABORT);
    }

    uint16_t block = (vaddr >> 6) & 127;
    if (*pdrs & 8) {
        if (block < *pdrs >> 8) {
            mmr0 |= 0040000;    // abort page length
            throw CPU1134Trap (T_MMABORT);
        }
    } else {
        if (block > *pdrs >> 8) {
            mmr0 |= 0040000;    // abort page length
            throw CPU1134Trap (T_MMABORT);
        }
    }

    if (write) {
        if (! (*pdrs & 4)) {
            mmr0 |= 0020000;    // abort read only
            throw CPU1134Trap (T_MMABORT);
        }
        *pdrs |= 0100;
    }

    return (((uint32_t) *pars) << 6) + (vaddr & 017777);
}

// read word given physical address
uint16_t CPU1134::rdwordphys (uint32_t physaddr)
{
    uint16_t data;
    if (! rdmaster (physaddr, &data)) throw CPU1134Trap (T_TIMEOUT);
    return data;
}

// write word given physical address
void CPU1134::wrwordphys (uint32_t physaddr, uint16_t data)
{
    if (! wrmaster (physaddr, data, false)) throw CPU1134Trap (T_TIMEOUT);
}

// write byte given physical address
void CPU1134::wrbytephys (uint32_t physaddr, uint8_t data)
{
    if (! wrmaster (physaddr, data, true)) throw CPU1134Trap (T_TIMEOUT);
}

CPU1134Trap::CPU1134Trap (uint8_t v)
{
    vector = v;
}
