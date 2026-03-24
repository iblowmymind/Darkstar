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

#include "beeper.h"
#include <cmath>

static constexpr double SAMPLE_RATE = 44100.0;
static constexpr double TIMER_FREQUENCY = 1843200.0; // 1.8432 MHz

Beeper::Beeper() {
    Reset();
}

void Beeper::Reset() {
    _loadLSB = true;
    _lsb = 0;
    _frequency = 0.0;
    _enabled = false;
    _position = 0.0;
    _periodInSamples = 0.0;
    _sampleOn = false;
    _sampleBuffer.resize(0x10000);
}

void Beeper::LoadPeriod(uint8_t value) {
    if (_loadLSB) {
        _lsb = value;
        _loadLSB = false;
    } else {
        // Complete the 16-bit period value
        uint16_t period = (static_cast<uint16_t>(value) << 8) | _lsb;
        _loadLSB = true;
        
        if (period > 0) {
            // Convert period to frequency
            // Period is in units of 1/1.8432MHz = 1/1843200 seconds
            _frequency = TIMER_FREQUENCY / period;
            _periodInSamples = SAMPLE_RATE / _frequency;
        } else {
            _frequency = 0.0;
            _periodInSamples = 0.0;
        }
    }
}

void Beeper::EnableTone() {
    _enabled = true;
    _position = 0.0;
}

void Beeper::DisableTone() {
    _enabled = false;
}

void Beeper::AudioCallback(uint8_t* stream, int length) {
    if (!_enabled || _frequency <= 0.0) {
        // Fill with silence (middle value for unsigned 8-bit)
        for (int i = 0; i < length; i++) {
            stream[i] = 0x80;
        }
        return;
    }
    
    for (int i = 0; i < length; i++) {
        // Generate square wave
        if (_periodInSamples > 0.0) {
            _sampleOn = (_position < _periodInSamples / 2.0);
            _position += 1.0;
            if (_position >= _periodInSamples) {
                _position -= _periodInSamples;
            }
        } else {
            _sampleOn = false;
        }
        
        // Output high (0x3f + 0x80) or low (0x00 + 0x80) for unsigned 8-bit
        stream[i] = _sampleOn ? 0xbf : 0x80;
    }
}