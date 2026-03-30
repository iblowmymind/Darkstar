/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>

namespace darkstar {

class IIOPDevice {
public:
    virtual ~IIOPDevice() = default;
    virtual const int* read_ports() const = 0;
    virtual int read_port_count() const = 0;
    virtual const int* write_ports() const = 0;
    virtual int write_port_count() const = 0;
    virtual void write_port(int port, uint8_t value) = 0;
    virtual uint8_t read_port(int port) = 0;
};

} // namespace darkstar
