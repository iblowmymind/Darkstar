/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>
#include "iop/iop_device.h"

namespace darkstar {

class I8085IOBus {
public:
    virtual ~I8085IOBus() = default;
    virtual void out(uint8_t port, uint8_t value) = 0;
    virtual uint8_t in(uint8_t port) = 0;
    virtual void register_device(IIOPDevice* device) = 0;
};

} // namespace darkstar
