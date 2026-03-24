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
// central_processor.h  –  Xerox Star Central Processor
//
// The Star CP is a 16-bit microcoded bit-slice processor built from four AMD
// AM2901 4-bit ALU slices.  It runs 8 co-operative tasks; in each "click"
// (~137 ns, one Scheduler time step) the highest-priority awake task executes
// exactly one 48-bit microinstruction.
//
// Sub-module ownership
// --------------------
//   ControlStore    – 8 × 1024 microcode ROMs (load, patch, decode).
//   CpAlu           – AM2901 wrapper with X-bus mux, SU register, LRot.
//   NiaEngine       – Next-Instruction-Address: normal/dispatch/pCall/ret.
//   CpMemInterface  – MAR/MDR/MD sequence, Map translation, error traps.
//   CpIo            – IOOut write side + IOXIn read side for all peripherals.
//
// Per-task state
// --------------
//   TaskContext[CP_TASK_COUNT]  – uPC, call stack, IB, T/L/RH, errorFlags.
//   _wakeup[CP_TASK_COUNT]      – task wakeup flags.
//
// Execution sequence per click (Clock)
// ------------------------------------
//  1.  SelectNextTask()      – highest-priority awake task wins.
//  2.  Decode MI             – ControlStore::At(task, uPC).
//  3.  X-bus source mux      – T, Byte constant, or 0.
//  4.  Carry-in resolve      – mi.Cin or ctx.cin16 if LoadCinFrompc16.
//  5.  CpAlu::Execute()      – run AM2901 (+ SU read/write + LoadRH).
//  6.  IOXIn read            – if fSfZ==IOXIn, xBus ← CpIo::ExecuteIOXIn().
//  7.  CpMem::ExecuteMem()   – MAR← / MDR← / ←MD as required.
//  8.  HandleXFunction()     – pCallRet, LoadRH, shift/cycle,
//                              LoadCinFrompc16, LoadMap, pop, push.
//  9.  HandleYFunction()     – YNorm / YDispBr / YIOOut / YByte.
// 10.  HandleZFunction()     – ZNorm / ZNibble / ZUaddr / ZIOXIn.
// 11.  ApplyLRot()           – late left-rotation on xBus result.
// 12.  NiaEngine::Compute()  – new uPC.
// 13.  ctx.T ← xBus          – latch result for the next click.
//
// Ported from D/CP/CentralProcessor.cs.
// ---------------------------------------------------------------------------

#include "../types.h"
#include "cp_types.h"
#include "control_store.h"
#include "cp_alu.h"
#include "cp_nia.h"
#include "cp_mem.h"
#include "cp_io.h"
#include "microinstruction.h"

class DSystem;

class CentralProcessor
{
public:
    CentralProcessor();
    explicit CentralProcessor(DSystem* system);
    ~CentralProcessor() = default;

    void SetSystem(DSystem* system);
    void Reset();

    // Execute one microcode click (~137 ns emulated time).
    void Clock();

    // Wake / sleep the specified task.
    void WakeTask(TaskType task);
    void SleepTask(TaskType task);

    // Control-store access (used by IOP to upload/patch microcode).
    ControlStore& GetControlStore() { return _controlStore; }

    // Diagnostics / debug.
    TaskType           CurrentTask()          const { return _currentTask; }
    int                CurrentUPC()           const { return _ctx[static_cast<int>(_currentTask)].uPC; }
    const TaskContext& Context(TaskType t)    const { return _ctx[static_cast<int>(t)]; }
    bool               TaskWakeup(TaskType t) const { return _wakeup[static_cast<int>(t)]; }

private:
    TaskType SelectNextTask();

    void HandleXFunction(const Microinstruction& mi, TaskContext& ctx, uint16_t aluOut);
    void HandleYFunction(const Microinstruction& mi, TaskContext& ctx, uint16_t aluOut, uint16_t mdWord);
    void HandleZFunction(const Microinstruction& mi, TaskContext& ctx, uint16_t& xBus);

    ControlStore   _controlStore;
    CpAlu          _alu;
    NiaEngine      _nia;
    CpMemInterface _mem;
    CpIo           _io;

    TaskContext    _ctx[CP_TASK_COUNT];
    bool           _wakeup[CP_TASK_COUNT]{};
    TaskType       _currentTask{TaskType::Emulator};
    DSystem*       _system{nullptr};
};
