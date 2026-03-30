/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>

#include "core/configuration.h"

namespace darkstar {

enum class TODAccessMode {
    Read  = 0x4,
    Clear = 0x2,
    Set   = 0x1,
    None  = 0,
};

enum class TODClockType {
    Read = 0,
    SetA,
    SetB,
    SetC,
    SetD,
};

/// Implements the Star's TOD clock and timing logic.
/// The Star does not use an off-the-shelf RTC chip, just a series of
/// 74LS393 dual 4-bit counters and 74LS165 shift registers
/// clocked off of a 1Hz clock -- these form a single 32-bit register
/// that counts seconds.  It can be read and written by the IOP.
class TODClock {
public:
    TODClock();
    ~TODClock();

    void reset();

    /// Resets the clock to the current value specified by the system configuration.
    void reset_tod_clock_time();

    bool interrupt();
    bool power_loss() const { return power_loss_; }

    void clear_interrupt();
    void set_mode(TODAccessMode mode);
    int read_clock_bit();
    void clock_bit(TODClockType type);

    TODPowerUpSetMode power_up_set_mode;
    int64_t power_up_set_time;

private:
    void set_tod_clock_internal();
    uint32_t get_xerox_time(int64_t unix_time);
    void timer_thread_func();

    std::atomic<bool> interrupt_;
    bool power_loss_;
    uint32_t tod_value_;
    int tod_read_bit_;
    TODAccessMode mode_;

    // Timer thread for real-time clocking
    std::thread timer_thread_;
    std::atomic<bool> timer_running_;
    mutable std::mutex mutex_;
};

} // namespace darkstar
