/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/
#include "core/scheduler.h"

#include <algorithm>

namespace darkstar {

// SchedulerQueue

bool SchedulerQueue::contains(Event* e) const {
    return std::find(queue_.begin(), queue_.end(), e) != queue_.end();
}

void SchedulerQueue::push(Event* e) {
    // Degenerate case: empty or new entry is earlier than head
    if (queue_.empty() || top_->timestamp_nsec >= e->timestamp_nsec) {
        queue_.push_front(e);
        top_ = e;
        return;
    }

    // Linear search for insertion point (list is sorted)
    for (auto it = queue_.begin(); it != queue_.end(); ++it) {
        if ((*it)->timestamp_nsec >= e->timestamp_nsec) {
            queue_.insert(it, e);
            return;
        }
    }

    // Add at end
    queue_.push_back(e);
}

Event* SchedulerQueue::pop() {
    Event* e = top_;
    queue_.pop_front();
    top_ = queue_.empty() ? nullptr : queue_.front();
    return e;
}

void SchedulerQueue::remove(Event* e) {
    auto it = std::find(queue_.begin(), queue_.end(), e);
    if (it != queue_.end()) {
        queue_.erase(it);
        top_ = queue_.empty() ? nullptr : queue_.front();
    }
}

// Scheduler

Scheduler::Scheduler() {
    reset();
}

void Scheduler::reset() {
    schedule_ = SchedulerQueue();
    current_time_nsec_ = 0;
}

void Scheduler::clock() {
    current_time_nsec_ += kTimeStepNsec;

    while (schedule_.top() != nullptr &&
           current_time_nsec_ >= schedule_.top()->timestamp_nsec) {
        Event* e = schedule_.pop();
        e->callback(current_time_nsec_ - e->timestamp_nsec, e->context);
        delete e;
    }
}

Event* Scheduler::schedule(uint64_t timestamp_nsec, void* context,
                           SchedulerEventCallback callback) {
    auto* e = new Event(timestamp_nsec + current_time_nsec_, context,
                        std::move(callback));
    schedule_.push(e);
    return e;
}

Event* Scheduler::schedule(uint64_t timestamp_nsec,
                           SchedulerEventCallback callback) {
    return schedule(timestamp_nsec, nullptr, std::move(callback));
}

void Scheduler::cancel(Event* e) {
    if (e) {
        schedule_.remove(e);
        delete e;
    }
}

} // namespace darkstar
