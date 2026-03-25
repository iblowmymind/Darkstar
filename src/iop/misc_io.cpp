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

#include "misc_io.h"
#include "io_processor.h"

// Clock control flags
enum ClockFlags {
    ClrMPanel = 0x40,
    IncMPanel = 0x20,
    TODRead   = 0x10,
    TODSetA   = 0x08,
    TODSetB   = 0x04,
    TODSetC   = 0x02,
    TODSetD   = 0x01,
};

// MiscInput1Flags
enum MiscInput1Flags {
    AltBootFlag = 0x80,
    TODData     = 0x40,
    PowerFailed = 0x20,
    TODInt      = 0x10,
    CSParity    = 0x08,
    MouseSw3    = 0x04,
    MouseSw2    = 0x02,
    MouseSw1    = 0x01,
};

// Port definitions
const std::vector<int> MiscIO::_readPorts = {0xd0, 0xe9, 0xea, 0xed, 0xee, 0xef};
const std::vector<int> MiscIO::_writePorts = {0x8d, 0x8f, 0xd0, 0xe9, 0xea, 0xed, 0xef};

MiscIO::MiscIO(IOProcessor* iop) : _iop(iop) {
    Reset();
}

void MiscIO::Reset() {
    _mPanelBlank = true;
    _mPanelValue = 0;
    _altBoot = AltBootValues::None;
    _altBootCounter = static_cast<int>(_altBoot);
    _lastClockFlags = 0;
    _dmaTestValue = 0;
    _todClock.Reset();
}

void MiscIO::SetAltBoot(AltBootValues v) {
    _altBoot = v;
    _altBootCounter = static_cast<int>(v);  // matches C#: _altBootCounter = (int)value
}

const std::vector<int>& MiscIO::ReadPorts() const {
    return _readPorts;
}

const std::vector<int>& MiscIO::WritePorts() const {
    return _writePorts;
}

void MiscIO::WritePort(int port, uint8_t value) {
    switch (port) {
    case 0x8d:
        // i8253 Timer channel #1 – keyboard bell frequency (16-bit, LSB first)
        _iop->GetBeeper()->LoadPeriod(value);
        break;
        
    case 0x8f:
        // i8253 Timer mode – ignored (no need to emulate the 8253 timer directly)
        break;
        
    case 0xd0:
        _dmaTestValue = value;
        break;
        
    case 0xe9:
        DoMiscClock(value);
        break;
        
    case 0xea:
        _todClock.ClearInterrupt();
        break;
        
    case 0xed:
        _iop->GetMouse()->Clear();
        break;
        
    case 0xef:
        // Control bits:
        // 0x40 - pReadKBData  – advance keyboard queue to next byte
        // 0x20 - KBTone       – enable/disable keyboard speaker
        // 0x10 - KBDiag       – enter keyboard diagnostic mode
        // 0x08 - BlankMPanel  – blank the MP display
        // 0x04 - ReadTimeMode – set TOD to Read mode
        // 0x02 - ClearTimeMode – set TOD to Clear mode
        // 0x01 - SetTimeMode  – set TOD to Set mode
        _mPanelBlank = (value & 0x08) != 0;
        if (MPChanged) MPChanged();

        if ((value & 0x40) != 0)
            _iop->GetKeyboard()->NextData();

        if ((value & 0x20) != 0)
            _iop->GetBeeper()->EnableTone();
        else
            _iop->GetBeeper()->DisableTone();

        if ((value & 0x10) != 0)
            _iop->GetKeyboard()->EnableDiagnosticMode();

        if ((value & 0x04) != 0)
            _todClock.SetMode(TODAccessMode::Read);

        if ((value & 0x02) != 0)
            _todClock.SetMode(TODAccessMode::Clear);

        if ((value & 0x01) != 0)
            _todClock.SetMode(TODAccessMode::Set);
        break;
    }
}

uint8_t MiscIO::ReadPort(int port) {
    switch (port) {
    case 0xd0:
        return _dmaTestValue;
        
    case 0xe9:
        // MiscInput0 – interrupt status flags, all active-low.
        return static_cast<uint8_t>(~(
            (_iop->GetFloppyController()->Interrupt() ? 0x80 : 0x00) |
            (_iop->GetKeyboard()->DataReady()         ? 0x40 : 0x00)));
        
    case 0xea:
        // Keyboard data latch – data is inverted.
        return static_cast<uint8_t>(~_iop->GetKeyboard()->ReadData());
        
    case 0xed:
        return static_cast<uint8_t>(_iop->GetMouse()->MouseX());
        
    case 0xee:
        return static_cast<uint8_t>(_iop->GetMouse()->MouseY());
        
    case 0xef:
        {
            // MiscInput1: AltBoot, TimeData, PowerFailed, TODInt, CSParError, MouseSw1-3
            uint8_t result = 0;

            // AltBoot: set while counter > 0, then decrement
            if (_altBootCounter > 0) {
                result = static_cast<uint8_t>(AltBootFlag);
                _altBootCounter--;
            }

            result = static_cast<uint8_t>(result |
                static_cast<uint8_t>(_todClock.ReadClockBit())               |
                (_todClock.PowerLoss()   ? static_cast<uint8_t>(PowerFailed) : 0) |
                (_todClock.Interrupt()   ? static_cast<uint8_t>(TODInt)      : 0) |
                static_cast<uint8_t>(_iop->GetMouse()->Buttons())            |
                static_cast<uint8_t>(CSParity)  /* active-low, keep set (no parity errors) */);

            return result;
        }
    }
    
    return 0;
}

void MiscIO::DoMiscClock(uint8_t clockFlags) {
    // On a 1→0 transition for each clock bit, take the corresponding action.
    for (int clockFlag = 0x1; clockFlag < 0x100; clockFlag <<= 1) {
        if ((clockFlags & clockFlag) == 0 && (_lastClockFlags & clockFlag) != 0) {
            switch (clockFlag) {
            case ClrMPanel:
                _mPanelValue = 0;
                if (MPChanged) MPChanged();
                break;
                
            case IncMPanel:
                _mPanelValue = (_mPanelValue + 1) % 10000;
                if (MPChanged) MPChanged();
                break;
                
            case TODRead:
                _todClock.ClockBit(TODClockType::Read);
                break;
                
            case TODSetA:
                _todClock.ClockBit(TODClockType::SetA);
                break;
                
            case TODSetB:
                _todClock.ClockBit(TODClockType::SetB);
                break;
                
            case TODSetC:
                _todClock.ClockBit(TODClockType::SetC);
                break;
                
            case TODSetD:
                _todClock.ClockBit(TODClockType::SetD);
                break;
            }
        }
    }
    
    _lastClockFlags = clockFlags;
}