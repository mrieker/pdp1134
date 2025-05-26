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

#include "obj_dir/VMyBoard.h"

#include "verisim.h"

static pthread_mutex_t mybdmtx = PTHREAD_MUTEX_INITIALIZER;
static VMyBoard *vmybd;

static void *conthread (void *confdv);
static void kerchunk ();

int main (int argc, char **argv)
{
    setlinebuf (stdout);

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

    vmybd = new VMyBoard ();
    vmybd->RESET_N = 0;
    for (int i = 0; i < 10; i ++) {
        kerchunk ();
    }
    vmybd->RESET_N = 1;
    for (int i = 0; i < 10; i ++) {
        kerchunk ();
    }

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
        if (tcpmsg.write) {
            uint32_t index = tcpmsg.indx;
            if (index > 1023) ABORT ();

            // put address and write data on AXI bus
            // say they are valid and also we are ready to accept write acknowledge
            if (pthread_mutex_lock (&mybdmtx) != 0) ABORT ();
            vmybd->saxi_AWADDR  = index * sizeof tcpmsg.data;
            vmybd->saxi_AWVALID = 1;
            vmybd->saxi_WDATA   = tcpmsg.data;
            vmybd->saxi_WVALID  = 1;
            vmybd->saxi_BREADY  = 1;

            // keep kerchunking until all 3 transfers have completed
            for (int i = 0; vmybd->saxi_AWVALID | vmybd->saxi_WVALID | vmybd->saxi_BREADY; i ++) {
                if (i > 100) ABORT ();
                bool awready = vmybd->saxi_AWREADY;
                bool wready  = vmybd->saxi_WREADY;
                bool bvalid  = vmybd->saxi_BVALID;
                kerchunk ();
                if (awready) vmybd->saxi_AWVALID = 0;
                if (wready)  vmybd->saxi_WVALID  = 0;
                if (bvalid)  vmybd->saxi_BREADY  = 0;
            }
            if (pthread_mutex_unlock (&mybdmtx) != 0) ABORT ();
        } else {
            uint32_t index = tcpmsg.indx;
            if (index > 1023) ABORT ();

            // send read address out over AXI bus and say we are ready to accept read data
            if (pthread_mutex_lock (&mybdmtx) != 0) ABORT ();
            vmybd->saxi_ARADDR  = index * sizeof tcpmsg.data;
            vmybd->saxi_ARVALID = 1;
            vmybd->saxi_RREADY  = 1;

            // keep kerchunking until both transfers have completed
            // capture read data on cycle where both RREADY and RVALID are set
            for (int i = 0; vmybd->saxi_ARVALID | vmybd->saxi_RREADY; i ++) {
                if (i > 100) ABORT ();
                bool arready = vmybd->saxi_ARREADY;
                bool rvalid  = vmybd->saxi_RVALID;
                if (rvalid & vmybd->saxi_RREADY) tcpmsg.data = vmybd->saxi_RDATA;
                kerchunk ();
                if (arready) vmybd->saxi_ARVALID = 0;
                if (rvalid)  vmybd->saxi_RREADY  = 0;
            }
            if (pthread_mutex_unlock (&mybdmtx) != 0) ABORT ();

            if (write (confd, &tcpmsg, sizeof tcpmsg) != (int) sizeof tcpmsg) ABORT ();
        }
    }
    close (confd);
    return NULL;
}

// call with clock still low and input signals just changed
// returns with output signals updated and clock just set low
static void kerchunk ()
{
    vmybd->eval ();    // let input signals soak in
    vmybd->CLOCK = 1;  // clock the state
    vmybd->eval ();    // let new state settle in
    vmybd->CLOCK = 0;  // get ready for more input changes
}
