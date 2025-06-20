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

#ifndef _ZGINTDEFS_H
#define _ZGINTDEFS_H

#define ZGIOCTL_WFI 7489330     // must match kernel module
#define ZG_INTENABS 0x1A        // regarmintena in zynq.v
#define ZG_INTFLAGS 0x1B        // regarmintreq in zynq.v
#define ZGINT_RL  0x00000001U   // rl11.v interrupt
#define ZGINT_ARM 0x40000000U   // arm interrupts itself (km probing)
#define ZGINT_REQ 0x80000000U   // composite request (in ZG_INTFLAGS)

#endif
