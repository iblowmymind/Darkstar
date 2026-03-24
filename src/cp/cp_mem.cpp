/*
    BSD 2-Clause License

    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this
      list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "cp_mem.h"
#include "../dsystem.h"
#include <cstring>

CpMemInterface::CpMemInterface(DSystem* system)
    : _system(system)
{
    Reset();
}

void CpMemInterface::Reset()
{
    memset(_map, 0, sizeof(_map));
    _marpEnable = false;
    _kernelMode = false;
    _memClick   = 0;
}

// ---------------------------------------------------------------------------
// CpMemInterface::ExecuteMem
//
// Executes one click of the three-click MAR/MDR/MD memory sequence:
//   Click 1 – MAR← : translate address and call LoadMAR().
//   Click 2 – MDR← : write data via LoadMDR(aluOut).
//   Click 3 – ←MD  : return the word from ReadMD(); set
//                     ErrorTrap::EmulatorMemoryError on an ECC fault.
// If mi.MarMapMDR is false the sequence is reset and 0 is returned.
// ---------------------------------------------------------------------------

uint16_t CpMemInterface::ExecuteMem(const Microinstruction& mi,
                                     TaskContext&            ctx,
                                     TaskType                task,
                                     uint16_t               aluOut)
{
    if (!mi.MarMapMDR)
    {
        _memClick = 0;
        return 0;
    }

    ++_memClick;

    switch (_memClick)
    {
        case 1:
            if (_system)
                _system->GetMemoryController()->LoadMAR(Translate(aluOut));
            return 0;

        case 2:
            if (_system)
                _system->GetMemoryController()->LoadMDR(aluOut);
            return 0;

        case 3:
        {
            _memClick = 0;
            if (!_system)
                return 0;
            bool valid = false;
            uint16_t w = _system->GetMemoryController()->ReadMD(task, valid);
            if (!valid)
                ctx.errorFlags |= static_cast<uint8_t>(1 << static_cast<int>(ErrorTrap::EmulatorMemoryError));
            return w;
        }

        default:
            _memClick = 0;
            return 0;
    }
}

void CpMemInterface::LoadMap(int mapIndex, uint16_t value)
{
    _map[mapIndex & (CP_MAP_SIZE - 1)] = value;
}

int CpMemInterface::Translate(int virtualAddress) const
{
    if (!_marpEnable)
        return virtualAddress;

    int page     = (virtualAddress >> 10) & (CP_MAP_SIZE - 1);
    int physPage = _map[page] & 0x3FF;
    return (physPage << 10) | (virtualAddress & 0x3FF);
}
