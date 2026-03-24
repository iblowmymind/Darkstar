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

#include "scheduler.h"
#include <algorithm>
#include <cassert>

// =========================================================================
// SchedulerEvent
// =========================================================================

SchedulerEvent::SchedulerEvent(uint64_t timestampNsec, void* context, SchedulerEventCallback callback)
    : _timestampNsec(timestampNsec)
    , _context(context)
    , _callback(std::move(callback))
{
}

// =========================================================================
// SchedulerQueue
// =========================================================================

SchedulerQueue::SchedulerQueue()
{
}

SchedulerEvent* SchedulerQueue::Top() const
{
    return _queue.empty() ? nullptr : _queue.front();
}

bool SchedulerQueue::Contains(SchedulerEvent* e) const
{
    for (auto* ev : _queue)
    {
        if (ev == e) return true;
    }
    return false;
}

void SchedulerQueue::Push(SchedulerEvent* e)
{
    // Degenerate case: list is empty, or new entry fires before the current head.
    if (_queue.empty() || _queue.front()->TimestampNsec() >= e->TimestampNsec())
    {
        _queue.push_front(e);
        return;
    }

    // Linear search for the insertion point – find the first existing event whose
    // timestamp is >= the new entry's timestamp, and insert before it.
    for (auto it = _queue.begin(); it != _queue.end(); ++it)
    {
        if ((*it)->TimestampNsec() >= e->TimestampNsec())
        {
            _queue.insert(it, e);
            return;
        }
    }

    // New entry fires latest: append at the back.
    _queue.push_back(e);
}

SchedulerEvent* SchedulerQueue::Pop()
{
    assert(!_queue.empty());
    SchedulerEvent* e = _queue.front();
    _queue.pop_front();
    return e;
}

void SchedulerQueue::Remove(SchedulerEvent* e)
{
    _queue.remove(e);
}

// =========================================================================
// Scheduler
// =========================================================================

Scheduler::Scheduler()
    : _currentTimeNsec(0)
{
}

void Scheduler::Reset()
{
    // Drain the queue and delete all pending events (we own them).
    while (_schedule.Top() != nullptr)
    {
        SchedulerEvent* e = _schedule.Pop();
        delete e;
    }
    _currentTimeNsec = 0;
}

void Scheduler::Clock()
{
    _currentTimeNsec += TIME_STEP_NSEC;

    // Fire every event whose due-time has arrived.
    while (_schedule.Top() != nullptr &&
           _currentTimeNsec >= _schedule.Top()->TimestampNsec())
    {
        SchedulerEvent* e = _schedule.Pop();
        // skew = actual time – requested time
        uint64_t skew = _currentTimeNsec - e->TimestampNsec();
        e->EventCallback()(skew, e->Context());
        // The callback may have re-scheduled itself (returning a new event),
        // but ownership of *this* event is transferred back to the caller via
        // Schedule(). Events allocated here are deleted in Reset() or when
        // they are explicitly cancelled with Cancel().
        delete e;
    }
}

SchedulerEvent* Scheduler::Schedule(uint64_t deltaNsec, void* context, SchedulerEventCallback callback)
{
    SchedulerEvent* e = new SchedulerEvent(_currentTimeNsec + deltaNsec, context, std::move(callback));
    _schedule.Push(e);
    return e;
}

SchedulerEvent* Scheduler::Schedule(uint64_t deltaNsec, SchedulerEventCallback callback)
{
    return Schedule(deltaNsec, nullptr, std::move(callback));
}

void Scheduler::Cancel(SchedulerEvent* e)
{
    if (e == nullptr) return;
    if (_schedule.Contains(e))
    {
        _schedule.Remove(e);
        delete e;
    }
}
