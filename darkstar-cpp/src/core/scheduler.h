/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>
#include <functional>
#include <list>

namespace darkstar {

using SchedulerEventCallback = std::function<void(uint64_t skew_nsec, void* context)>;

struct Event {
    uint64_t timestamp_nsec;
    void* context;
    SchedulerEventCallback callback;

    Event(uint64_t ts, void* ctx, SchedulerEventCallback cb)
        : timestamp_nsec(ts), context(ctx), callback(std::move(cb)) {}
};

class SchedulerQueue {
public:
    SchedulerQueue() : top_(nullptr) {}

    Event* top() const { return top_; }
    bool contains(Event* e) const;
    void push(Event* e);
    Event* pop();
    void remove(Event* e);

private:
    std::list<Event*> queue_;
    Event* top_ = nullptr;
};

class Scheduler {
public:
    Scheduler();

    uint64_t current_time_nsec() const { return current_time_nsec_; }

    void reset();
    void clock();

    Event* schedule(uint64_t timestamp_nsec, void* context, SchedulerEventCallback callback);
    Event* schedule(uint64_t timestamp_nsec, SchedulerEventCallback callback);
    void cancel(Event* e);

private:
    uint64_t current_time_nsec_ = 0;
    SchedulerQueue schedule_;

    // 137ns is approximately one CP system clock cycle
    static constexpr uint64_t kTimeStepNsec = 137;
};

} // namespace darkstar
