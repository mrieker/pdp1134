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

// Continuously dump the FPGA registers to the screen
// It does not write to the zynq memory pages so does not alter zynq state

//  ./z11dump.armv7l [-once] [-xmem <lo>..<hi>]...

#include <alloca.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "z11defs.h"
#include "z11util.h"

#define ESC_NORMV "\033[m"          // go back to normal video
#define ESC_REVER "\033[7m"         // turn reverse video on
#define ESC_UNDER "\033[4m"         // turn underlining on
#define ESC_BLINK "\033[5m"         // turn blink on
#define ESC_BOLDV "\033[1m"         // turn bold on
#define ESC_REDBG "\033[41m"        // red background
#define ESC_YELBG "\033[44m"        // yellow background
#define ESC_EREOL "\033[K"          // erase to end of line
#define ESC_EREOP "\033[J"          // erase to end of page
#define ESC_HOMEC "\033[H"          // home cursor

#define EOL ESC_EREOL "\n"
#define EOP ESC_EREOP

#define FIELD(index,mask) ((z11s[index] & mask) / (mask & - mask))
#define BUS12(index,topbit) ((z11s[index] / (topbit / 2048)) & 4095)

struct XMemRange {
    XMemRange *next;
    uint16_t loaddr;
    uint16_t hiaddr;
};

static char const *const fmstrs[] = { FMSTRS };

static bool volatile exitflag;
static char stdoutbuf[8000];

static void siginthand (int signum)
{
    exitflag = true;
}

int main (int argc, char **argv)
{
    setvbuf (stdout, stdoutbuf, _IOFBF, sizeof stdoutbuf);

    bool oncemode = false;
    bool pagemode = false;
    char const *eol = EOL;
    int fps = 20;
    XMemRange **lxmemrange, *xmemrange, *xmemranges;
    lxmemrange = &xmemranges;
    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("     Dump ZTurn FPGA state");
            puts ("     Does not alter the state");
            puts ("");
            puts ("  ./z11dump.armv7l [-fps <num> | -once | -page] [-xmem <lo>..<hi>]...");
            puts ("      -fps : this many frames per second 1..1000 (default 20)");
            puts ("     -once : just print the state once, else update continually");
            puts ("     -page : just dump the raw page then exit");
            puts ("     -xmem : dump the given extended memory range instead of register state");
            puts ("             may be given more than once");
            puts ("             Does not dump PDP-8/L core memory.  If need to dump low 4K memory,");
            puts ("             restart everything with z11real giving it the -enlo4k option.");
            puts ("");
            return 0;
        }
        if (strcasecmp (argv[i], "-fps") == 0) {
            if ((++ i >= argc) || (argv[i][0] == '-')) {
                fprintf (stderr, "missing <num> for -fps option\n");
                return 1;
            }
            char *p;
            fps = strtol (argv[i], &p, 0);
            if ((*p != 0) || (fps <= 0) || (fps > 1000)) {
                fprintf (stderr, "number frames per second %s must be integer 1..1000\n", argv[i]);
                return 1;
            }
            continue;
        }
        if (strcasecmp (argv[i], "-once") == 0) {
            eol = "\n";
            oncemode = true;
            continue;
        }
        if (strcasecmp (argv[i], "-page") == 0) {
            pagemode = true;
            continue;
        }
        if (strcasecmp (argv[i], "-xmem") == 0) {
            if ((++ i >= argc) || (argv[i][0] == '-')) {
                fprintf (stderr, "missing lo..hi address range after -xmem\n");
                return 1;
            }
            char *p;
            unsigned long hiaddr, loaddr;
            hiaddr = loaddr = strtoul (argv[i], &p, 8);
            if ((p[0] == '.') && (p[1] == '.')) {
                hiaddr = strtoul (p + 2, &p, 8);
            }
            if ((*p != 0) || (loaddr > hiaddr) || (hiaddr > 077777)) {
                fprintf (stderr, "bad address range %s must be <loaddr>..<hiaddr>\n", argv[i]);
                return 1;
            }
            xmemrange = (XMemRange *) alloca (sizeof *xmemrange);
            xmemrange->loaddr = loaddr;
            xmemrange->hiaddr = hiaddr;
            *lxmemrange = xmemrange;
            lxmemrange = &xmemrange->next;
            continue;
        }
        fprintf (stderr, "unknown argument %s\n", argv[i]);
        return 1;
    }
    *lxmemrange = NULL;

    Z11Page z8p;
    uint32_t volatile *pdpat = z8p.findev ("11", NULL, NULL, false);

    // maybe just dump the page and exit
    if (pagemode) {
        uint32_t words[1024];
        for (int i = 0; i < 1024; i ++) words[i] = pdpat[i];
        int k;
        for (k = 1024; words[--k] == 0xDEADBEEF;) { }
        for (int i = 0; i <= k; i += 8) {
            printf ("%03X:", i);
            for (int j = 0; j < 8; j ++) {
                printf (" %08X", words[i+j]);
            }
            printf ("\n");
        }
        return 0;
    }

