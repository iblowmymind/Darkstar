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

#include "floppy_controller.h"
#include "../dsystem.h"
#include "../scheduler.h"
#include "../types.h"

// Port definitions
const std::vector<int> FloppyController::_readPorts = {0x84, 0x85, 0x86, 0x87, 0xe8};
const std::vector<int> FloppyController::_writePorts = {0x84, 0x85, 0x86, 0x87, 0xe8};

// FDC Commands
enum FDCCommand {
    Restore     = 0x00,  // Bits 7-4: 0000, bits 3-0: flags
    Seek        = 0x10,  // Bits 7-4: 0001, bits 3-0: flags
    Step        = 0x20,  // Bits 7-4: 0010, bits 3-0: flags
    StepIn      = 0x40,  // Bits 7-4: 0100, bits 3-0: flags
    StepOut     = 0x60,  // Bits 7-4: 0110, bits 3-0: flags
    ReadSector  = 0x80,  // Bits 7-4: 1000, bits 3-0: flags
    WriteSector = 0xa0,  // Bits 7-4: 1010, bits 3-0: flags
    ReadAddress = 0xc0,  // Bits 7-4: 1100, bits 3-0: flags
    ReadTrack   = 0xe0,  // Bits 7-4: 1110, bits 3-0: flags
    WriteTrack  = 0xf0,  // Bits 7-4: 1111, bits 3-0: flags
    ForceInt    = 0xd0,  // Bits 7-4: 1101, bits 3-0: flags
};

// Status bits
enum StatusBits {
    NotReady    = 0x80,
    Protected   = 0x40,
    RecordType  = 0x20,
    RecordNotFound = 0x10,
    CrcError    = 0x08,
    Track0      = 0x04,
    LostData    = 0x02,
    Busy        = 0x01,
};

FloppyController::FloppyController(FloppyDrive* drive, DSystem* system) 
    : _drive(drive), _system(system) {
    Reset();
}

void FloppyController::Reset() {
    _interruptPending = false;
    _status = 0;
    _track = 0;
    _sector = 1;
    _data = 0;
    _extStatus = 0;
    _extState = 0;
    _busy = false;
    _drq = false;
    _drqCounter = 0;
    _trackBuffer.clear();
    _trackBufferIndex = 0;
}

bool FloppyController::DRQ() {
    if (_drqCounter > 0) {
        _drqCounter--;
        return _drqCounter == 0;
    }
    return _drq;
}

void FloppyController::DMAWrite(uint8_t value) {
    if (_trackBuffer.size() > _trackBufferIndex) {
        _trackBuffer[_trackBufferIndex] = value;
        _trackBufferIndex++;
    }
    _drq = false;
}

uint8_t FloppyController::DMARead() {
    uint8_t value = 0;
    if (_trackBuffer.size() > _trackBufferIndex) {
        value = _trackBuffer[_trackBufferIndex];
        _trackBufferIndex++;
    }
    _drq = false;
    return value;
}

void FloppyController::DMAComplete() {
    _drq = false;
    _busy = false;
    _status &= ~Busy;
    RaiseInterrupt();
}

const std::vector<int>& FloppyController::ReadPorts() const {
    return _readPorts;
}

const std::vector<int>& FloppyController::WritePorts() const {
    return _writePorts;
}

