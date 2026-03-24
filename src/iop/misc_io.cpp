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

// Forward declaration to resolve circular dependency
class IOProcessor;

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
    _altBootCounter = 0;
    _altBoot = AltBootValues::None;
    _lastClockFlags = 0;
    _dmaTestValue = 0;
    _todClock.Reset();
}

void MiscIO::SetAltBoot(AltBootValues v) {
    _altBoot = v;
    if (v != AltBootValues::None) {
        _altBootCounter = 6; // Set counter for AltBoot detection
    }
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
        // Beeper period (need access to beeper - will be handled by IOProcessor)
        break;
        
    case 0x8f:
        // Timer mode - ignored
        break;
        
    case 0xd0:
        // DMA test register
        _dmaTestValue = value;
        break;
        
    case 0xe9:
        // Misc clock
        DoMiscClock(value);
        break;
        
    case 0xea:
        // Clear TOD interrupt
        _todClock.ClearInterrupt();
        break;
        
    case 0xed:
        // Mouse clear (need access to mouse - will be handled by IOProcessor)
        break;
        
    case 0xef:
        // Various control bits
        // Bit 0: beeper enable/disable
        // Other bits: various controls
        break;
    }
}

uint8_t MiscIO::ReadPort(int port) {
    switch (port) {
    case 0xd0:
        // DMA test register
        return _dmaTestValue;
        
    case 0xe9:
        // MiscInput0 (floppy interrupt, keyboard data ready)
        // Bit 7: floppy interrupt (inverted)
        // Bit 6: keyboard data ready (inverted)
        {
            uint8_t result = 0xFF;
            // TODO: Get floppy interrupt status
            // TODO: Get keyboard data ready status
            return result;
        }
        
    case 0xea:
        // Keyboard data
        // TODO: Get keyboard read data
        return 0;
        
    case 0xed:
        // Mouse X coordinate
        // TODO: Get mouse X
        return 0;
        
    case 0xee:
        // Mouse Y coordinate  
        // TODO: Get mouse Y
        return 0;
        
    case 0xef:
        // MiscInput1
        {
            uint8_t result = 0;
            
            // Handle AltBoot detection
            if (_altBootCounter > 0) {
                _altBootCounter--;
                if (_altBootCounter == 0) {
                    result |= AltBootFlag;
                }
            }
            
            // TOD clock data bit
            result |= _todClock.ReadClockBit();
            
            // Power failed (always cleared)
            if (_todClock.PowerLoss()) {
                result |= PowerFailed;
            }
            
            // TOD interrupt
            if (_todClock.Interrupt()) {
                result |= TODInt;
            }
            
            // CS Parity (active low, always set for now)
            result |= CSParity;
            
            // Mouse button states
            // TODO: Get mouse button states
            
            return result;
        }
    }
    
    return 0;
}

void MiscIO::DoMiscClock(uint8_t clockFlags) {
    // Check for 1->0 transitions on each bit
    for (int bit = 0; bit < 8; bit++) {
        int mask = 1 << bit;
        if ((_lastClockFlags & mask) && !(clockFlags & mask)) {
            // 1->0 transition detected on this bit
            switch (mask) {
            case ClrMPanel:
                _mPanelBlank = true;
                _mPanelValue = 0;
                if (MPChanged) MPChanged();
                break;
                
            case IncMPanel:
                _mPanelBlank = false;
                _mPanelValue = (_mPanelValue + 1) & 0xFF;
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