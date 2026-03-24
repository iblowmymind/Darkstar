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

#include "central_processor.h"
#include <cstring>

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------

CentralProcessor::CentralProcessor()
    : _mem(nullptr)
    , _io(nullptr)
{
    memset(_wakeup, 0, sizeof(_wakeup));
}

CentralProcessor::CentralProcessor(DSystem* system)
    : _mem(system)
    , _io(system)
    , _system(system)
{
    memset(_wakeup, 0, sizeof(_wakeup));
}

void CentralProcessor::SetSystem(DSystem* system)
{
    _system = system;
}

// ---------------------------------------------------------------------------
// Reset
// ---------------------------------------------------------------------------

void CentralProcessor::Reset()
{
    _controlStore.Reset();
    _alu.Reset();
    _mem.Reset();
    _io.Reset();

    for (int i = 0; i < CP_TASK_COUNT; ++i)
        _ctx[i] = TaskContext();

    memset(_wakeup, 0, sizeof(_wakeup));
    _currentTask = TaskType::Emulator;
}

// ---------------------------------------------------------------------------
// WakeTask / SleepTask
// ---------------------------------------------------------------------------

void CentralProcessor::WakeTask(TaskType task)
{
    _wakeup[static_cast<int>(task)] = true;
}

void CentralProcessor::SleepTask(TaskType task)
{
    _wakeup[static_cast<int>(task)] = false;
}

// ---------------------------------------------------------------------------
// SelectNextTask
//
// Scan TaskType::Emulator(0) .. TaskType::Kernel(7).
// Return the first awake task, or _currentTask if none are awake.
// ---------------------------------------------------------------------------

TaskType CentralProcessor::SelectNextTask()
{
    for (int i = 0; i < CP_TASK_COUNT; ++i)
    {
        if (_wakeup[i])
            return static_cast<TaskType>(i);
    }
    return _currentTask;
}

// ---------------------------------------------------------------------------
// Clock – execute one microcode click (~137 ns emulated time)
//
// Full 13-step execution plan (to be implemented in a future session):
//  1.  _currentTask = SelectNextTask().
//  2.  TaskContext& ctx = _ctx[static_cast<int>(_currentTask)].
//  3.  Microinstruction mi = _controlStore.At(_currentTask, ctx.uPC).
//  4.  X-bus mux: xBus = mi.AluNeedsXBus ? ctx.T : 0;
//                 if (mi.fSfY==Byte || mi.fSfZ==Nibble) xBus = mi.Byte;
//  5.  bool cin = mi.LoadCinFrompc16 ? ctx.cin16 : mi.Cin;
//  6.  uint16_t aluOut = _alu.Execute(mi, ctx, xBus, cin).
//  7.  if (mi.fSfZ==IOXIn) xBus = _io.ExecuteIOXIn(mi, ctx).
//  8.  uint16_t mdWord = _mem.ExecuteMem(mi, ctx, _currentTask, aluOut).
//  9.  HandleXFunction(mi, ctx, aluOut).
// 10.  HandleYFunction(mi, ctx, aluOut).
// 11.  HandleZFunction(mi, ctx, xBus).
// 12.  xBus = _alu.ApplyLRot(mi, xBus).
// 13.  ctx.uPC = _nia.Compute(mi, ctx, _alu.Alu(), xBus, _io.MesaIntRq()).
// 14.  ctx.T = xBus.
// ---------------------------------------------------------------------------

void CentralProcessor::Clock()
{
    // TODO: implement full execution per plan above.
    _currentTask = SelectNextTask();
    TaskContext& ctx = _ctx[static_cast<int>(_currentTask)];
    ctx.uPC = (ctx.uPC + 1) & (CP_CS_SIZE - 1);
}

// ---------------------------------------------------------------------------
// HandleXFunction
//
// Implementation plan for a future session:
//   pCallRet0..7: push INIA onto callStack; jump to LinkAddress.
//   pop:          pop callStack (set StackUnderflow on empty stack).
//   push:         push uPC (set StackOverflow on full stack).
//   LoadCinFrompc16: ctx.cin16 = (aluOut >> 15) & 1.
//   LoadMap:      _mem.LoadMap(mi.rB, aluOut).
//   LoadRH/shift/cycle/Noop: handled elsewhere or noop.
// ---------------------------------------------------------------------------

void CentralProcessor::HandleXFunction(const Microinstruction& mi,
                                        TaskContext&            ctx,
                                        uint16_t               aluOut)
{
    // TODO: implement per plan above.
    (void)mi; (void)ctx; (void)aluOut;
}

// ---------------------------------------------------------------------------
// HandleYFunction
//
// Implementation plan for a future session:
//   fyNorm → YNormFunction:
//     ExitKern:    _mem.SetKernelMode(false); _mem.SetMarpEnable(true).
//     EnterKern:   _mem.SetKernelMode(true);  _mem.SetMarpEnable(true).
//     ClrIntErr:   _io.SetMesaIntRq(false).
//     MesaIntRq:   _io.SetMesaIntRq(true).
//     LoadstackP:  ctx.stackPointer = aluOut & (CP_CALL_STACK_DEPTH-1).
//     LoadIB:      fetch IB bytes from memory, set ibState.
//     ClrDPRq:     _system->GetDisplayController()->ClrDpRq().
//     ClrIOPRq:    SleepTask(TaskType::IOP).
//     ClrRefRq:    SleepTask(TaskType::Refresh).
//     Refresh/LoadMap/push/cycle/Noop: handled elsewhere or noop.
//   DispBr → side-effects only; branch computed in NiaEngine.
//   IOOut  → _io.ExecuteIOOut(mi, ctx, aluOut).
//   Byte   → constant already on xBus from mux; noop here.
// ---------------------------------------------------------------------------

void CentralProcessor::HandleYFunction(const Microinstruction& mi,
                                        TaskContext&            ctx,
                                        uint16_t               aluOut)
{
    if (mi.fSfY == FunctionSelectFY::IOOut)
    {
        _io.ExecuteIOOut(mi, ctx, aluOut);
        return;
    }
    // TODO: implement remaining cases per plan above.
    (void)mi; (void)ctx; (void)aluOut;
}

// ---------------------------------------------------------------------------
// HandleZFunction
//
// Implementation plan for a future session:
//   fzNorm → ZNormFunction:
//     LoadIBPtr1:      ctx.ibPtr = 1.
//     LoadIBPtr0:      ctx.ibPtr = 0.
//     LoadCinFrompc16: ctx.cin16 = (xBus >> 15) & 1.
//     LoadBank:        set address-map bank bits.
//     AltUaddr:        ctx.uPC = mi.UAddress.
//     pop/push:        handled cooperatively with HandleXFunction.
//     LRot0/4/8/12:    handled in CpAlu::ApplyLRot; noop here.
//     Refresh/Noop0..3: noop.
//   Nibble → xBus = mi.Byte (already set in mux; noop here).
//   Uaddr  → ctx.uPC = mi.UAddress.
//   IOXIn  → handled earlier; noop here.
// ---------------------------------------------------------------------------

void CentralProcessor::HandleZFunction(const Microinstruction& mi,
                                        TaskContext&            ctx,
                                        uint16_t&              xBus)
{
    // TODO: implement per plan above.
    (void)mi; (void)ctx; (void)xBus;
}
