/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/

#include "iop/beeper.h"
#include "core/log.h"

#include <cstring>

namespace darkstar {

static constexpr double kUsecToSec = 0.000001;

Beeper::Beeper()
{
    reset();
}

void Beeper::reset()
{
    lsb_ = 0;
    load_lsb_ = true;
    frequency_ = 0.0;
    enabled_ = false;
    sample_on_ = false;
    period_in_samples_ = 0;
    position_ = 0;

    sample_buffer_.resize(0x10000, 0);
}

void Beeper::load_period(uint8_t value)
{
    if (load_lsb_) {
        lsb_ = value;
    } else {
        // "The period (in usec*1.8432) will be in the range ~29..65535."
        double period = ((value << 8) | lsb_) / 1.8432;

        // The above is in usec; convert to seconds:
        period = period * kUsecToSec;

        // Invert to get the frequency in Hz:
        frequency_ = 1.0 / period;

        if (Log::enabled) Log::write(LogComponent::Beeper, "Tone frequency set to %f", frequency_);

        // And find out the length in samples at 44.1Khz.
        period_in_samples_ = 44100.0 / frequency_;
    }

    load_lsb_ = !load_lsb_;
}

void Beeper::enable_tone()
{
    if (Log::enabled) Log::write(LogComponent::Beeper, "Tone enabled.");
    enabled_ = true;
}

void Beeper::disable_tone()
{
    if (Log::enabled && enabled_) Log::write(LogComponent::Beeper, "Tone disabled.");
    enabled_ = false;
}

void Beeper::audio_callback(void* /*userdata*/, uint8_t* stream, int length)
{
    if (static_cast<size_t>(length) > sample_buffer_.size()) {
        sample_buffer_.resize(length, 0);
    }

    if (enabled_) {
        for (int i = 0; i < length; i++) {
            position_++;

            if (position_ > period_in_samples_) {
                position_ -= period_in_samples_;
                sample_on_ = !sample_on_;
            }

            sample_buffer_[i] = static_cast<uint8_t>(enabled_ ? (sample_on_ ? 0x3f : 0x00) : 0x00);
        }
    } else {
        std::memset(sample_buffer_.data(), 0, length);
    }

    std::memcpy(stream, sample_buffer_.data(), length);
}

} // namespace darkstar
