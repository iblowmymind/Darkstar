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

#include "keyboard.h"

Keyboard::Keyboard() {
    // Initialize empty
}

uint8_t Keyboard::ReadData() const {
    return static_cast<uint8_t>(_keyData);
}

void Keyboard::NextData() {
    if (!_keyboardQueue.empty()) {
        _keyData = _keyboardQueue.front();
        _keyboardQueue.pop();
    } else {
        _keyData = KeyCode::Invalid;
    }
}

bool Keyboard::DataReady() const {
    return !_keyboardQueue.empty();
}

void Keyboard::EnableDiagnosticMode() {
    // Clear queue and add diagnostic sequence
    std::queue<KeyCode> empty;
    _keyboardQueue.swap(empty);
    _keyboardQueue.push(KeyCode::D2);
    _keyboardQueue.push(KeyCode::D1);
}

void Keyboard::DisableDiagnosticMode() {
    // Clear the queue
    std::queue<KeyCode> empty;
    _keyboardQueue.swap(empty);
    _keyData = KeyCode::Invalid;
}

void Keyboard::KeyDown(KeyCode keycode) {
    // Key down: enqueue keycode with bit 7 clear
    _keyboardQueue.push(static_cast<KeyCode>(static_cast<uint8_t>(keycode) & 0x7f));
}

void Keyboard::KeyUp(KeyCode keycode) {
    // Key up: enqueue keycode with bit 7 set
    _keyboardQueue.push(static_cast<KeyCode>(static_cast<uint8_t>(keycode) | 0x80));
}