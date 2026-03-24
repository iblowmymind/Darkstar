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
// cp_types.h  –  Central-Processor private types shared across CP sub-modules
//
// This header is included by every CP sub-module (control_store, cp_alu,
// cp_nia, cp_mem, cp_io, central_processor).  It must not include any
// sub-module headers itself to avoid circular dependencies.
// ---------------------------------------------------------------------------

#include "../types.h"   // TaskType, IBState, ErrorTrap, etc.
#include "microinstruction.h"

#include <cstdint>
#include <cstring>

// ---------------------------------------------------------------------------
// CP constants
// ---------------------------------------------------------------------------

// Number of distinct task contexts (= TaskType::TaskCount).
static constexpr int CP_TASK_COUNT       = static_cast<int>(TaskType::TaskCount);

// Microcode words per task in the control store (12-bit uPC → 1024 entries).
static constexpr int CP_CS_SIZE          = 1024;

// Depth of the per-task microcode return stack.
static constexpr int CP_CALL_STACK_DEPTH = 8;

// Number of bytes in the Instruction Buffer (Mesa bytecode prefetch).
static constexpr int CP_IB_SIZE          = 4;

// Number of address-map entries (6-bit virtual page → physical page).
static constexpr int CP_MAP_SIZE         = 64;

// ---------------------------------------------------------------------------
// TaskContext
//
// All architectural state that is private to one task.  The CP saves and
// restores this block on every task switch (the real hardware uses banked
// register files; here we keep one struct per task).
//
// Ported from the per-task variables in D/CP/CentralProcessor.cs.
// ---------------------------------------------------------------------------
struct TaskContext
{
    // ---- Microcode program counter (12-bit, range 0..CP_CS_SIZE-1) --------
    int      uPC;

    // ---- Return / call stack (8 entries deep) -----------------------------
    // callStack[0] is the top (most-recently pushed) entry.
    // stackPointer is the number of valid entries (0 = empty).
    int      callStack[CP_CALL_STACK_DEPTH];
    int      stackPointer;

    // ---- Mesa Instruction Buffer (Mesa bytecode prefetch) -----------------
    // Up to 4 bytes of Mesa code fetched from memory.
    // ibPtr  : index of the next byte to dispatch (0..3).
    // ibState: how many valid bytes the buffer currently holds.
    uint8_t  ib[CP_IB_SIZE];
    int      ibPtr;
    IBState  ibState;

    // ---- Data-path latches ------------------------------------------------
    // T: the "T" (transfer) register – holds the ALU OE output from the
    //    previous click.  Used as the default X-bus source for the next click.
    uint16_t T;

    // L: "L" (latch) register – a second copy of the ALU output, used for
    //    address forwarding during multi-click memory sequences.
    uint16_t L;

    // RH: "right-half" 16-bit register.  LoadRH copies ALU F → RH;
    //     used to build 32-bit operands with the ALU's 16-bit output.
    uint16_t RH;

    // ---- Carry forwarding -------------------------------------------------
    // cin16: carry-out of bit 16 (bit 0 of the upper 16 bits) from the last
    //        32-bit arithmetic operation.  Loaded into Cin when
    //        Microinstruction::LoadCinFrompc16 is true.
    bool     cin16;

    // ---- Error flags (one bit per ErrorTrap value) ------------------------
    // Bits mirror the ErrorTrap enum.  Set by CpMem / CpIo on fault;
    // checked by CentralProcessor::HandleXFunction (ReadErrnIBnStkp Z-fn).
    uint8_t  errorFlags;

    TaskContext()
    {
        memset(this, 0, sizeof(*this));
    }
};
