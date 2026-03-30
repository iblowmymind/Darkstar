/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>
#include "iop/iop_device.h"
#include "iop/dma_interface.h"

namespace darkstar {

class IOProcessor;

enum class DMAType {
    Verify  = 0,
    Write   = 1,
    Read    = 2,
    Invalid = 3,
};

struct DMAChannel {
    DMAChannel();
    void reset();

    bool enabled;
    bool completed;
    uint16_t ch_addr;
    int ch_count;
    DMAType type;
    IDMAInterface* device;
};

/// Implements the general behavior of the Intel 8257 as used in the Star.
class DMAController : public IIOPDevice {
public:
    explicit DMAController(IOProcessor& iop);

    void register_device(IDMAInterface* device, int channel);
    DMAChannel& get_channel(int i) { return channels_[i]; }

    /// Hold request. Goes high when the DMA controller is taking control of the bus.
    bool hrq() const { return hrq_; }

    /// Terminal Count: activated when the selected channel's terminal count register equals zero.
    bool tc() const;

    void reset();
    void execute();

    // IIOPDevice interface
    const int* read_ports() const override;
    int read_port_count() const override;
    const int* write_ports() const override;
    int write_port_count() const override;
    void write_port(int port, uint8_t value) override;
    uint8_t read_port(int port) override;

private:
    int select_next_channel();

    IOProcessor& iop_;

    // Which address byte to store when loading registers.
    bool first_;

    DMAChannel channels_[4];

    // DMA Mode Flags
    bool rotating_priority_;
    bool extended_write_;
    bool tc_stop_;
    bool auto_load_;

    // Scheduling
    int next_to_service_;
    int last_selected_channel_;

    bool hrq_;

    enum class DMAPorts {
        DmaCh0Addr  = 0xa0,
        DmaCh0Count = 0xa1,
        DmaCh1Addr  = 0xa2,
        DmaCh1Count = 0xa3,
        DmaCh2Addr  = 0xa4,
        DmaCh2Count = 0xa5,
        DmaCh3Addr  = 0xa6,
        DmaCh3Count = 0xa7,
        DmaMode     = 0xa8,    // Write
        DmaStatus   = 0xa8,    // Read
    };

    static const int kReadPorts[];
    static const int kWritePorts[];
};

} // namespace darkstar
