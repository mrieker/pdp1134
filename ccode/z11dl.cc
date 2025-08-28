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
#include <time.h>
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

struct LookFor {
    char const *promptstr;  // prompt string
    char replystr[32];      // reply string
    int numechoed;          // number of characters sent and echoed
    int numatched;          // number of characters received that match

    LookFor ();
    bool GotPRChar (uint8_t prchar);
    uint8_t GetKBChar ();
    virtual void FormatReply () = 0;
};

struct LookForRSTSDate    : LookFor { LookForRSTSDate ();    virtual void FormatReply (); };
struct LookForRSTSTime    : LookFor { LookForRSTSTime ();    virtual void FormatReply (); };
struct LookForRSXDateTime : LookFor { LookForRSXDateTime (); virtual void FormatReply (); };
struct LookForXXDPDate    : LookFor { LookForXXDPDate ();    virtual void FormatReply (); };

static char const monthnames[12][4] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                        "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };

static bool nokb;
static bool upcase;
static bool volatile ctrlcflag;
static struct termios term_original;
static uint32_t cps = 960;
static uint32_t volatile *dlat;

static bool finddl (void *param, uint32_t volatile *dlat);
static void sigrunhand (int signum);

int main (int argc, char **argv)
{
    bool killit = false;
    bool rstsdt = true;
    bool rsxdt = true;
    bool xxdpdt = true;
    char const *logname = NULL;
    int port = 0777560;
    uint32_t mask = 0177 * DL2_XBUF0;
    char *p;
    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("     Access DL-11 TTY controller");
            puts ("");
            puts ("  ./z11dl [-8bit] [-cps <charspersec>] [-killit] [-log <filename>] [-nokb] [-norstsdt] [-norsxdt] [-noxxdpdt] [-upcase]");
            puts ("     -8bit    : pass all 8 printer bits to output, else strip to 7 bits");
            puts ("     -cps     : set chars per second, default 960");
            puts ("     -killit  : kill other process that is processing this port");
            puts ("     -log     : log output to given file");
            puts ("     -nokb    : do not pass stdin keyboard to pdp");
            puts ("    -norstsdt : do not answer RSTS/E format date/time prompts");
            puts ("     -norsxdt : do not answer RSX-11 format date/time prompt");
            puts ("    -noxxdpdt : do not answer XXDP format date prompts");
            puts ("     -upcase  : convert all keyboard input to upper case");
            puts ("");
            return 0;
        }
        if (strcasecmp (argv[i], "-8bit") == 0) {
            mask = 0377 * DL2_XBUF0;
            continue;
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
        if (strcasecmp (argv[i], "-norstsdt") == 0) {
            rstsdt = false;
            continue;
        }
        if (strcasecmp (argv[i], "-norsxdt") == 0) {
            rsxdt = false;
            continue;
        }
        if (strcasecmp (argv[i], "-noxxdpdt") == 0) {
            xxdpdt = false;
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

    // set up looking for rsts/rsx date/time prompts
    LookFor *lookfors[4];
    int nlookfors = 0;
    if (rstsdt) {
        lookfors[nlookfors++] = new LookForRSTSDate ();
        lookfors[nlookfors++] = new LookForRSTSTime ();
    }

    if (rsxdt) {
        lookfors[nlookfors++] = new LookForRSXDateTime ();
    }

    if (xxdpdt) {
        lookfors[nlookfors++] = new LookForXXDPDate ();
    }

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
                uint8_t prchar = (xreg & mask) / DL2_XBUF0;
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

                // check for date/time prompt
                for (int lfi = 0; lfi < nlookfors; lfi ++) {
                    if (lookfors[lfi]->GotPRChar (prchar)) {
                        // halt second until sending first reply character
                        readnextkbat = nowus + 500000;
                    }
                }
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

            // maybe send next char of rsx date/time reply
            else {
                for (int lfi = 0; lfi < nlookfors; lfi ++) {
                    uint8_t kbchar = lookfors[lfi]->GetKBChar ();
                    if (kbchar != 0) {
                        ZWR(dlat[1], (ZRD(dlat[1]) & ~ DL1_RBUF) | DL1_RRDY | DL1_RBUF0 * kbchar);
                        readnextkbat = nowus + 250000;
                    }
                }
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

LookForRSTSDate::LookForRSTSDate ()
{
    promptstr = "Today's date? ";
}

LookForRSTSTime::LookForRSTSTime ()
{
    promptstr = "Current time? ";
}

LookForRSXDateTime::LookForRSXDateTime ()
{
    promptstr = "PLEASE ENTER TIME AND DATE (HR:MN DD-MMM-YY) [S]: ";
}

LookForXXDPDate::LookForXXDPDate ()
{
    promptstr = "ENTER DATE (DD-MMM-YY): ";
}

LookFor::LookFor ()
{
    numechoed = -1;
    numatched =  0;
}

// just got a characater from pdp for printing
// see if it matches a date/time prompt or is an echo of a date/time reply
bool LookFor::GotPRChar (uint8_t prchar)
{
    if (numechoed < 0) {
        // check for prompt string character
        // if everything matched, get date/time and set up reply string
        if (promptstr[numatched] != prchar) numatched = 0;
        else if (promptstr[++numatched] == 0) {
            FormatReply ();
            numechoed = 0;
            return true;
        }
    } else {
        // sending reply, check for echo of character just sent
        // if mismatch or reached the end, clear all search state
        if ((replystr[numechoed] != prchar) ||
            (replystr[++numechoed] == 0)) {
            numechoed = -1;
            numatched = 0;
        }
    }
    return false;
}

uint8_t LookFor::GetKBChar ()
{
    return (numechoed < 0) ? 0 : replystr[numechoed];
}

static time_t rstsdtbin;

void LookForRSTSDate::FormatReply ()
{
    rstsdtbin = time (NULL) + 10;       // we take ~10 sec to send it out
    struct tm nowtm = *localtime (&rstsdtbin);
    sprintf (replystr, "%02d-%3s-%2d\r",
        nowtm.tm_mday, monthnames[nowtm.tm_mon], nowtm.tm_year % 100);
}

void LookForRSTSTime::FormatReply ()
{
    struct tm nowtm = *localtime (&rstsdtbin);
    sprintf (replystr, "%02d:%02d\r",
        nowtm.tm_hour, nowtm.tm_min);
}

void LookForRSXDateTime::FormatReply ()
{
    time_t nowbin = time (NULL) + 10;   // we take ~10 sec to send it out
    struct tm nowtm = *localtime (&nowbin);
    sprintf (replystr, "%02d:%02d:%02d %02d-%3s-%2d\r",
        nowtm.tm_hour, nowtm.tm_min, nowtm.tm_sec,
        nowtm.tm_mday, monthnames[nowtm.tm_mon], nowtm.tm_year % 100);
}

void LookForXXDPDate::FormatReply ()
{
    time_t nowbin = time (NULL);
    struct tm nowtm = *localtime (&nowbin);
    sprintf (replystr, "%02d-%3s-99\r", nowtm.tm_mday, monthnames[nowtm.tm_mon]);
}
