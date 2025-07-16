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

// Performs TTY I/O (DZ-11) for the PDP-11 Zynq I/O board

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

#define ADDRES 0760100  // dz11 technical manual oct78 p 3-1
#define INTVEC 0300

#define DZ1_ENABLE 0x80000000U
#define DZ1_INTVEC 0x03FC0000U
#define DZ1_ADDRES 0x0003FFFFU

#define DZ2_CSR    0x0000FFFFU

#define DZ4_KBFUL0 0x00008000U
#define DZ4_PRFUL0 0x00004000U
#define DZ4_KBSET0 0x00002000U
#define DZ4_PRCLR0 0x00001000U
#define DZ4_PRBUF0 0x000000FFU
#define DZ4_KBBUF0 0x000000FFU

static bool volatile ctrlcflag;
static struct termios term_original;
static uint32_t cps = 960;

static void sigrunhand (int signum);

int main (int argc, char **argv)
{
    bool killit = false;
    bool nokb = false;
    bool upcase = false;
    char const *logname = NULL;
    int port = -1;
    char *p;

    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("     Access DZ-11 serial line multiplexor");
            puts ("");
            puts ("  ./z11dz [-cps <charspersec>] [-killit] [-log <filename>] [-nokb] [-upcase] <linenum>");
            puts ("     -cps    : set chars per second, default 960");
            puts ("     -killit : kill other process that is processing this port");
            puts ("     -log    : log output to given file");
            puts ("     -nokb   : do not pass stdin keyboard to pdp");
            puts ("     -upcase : convert all keyboard to upper case");
            puts ("   <linenum> : line number 0..7");
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
        if (argv[i][0] == '-') {
            fprintf (stderr, "unknown option %s\n", argv[i]);
            return 1;
        }
        if (port >= 0) {
            fprintf (stderr, "unknown argument %s\n", argv[1]);
            return 1;
        }
        port = strtoul (argv[i], &p, 8);
        if ((*p != 0) || (port < 0) || (port > 7)) {
            fprintf (stderr, "port number %s must be octal integer 0..7\n", argv[i]);
            return 1;
        }
    }

    if (port < 0) {
        fprintf (stderr, "missing linenum argument\n");
        return 1;
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

    // access fpga page
    // point to DZ-11 register for this line
    // each register contains bits for an even/odd pair of lines, 16 bits each
    //   DZ4:  <line-1> <line-0>
    //   DZ5:  <line-3> <line-2>
    //   DZ6:  <line-5> <line-4>
    //   DZ7:  <line-7> <line-6>
    // locking happens on one of the 8 32-bit registers, not really related to the 16-bit subregister
    // design of 16-bits per line is such that two processes can access same 32-bit register at same time
    Z11Page z11p;
    uint32_t volatile *dzat = z11p.findev ("DZ", NULL, NULL, false, false);
    z11p.locksubdev (&dzat[port], 1, killit);
    ZWR(dzat[1], DZ1_ENABLE |
        (INTVEC * (DZ1_INTVEC & - DZ1_INTVEC)) |
        (ADDRES * (DZ1_ADDRES & - DZ1_ADDRES)));
    dzat += 4 + port / 2;
    int bn = (port & 1) * 16;

    struct termios term_modified;
    bool stdintty = isatty (STDIN_FILENO) > 0;
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

    bool stdoutty = isatty (STDOUT_FILENO) > 0;

    uint64_t nowus;
    uint64_t readnextkbat;
    uint64_t readnextprat;

    struct timeval nowtv;
    if (gettimeofday (&nowtv, NULL) < 0) ABORT ();
    nowus = nowtv.tv_sec * 1000000ULL + nowtv.tv_usec;

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

        // read register
        uint32_t reg = ZRD(*dzat) >> bn;

        // maybe see if PDP has a character to print
        if ((nowus >= readnextprat) && (reg & DZ4_PRFUL0)) {

            // tell PDP it can print another char
            ZWR(*dzat, DZ4_PRCLR0 << bn);

            // print character to stdout
            uint8_t prchar = (reg & DZ4_PRBUF0) / (DZ4_PRBUF0 & - DZ4_PRBUF0);
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
                ZWR(*dzat, (DZ4_KBSET0 | (DZ4_KBBUF0 & - DZ4_KBBUF0) * kbchar) << bn);
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
