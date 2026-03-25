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

// IOP-facing port lists
const std::vector<int> CpIo::_readPorts = {
    0xeb,  // CPDataIn
    0xec,  // CPStatus
    0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd,  // CS microcode word bytes 0-5
    0xfe,  // TPC high
    0xff,  // TPC low
};
const std::vector<int> CpIo::_writePorts = {
    0xeb,  // CPDataOut
    0xec,  // CPControl
    0xee,  // CPClrDmaComplete
    0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd,  // CS microcode word bytes a-f
    0xfe,  // TPC high
    0xff,  // TPC low
};

CpIo::CpIo(DSystem* system)
    : _system(system)
{
    _tpc.fill(0);
    _tc.fill(0);
    _microcode.fill(0);
    Reset();
}

void CpIo::Reset()
{
    _mesaIntRq     = false;
    _iopOutputData = 0;
    _iopCtl        = 0;

    // CP↔IOP communication state – mirrors C# CentralProcessorIO defaults
    // All C# bools default to false; active-low flags therefore begin asserted.
    _cpOutIntReq_   = false;
    _cpInIntReq_    = false;
    _cpDmaComplete_ = false;
    _cpDmaMode      = false;
    _cpDmaIn        = false;
    _cpAttn         = false;
    _emuWake        = false;
    _wakeMode0      = false;
    _wakeMode1      = false;
    _iopReq         = false;
    _iopAttn        = false;
    _inLatched      = false;
    _outLatched     = false;
    _cpInData       = 0;
    _cpOutData      = 0;
    _tpcAddr        = 0;
    _tpcTemp        = 0;
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
            WriteCPInBuffer(static_cast<uint8_t>(aluOut & 0xff));
            break;

        case YIOOutFunction::IOPCtl:
            _iopCtl = aluOut;
            WriteIOPCtl(static_cast<uint8_t>(aluOut & 0xff));
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

        case ZIOXIn::ReadIOPIData:
            return static_cast<uint16_t>(ReadIOPData());

        case ZIOXIn::ReadIOPStatus:
            return static_cast<uint16_t>(ReadIOPStatus());

        default:
            return 0;
    }
}

// ---------------------------------------------------------------------------
// IIOPDevice implementation – IOP 8085 port access
// ---------------------------------------------------------------------------

const std::vector<int>& CpIo::ReadPorts() const  { return _readPorts; }
const std::vector<int>& CpIo::WritePorts() const { return _writePorts; }

void CpIo::WritePort(int port, uint8_t value)
{
    switch (port)
    {
        case 0xeb:  // CPDataOut – IOP writes data for CP to read
            WriteCPInBuffer(value);
            break;

        case 0xec:  // CPControl – IOP control word
            WriteIOPCtl(value);
            break;

        case 0xee:  // CPClrDmaComplete
            _cpDmaComplete_ = false;
            break;

        case 0xf8: case 0xf9: case 0xfa:
        case 0xfb: case 0xfc: case 0xfd:
            // CS microcode word bytes a–f (complemented values from IOP)
            WriteIOPMicrocodeWord(port - 0xf8, static_cast<uint8_t>(~value));
            break;

        case 0xfe:  // TPC high : TPCAddr[0:2],,TPCData[0:4]'
            _tpcAddr = value >> 5;
            _tpcTemp = ((~value & 0x1f) << 7);
            break;

        case 0xff:  // TPC low : don't care,,TPCData[5:11]'
            if (_tpcAddr < static_cast<int>(_tpc.size()))
                _tpc[_tpcAddr] = _tpcTemp | (~value & 0x7f);
            break;

        default:
            break;
    }
}

uint8_t CpIo::ReadPort(int port)
{
    switch (port)
    {
        case 0xeb:  // CPDataIn – IOP reads data the CP sent
            return ReadCPOutBuffer();

        case 0xec:  // CPStatus
            return ReadCPStatus();

        case 0xf8: case 0xf9: case 0xfa:
        case 0xfb: case 0xfc: case 0xfd:
            return ReadIOPMicrocodeWord(port - 0xf8);

        case 0xfe:  // TPC high : TC[0:3],,TPCData[0:3]'
            if (_tpcAddr < static_cast<int>(_tpc.size()))
                return static_cast<uint8_t>(
                    ~((~_tc[_tpcAddr] << 4) | ((_tpc[_tpcAddr] & 0xf00) >> 8)));
            return 0xff;

        case 0xff:  // TPC low : TPCData[4:11]'
            if (_tpcAddr < static_cast<int>(_tpc.size()))
                return static_cast<uint8_t>(~_tpc[_tpcAddr]);
            return 0xff;

        default:
            return 0;
    }
}

// ---------------------------------------------------------------------------
// IDMAInterface implementation
// ---------------------------------------------------------------------------

