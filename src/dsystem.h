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

// Minimal DSystem interface header used by subsystem implementations.
// The full DSystem class is defined in dsystem.h (to be created in a later
// phase when all subsystems exist).  Include this header whenever you need
// to call back into DSystem from a subsystem.

#include "types.h"
#include "scheduler.h"
#include "memory.h"
#include "display_controller.h"

// Forward-declare subsystem classes that DSystem exposes but are not yet
// fully defined in this compilation unit.
class CentralProcessor;

// -------------------------------------------------------------------------
// DSystem – top-level system object.
// Subsystems hold a raw pointer to this and call it back for scheduling,
// memory access, display rendering, and inter-subsystem wakeup/sleep.
// -------------------------------------------------------------------------
class DSystem
{
public:
    DSystem();
    ~DSystem();

    void Reset();

    // ---- Subsystem accessors ----
    Scheduler*         GetScheduler()         { return _scheduler; }
    CentralProcessor*  GetCP()                { return _cp; }
    MemoryController*  GetMemoryController()  { return _memoryController; }
    DisplayController* GetDisplayController() { return _displayController; }
    DisplaySurface*    GetDisplay()           { return _displayController ? _displayController->Surface() : nullptr; }

private:
    Scheduler*         _scheduler;
    CentralProcessor*  _cp;
    MemoryController*  _memoryController;
    DisplayController* _displayController;
};
