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

// tcp daemon for verilator simulator
// run on x86_64 as daemon - ./verimain.x86_64
// then run z11ctrl, z11dump, z11ila on same x86_64

#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>

#define ABORT() do { fprintf (stderr, "ABORT %s %d\n", __FILE__, __LINE__); abort (); } while (0)

#include "axidev.h"
#include "bigmem.h"
#include "cpu1134.h"
#include "dl11.h"
#include "kl11.h"
#include "swlight.h"
#include "../verisim/verisim.h"

static pthread_mutex_t mybdmtx = PTHREAD_MUTEX_INITIALIZER;

static void *conthread (void *confdv);

int main (int argc, char **argv)
{
    setlinebuf (stdout);

    // listen for connections from z11xx programs so they can access axi bus
    sockaddr_in servaddr;
    memset (&servaddr, 0, sizeof servaddr);
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons (VeriTCPPORT);

    int lisfd = socket (AF_INET, SOCK_STREAM, 0);
    if (lisfd < 0) ABORT ();
    static int const one = 1;
    if (setsockopt (lisfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one) < 0) ABORT ();
    if (bind (lisfd, (sockaddr *)&servaddr, sizeof servaddr) < 0) {
        fprintf(stderr, "bind %d error: %m\n", VeriTCPPORT);
        ABORT ();
    }
    if (listen (lisfd, 5) < 0) ABORT ();

    // plug boards into axi and uni busses
    new BigMem  ();
    new CPU1134 ();
    new DL11 ();
    new KL11 ();
    new SWLight ();
    AxiDev::axiassign ();

    // process connections from z11xx programs
    while (1) {
        memset (&servaddr, 0, sizeof servaddr);
        socklen_t addrlen = sizeof servaddr;
        int confd = accept (lisfd, (sockaddr *)&servaddr, &addrlen);
        if (confd < 0) {
            fprintf (stderr, "accept error: %m\n");
            continue;
        }
        if (setsockopt (confd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof one) < 0) ABORT ();
        pthread_t tid;
        int rc = pthread_create (&tid, NULL, conthread, (void *)(long)confd);
        if (rc != 0) ABORT ();
        pthread_detach (tid);
    }
}

static void *conthread (void *confdv)
{
    int confd = (long)confdv;
    VeriTCPMsg tcpmsg;
    while (read (confd, &tcpmsg, sizeof tcpmsg) == (int) sizeof tcpmsg) {

        // get register number being accessed
        uint32_t index = tcpmsg.indx;
        if (index > 1023) {
            fprintf (stderr, "conthread: bad index %u\n", index);
            break;
        }

        // do write to axi bus
        if (tcpmsg.write) {
            if (pthread_mutex_lock (&mybdmtx) != 0) ABORT ();
            printf ("simmer*: write %04X <= %08X\n", index, tcpmsg.data);
            AxiDev::axiwrmas (index, tcpmsg.data);
            if (pthread_mutex_unlock (&mybdmtx) != 0) ABORT ();
        }

        // do read from axi bus
        else {
            if (pthread_mutex_lock (&mybdmtx) != 0) ABORT ();
            tcpmsg.data = AxiDev::axirdmas (index);
            printf ("simmer*:  read %04X => %08X\n", index, tcpmsg.data);
            if (pthread_mutex_unlock (&mybdmtx) != 0) ABORT ();
            if (write (confd, &tcpmsg, sizeof tcpmsg) != (int) sizeof tcpmsg) ABORT ();
        }
    }
    close (confd);
    return NULL;
}
