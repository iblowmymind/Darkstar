/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/

#include "iop/keyboard.h"
#include "core/log.h"

namespace darkstar {

Keyboard::Keyboard()
    : key_data_(KeyCode::Invalid)
{
}

uint8_t Keyboard::read_data()
{
    if (Log::enabled) Log::write(LogComponent::IOPKeyboard,
        "Key data 0x%02x read.", static_cast<int>(key_data_));
    return static_cast<uint8_t>(key_data_);
}

void Keyboard::next_data()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!keyboard_queue_.empty()) {
        key_data_ = keyboard_queue_.front();
        keyboard_queue_.pop();
        if (Log::enabled) Log::write(LogComponent::IOPKeyboard,
            "Key data 0x%02x dequeued.", static_cast<int>(key_data_));
    } else {
        key_data_ = KeyCode::Invalid;
    }
}

bool Keyboard::data_ready()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return !keyboard_queue_.empty();
}

void Keyboard::enable_diagnostic_mode()
{
    std::lock_guard<std::mutex> lock(mutex_);
    keyboard_queue_.push(KeyCode::D2);
    keyboard_queue_.push(KeyCode::D1);
}

void Keyboard::disable_diagnostic_mode()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::queue<KeyCode> empty;
    std::swap(keyboard_queue_, empty);
}

void Keyboard::key_down(KeyCode keycode)
{
    std::lock_guard<std::mutex> lock(mutex_);
    // Bit 0 = 0 indicates the key being pressed
    keyboard_queue_.push(static_cast<KeyCode>(static_cast<int>(keycode) & 0x7f));
}

void Keyboard::key_up(KeyCode keycode)
{
    std::lock_guard<std::mutex> lock(mutex_);
    // Bit 0 = 1 indicates the key being released
    keyboard_queue_.push(static_cast<KeyCode>(static_cast<int>(keycode) | 0x80));
}

} // namespace darkstar
