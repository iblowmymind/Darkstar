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

#include "cp_nia.h"

int NiaEngine::Compute(const Microinstruction& mi,
                       TaskContext&            ctx,
                       const AM2901&           alu,
                       uint16_t                xBus,
                       bool                    mesaIntRq)
{
    int nextUPC = mi.INIA;

    // pCallRet: push return address (INIA) onto call stack, jump to LinkAddress.
    if (mi.LinkAddress >= 0)
    {
        if (ctx.stackPointer < CP_CALL_STACK_DEPTH)
            ctx.callStack[ctx.stackPointer++] = mi.INIA;
        return mi.LinkAddress & (CP_CS_SIZE - 1);
    }

    // IBDisp: dispatch on the high nibble of the current IB byte.
    if (mi.AlwaysIBDisp)
    {
        uint8_t opcode = ctx.ib[ctx.ibPtr & (CP_IB_SIZE - 1)];
        return (mi.INIA & ~0xF) | ((opcode >> 4) & 0xF);
    }

    // DispBr: branch or multi-bit dispatch controlled by fY.
    if (mi.fSfY == FunctionSelectFY::DispBr)
    {
        YDispBrFunction fn = static_cast<YDispBrFunction>(mi.fY);
        if (fn <= YDispBrFunction::NibCarryBr)
        {
            // Single-bit branch: set bit 0 of INIA when condition is true.
            if (EvalDispBrCondition(fn, alu, mesaIntRq, xBus))
                nextUPC = mi.INIA | 1;
        }
        else
        {
            // Multi-bit dispatch: replace low 4 bits of INIA.
            uint16_t yBus = alu.R(mi.rA);
            nextUPC = (mi.INIA & ~0xF) | BuildDispatchOffset(fn, xBus, yBus, alu);
        }
        return nextUPC & (CP_CS_SIZE - 1);
    }

    // SJump: pop call stack and return to the saved address.
    if (mi.Pop && !mi.Push && ctx.stackPointer > 0)
        return ctx.callStack[--ctx.stackPointer] & (CP_CS_SIZE - 1);

    return nextUPC & (CP_CS_SIZE - 1);
}

bool NiaEngine::EvalDispBrCondition(YDispBrFunction fn,
                                     const AM2901&   alu,
                                     bool            mesaIntRq,
                                     uint16_t        xBus) const
{
    switch (fn)
    {
        case YDispBrFunction::NegBr:      return alu.Sign();
        case YDispBrFunction::ZeroBr:     return alu.Zero();
        case YDispBrFunction::NZeroBr:    return !alu.Zero();
        case YDispBrFunction::MesaIntBr:  return mesaIntRq;
        case YDispBrFunction::PgCarryBr:  return alu.PgCarry();
        case YDispBrFunction::CarryBr:    return alu.CarryOut();
        case YDispBrFunction::XRefBr:     return (xBus & 1) != 0;
        case YDispBrFunction::NibCarryBr: return alu.NibCarry();
        default:                          return false;
    }
}

int NiaEngine::BuildDispatchOffset(YDispBrFunction fn,
                                    uint16_t        xBus,
                                    uint16_t        yBus,
                                    const AM2901&   alu) const
{
    switch (fn)
    {
        case YDispBrFunction::XDisp:      return  xBus        & 0xF;
        case YDispBrFunction::YDisp:      return  yBus        & 0xF;
        case YDispBrFunction::XC2npcDisp: return (xBus >> 2)  & 0x3;
        case YDispBrFunction::YIODisp:    return  xBus        & 0xF;
        case YDispBrFunction::XwdDisp:    return  xBus        & 0x3;
        case YDispBrFunction::XHDisp:     return (xBus >> 12) & 0xF;
        case YDispBrFunction::XLDisp:     return (xBus >> 8)  & 0xF;
        case YDispBrFunction::PgCrOvDisp: return (alu.PgCarry() ? 2 : 0) | (alu.Overflow() ? 1 : 0);
        default:                          return 0;
    }
}
