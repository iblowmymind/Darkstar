/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>
#include <functional>
#include "iop/iop_device.h"
#include "iop/tod_clock.h"
#include "core/configuration.h"

namespace darkstar {

class IOProcessor;

class MiscIO : public IIOPDevice {
public:
    explicit MiscIO(IOProcessor& iop);

    void reset();

    // IIOPDevice interface
    const int* read_ports() const override;
    int read_port_count() const override;
    const int* write_ports() const override;
    int write_port_count() const override;
    void write_port(int port, uint8_t value) override;
    uint8_t read_port(int port) override;

    AltBootValues alt_boot() const { return alt_boot_; }
    void set_alt_boot(AltBootValues value);

    int mpanel_value() const { return mpanel_value_; }
    bool mpanel_blank() const { return mpanel_blank_; }

    TODClock& tod_clock() { return tod_clock_; }

    /// Callback for UI notification when the MP value changes.
    std::function<void()> mp_changed;

private:
    void do_misc_clock(uint8_t clock_flags);

    IOProcessor& iop_;
    TODClock tod_clock_;

    // MP data
    bool mpanel_blank_;
    int mpanel_value_;

    // Alt boot counter
    int alt_boot_counter_;
    AltBootValues alt_boot_;

    // Clock register data
    int last_clock_flags_;

    // DMA Test Register data
    uint8_t dma_test_value_;

    // Clock flag bits
    enum ClockFlags {
        kClrMPanel = 0x40,
        kIncMPanel = 0x20,
        kTODRead   = 0x10,
        kTODSetA   = 0x08,
        kTODSetB   = 0x04,
        kTODSetC   = 0x02,
        kTODSetD   = 0x01,
    };

    // MiscInput1 flag bits
    enum MiscInput1Flags {
        kAltBoot     = 0x80,
        kTODData     = 0x40,
        kPowerFailed = 0x20,
        kTODInt      = 0x10,
        kCSParity    = 0x08,
        kMouseSw3    = 0x04,
        kMouseSw2    = 0x02,
        kMouseSw1    = 0x01,
    };

    static const int kReadPorts[];
    static const int kWritePorts[];
};

} // namespace darkstar
