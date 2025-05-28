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

#include "bigmem.h"

BigMem::BigMem ()
{
    armaddr = 0;
    armdata = 0;
    enable  = 0;

    unidevmemory = this;
}

uint32_t BigMem::axirdslv (uint32_t index)
{
    switch (index) {
        case 0: return 0x424D2005;
        case 1: return enable;
        case 2: return enable >> 32;
        case 3: return armaddr;
        case 4: return armdata;
        default: return 0xDEADBEEF;
    }
}

void BigMem::axiwrslv (uint32_t index, uint32_t data)
{
    switch (index) {
        case 1: enable  = (enable & 0x3FFFFFFF00000000ULL) | data; break;
        case 2: enable  = (enable & 0xFFFFFFFFU) | (((uint64_t) (data & 0x3FFFFFFFU)) << 32); break;
        case 3: {
            armaddr = data & 0777776;
            switch (data >> 29) {
                case 1: {   // write lower byte
                    mem.bytes[armaddr|0] = armdata;
                    break;
                }
                case 2: {   // write upper byte
                    mem.bytes[armaddr|1] = armdata >> 8;
                    break;
                }
                case 3: {   // write word
                    mem.words[armaddr>>1] = armdata;
                    break;
                }
                case 4: {   // read word
                    armdata = mem.words[armaddr>>1];
                    break;
                }
            }
            break;
        }
        case 4: armdata = data & 0xFFFFU; break;
    }
}

// RESET instruction doesn't do anything to memory
void BigMem::resetslave ()
{ }

// memory does not interrupt the processor
uint8_t BigMem::getintslave (uint16_t level)
{
    return 0;
}

// something on unibus is attempting to read memory
bool BigMem::rdslave (uint32_t physaddr, uint16_t *data)
{
    if (! (enable >> (physaddr >> 12))) return false;
    *data = mem.words[physaddr>>1];
    return true;
}

// something on unibus is attempting to write memory
bool BigMem::wrslave (uint32_t physaddr, uint16_t data, bool byte)
{
    if (! (enable >> (physaddr >> 12))) return false;
    if (byte) mem.bytes[physaddr] = data;
      else mem.words[physaddr>>1] = data;
    return true;
}