#if 000
    uint32_t volatile *extmem = (xmemranges == NULL) ? NULL : z8p.extmem ();
#endif

    signal (SIGINT, siginthand);

    while (! exitflag) {
        usleep (1000000 / fps);

        uint32_t z11s[1024];
        if (xmemranges == NULL) {
            for (int i = 0; i < 1024; i ++) {
                z11s[i] = pdpat[i];
            }
        }

        if (! oncemode) printf (ESC_HOMEC);

        if (xmemranges == NULL) {
            uint32_t fpgamode = FIELD (Z_RA, a_fpgamode);
            uint32_t rsel1 = FIELD (Z_RC, c_rsel1_h);
            uint32_t rsel2 = FIELD (Z_RC, c_rsel2_h);
            uint32_t rsel3 = FIELD (Z_RC, c_rsel3_h);

            // zynq.v register dump
            printf ("VERSION=%08X 11  fpgamode=%o  FM_%s  debug=%08X%s", z11s[0], fpgamode, fmstrs[fpgamode], z11s[8], eol);
            printf ("      man_a_out_h=%06o  dmx_a_in_h=%06o     dev_a_h=%06o%s",           FIELD(Z_RB,b_a_out_h),      FIELD(Z_RF,f_a_in_h),      FIELD(Z_RD,d_a_out_h),      eol);
            printf ("   man_bbsy_out_h=%o        bbsy_in_h=%o       dev_bbsy_h=%o%s",       FIELD(Z_RA,a_bbsy_out_h),   FIELD(Z_RC,c_bbsy_in_h),   FIELD(Z_RD,d_bbsy_out_h),   eol);
            printf ("     man_bg_out_l=%02o         bg_in_l=%02o        dev_bg_l=%02o%s",   FIELD(Z_RB,b_bg_out_l),     FIELD(Z_RC,c_bg_in_l),     FIELD(Z_RE,e_bg_out_l),     eol);
            printf ("     man_br_out_h=%02o     dmx_br_in_h=%02o        dev_br_h=%02o%s",   FIELD(Z_RB,b_br_out_h),     FIELD(Z_RE,e_br_in_h),     FIELD(Z_RE,e_br_out_h),     eol);
            printf ("      man_c_out_h=%o       dmx_c_in_h=%o          dev_c_h=%o%s",       FIELD(Z_RB,b_c_out_h),      FIELD(Z_RE,e_c_in_h),      FIELD(Z_RE,e_c_out_h),      eol);
            printf ("      man_d_out_h=%06o  dmx_d_in_h=%06o     dev_d_h=%06o%s",           FIELD(Z_RA,a_d_out_h),      FIELD(Z_RG,g_d_in_h),      FIELD(Z_RG,g_d_out_h),      eol);
            printf ("                          hltgr_in_l=%o      dev_hltgr_l=%o%s",                                    FIELD(Z_RC,c_hltgr_in_l),  FIELD(Z_RD,d_hltgr_out_l),  eol);
            printf ("  man_hltrq_out_h=%o   dmx_hltrq_in_h=%o      dev_hltrq_h=%o%s",       FIELD(Z_RA,a_hltrq_out_h),  FIELD(Z_RE,e_hltrq_in_h),  FIELD(Z_RD,d_hltrq_out_h),  eol);
            printf ("   man_init_out_h=%o        init_in_h=%o       dev_init_h=%o%s",       FIELD(Z_RA,a_init_out_h),   FIELD(Z_RC,c_init_in_h),   FIELD(Z_RD,d_init_out_h),   eol);
            printf ("   man_intr_out_h=%o        intr_in_h=%o       dev_intr_h=%o%s",       FIELD(Z_RA,a_intr_out_h),   FIELD(Z_RC,c_intr_in_h),   FIELD(Z_RD,d_intr_out_h),   eol);
            printf ("   man_msyn_out_h=%o        msyn_in_h=%o       dev_msyn_h=%o%s",       FIELD(Z_RA,a_msyn_out_h),   FIELD(Z_RC,c_msyn_in_h),   FIELD(Z_RD,d_msyn_out_h),   eol);
            printf ("    man_npg_out_l=%o         npg_in_l=%o        dev_npg_l=%o%s",       FIELD(Z_RA,a_npg_out_l),    FIELD(Z_RC,c_npg_in_l),    FIELD(Z_RD,d_npg_out_l),    eol);
            printf ("    man_npr_out_h=%o     dmx_npr_in_h=%o        dev_npr_h=%o%s",       FIELD(Z_RA,a_npr_out_h),    FIELD(Z_RE,e_npr_in_h),    FIELD(Z_RD,d_npr_out_h),    eol);
            printf ("   man_sack_out_h=%o        sack_in_h=%o       dev_sack_h=%o%s",       FIELD(Z_RA,a_sack_out_h),   FIELD(Z_RC,c_sack_in_h),   FIELD(Z_RD,d_sack_out_h),   eol);
            printf ("   man_ssyn_out_h=%o        ssyn_in_h=%o       dev_ssyn_h=%o%s",       FIELD(Z_RA,a_ssyn_out_h),   FIELD(Z_RC,c_ssyn_in_h),   FIELD(Z_RD,d_ssyn_out_h),   eol);
            printf ("       man_rsel_h=%o      rsel1,2,3_h=%o,%o,%o          mux=%05o%s",   FIELD(Z_RB,b_rsel_h),       rsel1, rsel2, rsel3,       (z11s[Z_RC]>>17)&32767,     eol);

            for (int i = 0; i < 1024;) {
                uint32_t idver = z11s[i];
                if ((idver & 0xF000U) == 0x0000U) {
                    printf ("%sVERSION=%08X %c%c %08X%s", eol, idver, idver >> 24, idver >> 16, z11s[i+1], eol);
                }
                if ((idver & 0xF000U) == 0x1000U) {
                    printf ("%sVERSION=%08X %c%c %08X %08X %08X%s", eol, idver, idver >> 24, idver >> 16, z11s[i+1], z11s[i+2], z11s[i+3], eol);
                }
                if ((idver & 0xF000U) == 0x2000U) {
                    printf ("%sVERSION=%08X %c%c %08X %08X %08X %08X %08X %08X %08X%s", eol, idver, idver >> 24, idver >> 16,
                        z11s[i+1], z11s[i+2], z11s[i+3], z11s[i+4], z11s[i+5], z11s[i+6], z11s[i+7], eol);
                }
                i += 2 << ((idver >> 12) & 15);
            }
        } else {
#if 000
            // memory dump
            for (xmemrange = xmemranges; xmemrange != NULL; xmemrange = xmemrange->next) {
                uint16_t loaddr = xmemrange->loaddr;
                uint16_t hiaddr = xmemrange->hiaddr;
                fputs (eol, stdout);
                for (uint16_t lnaddr = loaddr & -16; lnaddr <= hiaddr; lnaddr += 16) {
                    printf (" %05o :", lnaddr);
                    for (uint16_t i = 0; i < 16; i ++) {
                        uint16_t addr = lnaddr + i;
                        if ((addr < loaddr) || (addr > hiaddr)) {
                            printf ("     ");
                        } else {
                            printf (" %04o", extmem[addr]);
                        }
                    }
                    fputs (eol, stdout);
                }
            }
#endif
        }

        if (oncemode) break;

        printf (EOP);
        fflush (stdout);
    }
    printf ("\n");
    return 0;
}