void FloppyController::WritePort(int port, uint8_t value) {
    switch (port) {
    case 0x84: // FDC Command
        {
            uint8_t cmd = value & 0xf0;
            _status |= Busy;
            _busy = true;
            
            switch (cmd) {
            case Restore:
                _track = 0;
                if (_drive) {
                    _drive->SeekTo(0);
                }
                // Schedule completion
                _system->GetScheduler()->Schedule(
                    6 * Conversion::MsecToNsec,
                    [this](uint64_t skew, void* ctx) { SeekCallback(skew, ctx); }
                );
                break;
                
            case Seek:
                _track = _data;
                if (_drive) {
                    _drive->SeekTo(_track);
                }
                _system->GetScheduler()->Schedule(
                    6 * Conversion::MsecToNsec,
                    [this](uint64_t skew, void* ctx) { SeekCallback(skew, ctx); }
                );
                break;
                
            case ReadSector:
                // Start sector read operation
                _drq = true;
                _drqCounter = 5;
                // Schedule sector transfer
                _system->GetScheduler()->Schedule(
                    12 * Conversion::UsecToNsec,
                    [this](uint64_t skew, void* ctx) { SectorTransferCallback(skew, ctx); }
                );
                break;
                
            case WriteSector:
                // Start sector write operation
                _drq = true;
                _drqCounter = 5;
                _system->GetScheduler()->Schedule(
                    12 * Conversion::UsecToNsec,
                    [this](uint64_t skew, void* ctx) { SectorTransferCallback(skew, ctx); }
                );
                break;
                
            case WriteTrack:
                // Start track write operation
                _trackBuffer.resize(6250); // Max track size
                _trackBufferIndex = 0;
                _drq = true;
                _drqCounter = 5;
                _system->GetScheduler()->Schedule(
                    10 * Conversion::MsecToNsec,
                    [this](uint64_t skew, void* ctx) { WriteTrackCallback(skew, ctx); }
                );
                break;
                
            default:
                _status &= ~Busy;
                _busy = false;
                break;
            }
        }
        break;
        
    case 0x85: // Track register
        _track = value;
        break;
        
    case 0x86: // Sector register
        _sector = value;
        break;
        
    case 0x87: // Data register
        _data = value;
        break;
        
    case 0xe8: // Extended FDC state
        _extState = value;
        if (_drive) {
            _drive->DriveSelect = (value & 0x01) != 0;
        }
        break;
    }
}

uint8_t FloppyController::ReadPort(int port) {
    switch (port) {
    case 0x84: // FDC Status
        {
            uint8_t status = _status;
            if (_drive) {
                if (!_drive->IsLoaded) status |= NotReady;
                if (_drive->IsWriteProtected) status |= Protected;
                if (_drive->Track0) status |= Track0;
            }
            return status;
        }
        
    case 0x85: // Track register
        return _track;
        
    case 0x86: // Sector register
        return _sector;
        
    case 0x87: // Data register
        return _data;
        
    case 0xe8: // Extended FDC status
        {
            uint8_t status = _extStatus;
            if (_drive) {
                if (_drive->DiskChange) status |= 0x40;
                if (_drive->Index) status |= 0x20;
            }
            return status;
        }
    }
    
    return 0;
}

void FloppyController::RaiseInterrupt() {
    _interruptPending = true;
    if (_raiseInterruptCallback) {
        _raiseInterruptCallback();
    }
}

void FloppyController::SeekCallback(uint64_t skewNsec, void* context) {
    _status &= ~Busy;
    _busy = false;
    RaiseInterrupt();
}

void FloppyController::StepCallback(uint64_t skewNsec, void* context) {
    _status &= ~Busy;
    _busy = false;
    RaiseInterrupt();
}

void FloppyController::SectorTransferCallback(uint64_t skewNsec, void* context) {
    // Sector transfer completed
    _status &= ~Busy;
    _busy = false;
    _drq = false;
    RaiseInterrupt();
}

void FloppyController::WriteTrackCallback(uint64_t skewNsec, void* context) {
    // Track write completed
    FinishWriteTrack();
    _status &= ~Busy;
    _busy = false;
    _drq = false;
    RaiseInterrupt();
}

void FloppyController::ResetIndexCallback(uint64_t skewNsec, void* context) {
    if (_drive) {
        _drive->Index = false;
    }
}

void FloppyController::FinishWriteTrack() {
    // Parse the track data and format the track
    // This is a simplified implementation
    if (_drive && _drive->Disk && _trackBuffer.size() > 0) {
        // Simple track format: just write sectors sequentially
        int sectorSize = 256; // Default sector size
        int sectorCount = _trackBuffer.size() / sectorSize;
        if (sectorCount > 10) sectorCount = 10; // Limit to reasonable number
        
        _drive->Disk->FormatTrack(Format::FM500, _track, 0, sectorCount, sectorSize);
        _drive->Disk->SetModified();
    }
}