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

// add disassembly to z11ila output

// assumes 'standard' z11ila output:

/*
[   65]  03  003262 17 00 0 012767  0 1 0 1
[   66]  12  003264 17 00 0 006402  0 1 0 1
[   67]  14  003266 17 00 0 003176  0 1 0 1
[   68]  39  006466 17 00 2 006402  0 1 0 1
[   69]  03  003270 17 00 0 012701  0 1 0 1
[   70]  12  003272 17 00 0 006576  0 1 0 1
[   71]  03  003274 17 00 0 105067  0 1 0 1
[   72]  14  003276 17 00 0 003105  0 1 0 1
[   73]  39  006405 17 00 3 000000  0 1 0 1
[   74]  03  003300 17 00 0 105241  0 1 0 1
[   75]  15  006575 17 00 1 000000  0 1 0 1
[   76]  39  006575 17 00 3 000400  0 1 0 1
[   77]  03  003302 17 00 0 005767  0 1 0 1
         ^state           ^func
 ^index      ^address       ^data

    state = 03 : sim is doing a fetch
            45 : sim is doing a trap

     func =  0 : DATI
             1 : DATIP
             2 : DATO
             3 : DATOB
*/

#include <stdio.h>
#include <stdlib.h>

#include "disassem.h"

#define S_FETCH2 03     // from sim1134.v
#define S_TRAP2  44

struct Line {
    uint32_t addr;
    uint16_t data;
    uint16_t indx;
    uint8_t state;
    uint8_t func;
};

static int nlines;
static Line lines[16];

static bool readline (Line *line);
static bool readword (void *param, uint32_t addr, uint16_t *data_r);

int main ()
{
    while (readline (&lines[0])) {
        if (lines[0].state == 3) goto fetch;
    }
    printf ("no fetch\n");
    return 0;

fetch:;
    while (true) {

        // lines[0] contains fetch (state 3) or trap (state 45)
        // read subsequent lines until next fetch or trap
        for (nlines = 1; nlines < 16; nlines ++) {
            lines[nlines].state = 0;
            if (! readline (&lines[nlines])) break; // eof (state = 0)
            if (lines[nlines].state == S_FETCH2) break;   // fetch
            if (lines[nlines].state == S_TRAP2)  break;   // trap
        }

        // lines[0] = this fetch/trap to process
        // lines[1..nlines-1] = operands
        // lines[nlines] = next eof/fetch/trap

        char const *mne = "";
        std::string strbuf;

        if (lines[0].state == S_FETCH2) {

            // get opcode physical address and opcode itself
            uint32_t phaddr = lines[0].addr;
            uint16_t opcode = lines[0].data;

            // get possible two operands from two words following opcode
            uint16_t operand1 = 0;
            uint16_t operand2 = 0;
            for (int j = 1; j < nlines; j ++) {
                if (lines[j].addr == phaddr + 2) operand1 = lines[j].data;
                if (lines[j].addr == phaddr + 4) operand2 = lines[j].data;
            }

            // disassemble the opcode
            int rc = disassem (&strbuf, opcode, operand1, operand2, readword, NULL);
            mne = (rc == 0) ? "*ilop*" : strbuf.c_str () + 7;
        }

        if (lines[0].state == S_TRAP2) mne = "*trap*";

        // print out result
        for (int j = 0; j < nlines; j ++) {
            printf ("[%5u]  %02u  %06o %o %06o  %s\n", lines[j].indx, lines[j].state, lines[j].addr, lines[j].func, lines[j].data, mne);
            mne = "";
        }

        // start processing next opcode
        if (nlines >= 16) break;
        if (lines[nlines].state == 0) break;
        lines[0] = lines[nlines];
    }

    return 0;
}

// read line from z11ila and parse
static bool readline (Line *line)
{
    char buff[80];
    if (fgets (buff, sizeof buff, stdin) == NULL) return false;
    line->indx  = strtoul (buff + 1, NULL, 10);
    line->state = strtoul (buff + 9, NULL, 10);
    line->addr  = strtoul (buff + 13, NULL, 8);
    line->func  = strtoul (buff + 26, NULL, 8);
    line->data  = strtoul (buff + 28, NULL, 8);
    return true;
}

// read operand word from lines[] array
static bool readword (void *param, uint32_t addr, uint16_t *data_r)
{
    for (int j = 0; j < nlines; j ++) {
        if ((lines[j].addr & 0177776) == (addr & 0177776)) {
            *data_r = lines[j].data;
            return true;
        }
    }
    return false;
}
