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
// cp_io.h  –  Central-Processor I/O bus interface
//
// Dispatches all IOOut (write) and IOXIn (read) microcode operations to the
// correct peripheral subsystem.
//
// Write side  (fSfY == FunctionSelectFY::IOOut, fn = (YIOOutFunction)mi.fY)
// --------------------------------------------------------------------------
//   DCtlFifo  → DisplayController::SetDCtlFifo(aluOut)
//   DCtl      → DisplayController::SetDCtl(aluOut)
//   DBorder   → DisplayController::SetDBorder(aluOut)
//   MCtl      → MemoryController::SetMCtl(aluOut)
//   IOPOData  → latch aluOut as IOP output data byte
//   IOPCtl    → latch aluOut as IOP control word; TODO: trigger RST7.5 on IOP
//   KOData / KCtl / KCmd → TODO: keyboard subsystem
//   EOData / EICtl / EOCtl → TODO: ethernet subsystem
//   PCtl / POData → TODO: printer subsystem
//   Invalid0/1 → silently ignored
//
// Read side   (fSfZ == FunctionSelectFZ::IOXIn, fn = (ZIOXIn)mi.fZ)
// --------------------------------------------------------------------------
//   ReadMStatus        → MemoryController::MStatus()
//   ReadErrnIBnStkp    → (ctx.errorFlags<<8) | (ibState<<6) | stackPointer
//   ReadRH             → ctx.RH
//   Readib             → (ctx.ib[0]<<8) | ctx.ib[1]
//   ReadibLow          → ctx.ib[1]
//   ReadibHigh         → ctx.ib[0]
//   ReadibNA           → ctx.ibPtr
//   ReadIOPIData       → IOP→CP data byte (TODO: wire IOP)
//   ReadIOPStatus      → IOP status register (TODO: wire IOP)
//   ReadKIData / ReadKStatus / KStrobe → TODO: keyboard
//   ReadEIdata / ReadEStatus / EStrobe → TODO: ethernet
//   ReadKTest          → TODO: keyboard test mode
//
// Ported from D/CP/CentralProcessor.cs (I/O read/write sections).
// ---------------------------------------------------------------------------

#include "cp_types.h"
#include "microinstruction.h"
#include <cstdint>

class DSystem;

class CpIo
{
public:
    explicit CpIo(DSystem* system);

    void Reset();

    // Execute an IOOut write for the current click.
    void ExecuteIOOut(const Microinstruction& mi,
                      TaskContext&            ctx,
                      uint16_t               aluOut);

    // Execute an IOXIn read for the current click.
    // Returns the 16-bit value to place on the X-bus.
    uint16_t ExecuteIOXIn(const Microinstruction& mi,
                          TaskContext&            ctx);

    // Mesa interrupt-request flag (read by NiaEngine for MesaIntBr).
    bool MesaIntRq() const { return _mesaIntRq; }
    void SetMesaIntRq(bool rq) { _mesaIntRq = rq; }

private:
    DSystem* _system;
    bool     _mesaIntRq{false};
    uint16_t _iopOutputData{0};
    uint16_t _iopCtl{0};
};
