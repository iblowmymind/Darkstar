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
// control_store.h  –  CP microcode control store
//
// The Xerox Star CP has 8 independent control stores, one per task.  Each
// store holds up to 1024 48-bit microcode words addressed by a 12-bit uPC.
//
// Responsibilities
// ----------------
//   • Hold 8 × 1024 raw 48-bit words (stored right-aligned in uint64_t).
//   • LoadWords(): bulk-load a task's microcode from a flat array (used at
//     boot time when the IOP uploads the microcode binary).
//   • Write(): single-word write used by the IOPcs task, which patches
//     individual control-store entries at run time.
//   • At():    decode and return a Microinstruction object for a given
//     (task, uPC) pair.  Decoding is done on demand (no cache) so that
//     Write() patches are always reflected immediately.
//   • RawAt(): return the undecoded 48-bit word (used for diagnostics and
//     parity/checksum verification by microcode).
//   • Reset(): zero all stores (all microwords become no-ops).
//
// Ported from D/CP/ControlStore.cs.
// ---------------------------------------------------------------------------

#include "cp_types.h"
#include "microinstruction.h"

class ControlStore
{
public:
    ControlStore();

    // Reset all 8 stores to zero (no-op microwords).
    void Reset();

    // Bulk-load microcode for one task.
    //   task  – which of the 8 task stores to target.
    //   words – array of 48-bit words packed in the low bits of uint64_t.
    //   count – number of words to load (clamped to CP_CS_SIZE).
    void LoadWords(TaskType task, const uint64_t* words, int count);

    // Write a single 48-bit word into the control store (IOPcs patching).
    //   task  – target task store.
    //   uPC   – microcode address (0..CP_CS_SIZE-1).
    //   word  – 48-bit microcode word (right-aligned in uint64_t).
    void Write(TaskType task, int uPC, uint64_t word);

    // Decode the microinstruction at (task, uPC) and return it.
    // The returned object is freshly decoded each call.
    Microinstruction At(TaskType task, int uPC) const;

    // Return the raw 48-bit word at (task, uPC) without decoding.
    uint64_t RawAt(TaskType task, int uPC) const;

private:
    uint64_t _store[CP_TASK_COUNT][CP_CS_SIZE];
};
