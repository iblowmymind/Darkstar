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

/// Stub implementation of the Printer port.  This is just
/// enough to get the rigid diagnostic set to pass.
class Printer : public IIOPDevice {
public:
    Printer();

    void reset();

    bool rx_request() const { return rx_request_; }
    bool tx_request() const { return tx_request_; }

    // IIOPDevice interface
    const int* read_ports() const override;
    int read_port_count() const override;
    const int* write_ports() const override;
    int write_port_count() const override;
    void write_port(int port, uint8_t value) override;
    uint8_t read_port(int port) override;

private:
    bool rx_request_;
    bool tx_request_;
    uint8_t tx_data_;

    static const int kReadPorts[];
    static const int kWritePorts[];
};

} // namespace darkstar
