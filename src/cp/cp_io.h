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
//   ReadIOPIData       → IOP→CP data byte
//   ReadIOPStatus      → IOP status register
//   ReadKIData / ReadKStatus / KStrobe → TODO: keyboard
//   ReadEIdata / ReadEStatus / EStrobe → TODO: ethernet
//   ReadKTest          → TODO: keyboard test mode
//
// IOP-facing interface (IIOPDevice + IDMAInterface):
// --------------------------------------------------------------------------
//   Port 0xeb read  → CPDataIn  (CP→IOP data byte)
//   Port 0xec read  → CPStatus  (CP status flags)
//   Port 0xf8-0xfd read → CS microcode word bytes 0-5
//   Port 0xfe read  → TPC high
//   Port 0xff read  → TPC low
//   Port 0xeb write → CPDataOut  (IOP→CP data byte)
//   Port 0xec write → CPControl  (IOP control word)
//   Port 0xee write → CPClrDmaComplete
//   Port 0xf8-0xfd write → CS microcode word bytes a-f
//   Port 0xfe write → TPC high
//   Port 0xff write → TPC low
//
// Ported from D/CP/CentralProcessor.cs (I/O read/write sections).
// ---------------------------------------------------------------------------

#include "cp_types.h"
#include "microinstruction.h"
#include "../iop/iop_device.h"
#include "../iop/dma_controller.h"
#include <cstdint>
#include <array>
#include <vector>

class DSystem;

// CP status register flags (returned on port 0xec read, active-low where named with _)
enum class CPStatusFlags : uint8_t {
    CPAttn        = 0x80,
    EmuWake       = 0x40,
    IOPAttn_      = 0x20,
    CPDmaMode_    = 0x10,
    CPDmaIn_      = 0x08,
    CPInIntReq_   = 0x04,
    CPOutIntReq_  = 0x02,
    CPDmaComplete_= 0x01,
};

// IOP control register flags (written to port 0xec)
enum class IOPCtlFlags : uint8_t {
    EmuWake   = 0x8,
    CPAttn    = 0x4,
    WakeMode0 = 0x2,
    WakeMode1 = 0x1,
};

// IOP status register flags (readable by CP via IOXIn ReadIOPStatus)
enum class IOPStatusFlags : uint8_t {
    IOPAttn   = 0x20,
    EmuWake_  = 0x10,
    CPAttn_   = 0x08,
    WakeMode0_= 0x04,
    WakeMode1_= 0x02,
    IOPReq    = 0x01,
};

class CpIo : public IIOPDevice, public IDMAInterface
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

    // IIOPDevice interface – IOP-side port access
    const std::vector<int>& ReadPorts()  const override;
    const std::vector<int>& WritePorts() const override;
    void    WritePort(int port, uint8_t value) override;
    uint8_t ReadPort(int port)            override;

    // IDMAInterface – used by DMA controller for CP DMA transfers
    bool    DRQ()                  override;
    void    DMAWrite(uint8_t value) override;
    uint8_t DMARead()              override;
    void    DMAComplete()          override;

private:
    // CP↔IOP data transfer helpers
    uint8_t ReadCPOutBuffer();
    void    WriteCPInBuffer(uint8_t value);
    uint8_t ReadCPStatus() const;
    void    WriteIOPCtl(uint8_t value);
    uint8_t ReadIOPStatus() const;
    uint8_t ReadIOPData();      // CP reads IOP→CP data latch

    // Microcode loading helpers
    void    WriteIOPMicrocodeWord(int b, uint8_t value);
    uint8_t ReadIOPMicrocodeWord(int b) const;

    DSystem* _system;
    bool     _mesaIntRq{false};
    uint16_t _iopOutputData{0};
    uint16_t _iopCtl{0};

    // CP↔IOP communication state
    bool    _cpOutIntReq_{false};   // active-low; false = CP output data available for IOP
    bool    _cpInIntReq_{false};    // active-low; false = IOP data pending for CP
    bool    _cpDmaComplete_{false};
    bool    _cpDmaMode{false};
    bool    _cpDmaIn{false};
    bool    _cpAttn{false};
    bool    _emuWake{false};
    bool    _wakeMode0{false};
    bool    _wakeMode1{false};
    bool    _iopReq{false};
    bool    _iopAttn{false};

    // Data latches
    bool    _inLatched{false};   // IOP→CP data is latched
    bool    _outLatched{false};  // CP→IOP data is latched
    uint8_t _cpInData{0};        // data written by IOP for CP to read
    uint8_t _cpOutData{0};       // data written by CP for IOP to read

    // Microcode loading state
    int  _tpcAddr{0};
    int  _tpcTemp{0};
    std::array<int, 8>      _tpc{};
    std::array<int, 8>      _tc{};
    std::array<uint64_t, 1024> _microcode{};

    static const std::vector<int> _readPorts;
    static const std::vector<int> _writePorts;
};
