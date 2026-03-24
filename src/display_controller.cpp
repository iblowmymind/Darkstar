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

#include "display_controller.h"
#include "cp/central_processor.h"
#include "dsystem.h"
#include <cstring>
#include <cstdio>

// =========================================================================
// DisplaySurface
// =========================================================================

DisplaySurface::DisplaySurface()
    : _pixels(nullptr)
{
    _pixels = new uint8_t[DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_LINES * 4]();
}

DisplaySurface::~DisplaySurface()
{
    delete[] _pixels;
}

void DisplaySurface::Clear()
{
    memset(_pixels, 0, DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_LINES * 4);
}

void DisplaySurface::DrawScanline(int effectiveScanline, const uint16_t* scanlineData, bool invert)
{
    if (effectiveScanline < 0 || effectiveScanline >= DISPLAY_HEIGHT_LINES)
        return;

    // Each entry in scanlineData is a 16-bit word; each bit maps to one pixel
    // (1 = white, 0 = black on a b/w display).
    uint8_t* row = _pixels + effectiveScanline * DISPLAY_WIDTH_PIXELS * 4;

    int pixel = 0;
    // Total pixels: SCANLINE_WORDS_IN_USE * 16  (68 words * 16 = 1088 pixels)
    for (int word = 0; word < DisplayController::SCANLINE_WORDS; ++word)
    {
        uint16_t w = scanlineData[word];
        if (invert) w = ~w;

        for (int bit = 15; bit >= 0 && pixel < DISPLAY_WIDTH_PIXELS; --bit, ++pixel)
        {
            uint8_t intensity = ((w >> bit) & 1) ? 0xFF : 0x00;
            uint8_t* p = row + pixel * 4;
            p[0] = intensity; // R
            p[1] = intensity; // G
            p[2] = intensity; // B
            p[3] = 0xFF;      // A
        }
    }
}

void DisplaySurface::Render()
{
    // The pixel buffer is always up-to-date; the host reads it directly.
    // No double-buffering is required in this single-threaded design.
}

// =========================================================================
// DisplayController
// =========================================================================

DisplayController::DisplayController(DSystem* system)
    : _displayOn(false)
    , _blank(false)
    , _picture(false)
    , _invert(false)
    , _oddLine(false)
    , _displayBorder(0)
    , _scanline(0)
    , _syncPresent(false)
    , _system(system)
    , _lostSyncEvent(nullptr)
{
    memset(_scanlineData, 0, sizeof(_scanlineData));
}

DisplayController::~DisplayController()
{
    // Cancel any pending scheduler event to avoid dangling callbacks.
    if (_system && _lostSyncEvent)
    {
        _system->GetScheduler()->Cancel(_lostSyncEvent);
        _lostSyncEvent = nullptr;
    }
}

void DisplayController::Reset()
{
    _displayOn = false;
    _blank     = false;
    _picture   = false;
    _invert    = false;
    _oddLine   = false;
    _scanline  = 0;

    while (!_fifo.empty()) _fifo.pop();

    if (_system && _system->GetDisplay())
    {
        _system->GetDisplay()->Clear();
    }
}

void DisplayController::ClrDpRq()
{
    if (_system && _system->GetCP())
        _system->GetCP()->SleepTask(TaskType::Display);
}

void DisplayController::SetDCtlFifo(uint16_t value)
{
    if (_fifo.size() < 16)
    {
        _fifo.push(value);
    }
    // FIFO overflow: word is silently dropped (same as C# original).
}

void DisplayController::SetDCtl(uint16_t value)
{
    bool prevDisplayOn = _displayOn;

    _displayOn = (value & 0x01) != 0;
    _blank     = (value & 0x02) != 0;
    _picture   = (value & 0x04) != 0;
    _invert    = (value & 0x08) != 0;

    if ((value & 0x20) != 0)
    {
        // Vertical sync: back to the top of the screen.
        _scanline     = 0;
        _oddLine      = (value & 0x10) != 0;
        _syncPresent  = true;
        if (_system && _system->GetDisplay())
            _system->GetDisplay()->Render();
    }

    if ((value & 0x40) == 0)
    {
        // Bit clear: flush the control FIFO.
        while (!_fifo.empty()) _fifo.pop();
    }

    if (!prevDisplayOn && _displayOn)
    {
        // Display just turned on: kick off the horizontal-retrace callback.
        _system->GetScheduler()->Schedule(
            HORIZONTAL_RETRACE_DELAY,
            [this](uint64_t skew, void* ctx)
            {
                HorizontalRetraceCallback(skew, ctx);
            });

        // (Re)schedule the lost-sync watchdog.
        if (_lostSyncEvent)
            _system->GetScheduler()->Cancel(_lostSyncEvent);

        _lostSyncEvent = _system->GetScheduler()->Schedule(
            LOST_SYNC_INTERVAL,
            [this](uint64_t skew, void* ctx)
            {
                LostSyncCallback(skew, ctx);
            });
    }
    else if (!_displayOn)
    {
        // Display turned off: put the display task to sleep.
        if (_system && _system->GetCP())
            _system->GetCP()->SleepTask(TaskType::Display);
    }
}

