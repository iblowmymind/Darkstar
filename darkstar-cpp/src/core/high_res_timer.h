/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <chrono>
#include <cstdint>
#include <thread>
#include <condition_variable>
#include <mutex>

namespace darkstar {

class HighResTimer {
public:
    double get_current_time() const {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration<double>(duration).count();
    }
};

class FrameTimer {
public:
    explicit FrameTimer(double frames_per_second);
    ~FrameTimer();

    void wait_for_frame();

private:
    void timer_thread_fn();

    double frame_interval_sec_;
    bool running_ = true;
    std::thread timer_thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool frame_ready_ = false;
};

} // namespace darkstar
