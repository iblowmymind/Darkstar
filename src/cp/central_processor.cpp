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
#include "../dsystem.h"
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
// 10.  HandleYFunction(mi, ctx, aluOut, mdWord).
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
// Handles the fX-encoded operations that are not covered by the NIA engine
// or CpAlu:
//
//   pCallRet0..7 – NiaEngine pushes INIA and returns LinkAddress.
//                  HandleXFunction only checks for stack overflow.
//   pop          – NiaEngine pops and returns the saved address (SJump).
//                  HandleXFunction only checks for stack underflow.
//   push         – Push ctx.uPC as a return address; check stack overflow.
//   LoadCinFrompc16 – ctx.cin16 = (aluOut >> 15) & 1.
//   LoadMap      – _mem.LoadMap(mi.rB, aluOut). Also triggered by fY==LoadMap.
//   LoadRH / shift / cycle / Noop – handled by CpAlu or are no-ops here.
// ---------------------------------------------------------------------------

void CentralProcessor::HandleXFunction(const Microinstruction& mi,
                                        TaskContext&            ctx,
                                        uint16_t               aluOut)
{
    // pCallRet0..7: NIA engine handles the push and jump.
    // Set StackOverflow if the stack was full at that point.
    if (mi.LinkAddress >= 0)
    {
        if (ctx.stackPointer >= CP_CALL_STACK_DEPTH)
            ctx.errorFlags |= static_cast<uint8_t>(
                1 << static_cast<int>(ErrorTrap::StackOverflow));
    }
    else
    {
        switch (mi.fX)
        {
            case XFunction::pop:
                // SJump: NIA engine pops and returns the saved address.
                // Just set StackUnderflow if the stack is empty.
                if (ctx.stackPointer == 0)
                    ctx.errorFlags |= static_cast<uint8_t>(
                        1 << static_cast<int>(ErrorTrap::StackUnderflow));
                break;

            case XFunction::push:
                // Save the current uPC as a return address.
                if (ctx.stackPointer >= CP_CALL_STACK_DEPTH)
                    ctx.errorFlags |= static_cast<uint8_t>(
                        1 << static_cast<int>(ErrorTrap::StackOverflow));
                else
                    ctx.callStack[ctx.stackPointer++] = ctx.uPC;
                break;

            case XFunction::LoadCinFrompc16:
                ctx.cin16 = ((aluOut >> 15) & 1) != 0;
                break;

            default:
                break;
        }
    }

    // LoadMap is asserted by fX == LoadMap or fY == LoadMap (mi.LoadMap
    // aggregates both sources).  Check it independently of the pCallRet path
    // because fY == LoadMap can co-occur with fX == pCallRet.
    if (mi.LoadMap)
        _mem.LoadMap(mi.rB, aluOut);
}

// ---------------------------------------------------------------------------
// HandleYFunction
//
// Dispatches the fSfY-encoded function for the current click.
//
//   IOOut  → _io.ExecuteIOOut(mi, ctx, aluOut).
//   Byte   → constant already on xBus from mux; noop here.
//   DispBr → branch is computed in NiaEngine; noop here.
//   fyNorm → dispatch on YNormFunction (mi.fY):
//     ExitKern:    _mem.SetKernelMode(false); _mem.SetMarpEnable(true).
//     EnterKern:   _mem.SetKernelMode(true);  _mem.SetMarpEnable(true).
//     ClrIntErr:   _io.SetMesaIntRq(false).
//     MesaIntRq:   _io.SetMesaIntRq(true).
//     LoadstackP:  ctx.stackPointer = aluOut & (CP_CALL_STACK_DEPTH-1).
//     LoadIB:      fill IB from mdWord (memory-fetch result); update ibState.
//     ClrDPRq:     _system->GetDisplayController()->ClrDpRq().
//     ClrIOPRq:    SleepTask(TaskType::IOP).
//     ClrRefRq:    SleepTask(TaskType::Refresh).
//     ClrKFlags:   ctx.errorFlags = 0.
//     IBDisp/cycle/LoadMap/push/Refresh/Noop: handled elsewhere or noop.
// ---------------------------------------------------------------------------