void DisplayController::SetDBorder(uint16_t value)
{
    _displayBorder = value;
}

void DisplayController::HorizontalRetraceCallback(uint64_t /*skewNsec*/, void* /*context*/)
{
    int visibleOffset    = _oddLine ? 37 : 36;
    int effectiveScanline = _scanline - visibleOffset;

    if (_blank)
    {
        // Blank scanline – clear the buffer.
        memset(_scanlineData, 0, sizeof(_scanlineData));
    }
    else
    {
        if (_picture)
        {
            // Normal line: 2 border words | 64 picture words | 2 border words.
            // Border pattern: low byte on lines 4n / 4n+1; high byte on 4n+2 / 4n+3.
            int patternByte = ((effectiveScanline & 0x2) == 0)
                ? (_displayBorder & 0xff)
                : (_displayBorder >> 8);
            uint16_t patternWord = static_cast<uint16_t>(patternByte | (patternByte << 8));

            _scanlineData[0] = patternWord;
            _scanlineData[1] = patternWord;

            if (!_fifo.empty())
            {
                uint16_t fifoWord  = _fifo.front(); _fifo.pop();
                int      lastWord  = fifoWord >> 10;
                int      lineNumber = fifoWord & 0x3ff;
                bool     valid     = false;
                Memory*  mem       = _system->GetMemoryController()->DebugMemory();

                for (int word = 0; word < 64; ++word)
                {
                    _scanlineData[word + 2] =
                        mem->ReadWord((lineNumber << 6) | word, valid);

                    // Fetch the next FIFO segment when the current segment ends.
                    if (word != 63 && word == lastWord && !_fifo.empty())
                    {
                        fifoWord   = _fifo.front(); _fifo.pop();
                        lastWord   = fifoWord >> 10;
                        lineNumber = fifoWord & 0x3ff;
                    }
                }
            }
            else
            {
                // FIFO empty – blank the picture area.
                for (int i = 2; i < 66; ++i)
                    _scanlineData[i] = 0;
            }

            _scanlineData[66] = patternWord;
            _scanlineData[67] = patternWord;
        }
        else
        {
            // Border-only line.
            int patternByte = ((effectiveScanline & 0x2) == 0)
                ? (_displayBorder & 0xff)
                : (_displayBorder >> 8);
            uint16_t patternWord = static_cast<uint16_t>(patternByte | (patternByte << 8));

            for (int i = 0; i < SCANLINE_WORDS; ++i)
                _scanlineData[i] = patternWord;
        }
    }

    // NOTE: matches the C# original (effectiveScanline > 0), skipping line 0
    // which corresponds to the first visible line after the sync margin.
    if (effectiveScanline > 0 && effectiveScanline < 860)
    {
        if (_system && _system->GetDisplay())
            _system->GetDisplay()->DrawScanline(effectiveScanline, _scanlineData, _invert);
    }

    // Advance two lines (interlaced display).
    _scanline += 2;

    if (_displayOn)
    {
        // Schedule the next horizontal retrace.
        _system->GetScheduler()->Schedule(
            HORIZONTAL_RETRACE_DELAY,
            [this](uint64_t skew, void* ctx)
            {
                HorizontalRetraceCallback(skew, ctx);
            });

        // Wake the display task at end of scanline.
        if (_system->GetCP())
            _system->GetCP()->WakeTask(TaskType::Display);
    }
}

void DisplayController::LostSyncCallback(uint64_t /*skewNsec*/, void* /*context*/)
{
    _lostSyncEvent = nullptr; // Event was consumed; pointer is now stale.

    if (_syncPresent)
    {
        // Sync was received since last check – keep the display alive.
        _lostSyncEvent = _system->GetScheduler()->Schedule(
            LOST_SYNC_INTERVAL,
            [this](uint64_t skew, void* ctx)
            {
                LostSyncCallback(skew, ctx);
            });
    }
    else
    {
        // No sync since last check – blank the display.
        if (_system && _system->GetDisplay())
            _system->GetDisplay()->Clear();
    }

    _syncPresent = false;
}
