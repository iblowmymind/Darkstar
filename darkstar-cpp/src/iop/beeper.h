/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>
#include <vector>

namespace darkstar {

/// Implements the tone generator used to generate simple beeps.
/// This is driven by an i8253 programmable interval timer.
class Beeper {
public:
    Beeper();

    void reset();

    void load_period(uint8_t value);
    void enable_tone();
    void disable_tone();

    /// SDL2 audio callback - fills the stream buffer with square wave data.
    void audio_callback(void* userdata, uint8_t* stream, int length);

private:
    std::vector<uint8_t> sample_buffer_;

    bool load_lsb_;
    uint8_t lsb_;

    double frequency_;
    bool enabled_;
    double position_;
    double period_in_samples_;
    bool sample_on_;
};

} // namespace darkstar
