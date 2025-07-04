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

#ifndef _FUTEX_H
#define _FUTEX_H

#include <linux/futex.h>
#include <syscall.h>

static inline bool atomic_compare_exchange (int *ptr, int *oldptr, int newval)
{
    return __atomic_compare_exchange_n (ptr, oldptr, newval, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

static inline int futex (int *uaddr, int futex_op, int val,
             const struct timespec *timeout, int *uaddr2, int val3)
{
    return syscall (SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3);
}

#endif
