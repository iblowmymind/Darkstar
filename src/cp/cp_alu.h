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
// cp_alu.h  –  Central-Processor ALU wrapper
//
// Wraps the AM2901 4-bit-slice ALU array and implements the surrounding
// CP microarchitecture logic:
//
//   X-bus source multiplexer
//     Before the AM2901 executes the CP must supply a value on the D (direct
//     data) input:
//       • Default (AluNeedsXBus true)  → D = ctx.T  (T register from last click)
//       • Byte constant (fSfY==Byte or fSfZ==Nibble) → D = mi.Byte
//       • Zero (AluNeedsXBus false)    → D = 0
//     The resolved value is passed as the xBus parameter to Execute().
//
//   Carry-in resolution
//     • mi.Cin                          – literal carry-in bit.
//     • ctx.cin16 when LoadCinFrompc16  – carry saved from previous 32-bit op.
//     The caller resolves this and passes the result as the cin parameter.
//
//   AM2901 invocation
//     Execute() calls AM2901::Execute(src, func, dest, rA, rB, xBus, cin).
//     dest = mi.AluDestination (mi.aD | shift modifier from Shift/Cycle flag).
//
//   SU (supplementary / shift-unit) register file  (16 × 16-bit)
//     mi.SURead  → override D input with _su[mi.rB] before AM2901 runs.
//     mi.SUWrite → store AM2901 F into _su[mi.rB] after AM2901 runs.
//
//   RH register
//     mi.fX == XFunction::LoadRH → ctx.RH ← AM2901 F output.
//
//   LRot (late left-rotation of the ALU output)
//     Applied after all ALU work, to the value that becomes the new T:
//       ZNormFunction::LRot0  → no rotation
//       ZNormFunction::LRot4  → rotate left  4 bits
//       ZNormFunction::LRot8  → rotate left  8 bits (byte-swap)
//       ZNormFunction::LRot12 → rotate left 12 bits
//     ApplyLRot() is called after memory / I/O so those results are also
//     subject to rotation.
//
// Ported from D/CP/CentralProcessor.cs (ALU / SU / LRot sections).
// ---------------------------------------------------------------------------

#include "cp_types.h"
#include "microinstruction.h"
#include "am2901.h"

class CpAlu
{
public:
    CpAlu();
    void Reset();

    // Execute the ALU for one click.
    //   mi   – decoded microinstruction.
    //   ctx  – mutable per-task context (RH updated when LoadRH).
    //   xBus – resolved D-bus / X-bus value for the AM2901 D input.
    //   cin  – resolved carry-in.
    // Returns the AM2901 OE (shifter output).
    uint16_t Execute(const Microinstruction& mi,
                     TaskContext&            ctx,
                     uint16_t               xBus,
                     bool                   cin);

    // Apply late left-rotation to value according to the Z-function in mi.
    // Returns value unchanged when mi.LateLRotN is false or the Z-function
    // is not in the LRot group.
    uint16_t ApplyLRot(const Microinstruction& mi, uint16_t value) const;

    // Flag accessors (delegate to AM2901).
    bool Carry()    const { return _alu.CarryOut(); }
    bool Zero()     const { return _alu.Zero();     }
    bool Sign()     const { return _alu.Sign();     }
    bool PgCarry()  const { return _alu.PgCarry();  }
    bool NibCarry() const { return _alu.NibCarry(); }
    bool Overflow() const { return _alu.Overflow(); }

    // Expose the underlying AM2901 (read-only) for NiaEngine flag checks.
    const AM2901& Alu() const { return _alu; }

private:
    AM2901   _alu;
    uint16_t _su[16]{};   // SU (supplementary) register file: 16 × 16-bit
};
