/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/

#include "iop/printer.h"
#include "core/log.h"

namespace darkstar {

const int Printer::kReadPorts[] = {
    0x88,   // data
    0x89,   // status
};

const int Printer::kWritePorts[] = {
    0x88,   // data
    0x89,   // commands
};

Printer::Printer()
    : rx_request_(true)
    , tx_request_(true)
    , tx_data_(0)
{
    reset();
}

void Printer::reset()
{
    rx_request_ = true;     // active low
    tx_request_ = true;     // always ready to transmit, makes the diags happy.
}

const int* Printer::read_ports() const { return kReadPorts; }
int Printer::read_port_count() const { return 2; }
const int* Printer::write_ports() const { return kWritePorts; }
int Printer::write_port_count() const { return 2; }

uint8_t Printer::read_port(int port)
{
    uint8_t value = 0;
    switch (port) {
        case 0x88:
            if (Log::enabled) Log::write(LogComponent::IOPPrinter, "Stub: Printer data port read.");
            value = tx_data_;   // just loopback data to make diags happy.
            break;

        case 0x89:
            if (Log::enabled) Log::write(LogComponent::IOPPrinter, "Stub: Printer status port read.  Returning DTR");
            value = 0x00;
            rx_request_ = true;
            break;
    }
    return value;
}

void Printer::write_port(int port, uint8_t data)
{
    switch (port) {
        case 0x88:
            if (Log::enabled) Log::write(LogComponent::IOPPrinter, "Stub: Printer data port write 0x%02x.", data);
            tx_data_ = data;
            rx_request_ = false;    // "TTY request is active low."
            break;

        case 0x89:
            if (Log::enabled) Log::write(LogComponent::IOPPrinter, "Stub: Printer control port write 0x%02x.", data);
            break;
    }
}

} // namespace darkstar
