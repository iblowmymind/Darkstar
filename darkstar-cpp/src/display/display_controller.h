/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>
#include <queue>

namespace darkstar {

class DSystem;
struct Event;

class DisplayController {
public:
    explicit DisplayController(DSystem& system);

    void reset();

    bool display_on() const { return display_on_; }

    void clr_dp_rq();
    void set_d_ctl_fifo(uint16_t value);
    void set_d_ctl(uint16_t value);
    void set_d_border(uint16_t value);

private:
    void horizontal_retrace_callback(uint64_t skew_nsec, void* context);
    void lost_sync_callback(uint64_t skew_nsec, void* context);

    // Control bits
    bool display_on_ = false;
    bool blank_ = false;
    bool picture_ = false;
    bool invert_ = false;
    bool odd_line_ = false;

    // Border bitmap
    uint16_t display_border_ = 0;

    // Control FIFO. Max 16 entries.
    std::queue<uint16_t> fifo_;

    // Scanline
    int scanline_ = 0;
    uint16_t scanline_data_[64 + 4] = {};  // 1024 bits picture, 32 bits border each side

    bool sync_present_ = false;

    DSystem& system_;

    // Timing and events
    uint64_t horizontal_retrace_delay_;   // ~28.8us
    Event* lost_sync_event_ = nullptr;
    uint64_t lost_sync_interval_;         // ~53ms (one frame time)
};

} // namespace darkstar
