/*
    BSD 2-Clause License

    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this
      list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "tod_clock.h"
#include "../types.h"
#include <ctime>

// Jan 1 1901 00:00:00 UTC in Unix time
static constexpr int64_t XEROX_EPOCH = -2208988800LL;

TODClock::TODClock()
    : PowerUpSetMode(TODPowerUpSetMode::HostTime), PowerUpSetTime(0) {
    Reset();
}

void TODClock::Reset() {
    _interrupt = false;
    _powerLoss = true;
    _todValue = 0;
    _todReadBit = 0;
    _mode = TODAccessMode::None;
}

void TODClock::ResetTODClockTime() {
    SetTODClockInternal();
    _powerLoss = false;
}

void TODClock::Tick() {
    // Called once per second - increment the TOD value
    _todValue++;
    _interrupt = true;
}

void TODClock::ClearInterrupt() {
    _interrupt = false;
}

void TODClock::SetMode(TODAccessMode mode) {
    _mode = mode;
    if (mode == TODAccessMode::Read) {
        _todReadBit = 0;  // start at 0, increment toward 31 (MSB first: 0x80000000 >> 0)
    }
}

int TODClock::ReadClockBit() const {
    if (_mode != TODAccessMode::Read) {
        return 0;
    }
    
    // Return bit at position _todReadBit (0x40 if set, 0 if clear)
    uint32_t mask = 0x80000000 >> (_todReadBit & 0x1f);
    return (_todValue & mask) ? 0x40 : 0;
}

void TODClock::ClockBit(TODClockType type) {
    switch (type) {
    case TODClockType::Read:
        if (_mode == TODAccessMode::Read && _todReadBit < 32) {
            _todReadBit++;   // increment (matches C#: MSB-first bit order)
        }
        break;
        
    case TODClockType::SetA:
        // Increment least significant byte
        _todValue = (_todValue & 0xffffff00) | ((_todValue + 1) & 0xff);
        _powerLoss = false;
        break;
        
    case TODClockType::SetB:
        // Increment second byte
        _todValue = (_todValue & 0xffff00ff) | (((_todValue >> 8) + 1) & 0xff) << 8;
        _powerLoss = false;
        break;
        
    case TODClockType::SetC:
        // Increment third byte
        _todValue = (_todValue & 0xff00ffff) | (((_todValue >> 16) + 1) & 0xff) << 16;
        _powerLoss = false;
        break;
        
    case TODClockType::SetD:
        // Increment most significant byte
        _todValue = (_todValue & 0x00ffffff) | (((_todValue >> 24) + 1) & 0xff) << 24;
        _powerLoss = false;
        break;
    }
}

uint32_t TODClock::GetXeroxTime(time_t t) {
    return (uint32_t)(int64_t(t) - XEROX_EPOCH);
}

void TODClock::SetTODClockInternal() {
    time_t currentTime = time(nullptr);
    
    switch (PowerUpSetMode) {
    case TODPowerUpSetMode::HostTimeY2K:
        // Subtract 28 years (Y2K bug workaround)
        currentTime -= 28LL * 365 * 24 * 3600; // Approximate 28 years
        _todValue = GetXeroxTime(currentTime);
        break;
        
    case TODPowerUpSetMode::HostTime:
        _todValue = GetXeroxTime(currentTime);
        break;
        
    case TODPowerUpSetMode::SpecificDateAndTime:
    case TODPowerUpSetMode::SpecificDate:
        _todValue = GetXeroxTime(PowerUpSetTime);
        break;
        
    case TODPowerUpSetMode::NoChange:
        // Don't change the current value
        break;
    }
}