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

// ---------------------------------------------------------------------------
// NiaEngine::Compute
//
// Implementation plan for a future session:
//  1. Default: nextUPC = mi.INIA.
//  2. pCallRet (mi.LinkAddress >= 0): push INIA onto callStack; next = LinkAddress.
//  3. fSfY == DispBr && AlwaysIBDisp: next = (INIA & ~0xF) | (opcode >> 4).
//  4. fSfY == DispBr, single-bit: if EvalDispBrCondition() → next = INIA | 1.
//  5. fSfY == DispBr, multi-bit:  next = (INIA & ~0xF) | BuildDispatchOffset().
//  6. SJump (fX==pop && pop-only): next = callStack[--stackPointer].
//  7. Return next & (CP_CS_SIZE - 1).
// ---------------------------------------------------------------------------

int NiaEngine::Compute(const Microinstruction& mi,
                       TaskContext&            ctx,
                       const AM2901&           alu,
                       uint16_t                xBus,
                       bool                    mesaIntRq)
{
    // TODO: implement full NIA logic per plan above.
    // Stub: sequential advance.
    (void)mi; (void)alu; (void)xBus; (void)mesaIntRq;
    return (ctx.uPC + 1) & (CP_CS_SIZE - 1);
}

// ---------------------------------------------------------------------------
// NiaEngine::EvalDispBrCondition
//
// Implementation plan:
//   NegBr      → alu.Sign()
//   ZeroBr     → alu.Zero()
//   NZeroBr    → !alu.Zero()
//   MesaIntBr  → mesaIntRq
//   PgCarryBr  → alu.PgCarry()
//   CarryBr    → alu.CarryOut()
//   XRefBr     → (xBus & 1) != 0
//   NibCarryBr → alu.NibCarry()
// ---------------------------------------------------------------------------

bool NiaEngine::EvalDispBrCondition(YDispBrFunction fn,
                                     const AM2901&   alu,
                                     bool            mesaIntRq,
                                     uint16_t        xBus) const
{
    // TODO: implement per plan above.
    (void)fn; (void)alu; (void)mesaIntRq; (void)xBus;
    return false;
}

// ---------------------------------------------------------------------------
// NiaEngine::BuildDispatchOffset
//
// Implementation plan:
//   XDisp      → xBus & 0xF
//   YDisp      → yBus & 0xF
//   XC2npcDisp → (xBus >> 2) & 0x3
//   YIODisp    → xBus & 0xF
//   XwdDisp    → xBus & 0x3
//   XHDisp     → (xBus >> 12) & 0xF
//   XLDisp     → (xBus >> 8)  & 0xF
//   PgCrOvDisp → (alu.PgCarry() ? 2 : 0) | (alu.Overflow() ? 1 : 0)
// ---------------------------------------------------------------------------

int NiaEngine::BuildDispatchOffset(YDispBrFunction fn,
                                    uint16_t        xBus,
                                    uint16_t        yBus,
                                    const AM2901&   alu) const
{
    // TODO: implement per plan above.
    (void)fn; (void)xBus; (void)yBus; (void)alu;
    return 0;
}
