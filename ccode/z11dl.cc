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

// Performs TTY I/O (DL-11) for the PDP-11 Zynq I/O board

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

#include "z11defs.h"
#include "z11util.h"

#define DL1_RRDY  0x00000080U
#define DL1_RBUF  0x00FF0000U
#define DL1_RBUF0 0x00010000U

#define DL2_XRDY  0x00000080U
#define DL2_XBUF  0x00FF0000U
#define DL2_XBUF0 0x00010000U

#define DL3_ENAB  0x80000000U
#define DL3_PORT  0x0003FFFFU
#define DL3_PORT0 0x00000001U

static bool nokb;
static bool upcase;
static bool volatile ctrlcflag;
static struct termios term_original;
static uint32_t cps = 10;
static uint32_t volatile *dlat;

static bool finddl (void *param, uint32_t volatile *dlat);
static void sigrunhand (int signum);

int main (int argc, char **argv)
{
    bool killit = false;
    char const *logname = NULL;
    int port = 0777560;
    char *p;
    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("     Access DL-11 TTY controller");
            puts ("");
            puts ("  ./z11dl [-cps <charspersec>] [-killit] [-log <filename>] [-nokb] [-upcase]");
            puts ("     -cps    : set chars per second, default 10");
            puts ("     -killit : kill other process that is processing this port");
            puts ("     -log    : log output to given file");
            puts ("     -nokb   : do not pass stdin keyboard to pdp");
            puts ("     -upcase : convert all keyboard to upper case");
            puts ("");
            return 0;
        }
        if (strcasecmp (argv[i], "-cps") == 0) {
            if ((++ i >= argc) || (argv[i][0] == '-')) {
                fprintf (stderr, "missing value for -cps\n");
                return 1;
            }
            cps = strtoul (argv[i], &p, 0);
            if ((*p != 0) || (cps == 0) || (cps > 1000)) {
                fprintf (stderr, "-cps value %s must be integer in range 1..1000\n", argv[i]);
                return 1;
            }
            continue;
        }
        if (strcasecmp (argv[i], "-killit") == 0) {
            killit = true;
            continue;
        }
        if (strcasecmp (argv[i], "-log") == 0) {
            if ((++ i >= argc) || (argv[i][0] == '-')) {
                fprintf (stderr, "-log missing filename\n");
                return 1;
            }
            logname = argv[i];
            continue;
        }
        if (strcasecmp (argv[i], "-nokb") == 0) {
            nokb = true;
            continue;
        }
        if (strcasecmp (argv[i], "-upcase") == 0) {
            upcase = true;
            continue;
        }
        if ((argv[i][0] == '-') || (port >= 0)) {
            fprintf (stderr, "unknown option %s\n", argv[i]);
            return 1;
        }
        port = strtoul (argv[i], &p, 8);
        if (*p != 0) {
            fprintf (stderr, "port number %s must be octal integer\n", argv[i]);
            return 1;
        }
    }

    FILE *logfile = NULL;
    if (logname != NULL) {
        logfile = fopen (logname, "w");
        if (logfile == NULL) {
            fprintf (stderr, "error creating %s: %m\n", logname);
            return 1;
        }
        setlinebuf (logfile);
    }

    Z11Page z11p;
    dlat = z11p.findev ("DL", finddl, &port, true, killit);
    ZWR(dlat[3], DL3_ENAB);     // enable board to process io instructions

    bool stdintty, stdoutty;
    uint64_t nowus;
    uint64_t readnextkbat;
    uint64_t readnextprat;

    struct termios term_modified;
    stdintty = isatty (STDIN_FILENO) > 0;
    if (stdintty && ! nokb) {
        // stdin is a tty, set it to raw mode
        fprintf (stderr, "z11dl: use control-\\ for stop char\n");
        if (tcgetattr (STDIN_FILENO, &term_original) < 0) ABORT ();
        if (signal (SIGHUP,  sigrunhand) != SIG_DFL) ABORT ();
        if (signal (SIGTERM, sigrunhand) != SIG_DFL) ABORT ();
        term_modified = term_original;
        cfmakeraw (&term_modified);
        if (tcsetattr (STDIN_FILENO, TCSADRAIN, &term_modified) < 0) ABORT ();
    } else {
        // stdin not a tty (or we are supposed to ignore it)
        // in case user presses control-\ to terminate
        if (signal (SIGINT,  sigrunhand) != SIG_DFL) ABORT ();
        if (signal (SIGQUIT, sigrunhand) != SIG_DFL) ABORT ();
    }

    struct timeval nowtv;
    if (gettimeofday (&nowtv, NULL) < 0) ABORT ();
    nowus = nowtv.tv_sec * 1000000ULL + nowtv.tv_usec;

    stdoutty = isatty (STDOUT_FILENO) > 0;
    readnextprat = nowus + 1111111 / cps;
    readnextkbat = nokb ? 0xFFFFFFFFFFFFFFFFULL : readnextprat;

    // keep processing until control-backslash
    // control-C is recognized only if -nokb mode
    while (! ctrlcflag) {
        if (gettimeofday (&nowtv, NULL) < 0) ABORT ();
        nowus = nowtv.tv_sec * 1000000ULL + nowtv.tv_usec;
        usleep (1000 - nowus % 1000);
        if (gettimeofday (&nowtv, NULL) < 0) ABORT ();
        nowus = nowtv.tv_sec * 1000000ULL + nowtv.tv_usec;

        // maybe see if PDP has a character to print
        if (nowus >= readnextprat) {
            uint32_t xreg = ZRD(dlat[2]);
            if (! (xreg & DL2_XRDY)) {

                // tell PDP it can print another char
                ZWR(dlat[2], xreg | DL2_XRDY);

                // print character to stdout
                uint8_t prchar = (xreg & DL2_XBUF) / DL2_XBUF0;
                if ((prchar == 7) && stdoutty) {
                    int rc = write (STDOUT_FILENO, "<BEL>", 5);
                    if (rc < 5) ABORT ();
                } else {
                    int rc = write (STDOUT_FILENO, &prchar, 1);
                    if (rc <= 0) ABORT ();
                }
                if (logfile != NULL) fputc (prchar, logfile);

                // check for another char to print after 1000000/cps usec
                readnextprat = nowus + 1000000 / cps;
            }
        }

        // maybe there is a character from keyboard to pass to PDP
        if (nowus >= readnextkbat) {
            struct pollfd polls[1] = { STDIN_FILENO, POLLIN, 0 };
            int rc = nokb ? 0 : poll (polls, 1, 0);
            if ((rc < 0) && (errno != EINTR)) ABORT ();
            if ((rc > 0) && (polls[0].revents & POLLIN)) {

                // stdin char ready, read and pass along to pdp
                // but exit if it is a ctrl-backslash
                uint8_t kbchar;
                rc = read (STDIN_FILENO, &kbchar, 1);
                if ((rc == 0) && ! stdintty) break;
                if (rc <= 0) ABORT ();
                if ((kbchar == '\\' - '@') && stdintty) break;
                if (upcase && (kbchar >= 'a') && (kbchar <= 'z')) kbchar -= 'a' - 'A';
                ZWR(dlat[1], (ZRD(dlat[1]) & ~ DL1_RBUF) | DL1_RRDY | DL1_RBUF0 * kbchar);
                readnextkbat = nowus + 1000000 / cps;
            }
        }
    }

    if (stdintty && ! nokb) {
        tcsetattr (STDIN_FILENO, TCSADRAIN, &term_original);
    }
    fprintf (stderr, "\n");

    if (logfile != NULL) fclose (logfile);

    return 0;
}

static bool finddl (void *param, uint32_t volatile *dlat)
{
    int port = *(int *) param;
    if (dlat == NULL) {
        fprintf (stderr, "finddl: cannot find DL port %02o\n", port);
        ABORT ();
    }
    return (ZRD(dlat[3]) & DL3_PORT) / DL3_PORT0 == (uint32_t) port;
}

static void sigrunhand (int signum)
{
    if (signum == SIGQUIT) {
        if (! ctrlcflag) {
            ctrlcflag = true;
            return;
        }
    } else {
        tcsetattr (STDIN_FILENO, TCSADRAIN, &term_original);
    }
    dprintf (STDERR_FILENO, "\nz11dl: terminated by signal %d\n", signum);
    exit (1);
}
