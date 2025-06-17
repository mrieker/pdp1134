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

// Performs paper tape reader and punch I/O for the PDP-11 Zynq I/O board

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
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
#define PC1_RERR  0x00008000U
#define PC1_RBUF0 0x00010000U

#define PC2_EMPTY 0x00000080U
#define PC2_ERROR 0x00008000U
#define PC2_BUFFR 0x00FF0000U

#define PC3_ENAB  0x80000000U

static bool volatile ctrlcflag;

static void sighand (int signum);

int main (int argc, char **argv)
{
    bool inscr = false;
    bool killit = false;
    bool quiet = false;
    bool remcr = false;
    char const *filename = NULL;
    int mode = -1;
    int32_t cps = 0;
    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("     Access paper tape reader and punch");
            puts ("");
            puts ("  ./z11pc reader [-cps <charspersec>] [-inscr] [-killit] [-quiet] [-remnull] <filename>");
            puts ("     -cps    : set chars per second, default 300");
            puts ("     -inscr  : insert <CR> before <LF>");
            puts ("     -killit : kill other process that is processing paper tape reader");
            puts ("     -quiet  : don't print progress message");
            puts ("   filename can be - for stdin");
            puts ("");
            puts ("  ./z11pc punch [-7bit] [-cps <charspersec>] [-remcr] [-killit] [-quiet] <filename>");
            puts ("     -cps    : set chars per second, default 50");
            puts ("     -remcr  : remove <CR> before <LF>");
            puts ("     -killit : kill other process that is processing paper tape punch");
            puts ("     -quiet  : don't print progress message");
            puts ("   filename can be - for stdout");
            puts ("   Terminate with SIGINT (control-C) or SIGTERM (plain kill)");
            puts ("");
            return 0;
        }
        if (strcasecmp (argv[i], "-cps") == 0) {
            if ((++ i >= argc) || (argv[i][0] == '-')) {
                fprintf (stderr, "missing value for -cps\n");
                return 1;
            }
            char *p;
            cps = strtol (argv[i], &p, 0);
            if ((*p != 0) || (cps <= 0) || (cps > 1000000)) {
                fprintf (stderr, "-cps value %s must be integer in range 1..1000000\n", argv[i]);
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
        if ((argv[i][0] == '-') && (argv[i][1] != 0)) {
            fprintf (stderr, "unknown option %s\n", argv[i]);
            return 1;
        }
        if (mode < 0) {
            if (strcasecmp (argv[i], "reader") == 0) mode = 0;
            if (strcasecmp (argv[i], "punch") == 0) mode = 1;
            if (mode < 0) {
                fprintf (stderr, "expecting reader or punch, not %s\n", argv[i]);
                return 1;
            }
            continue;
        }
        if (filename == NULL) {
            filename = argv[i];
            continue;
        }
        fprintf (stderr, "unknown argument %s\n", argv[i]);
        return 1;
    }

    if (filename == NULL) {
        fprintf (stderr, "missing filename\n");
        return 1;
    }

    if (mode) {

        // punch mode

        FILE *file = (strcmp (filename, "-") == 0) ? stdout : fopen (filename, "w");
        if (file == NULL) {
            fprintf (stderr, "error creating %s: %m\n", filename);
            return 1;
        }

        struct stat statbuf;
        if (fstat (fileno (file), &statbuf) < 0) ABORT ();
        if (! S_ISREG (statbuf.st_mode)) setlinebuf (file);

        Z11Page z11p;
        uint32_t volatile *pcat = z11p.findev ("PC", NULL, NULL, false, false);
        z11p.locksubdev (&pcat[2], 1, killit);
        pcat[3] = PC3_ENAB;

        if (signal (SIGINT,  sighand) == SIG_ERR) ABORT ();
        if (signal (SIGTERM, sighand) == SIG_ERR) ABORT ();

        if (pcat[2] & PC2_ERROR) {  // see if previously reporting 'out of tape'
            pcat[2] = 0;            // two separate steps to trigger interrupt
            pcat[2] = PC2_EMPTY;    // ok, now we are ready to accept bytes
        }

        bool lastcr = false;
        int32_t  idlect = 0;
        uint32_t fbytes = 0;
        uint32_t nbytes = 0;
        if (cps == 0) cps = 50;
        while (true) {
            if (! quiet) {
                fprintf (stderr, "\r%u byte%s so far ", nbytes, ((nbytes == 1) ? "" : "s"));
                fflush (stderr);
            }

            uint32_t pc2;
            do {
                usleep (1000000 / cps);

                // flush buffer to file if idle for a second or have been terminated
                if ((++ idlect >= cps) || ctrlcflag) {

                    // write buffer to file
                    if ((fbytes < nbytes) && (fflush (file) < 0)) {
                        pcat[2] = PC2_ERROR;
                        fprintf (stderr, "error writing %s: %m\n", filename);
                        return 1;
                    }
                    fbytes = nbytes;

                    // if terminated, close and exit
                    if (ctrlcflag) {
                        pcat[2] = PC2_ERROR;
                        if (! quiet) fputc ('\n', stderr);
                        if ((file != stdout) && (fclose (file) < 0)) {
                            fprintf (stderr, "error closing %s: %m\n", filename);
                            return 1;
                        }
                        return 0;
                    }

                    // don't need to flush again until another byte is punched
                    idlect = -999999999;
                }

                // keep waiting if nothing to punch
            } while ((pc2 = pcat[2]) & PC2_EMPTY);
            idlect = 0;

            // tell pdp it is ok to punch another byte now
            pcat[2] = PC2_EMPTY;

            // get byte being punched
            uint8_t wrbyte = (pc2 & PC2_BUFFR) / (PC2_BUFFR & - PC2_BUFFR);

            // if we skipped a CR last time and this is not an LF, punch the CR
            if (lastcr && ((wrbyte & 0177) != '\n')) {
                if (fputc ('\r', file) < 0) {
                    pcat[2] = PC2_ERROR;
                    fprintf (stderr, "error writing %s: %m\n", filename);
                    return 1;
                }
                ++ nbytes;
            }

            // if -remcr and this is a CR, hold off on punching it
            lastcr = (remcr && ((wrbyte & 0177) == '\r'));
            if (! lastcr) {
                if (fputc (wrbyte, file) < 0) {
                    pcat[2] = PC2_ERROR;
                    fprintf (stderr, "error writing %s: %m\n", filename);
                    return 1;
                }
                ++ nbytes;
            }
        }
    } else {

        // reader mode

        FILE *file = (strcmp (filename, "-") == 0) ? stdin : fopen (filename, "r");
        if (file == NULL) {
            fprintf (stderr, "error opening %s: %m\n", filename);
            return 1;
        }

        struct stat statbuf;
        if (fstat (fileno (file), &statbuf) < 0) ABORT ();
        uint32_t fsize = statbuf.st_size;

        Z11Page z11p;
        uint32_t volatile *pcat = z11p.findev ("PC", NULL, NULL, false, false);
        z11p.locksubdev (&pcat[1], 1, killit);
        pcat[3] = PC3_ENAB;

        if (pcat[1] & PC1_RERR) {   // see if was reporting no tape in reader
            pcat[1] = 0;            // ok say we have a tape now
        }

        bool lastcr = false;
        uint32_t nbytes = 0;
        if (cps == 0) cps = 300;
        while (true) {
            if (! quiet) {
                if (fsize == 0) fprintf (stderr, "\r%u byte%s so far ", nbytes, ((nbytes == 1) ? "" : "s"));
                 else fprintf (stderr, "\r%u/%u byte%s so far ", nbytes, fsize, ((nbytes == 1) ? "" : "s"));
                fflush (stderr);
            }

            do usleep (1000000 / cps);
            while (! (pcat[1] & PC1_STEP));

            int rc = fgetc (file);
            if (rc < 0) {
                pcat[1] = PC1_RERR;
                if (ferror (file)) {
                    fprintf (stderr, "error reading %s: %m\n", filename);
                    return 1;
                }
                if (! quiet) fprintf (stderr, "\nend of file reached\n");
                return 0;
            }
            ++ nbytes;

            if (((rc & 0177) == '\n') && inscr && ! lastcr) {
                pcat[1] = PC1_RRDY | (rc - '\n' + '\r') * PC1_RBUF0;
                do usleep (1000000 / cps);
                while (! (pcat[1] & PC1_STEP));
            }

            lastcr = (rc & 0177) == '\r';

            pcat[1] = PC1_RRDY | rc * PC1_RBUF0;
        }
    }
}

static void sighand (int signum)
{
    ctrlcflag = true;
}
