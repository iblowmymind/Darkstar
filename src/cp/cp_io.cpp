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

#include "cp_io.h"
#include "../dsystem.h"
#include "../display_controller.h"
#include "../memory.h"

CpIo::CpIo(DSystem* system)
    : _system(system)
{
    Reset();
}

void CpIo::Reset()
{
    _mesaIntRq     = false;
    _iopOutputData = 0;
    _iopCtl        = 0;
}

// ---------------------------------------------------------------------------
// CpIo::ExecuteIOOut
//
// Dispatches a CP→peripheral write.  The target is selected by mi.fY
// interpreted as a YIOOutFunction.
//
//   DCtlFifo  → DisplayController::SetDCtlFifo(aluOut)
//   DCtl      → DisplayController::SetDCtl(aluOut)
//   DBorder   → DisplayController::SetDBorder(aluOut)
//   MCtl      → MemoryController::SetMCtl(aluOut)
//   IOPOData  → _iopOutputData = aluOut
//   IOPCtl    → _iopCtl = aluOut  (TODO: trigger RST7.5 on IOP)
//   KOData/KCtl/KCmd       → TODO: keyboard subsystem (silently ignored)
//   EOData/EICtl/EOCtl     → TODO: ethernet subsystem (silently ignored)
//   PCtl/POData            → TODO: printer subsystem   (silently ignored)
//   Invalid0/Invalid1      → silently ignored
// ---------------------------------------------------------------------------

void CpIo::ExecuteIOOut(const Microinstruction& mi,
                        TaskContext&            ctx,
                        uint16_t               aluOut)
{
    (void)ctx;  // ctx is not written by IOOut operations

    YIOOutFunction fn = static_cast<YIOOutFunction>(mi.fY);
    switch (fn)
    {
        case YIOOutFunction::DCtlFifo:
            if (_system && _system->GetDisplayController())
                _system->GetDisplayController()->SetDCtlFifo(aluOut);
            break;

        case YIOOutFunction::DCtl:
            if (_system && _system->GetDisplayController())
                _system->GetDisplayController()->SetDCtl(aluOut);
            break;

        case YIOOutFunction::DBorder:
            if (_system && _system->GetDisplayController())
                _system->GetDisplayController()->SetDBorder(aluOut);
            break;

        case YIOOutFunction::MCtl:
            if (_system && _system->GetMemoryController())
                _system->GetMemoryController()->SetMCtl(aluOut);
            break;

        case YIOOutFunction::IOPOData:
            _iopOutputData = aluOut;
            break;

        case YIOOutFunction::IOPCtl:
            _iopCtl = aluOut;
            // TODO: trigger RST7.5 interrupt on the IOP CPU.
            break;

        // Keyboard, ethernet, printer, and invalid function codes are
        // silently ignored until those subsystems are implemented.
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// CpIo::ExecuteIOXIn – partially implemented; full plan in cp_io.h
// ---------------------------------------------------------------------------

uint16_t CpIo::ExecuteIOXIn(const Microinstruction& mi,
                              TaskContext&            ctx)
{
    ZIOXIn fn = static_cast<ZIOXIn>(mi.fZ);
    switch (fn)
    {
        case ZIOXIn::ReadMStatus:
            return _system
                ? static_cast<uint16_t>(_system->GetMemoryController()->MStatus())
                : 0;

        case ZIOXIn::ReadErrnIBnStkp:
            return static_cast<uint16_t>(
                (static_cast<uint16_t>(ctx.errorFlags) << 8) |
                (static_cast<uint16_t>(static_cast<int>(ctx.ibState)) << 6) |
                (ctx.stackPointer & 0x3F));

        case ZIOXIn::ReadRH:
            return ctx.RH;

        case ZIOXIn::Readib:
            return static_cast<uint16_t>((ctx.ib[0] << 8) | ctx.ib[1]);

        case ZIOXIn::ReadibLow:
            return ctx.ib[1];

        case ZIOXIn::ReadibHigh:
            return ctx.ib[0];

        case ZIOXIn::ReadibNA:
            return static_cast<uint16_t>(ctx.ibPtr);

        default:
            return 0;
    }
}
