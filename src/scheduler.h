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

#include "types.h"
#include <functional>
#include <list>

// -------------------------------------------------------------------------
// SchedulerEventCallback  (maps to C# delegate SchedulerEventCallback)
// Parameters: skewNsec – delta between requested and actual exec time
//             context  – user-supplied context pointer
// -------------------------------------------------------------------------
using SchedulerEventCallback = std::function<void(uint64_t /*skewNsec*/, void* /*context*/)>;

// -------------------------------------------------------------------------
// SchedulerEvent  (maps to C# class Event)
// -------------------------------------------------------------------------
class SchedulerEvent
{
public:
    SchedulerEvent(uint64_t timestampNsec, void* context, SchedulerEventCallback callback);

    uint64_t TimestampNsec() const  { return _timestampNsec; }
    void     SetTimestamp(uint64_t t) { _timestampNsec = t; }

    void*    Context() const        { return _context; }
    void     SetContext(void* ctx)  { _context = ctx; }

    const SchedulerEventCallback& EventCallback() const { return _callback; }

private:
    uint64_t               _timestampNsec;
    void*                  _context;
    SchedulerEventCallback _callback;
};

// -------------------------------------------------------------------------
// SchedulerQueue  (maps to C# class SchedulerQueue)
// Maintains an ordered linked list; the front is always the next event to fire.
// -------------------------------------------------------------------------
class SchedulerQueue
{
public:
    SchedulerQueue();

    // Returns the top event or nullptr if the queue is empty.
    SchedulerEvent* Top() const;

    bool Contains(SchedulerEvent* e) const;

    // Insert event in timestamp order.
    void Push(SchedulerEvent* e);

    // Remove and return the front event.
    SchedulerEvent* Pop();

    // Remove an arbitrary event.
    void Remove(SchedulerEvent* e);

private:
    std::list<SchedulerEvent*> _queue;
};

// -------------------------------------------------------------------------
// Scheduler  (maps to C# class Scheduler)
// -------------------------------------------------------------------------
class Scheduler
{
public:
    Scheduler();

    // Current emulated time in nanoseconds.
    uint64_t CurrentTimeNsec() const { return _currentTimeNsec; }

    // Advance the clock by one TIME_STEP_NSEC tick and fire any due events.
    void Clock();

    // Schedule an event delta nanoseconds from now with a context pointer.
    SchedulerEvent* Schedule(uint64_t deltaNsec, void* context, SchedulerEventCallback callback);

    // Schedule an event delta nanoseconds from now (no context).
    SchedulerEvent* Schedule(uint64_t deltaNsec, SchedulerEventCallback callback);

    // Cancel a previously scheduled event (safe to call with nullptr).
    void Cancel(SchedulerEvent* e);

    // Reset the scheduler to time 0 and clear all pending events.
    void Reset();

    // 137 ns ≈ one central-processor system clock cycle.
    static constexpr uint64_t TIME_STEP_NSEC = 137ULL;

private:
    uint64_t       _currentTimeNsec;
    SchedulerQueue _schedule;
};
