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

#include "dma_controller.h"
#include "io_processor.h"

// Port definitions
const std::vector<int> DMAController::_readPorts = {0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8};
const std::vector<int> DMAController::_writePorts = {0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8};

DMAChannel::DMAChannel() {
    Reset();
}

void DMAChannel::Reset() {
    Enabled = false;
    Completed = false;
    ChAddr = 0;
    ChCount = -1;
    Type = DMAType::Invalid;
    Device = nullptr;
}

DMAController::DMAController(IOProcessor* iop) : _iop(iop) {
    Reset();
}

void DMAController::RegisterDevice(IDMAInterface* device, int channel) {
    if (channel >= 0 && channel < 4) {
        _channels[channel].Device = device;
    }
}

bool DMAController::TC() const {
    // Terminal count: check the last-selected channel (matches C# semantics)
    if (_lastSelectedChannel != -1) {
        return _channels[_lastSelectedChannel].ChCount == 0;
    }
    return false;
}

void DMAController::Reset() {
    for (int i = 0; i < 4; i++) {
        _channels[i].Reset();
    }
    _rotatingPriority = false;
    _extendedWrite = false;
    _tcStop = false;
    _autoLoad = false;
    _first = true;
    _nextToService = 0;
    _lastSelectedChannel = -1;
    _hrq = false;
}

void DMAController::Execute() {
    int nextChannel = SelectNextChannel();

    _hrq = (nextChannel != -1);

    if (!_hrq) return;

    DMAChannel& c = _channels[nextChannel];

    // Perform the actual memory↔device transfer
    switch (c.Type) {
    case DMAType::Read:
        // Read from memory, write to device
        {
            uint8_t dmaWrite = _iop->Memory()->ReadByte(c.ChAddr);
            c.Device->DMAWrite(dmaWrite);
        }
        break;

    case DMAType::Write:
        // Read from device, write to memory
        {
            uint8_t dmaRead = c.Device->DMARead();
            _iop->Memory()->WriteByte(c.ChAddr, dmaRead);
        }
        break;

    case DMAType::Verify:
        // Verify: read from device (no memory write)
        c.Device->DMARead();
        break;

    case DMAType::Invalid:
        break;
    }

    // Increment address and decrement counter
    c.ChAddr++;
    c.ChCount--;

    // Terminal count: stop channel if enabled and notify device
    if (c.ChCount == 0) {
        if (_tcStop) {
            c.Enabled = false;
        }
        c.Completed = true;
        c.Device->DMAComplete();
    }

    _lastSelectedChannel = nextChannel;
}

int DMAController::SelectNextChannel() {
    int startChannel = _rotatingPriority ? _nextToService : 0;
    
    for (int i = 0; i < 4; i++) {
        int channel = (startChannel + i) % 4;
        DMAChannel& ch = _channels[channel];
        
        if (ch.Enabled && !ch.Completed && ch.Device && ch.Device->DRQ()) {
            if (_rotatingPriority) {
                _nextToService = (channel + 1) % 4;
            }
            return channel;
        }
    }
    
    return -1;
}

const std::vector<int>& DMAController::ReadPorts() const {
    return _readPorts;
}

const std::vector<int>& DMAController::WritePorts() const {
    return _writePorts;
}

void DMAController::WritePort(int port, uint8_t value) {
    if (port >= 0xa0 && port <= 0xa7) {
        int ch = (port - 0xa0) / 2;
        bool isCount = (port & 1) != 0;

        if (!isCount) {
            // Address register (even ports 0xa0, 0xa2, 0xa4, 0xa6)
            if (_first) {
                _channels[ch].ChAddr = value;
            } else {
                _channels[ch].ChAddr = static_cast<uint16_t>(_channels[ch].ChAddr | (static_cast<uint16_t>(value) << 8));
            }
            _first = !_first;
        } else {
            // Count register (odd ports 0xa1, 0xa3, 0xa5, 0xa7)
            // First byte: low 8 bits of 14-bit count
            // Second byte: high 6 bits [5:0] of count + DMA type in bits [7:6]
            if (_first) {
                _channels[ch].ChCount = value;
            } else {
                // Combine: 14-bit count = (LSB | ((MSB & 0x3f) << 8)) + 1
                // Type encoded in bits [7:6] of MSB
                _channels[ch].ChCount = (_channels[ch].ChCount | ((value & 0x3f) << 8)) + 1;
                _channels[ch].Type    = static_cast<DMAType>(value >> 6);
            }
            _first = !_first;
        }
    } else if (port == 0xa8) {
        // Mode/control register – matches C# 8257 behaviour:
        // bits[3:0]: channel enables (one bit per channel)
        // bit 4: rotating priority
        // bit 5: extended write
        // bit 6: TC stop
        // bit 7: autoload
        _first = true;   // reset the two-byte latch
        _channels[0].Enabled = (value & 0x01) != 0;
        _channels[1].Enabled = (value & 0x02) != 0;
        _channels[2].Enabled = (value & 0x04) != 0;
        _channels[3].Enabled = (value & 0x08) != 0;
        _rotatingPriority = (value & 0x10) != 0;
        _extendedWrite    = (value & 0x20) != 0;
        _tcStop           = (value & 0x40) != 0;
        _autoLoad         = (value & 0x80) != 0;
    }
}

uint8_t DMAController::ReadPort(int port) {
    if (port >= 0xa0 && port <= 0xa7) {
        int ch = (port - 0xa0) / 2;
        bool isCount = (port & 1) != 0;
        
        if (!isCount) {
            // Address register
            if (_first) {
                _first = false;
                return static_cast<uint8_t>(_channels[ch].ChAddr & 0xFF);
            } else {
                _first = true;
                return static_cast<uint8_t>(_channels[ch].ChAddr >> 8);
            }
        } else {
            // Count register
            if (_first) {
                _first = false;
                return static_cast<uint8_t>(_channels[ch].ChCount & 0xFF);
            } else {
                _first = true;
                return static_cast<uint8_t>(_channels[ch].ChCount >> 8);
            }
        }
    } else if (port == 0xa8) {
        // Status register – low 4 bits = TC (completed) flags for channels 0-3.
        // Reading clears the completed flags (matches C# behaviour).
        uint8_t status = static_cast<uint8_t>(
            (_channels[0].Completed ? 0x01 : 0x00) |
            (_channels[1].Completed ? 0x02 : 0x00) |
            (_channels[2].Completed ? 0x04 : 0x00) |
            (_channels[3].Completed ? 0x08 : 0x00));
        _channels[0].Completed = false;
        _channels[1].Completed = false;
        _channels[2].Completed = false;
        _channels[3].Completed = false;
        return status;
    }
    
    return 0;
}