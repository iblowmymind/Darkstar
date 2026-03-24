/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
*/
#pragma once
#include "../types.h"
#include "sa1000.h"

class DSystem;

// ShugartController emulates the SA1000-compatible hard disk controller.
class ShugartController {
public:
    explicit ShugartController(DSystem* system, SA1000Drive* drive);

    void Reset();

    // Read a port value
    uint8_t ReadPort(int port);
    // Write a port value
    void WritePort(int port, uint8_t value);

    // Execute one controller step (called from DMA or scheduler)
    void Execute();

private:
    DSystem*    _system;
    SA1000Drive* _drive;

    int  _cylinder{0};
    int  _head{0};
    int  _sector{0};
    bool _seekComplete{false};
    bool _dataReady{false};
    uint8_t _sectorBuf[SA1000Drive::BYTES_PER_SECTOR]{};
    int  _bufPos{0};
    bool _reading{false};
    bool _writing{false};
};
