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
// cp_mem.h  –  Central-Processor memory-bus interface
//
// Encapsulates the three-click MAR/MDR/MD memory sequence, the 64-entry
// virtual-to-physical address map, and memory error trap handling.
//
// Memory sequence (mi.MarMapMDR == true gates the whole trio):
//   Click 1 – MAR← : translate address, call MemoryController::LoadMAR().
//   Click 2 – MDR← : commit write, call MemoryController::LoadMDR(aluOut).
//   Click 3 – ←MD  : return prefetched word via MemoryController::ReadMD();
//                     set ErrorTrap::EmulatorMemoryError in ctx.errorFlags on fault.
//
// Address Map (64 entries, 6-bit virtual page → 10-bit physical page):
//   LoadMap() writes one entry: _map[mapIndex & 0x3F] = value.
//   Translate(): if _marpEnable, physAddr = (_map[virt>>10]&0x3FF)<<10 | (virt&0x3FF).
//
// Ported from D/CP/CentralProcessor.cs (memory + map sections).
// ---------------------------------------------------------------------------

#include "cp_types.h"
#include "microinstruction.h"
#include "../memory.h"

class DSystem;

class CpMemInterface
{
public:
    explicit CpMemInterface(DSystem* system);

    void Reset();

    // Execute memory operations for the current click.
    //   mi      – decoded microinstruction.
    //   ctx     – mutable per-task context (errorFlags updated on bad MD).
    //   task    – current task (for MemoryController error logging).
    //   aluOut  – MAR address (click 1) or MDR write data (click 2).
    // Returns the MD word on click 3 (←MD), 0 otherwise.
    uint16_t ExecuteMem(const Microinstruction& mi,
                        TaskContext&            ctx,
                        TaskType                task,
                        uint16_t               aluOut);

    // Write one address-map entry (when mi.LoadMap is true).
    //   mapIndex – rB register address masked to 0x3F.
    //   value    – 16-bit word from ALU F output.
    void LoadMap(int mapIndex, uint16_t value);

    // Translate a virtual word address to a physical address.
    int Translate(int virtualAddress) const;

    void SetMarpEnable(bool enable) { _marpEnable = enable; }
    void SetKernelMode(bool kernel) { _kernelMode = kernel; }
    bool MarpEnable() const { return _marpEnable; }
    bool KernelMode() const { return _kernelMode; }

private:
    DSystem*  _system;
    uint16_t  _map[CP_MAP_SIZE]{};
    bool      _marpEnable{false};
    bool      _kernelMode{false};
    int       _memClick{0};
};
