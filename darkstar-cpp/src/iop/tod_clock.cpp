/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/

#include "iop/tod_clock.h"
#include "core/configuration.h"

#include <chrono>

namespace darkstar {

// The Star's epoch is 1/1/1901.
// Difference in seconds between Unix epoch (1970-01-01) and Xerox epoch (1901-01-01).
// This is 69 years worth of seconds, accounting for leap years.
static constexpr int64_t kXeroxEpochOffsetSeconds = 2208988800LL;

TODClock::TODClock()
    : interrupt_(false)
    , power_loss_(false)
    , tod_value_(0)
    , tod_read_bit_(0)
    , mode_(TODAccessMode::None)
    , timer_running_(true)
{
    power_up_set_mode = Configuration::tod_set_mode;
    power_up_set_time = (power_up_set_mode == TODPowerUpSetMode::SpecificDate)
        ? Configuration::tod_date
        : Configuration::tod_date_time;

    power_loss_ = (power_up_set_mode == TODPowerUpSetMode::NoChange);

    reset();

    // Start the timer thread - ticks once per second
    timer_thread_ = std::thread(&TODClock::timer_thread_func, this);
}

TODClock::~TODClock()
{
    timer_running_ = false;
    if (timer_thread_.joinable()) {
        timer_thread_.join();
    }
}

void TODClock::reset()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        interrupt_ = false;
    }

    tod_read_bit_ = 0;
    mode_ = TODAccessMode::None;

    power_up_set_mode = Configuration::tod_set_mode;
    power_up_set_time = (power_up_set_mode == TODPowerUpSetMode::SpecificDate)
        ? Configuration::tod_date
        : Configuration::tod_date_time;

    set_tod_clock_internal();
}

void TODClock::reset_tod_clock_time()
{
    set_tod_clock_internal();
}

bool TODClock::interrupt()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return interrupt_;
}

void TODClock::clear_interrupt()
{
    std::lock_guard<std::mutex> lock(mutex_);
    interrupt_ = false;
}

void TODClock::set_mode(TODAccessMode mode)
{
    mode_ = mode;

    switch (mode) {
        case TODAccessMode::Read:
            tod_read_bit_ = 0;
            break;

        case TODAccessMode::Set:
            break;

        case TODAccessMode::Clear:
            tod_value_ = 0;
            break;

        default:
            break;
    }
}

int TODClock::read_clock_bit()
{
    std::lock_guard<std::mutex> lock(mutex_);
    // "Bits from clock come in true, and most significant bit first."
    int value = (tod_value_ & (0x80000000u >> (tod_read_bit_ & 0x1f))) != 0 ? 0x40 : 0;
    return value;
}

void TODClock::clock_bit(TODClockType type)
{
    switch (type) {
        case TODClockType::Read:
            tod_read_bit_++;
            break;

        case TODClockType::SetA:
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tod_value_ = (tod_value_ & 0xffffff00u) | ((tod_value_ + 1) & 0xffu);
            power_loss_ = false;
            break;
        }

        case TODClockType::SetB:
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tod_value_ = (tod_value_ & 0xffff00ffu) | ((tod_value_ + 0x100u) & 0xff00u);
            power_loss_ = false;
            break;
        }

        case TODClockType::SetC:
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tod_value_ = (tod_value_ & 0xff00ffffu) | ((tod_value_ + 0x10000u) & 0xff0000u);
            power_loss_ = false;
            break;
        }

        case TODClockType::SetD:
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tod_value_ = (tod_value_ & 0x00ffffffu) | ((tod_value_ + 0x1000000u) & 0xff000000u);
            power_loss_ = false;
            break;
        }
    }
}

void TODClock::set_tod_clock_internal()
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);

    switch (power_up_set_mode) {
        case TODPowerUpSetMode::HostTimeY2K:
        {
            // Move date back 28 years since most Star software isn't happy with Y2K.
            int64_t adjusted = now_time_t - (28LL * 365 * 24 * 3600);  // approximate
            tod_value_ = get_xerox_time(adjusted);
            break;
        }

        case TODPowerUpSetMode::HostTime:
            tod_value_ = get_xerox_time(static_cast<int64_t>(now_time_t));
            break;

        case TODPowerUpSetMode::SpecificDateAndTime:
            tod_value_ = get_xerox_time(power_up_set_time);
            break;

        case TODPowerUpSetMode::SpecificDate:
        {
            // Use the specified date but with the current time of day
            int64_t time_of_day = static_cast<int64_t>(now_time_t) % (24 * 3600);
            int64_t base_date = power_up_set_time - (power_up_set_time % (24 * 3600));
            tod_value_ = get_xerox_time(base_date + time_of_day);
            break;
        }

        case TODPowerUpSetMode::NoChange:
            // Do nothing.
            break;
    }
}

uint32_t TODClock::get_xerox_time(int64_t unix_time)
{
    // Convert Unix timestamp to Xerox time (seconds since 1/1/1901)
    return static_cast<uint32_t>(unix_time + kXeroxEpochOffsetSeconds);
}

void TODClock::timer_thread_func()
{
    while (timer_running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (!timer_running_) break;

        // One real second has elapsed.
        // Raise the interrupt flag and increment the clock value.
        std::lock_guard<std::mutex> lock(mutex_);
        interrupt_ = true;
        tod_value_++;
    }
}

} // namespace darkstar