bool CpIo::DRQ()
{
    if (_cpDmaMode)
    {
        if (_cpDmaIn)
            return _outLatched;   // CP has output data waiting for IOP to DMA-read
        else
            return !_inLatched;   // IOP→CP buffer is empty: CP wants more data via DMA
    }
    return false;
}

void CpIo::DMAWrite(uint8_t value)
{
    WriteCPInBuffer(value);
}

uint8_t CpIo::DMARead()
{
    return ReadCPOutBuffer();
}

void CpIo::DMAComplete()
{
    _cpDmaComplete_ = true;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

uint8_t CpIo::ReadCPOutBuffer()
{
    _outLatched = false;
    // Signal IOP: CP has read the data (active-low flag goes inactive)
    _cpInIntReq_ = true;
    return _cpOutData;
}

void CpIo::WriteCPInBuffer(uint8_t value)
{
    // Signal IOP→CP data pending (active-low flag goes active)
    _cpOutIntReq_ = true;
    _cpOutData    = value;
    _inLatched    = true;
}

uint8_t CpIo::ReadIOPData()
{
    _inLatched    = false;
    // CP has read data; raise active-low flag to say buffer is clear
    _cpOutIntReq_ = false;
    return _cpInData;
}

uint8_t CpIo::ReadCPStatus() const
{
    return static_cast<uint8_t>(
        (_cpDmaComplete_              ? static_cast<uint8_t>(CPStatusFlags::CPDmaComplete_) : 0) |
        (!_cpOutIntReq_               ? static_cast<uint8_t>(CPStatusFlags::CPOutIntReq_)  : 0) |
        (!_cpInIntReq_                ? static_cast<uint8_t>(CPStatusFlags::CPInIntReq_)   : 0) |
        (!_cpDmaIn                    ? static_cast<uint8_t>(CPStatusFlags::CPDmaIn_)      : 0) |
        (!_cpDmaMode                  ? static_cast<uint8_t>(CPStatusFlags::CPDmaMode_)    : 0) |
        (_emuWake                     ? static_cast<uint8_t>(CPStatusFlags::EmuWake)       : 0) |
        (!_cpAttn                     ? static_cast<uint8_t>(CPStatusFlags::CPAttn)        : 0));
}

void CpIo::WriteIOPCtl(uint8_t value)
{
    _wakeMode1 = (value & static_cast<uint8_t>(IOPCtlFlags::WakeMode1)) != 0;
    _wakeMode0 = (value & static_cast<uint8_t>(IOPCtlFlags::WakeMode0)) != 0;
    _cpAttn    = (value & static_cast<uint8_t>(IOPCtlFlags::CPAttn))    != 0;
    _emuWake   = (value & static_cast<uint8_t>(IOPCtlFlags::EmuWake))   != 0;
}

uint8_t CpIo::ReadIOPStatus() const
{
    return static_cast<uint8_t>(
        (_iopReq    ? static_cast<uint8_t>(IOPStatusFlags::IOPReq)    : 0) |
        (!_wakeMode1? static_cast<uint8_t>(IOPStatusFlags::WakeMode1_): 0) |
        (!_wakeMode0? static_cast<uint8_t>(IOPStatusFlags::WakeMode0_): 0) |
        (!_cpAttn   ? static_cast<uint8_t>(IOPStatusFlags::CPAttn_)   : 0) |
        (!_emuWake  ? static_cast<uint8_t>(IOPStatusFlags::EmuWake_)  : 0) |
        (_iopAttn   ? static_cast<uint8_t>(IOPStatusFlags::IOPAttn)   : 0));
}

void CpIo::WriteIOPMicrocodeWord(int b, uint8_t value)
{
    // TPC register 6 is always used for IOP microcode writes.
    int tpc6 = _tpc[6];
    if (tpc6 < 0 || tpc6 >= static_cast<int>(_microcode.size())) return;
    uint64_t word = _microcode[tpc6];
    // Bytes are ordered: CSa=byte0 (MSB bits 47-40) … CSf=byte5 (LSB bits 7-0)
    // From SysDefs.asm:
    //   CSa: rA[0:3],,rB[0:3]
    //   CSb: aS[0:2],,aF[0:2],,aD[0:1]
    //   CSc: EP,,CIN,,EnSU,,mem,,fS[0:3]
    //   CSd: fY[0:3], INIA[0:3]
    //   CSe: fX[0:3], INIA[4:7]
    //   CSf: fZ[0:3], INIA[8:11]
    int shift = (5 - b) * 8;
    word = (word & ~(static_cast<uint64_t>(0xff) << shift)) |
           (static_cast<uint64_t>(value) << shift);
    _microcode[tpc6] = word;
}

uint8_t CpIo::ReadIOPMicrocodeWord(int b) const
{
    int tpc6 = _tpc[6];
    if (tpc6 < 0 || tpc6 >= static_cast<int>(_microcode.size())) return 0;
    int shift = (5 - b) * 8;
    return static_cast<uint8_t>((_microcode[tpc6] >> shift) & 0xff);
}
