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

// Performs paper tape reader I/O for the PDP-11 Zynq I/O board

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "z11defs.h"
#include "z11util.h"

#define PC1_STEP  0x00000001U
#define PC1_RRDY  0x00000080U
#define PC1_RBUF0 0x00010000U
#define PC3_ENAB  0x80000000U

int main (int argc, char **argv)
{
    bool inscr = false;
    bool killit = false;
    char const *filename = NULL;
    uint32_t cps = 300;
    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("     Access paper tape reader");
            puts ("");
            puts ("  ./z11ptr [-cps <charspersec>] [-inscr] [-killit] <filename>");
            puts ("     -cps    : set chars per second, default 300");
            puts ("     -inscr  : insert <CR> before <LF>");
            puts ("     -killit : kill other process that is processing paper tape reader");
            puts ("");
            return 0;
        }
        if (strcasecmp (argv[i], "-cps") == 0) {
            if ((++ i >= argc) || (argv[i][0] == '-')) {
                fprintf (stderr, "missing value for -cps\n");
                return 1;
            }
            char *p;
            cps = strtoul (argv[i], &p, 0);
            if ((*p != 0) || (cps == 0) || (cps > 1000)) {
                fprintf (stderr, "-cps value %s must be integer in range 1..1000\n", argv[i]);
                return 1;
            }
            continue;
        }
        if (strcasecmp (argv[i], "-inscr") == 0) {
            inscr = true;
            continue;
        }
        if (strcasecmp (argv[i], "-killit") == 0) {
            killit = true;
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf (stderr, "unknown option %s\n", argv[i]);
            return 1;
        }
        if (filename != NULL) {
            fprintf (stderr, "unknown argument %s\n", argv[i]);
            return 1;
        }
        filename = argv[i];
    }

    if (filename == NULL) {
        fprintf (stderr, "missing filename\n");
        return 1;
    }
    int filedes = open (filename, O_RDONLY);
    if (filedes < 0) {
        fprintf (stderr, "error opening %s: %m\n", filename);
        return 1;
    }

    struct stat statbuf;
    if (fstat (filedes, &statbuf) < 0) ABORT ();
    uint32_t fsize = statbuf.st_size;

    Z11Page z11p;
    uint32_t volatile *pcat = z11p.findev ("PC", NULL, NULL, false, false);
    z11p.locksubdev (&pcat[1], 1, killit);
    pcat[3] = PC3_ENAB;

    bool lastcr = false;
    uint32_t nbytes = 0;
    while (true) {
        printf ("\r%u/%u byte%s so far ", nbytes, fsize, ((nbytes == 1) ? "" : "s"));
        fflush (stdout);

        do usleep (1000000 / cps);
        while (! (pcat[1] & PC1_STEP));

        uint8_t rdbyte;
        int rc = read (filedes, &rdbyte, 1);
        if (rc < 0) {
            fprintf (stderr, "error reading %s: %m\n", filename);
            return 1;
        }
        if (rc == 0) {
            printf ("\nend of file reached\n");
            return 0;
        }

        if (((rdbyte & 0177) == '\n') && inscr && ! lastcr) {
            pcat[1] = PC1_RRDY | (rdbyte - '\n' + '\r') * PC1_RBUF0;
            do usleep (1000000 / cps);
            while (! (pcat[1] & PC1_STEP));
        }

        lastcr = (rdbyte & 0177) == '\r';

        pcat[1] = PC1_RRDY | rdbyte * PC1_RBUF0;
        ++ nbytes;
    }
}
