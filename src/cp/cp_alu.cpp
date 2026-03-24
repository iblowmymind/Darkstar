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

#include "cp_alu.h"
#include <cstring>

CpAlu::CpAlu()
{
    Reset();
}

void CpAlu::Reset()
{
    _alu.Reset();
    memset(_su, 0, sizeof(_su));
}

uint16_t CpAlu::Execute(const Microinstruction& mi,
                        TaskContext&            ctx,
                        uint16_t               xBus,
                        bool                   cin)
{
    // SU read: substitute the D input from the supplementary register file.
    if (mi.SURead)
        xBus = _su[mi.rB & 0xF];

    // Run the AM2901.
    _alu.Execute(
        static_cast<int>(mi.aS),
        static_cast<int>(mi.aF),
        mi.AluDestination,
        mi.rA,
        mi.rB,
        xBus,
        cin);

    // SU write: capture the ALU result into the supplementary register file.
    if (mi.SUWrite)
        _su[mi.rB & 0xF] = _alu.F();

    // LoadRH: latch the ALU F output into the right-half register.
    if (mi.fX == XFunction::LoadRH)
        ctx.RH = _alu.F();

    return _alu.OE();
}

uint16_t CpAlu::ApplyLRot(const Microinstruction& mi, uint16_t value) const
{
    if (!mi.LateLRotN)
        return value;

    if (mi.fSfZ != FunctionSelectFZ::fzNorm)
        return value;

    switch (static_cast<ZNormFunction>(mi.fZ))
    {
        case ZNormFunction::LRot0:  return value;
        case ZNormFunction::LRot4:  return static_cast<uint16_t>((value << 4)  | (value >> 12));
        case ZNormFunction::LRot8:  return static_cast<uint16_t>((value << 8)  | (value >> 8));
        case ZNormFunction::LRot12: return static_cast<uint16_t>((value << 12) | (value >> 4));
        default:                    return value;
    }
}
