/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include "iop/i8085_io_bus.h"
#include "iop/iop_device.h"

#include <cstdint>

namespace darkstar {

class IOPIOBus : public I8085IOBus {
public:
    IOPIOBus();

    void out(uint8_t port, uint8_t value) override;
    uint8_t in(uint8_t port) override;
    void register_device(IIOPDevice* device) override;

private:
    IIOPDevice* write_dispatch_[256];
    IIOPDevice* read_dispatch_[256];
};

} // namespace darkstar
