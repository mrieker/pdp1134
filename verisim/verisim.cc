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

// Interface z11ctrl to verilated zynq code
// Pretends to be the 4KB Zynq page by doing TCP connection to verimain daemon
// Call verisim_read() and verisim_write() to access the Zynq-like register page

#include <arpa/inet.h>
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

#include "verisim.h"

static int confd;
static pthread_mutex_t conmutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t volatile *pageptr;

// equivalent of opening /proc/zynqpdp11
// set up no-access page so any reference that isn;"t wrapped with ZRD() or ZWR() will abort
// connect to verimain daemon
uint32_t volatile *verisim_init ()
{
    sockaddr_in servaddr;
    memset (&servaddr, 0, sizeof servaddr);
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons (VeriTCPPORT);
    if (! inet_aton ("127.0.0.1", &servaddr.sin_addr)) ABORT ();

    confd = socket (AF_INET, SOCK_STREAM, 0);
    if (confd < 0) ABORT ();
    if (connect (confd, (sockaddr *)&servaddr, sizeof servaddr) < 0) {
        fprintf(stderr, "connect %s:%u error: %m\n",
                inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port));
        ABORT ();
    }
    static int const one = 1;
    if (setsockopt (confd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof one) < 0) ABORT ();

    void *ptr = mmap (NULL, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) ABORT ();
    pageptr = (uint32_t volatile *) ptr;
    return pageptr;
}

// wrapper for all read accesses to fpga register page
uint32_t verisim_read (uint32_t volatile *addr)
{
    pthread_mutex_lock (&conmutex);
    VeriTCPMsg tcpmsg;
    memset (&tcpmsg, 0, sizeof tcpmsg);
    tcpmsg.indx = addr - pageptr;
    if (write (confd, &tcpmsg, sizeof tcpmsg) != (int) sizeof tcpmsg) ABORT ();
    if (read (confd, &tcpmsg, sizeof tcpmsg) != (int) sizeof tcpmsg) ABORT ();
    pthread_mutex_unlock (&conmutex);
    return tcpmsg.data;
}

// wrapper for all write accesses to fpga register page
void verisim_write (uint32_t volatile *addr, uint32_t data)
{
    pthread_mutex_lock (&conmutex);
    VeriTCPMsg tcpmsg;
    memset (&tcpmsg, 0, sizeof tcpmsg);
    tcpmsg.indx = addr - pageptr;
    tcpmsg.data = data;
    tcpmsg.write = true;
    if (write (confd, &tcpmsg, sizeof tcpmsg) != (int) sizeof tcpmsg) ABORT ();
    pthread_mutex_unlock (&conmutex);
}
