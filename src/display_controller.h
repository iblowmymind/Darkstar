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

#pragma once

#include "types.h"
#include "scheduler.h"
#include <queue>
#include <cstdint>

// -------------------------------------------------------------------------
// Pixel-buffer display surface (replaces the C# DWindow / SDL surface).
// The controller writes scanlines here; the JavaScript/Emscripten host reads
// the pixel buffer to paint the canvas.
//
// Layout: 32-bit RGBA, row-major.
//   width  = DISPLAY_WIDTH_PIXELS  (1024 + 2 * 32  = 1088)
//   height = DISPLAY_HEIGHT_LINES  (860, covers all possible scanlines)
// -------------------------------------------------------------------------
class DisplaySurface
{
public:
    // Physical pixel dimensions of the emulated screen including border.
    static constexpr int DISPLAY_WIDTH_PIXELS  = 1088; // 32-pixel border each side + 1024 picture
    static constexpr int DISPLAY_HEIGHT_LINES  = 860;

    DisplaySurface();
    ~DisplaySurface();

    // Fill the entire buffer with black.
    void Clear();

    // Blit one decoded scanline into the pixel buffer.
    // scanlineData[68] – 16-bit packed words (2 border + 64 picture + 2 border).
    // effectiveScanline – row index (0-based) within the visible area.
    // invert            – if true, pixel bits are inverted.
    void DrawScanline(int effectiveScanline, const uint16_t* scanlineData, bool invert);

    // Called at vertical sync; present the back-buffer (no-op here – the host
    // reads _pixels directly, so double-buffering is not needed).
    void Render();

    // Raw RGBA pixel buffer accessible by the Emscripten host.
    // Size: DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_LINES * 4 bytes.
    const uint8_t* Pixels()  const { return _pixels; }
          uint8_t* Pixels()        { return _pixels; }

    int Width()  const { return DISPLAY_WIDTH_PIXELS; }
    int Height() const { return DISPLAY_HEIGHT_LINES; }

private:
    uint8_t* _pixels;
};

// -------------------------------------------------------------------------
// DisplayController  (maps to D/Display/DisplayController.cs)
// -------------------------------------------------------------------------
class DisplayController
{
public:
    explicit DisplayController(DSystem* system);
    ~DisplayController();

    void Reset();

    // ---- Status ----
    bool DisplayOn() const { return _displayOn; }

    // ---- CP-facing register writes ----

    // DCtlFIFO<- : enqueue a scanline descriptor word.
    void SetDCtlFifo(uint16_t value);

    // DCtl<-     : set display control bits and handle sync/enable.
    void SetDCtl(uint16_t value);

    // DBorder<-  : set the border pattern register.
    void SetDBorder(uint16_t value);

    // ClrDpRq    : put the display task to sleep.
    void ClrDpRq();

    // ---- Pixel buffer access (for Emscripten host) ----
    DisplaySurface* Surface() { return &_surface; }

private:
    // Scheduler callbacks
    void HorizontalRetraceCallback(uint64_t skewNsec, void* context);
    void LostSyncCallback(uint64_t skewNsec, void* context);

    // ---- Control bits ----
    bool _displayOn;
    bool _blank;
    bool _picture;
    bool _invert;
    bool _oddLine;

    // Border pattern register.
    uint16_t _displayBorder;

    // Control FIFO – at most 16 entries.
    std::queue<uint16_t> _fifo;

    // Current scanline counter (increments by 2 per retrace callback).
    int _scanline;

public:
    // Per-scanline buffer word count: 2 border + 64 picture + 2 border = 68 words.
    static constexpr int SCANLINE_WORDS = 68;

private:
    uint16_t _scanlineData[SCANLINE_WORDS];

    // Set to true whenever a vertical-sync pulse is received; cleared by the
    // LostSyncCallback.  Used to detect loss of sync.
    bool _syncPresent;

    DSystem*       _system;
    DisplaySurface _surface;

    // ---- Timing constants ----
    // 28.8 µs horizontal retrace period
    static constexpr uint64_t HORIZONTAL_RETRACE_DELAY =
        static_cast<uint64_t>(28.8 * Conversion::UsecToNsec);

    // 52.91 ms ≈ one frame (lost-sync detection interval)
    static constexpr uint64_t LOST_SYNC_INTERVAL =
        static_cast<uint64_t>(52.91 * Conversion::MsecToNsec);

    // Pointer to the currently-scheduled lost-sync event (nullptr when display is off).
    SchedulerEvent* _lostSyncEvent;
};
