/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/

#include "iop/iop_io_bus.h"
#include "core/log.h"

#include <cstring>
#include <stdexcept>

namespace darkstar {

IOPIOBus::IOPIOBus() {
    std::memset(write_dispatch_, 0, sizeof(write_dispatch_));
    std::memset(read_dispatch_, 0, sizeof(read_dispatch_));
}

void IOPIOBus::register_device(IIOPDevice* device) {
    const int* read_ports = device->read_ports();
    int read_count = device->read_port_count();
    for (int i = 0; i < read_count; i++) {
        int port = read_ports[i];
        if (read_dispatch_[port] != nullptr) {
            throw std::runtime_error("Read port collision when adding IOP device");
        }
        read_dispatch_[port] = device;
    }

    const int* write_ports = device->write_ports();
    int write_count = device->write_port_count();
    for (int i = 0; i < write_count; i++) {
        int port = write_ports[i];
        if (write_dispatch_[port] != nullptr) {
            throw std::runtime_error("Write port collision when adding IOP device");
        }
        write_dispatch_[port] = device;
    }
}

void IOPIOBus::out(uint8_t port, uint8_t value) {
    if (write_dispatch_[port] != nullptr) {
        write_dispatch_[port]->write_port(port, value);
    } else {
        if (Log::enabled) Log::write(LogComponent::IOPIO, "Unhandled write to IO port %02x, %02x", port, value);
    }
}

uint8_t IOPIOBus::in(uint8_t port) {
    if (read_dispatch_[port] != nullptr) {
        return read_dispatch_[port]->read_port(port);
    } else {
        if (Log::enabled) Log::write(LogComponent::IOPIO, "Unhandled read from IO port %02x", port);
        return 0x00;
    }
}

} // namespace darkstar
