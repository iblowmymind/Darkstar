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
    // Terminal count: any channel completed
    for (int i = 0; i < 4; i++) {
        if (_channels[i].Completed) {
            return true;
        }
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
    // Check if any enabled channel needs service
    _hrq = false;
    
    for (int i = 0; i < 4; i++) {
        DMAChannel& ch = _channels[i];
        if (ch.Enabled && !ch.Completed && ch.Device && ch.Device->DRQ()) {
            _hrq = true;
            break;
        }
    }
    
    if (!_hrq) return;
    
    // Select next channel to service
    int channel = SelectNextChannel();
    if (channel < 0) return;
    
    DMAChannel& ch = _channels[channel];
    if (!ch.Device || !ch.Device->DRQ() || ch.Completed) return;
    
    // Perform DMA transfer
    switch (ch.Type) {
    case DMAType::Write:
        ch.Device->DMAWrite(0); // Data from memory (stubbed)
        break;
        
    case DMAType::Read:
        ch.Device->DMARead(); // Data to memory (stubbed)
        break;
        
    case DMAType::Verify:
        ch.Device->DMARead(); // Verify operation
        break;
        
    case DMAType::Invalid:
        break;
    }
    
    // Update address and count
    ch.ChAddr++;
    if (ch.ChCount > 0) {
        ch.ChCount--;
        if (ch.ChCount == 0) {
            ch.Completed = true;
            ch.Device->DMAComplete();
        }
    }
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
        int channel = (port - 0xa0) / 2;
        bool isCount = (port & 1) == 1;
        
        if (isCount) {
            // Count register
            if (_first) {
                _channels[channel].ChCount = value;
                _first = false;
            } else {
                _channels[channel].ChCount |= (static_cast<int>(value) << 8);
                _first = true;
            }
        } else {
            // Address register
            if (_first) {
                _channels[channel].ChAddr = value;
                _first = false;
            } else {
                _channels[channel].ChAddr |= (static_cast<uint16_t>(value) << 8);
                _first = true;
            }
        }
    } else if (port == 0xa8) {
        // Mode register
        int channel = value & 0x03;
        _channels[channel].Type = static_cast<DMAType>((value >> 2) & 0x03);
        _channels[channel].Enabled = true;
        
        // Control bits
        _rotatingPriority = (value & 0x10) != 0;
        _extendedWrite = (value & 0x20) != 0;
        _tcStop = (value & 0x40) != 0;
        _autoLoad = (value & 0x80) != 0;
    }
}

uint8_t DMAController::ReadPort(int port) {
    if (port >= 0xa0 && port <= 0xa7) {
        int channel = (port - 0xa0) / 2;
        bool isCount = (port & 1) == 1;
        
        if (isCount) {
            // Count register
            if (_first) {
                _first = false;
                return static_cast<uint8_t>(_channels[channel].ChCount & 0xFF);
            } else {
                _first = true;
                return static_cast<uint8_t>(_channels[channel].ChCount >> 8);
            }
        } else {
            // Address register
            if (_first) {
                _first = false;
                return static_cast<uint8_t>(_channels[channel].ChAddr & 0xFF);
            } else {
                _first = true;
                return static_cast<uint8_t>(_channels[channel].ChAddr >> 8);
            }
        }
    } else if (port == 0xa8) {
        // Status register
        uint8_t status = 0;
        for (int i = 0; i < 4; i++) {
            if (_channels[i].Completed) {
                status |= (1 << i);
            }
        }
        return status;
    }
    
    return 0;
}