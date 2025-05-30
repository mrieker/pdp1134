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

#ifndef _CPU1134_H
#define _CPU1134_H

#include <exception>
#include <stdint.h>

#include "axidev.h"
#include "stepper.h"
#include "unidev.h"

struct CPU1134 : AxiDev, Stepper, UniDev {
    static bool cpuhaltins ();

    CPU1134 ();

protected:
    // AxiDev
    virtual uint32_t axirdslv (uint32_t index);
    virtual void axiwrslv (uint32_t index, uint32_t data);

    // Stepper
    virtual void stepit ();

    // UniDev
    virtual void resetslave (); 
    virtual uint8_t getintslave (uint16_t level);
    virtual bool rdslave (uint32_t addr, uint16_t *data);
    virtual bool wrslave (uint32_t addr, uint16_t data, bool byte);

private:
    static CPU1134 *singleton;

    bool dbg1, dbg2;
    bool havedstaddr;
    bool lastpoweron;
    bool yellowstkck;
    uint16_t dstaddr;
    uint16_t gprs[16];
    uint16_t instreg;
    uint16_t knlpars[8];
    uint16_t knlpdrs[8];
    uint16_t lastprint;
    uint16_t mmr0;
    uint16_t mmr2;
    uint16_t psw;
    uint16_t usrpars[8];
    uint16_t usrpdrs[8];
    uint32_t dstphys;
    uint32_t regctla;
    uint32_t regctlb;
    uint32_t regctli;

    bool jammedup ();
    uint16_t readsrc (bool byte);
    uint16_t readdst (bool byte);
    void writedst (uint16_t data, bool byte);
    uint16_t getopaddr (uint16_t mr, bool byte);
    uint16_t gprx (uint16_t r, uint16_t mode);

    uint16_t rdwordvirt (uint16_t vaddr, uint16_t mode);
    uint8_t rdbytevirt (uint16_t vaddr, uint16_t mode);
    void wrwordvirt (uint16_t vaddr, uint16_t data, uint16_t mode);
    void wrbytevirt (uint16_t vaddr, uint8_t data, uint16_t mode);

    uint32_t getphysaddr (uint16_t vaddr, bool write, uint16_t mode);

    static bool addvbit (uint16_t a, uint16_t b, bool byte);
    static bool addcbit (uint16_t a, uint16_t b, bool byte);
    static bool subvbit (uint16_t a, uint16_t b, bool byte);
    static bool subcbit (uint16_t a, uint16_t b, bool byte);
    static bool aslvbit (uint16_t dstval, uint16_t result, bool byte);
    static bool asrvbit (uint16_t dstval, uint16_t result, bool byte);

    void updnzvc (uint16_t result, bool byte, uint16_t vbit, uint16_t cbit);

    uint16_t rdwordphys (uint32_t physaddr);
    void wrwordphys (uint32_t physaddr, uint16_t data);
    void wrbytephys (uint32_t physaddr, uint8_t data);
};

#endif
