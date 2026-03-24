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

#include "mouse.h"
#include <algorithm>

Mouse::Mouse() {
    // Initialize to zero position and no buttons
}

void Mouse::Clear() {
    _mouseX = 0;
    _mouseY = 0;
}

void Mouse::MouseDown(StarMouseButton button) {
    _buttons |= button;
}

void Mouse::MouseUp(StarMouseButton button) {
    _buttons &= ~button;
}

void Mouse::MouseMove(int dx, int dy) {
    // Apply delta movement and clamp to signed 8-bit range
    _mouseX += dx;
    _mouseY += dy;
    
    _mouseX = std::max(-128, std::min(127, _mouseX));
    _mouseY = std::max(-128, std::min(127, _mouseY));
}