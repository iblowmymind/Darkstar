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

#include "control_store.h"
#include <cstring>
#include <cassert>

ControlStore::ControlStore()
{
    Reset();
}

void ControlStore::Reset()
{
    memset(_store, 0, sizeof(_store));
}

void ControlStore::LoadWords(TaskType task, const uint64_t* words, int count)
{
    int t = static_cast<int>(task);
    assert(t >= 0 && t < CP_TASK_COUNT);
    if (count > CP_CS_SIZE)
        count = CP_CS_SIZE;

    for (int i = 0; i < count; ++i)
        _store[t][i] = words[i] & 0x0000FFFFFFFFFFFFull;
}

void ControlStore::Write(TaskType task, int uPC, uint64_t word)
{
    int t = static_cast<int>(task);
    assert(t >= 0 && t < CP_TASK_COUNT);
    assert(uPC >= 0 && uPC < CP_CS_SIZE);
    _store[t][uPC & (CP_CS_SIZE - 1)] = word & 0x0000FFFFFFFFFFFFull;
}

Microinstruction ControlStore::At(TaskType task, int uPC) const
{
    int t = static_cast<int>(task);
    assert(t >= 0 && t < CP_TASK_COUNT);
    return Microinstruction(_store[t][uPC & (CP_CS_SIZE - 1)]);
}

uint64_t ControlStore::RawAt(TaskType task, int uPC) const
{
    int t = static_cast<int>(task);
    assert(t >= 0 && t < CP_TASK_COUNT);
    return _store[t][uPC & (CP_CS_SIZE - 1)];
}
