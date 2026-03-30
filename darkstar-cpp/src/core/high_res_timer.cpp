/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/
#include "core/high_res_timer.h"

namespace darkstar {

FrameTimer::FrameTimer(double frames_per_second)
    : frame_interval_sec_(1.0 / frames_per_second) {
    timer_thread_ = std::thread(&FrameTimer::timer_thread_fn, this);
}

FrameTimer::~FrameTimer() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
    }
    cv_.notify_all();
    if (timer_thread_.joinable()) {
        timer_thread_.join();
    }
}

void FrameTimer::wait_for_frame() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return frame_ready_ || !running_; });
    frame_ready_ = false;
}

void FrameTimer::timer_thread_fn() {
    auto next_frame = std::chrono::high_resolution_clock::now();

    while (true) {
        next_frame += std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(
            std::chrono::duration<double>(frame_interval_sec_));

        std::this_thread::sleep_until(next_frame);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_) break;
            frame_ready_ = true;
        }
        cv_.notify_one();
    }
}

} // namespace darkstar