void CentralProcessor::HandleYFunction(const Microinstruction& mi,
                                        TaskContext&            ctx,
                                        uint16_t               aluOut,
                                        uint16_t               mdWord)
{
    if (mi.fSfY == FunctionSelectFY::IOOut)
    {
        _io.ExecuteIOOut(mi, ctx, aluOut);
        return;
    }

    // Byte: constant is already on xBus from the mux; no side effects here.
    // DispBr: branch is computed in NiaEngine; no side effects here.
    if (mi.fSfY != FunctionSelectFY::fyNorm)
        return;

    switch (static_cast<YNormFunction>(mi.fY))
    {
        case YNormFunction::ExitKern:
            _mem.SetKernelMode(false);
            _mem.SetMarpEnable(true);
            break;

        case YNormFunction::EnterKern:
            _mem.SetKernelMode(true);
            _mem.SetMarpEnable(true);
            break;

        case YNormFunction::ClrIntErr:
            _io.SetMesaIntRq(false);
            break;

        case YNormFunction::MesaIntRq:
            _io.SetMesaIntRq(true);
            break;

        case YNormFunction::LoadstackP:
            ctx.stackPointer = aluOut & (CP_CALL_STACK_DEPTH - 1);
            break;

        case YNormFunction::LoadIB:
        {
            // Load the memory-fetch word into the IB.
            // Fill the first slot (ib[0..1]) if the IB is not yet Full,
            // otherwise fill the second slot (ib[2..3]).
            if (ctx.ibState < IBState::Full)
            {
                ctx.ib[0]   = static_cast<uint8_t>(mdWord >> 8);
                ctx.ib[1]   = static_cast<uint8_t>(mdWord & 0xFF);
                ctx.ibPtr   = 0;
                ctx.ibState = IBState::Full;
            }
            else
            {
                ctx.ib[2]   = static_cast<uint8_t>(mdWord >> 8);
                ctx.ib[3]   = static_cast<uint8_t>(mdWord & 0xFF);
                ctx.ibState = IBState::Word;
            }
            break;
        }

        case YNormFunction::ClrDPRq:
            if (_system && _system->GetDisplayController())
                _system->GetDisplayController()->ClrDpRq();
            break;

        case YNormFunction::ClrIOPRq:
            SleepTask(TaskType::IOP);
            break;

        case YNormFunction::ClrRefRq:
            SleepTask(TaskType::Refresh);
            break;

        case YNormFunction::ClrKFlags:
            ctx.errorFlags = 0;
            break;

        // IBDisp: dispatch is handled by NiaEngine (AlwaysIBDisp); noop here.
        // cycle:  handled by CpAlu; noop here.
        // LoadMap: handled by HandleXFunction; noop here.
        // push:   handled by HandleXFunction; noop here.
        // Refresh / Noop: noop.
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// HandleZFunction
//
// Dispatches the fSfZ-encoded function for the current click.
//
//   Uaddr  → ctx.uPC = mi.UAddress (direct uPC load from the UAddress field).
//   Nibble → constant already on xBus from mux; noop here.
//   IOXIn  → handled earlier in the clock cycle; noop here.
//   fzNorm → dispatch on ZNormFunction (mi.fZ):
//     LoadIBPtr1:      ctx.ibPtr = 1.
//     LoadIBPtr0:      ctx.ibPtr = 0.
//     LoadCinFrompc16: ctx.cin16 = (xBus >> 15) & 1.
//     LoadBank:        noop (bank switching not yet specified).
//     AltUaddr:        ctx.uPC = mi.UAddress.
//     pop/push:        handled cooperatively with HandleXFunction; noop here.
//     LRot0/4/8/12:    handled in CpAlu::ApplyLRot; noop here.
//     Refresh/Noop0..3: noop.
// ---------------------------------------------------------------------------

void CentralProcessor::HandleZFunction(const Microinstruction& mi,
                                        TaskContext&            ctx,
                                        uint16_t&              xBus)
{
    // Uaddr: direct uPC load from the combined (rA<<4)|fZ address field.
    if (mi.fSfZ == FunctionSelectFZ::Uaddr)
    {
        ctx.uPC = mi.UAddress & (CP_CS_SIZE - 1);
        return;
    }

    // Nibble: constant is already on xBus from the mux; noop here.
    // IOXIn:  handled earlier in the clock cycle; noop here.
    if (mi.fSfZ != FunctionSelectFZ::fzNorm)
        return;

    switch (static_cast<ZNormFunction>(mi.fZ))
    {
        case ZNormFunction::LoadIBPtr1:
            ctx.ibPtr = 1;
            break;

        case ZNormFunction::LoadIBPtr0:
            ctx.ibPtr = 0;
            break;

        case ZNormFunction::LoadCinFrompc16:
            ctx.cin16 = ((xBus >> 15) & 1) != 0;
            break;

        case ZNormFunction::AltUaddr:
            ctx.uPC = mi.UAddress & (CP_CS_SIZE - 1);
            break;

        // LoadBank: bank switching is not yet specified; noop.
        // pop/push: handled cooperatively with HandleXFunction.
        // LRot0/4/8/12: handled in CpAlu::ApplyLRot; noop here.
        // Refresh / Noop0..3: noop.
        default:
            break;
    }

    (void)xBus;  // xBus is an in/out; no Z-function modifies it directly here.
}
