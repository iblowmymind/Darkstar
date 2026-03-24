/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
*/
#include "shugart_controller.h"

ShugartController::ShugartController(DSystem* system, SA1000Drive* drive)
    : _system(system), _drive(drive) {
    Reset();
}

void ShugartController::Reset() {
    _cylinder = 0;
    _head = 0;
    _sector = 0;
    _seekComplete = false;
    _dataReady = false;
    _bufPos = 0;
    _reading = false;
    _writing = false;
}

uint8_t ShugartController::ReadPort(int port) {
    (void)port;
    if (_dataReady && _reading && _bufPos < SA1000Drive::BYTES_PER_SECTOR)
        return _sectorBuf[_bufPos++];
    return 0;
}

void ShugartController::WritePort(int port, uint8_t value) {
    (void)port; (void)value;
}

void ShugartController::Execute() {
    if (_reading && !_dataReady) {
        if (_drive && _drive->IsLoaded()) {
            _drive->ReadSector(_cylinder, _head, _sector, _sectorBuf);
        }
        _dataReady = true;
        _bufPos = 0;
    }
}
