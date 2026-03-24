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

#pragma once

// ---------------------------------------------------------------------------
// cp_nia.h  –  Next Instruction Address (NIA) engine
//
// Computes the uPC for the next microcode cycle from the current
// Microinstruction, ALU flags, call stack, and optional dispatch data.
//
// Dispatch modes (NiaModifierType)
// ---------------------------------
//   Normal     – next uPC = INIA field of the current MI.
//
//   pCallRet   – fX in pCallRet0..pCallRet7:
//     CALL:  push INIA onto call stack; jump to LinkAddress.
//     (Stack pop is handled in HandleXFunction; Compute just advances uPC.)
//
//   Dispatch   – fSfY == DispBr, selecting one of 16 YDispBrFunction codes:
//     Single-bit branch conditions (if true → next uPC = INIA | 1):
//       NegBr, ZeroBr, NZeroBr, MesaIntBr, PgCarryBr, CarryBr,
//       XRefBr, NibCarryBr
//     Multi-bit dispatch (next uPC = (INIA & ~0xF) | offset):
//       XDisp     – low 4 bits of X-bus
//       YDisp     – low 4 bits of AM2901 R[rA] (Y-bus)
//       XC2npcDisp– bits [3:2] of X-bus → 2-bit table
//       YIODisp   – low 4 bits of X-bus (I/O status nibble)
//       XwdDisp   – low 2 bits of X-bus (word index)
//       XHDisp    – high nibble of X-bus high byte
//       XLDisp    – low nibble of X-bus low byte
//       PgCrOvDisp– {PgCarry, Overflow} → 2-bit combined dispatch
//
//   IBDisp     – AlwaysIBDisp: opcode high nibble used as 4-bit offset.
//
//   SJump      – pop call stack; jump to popped address.
//
// Ported from D/CP/CentralProcessor.cs (NextInstruction / pCallRet section).
// ---------------------------------------------------------------------------

#include "cp_types.h"
#include "microinstruction.h"
#include "am2901.h"

class NiaEngine
{
public:
    NiaEngine() = default;

    // Compute the next uPC.
    //   mi         – decoded microinstruction for the current click.
    //   ctx        – mutable per-task context (call stack updated here).
    //   alu        – current AM2901 state (flags read here).
    //   xBus       – X-bus value after IOXIn / LRot.
    //   mesaIntRq  – current Mesa interrupt-request flag.
    // Returns the 12-bit uPC for the next click.
    int Compute(const Microinstruction& mi,
                TaskContext&            ctx,
                const AM2901&           alu,
                uint16_t                xBus,
                bool                    mesaIntRq);

private:
    // Evaluate a single-bit branch condition.
    bool EvalDispBrCondition(YDispBrFunction fn,
                              const AM2901&   alu,
                              bool            mesaIntRq,
                              uint16_t        xBus) const;

    // Build a 4-bit dispatch offset from a multi-bit dispatch source.
    //   yBus – AM2901 R[rA] value (used by YDisp).
    int BuildDispatchOffset(YDispBrFunction fn,
                             uint16_t        xBus,
                             uint16_t        yBus,
                             const AM2901&   alu) const;
};
