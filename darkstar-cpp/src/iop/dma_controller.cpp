/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/

#include "iop/dma_controller.h"
#include "iop/io_processor.h"
#include "iop/iop_memory_bus.h"
#include "core/log.h"

#include <stdexcept>

namespace darkstar {

// DMAChannel

DMAChannel::DMAChannel()
    : enabled(false)
    , completed(false)
    , ch_addr(0)
    , ch_count(-1)
    , type(DMAType::Invalid)
    , device(nullptr)
{
}

void DMAChannel::reset()
{
    enabled = false;
    completed = false;
    ch_addr = 0;
    ch_count = -1;
    type = DMAType::Invalid;
}

// DMAController

const int DMAController::kReadPorts[] = {
    static_cast<int>(DMAPorts::DmaStatus),
};

const int DMAController::kWritePorts[] = {
    static_cast<int>(DMAPorts::DmaCh0Addr),
    static_cast<int>(DMAPorts::DmaCh0Count),
    static_cast<int>(DMAPorts::DmaCh1Addr),
    static_cast<int>(DMAPorts::DmaCh1Count),
    static_cast<int>(DMAPorts::DmaCh2Addr),
    static_cast<int>(DMAPorts::DmaCh2Count),
    static_cast<int>(DMAPorts::DmaCh3Addr),
    static_cast<int>(DMAPorts::DmaCh3Count),
    static_cast<int>(DMAPorts::DmaMode),
};

DMAController::DMAController(IOProcessor& iop)
    : iop_(iop)
    , first_(true)
    , rotating_priority_(false)
    , extended_write_(false)
    , tc_stop_(false)
    , auto_load_(false)
    , next_to_service_(0)
    , last_selected_channel_(0)
    , hrq_(false)
{
}

void DMAController::register_device(IDMAInterface* device, int channel)
{
    channels_[channel].device = device;
}

bool DMAController::tc() const
{
    if (last_selected_channel_ != -1) {
        return channels_[last_selected_channel_].ch_count == 0;
    }
    return false;
}

void DMAController::reset()
{
    first_ = true;
    rotating_priority_ = false;
    extended_write_ = false;
    tc_stop_ = false;
    auto_load_ = false;
    last_selected_channel_ = 0;
    next_to_service_ = 0;

    for (int i = 0; i < 4; i++) {
        channels_[i].reset();
    }
}

void DMAController::execute()
{
    // See if there's anything to do.
    int next_channel = select_next_channel();

    // Raise HRQ if so.
    hrq_ = (next_channel != -1);

    if (hrq_) {
        DMAChannel& c = channels_[next_channel];

        if (Log::enabled) Log::write(LogComponent::IOPDMA,
            "Channel %d selected.  %d bytes to %d, addr 0x%04x.",
            next_channel, c.ch_count, static_cast<int>(c.type), c.ch_addr);

        switch (c.type) {
            case DMAType::Verify:
                throw std::runtime_error("DMA Verify not implemented.");

            case DMAType::Read:
            {
                // Read byte from memory and transfer to device
                uint8_t dma_write = iop_.memory().read_byte(c.ch_addr);
                c.device->dma_write(dma_write);

                if (Log::enabled) Log::write(LogComponent::IOPDMA,
                    "DMA read transfer of byte 0x%02x from address 0x%04x", dma_write, c.ch_addr);
                break;
            }

            case DMAType::Write:
            {
                // Read byte from device and transfer to memory.
                uint8_t dma_read = c.device->dma_read();
                iop_.memory().write_byte(c.ch_addr, dma_read);

                if (Log::enabled) Log::write(LogComponent::IOPDMA,
                    "DMA write transfer of byte 0x%02x to address 0x%04x", dma_read, c.ch_addr);
                break;
            }

            default:
                break;
        }

        // Increment address, decrement counter.
        c.ch_addr++;
        c.ch_count--;

        // If the counter runs out, stop the channel if so enabled.
        if (c.ch_count == 0) {
            if (Log::enabled) Log::write(LogComponent::IOPDMA, "Channel %d completed.", next_channel);

            if (tc_stop_) {
                c.enabled = false;
                if (Log::enabled) Log::write(LogComponent::IOPDMA, "Channel %d disabled.", next_channel);
            }

            c.completed = true;
            c.device->dma_complete();
        }

        last_selected_channel_ = next_channel;
    }
}

const int* DMAController::read_ports() const { return kReadPorts; }
int DMAController::read_port_count() const { return 1; }
const int* DMAController::write_ports() const { return kWritePorts; }
int DMAController::write_port_count() const { return 9; }

void DMAController::write_port(int port, uint8_t value)
{
    switch (static_cast<DMAPorts>(port)) {
        case DMAPorts::DmaMode:
            first_ = true;

            channels_[0].enabled = (value & 0x01) != 0;
            channels_[1].enabled = (value & 0x02) != 0;
            channels_[2].enabled = (value & 0x04) != 0;
            channels_[3].enabled = (value & 0x08) != 0;

            rotating_priority_ = (value & 0x10) != 0;
            extended_write_ = (value & 0x20) != 0;
            tc_stop_ = (value & 0x40) != 0;
            auto_load_ = (value & 0x80) != 0;

            if (Log::enabled) Log::write(LogComponent::IOPDMA,
                "DMAMode: en: %d,%d,%d,%d rp %d ew %d tc %d al %d",
                channels_[0].enabled, channels_[1].enabled,
                channels_[2].enabled, channels_[3].enabled,
                rotating_priority_, extended_write_, tc_stop_, auto_load_);

            if (auto_load_) {
                throw std::runtime_error("AutoLoad not yet implemented.");
            }
            break;

        case DMAPorts::DmaCh0Addr:
        case DMAPorts::DmaCh1Addr:
        case DMAPorts::DmaCh2Addr:
        case DMAPorts::DmaCh3Addr:
        {
            int ch = (port - 0xa0) / 2;
            if (first_) {
                channels_[ch].ch_addr = value;
            } else {
                channels_[ch].ch_addr = static_cast<uint16_t>(channels_[ch].ch_addr | (value << 8));
                if (Log::enabled) Log::write(LogComponent::IOPDMA,
                    "Channel %d address set to 0x%04x", ch, channels_[ch].ch_addr);
            }
            first_ = !first_;
            break;
        }

        case DMAPorts::DmaCh0Count:
        case DMAPorts::DmaCh1Count:
        case DMAPorts::DmaCh2Count:
        case DMAPorts::DmaCh3Count:
        {
            int ch = (port - 0xa1) / 2;
            if (first_) {
                channels_[ch].ch_count = value;
            } else {
                // + 1 because the value loaded is the number of bytes-1.
                channels_[ch].ch_count = static_cast<uint16_t>(channels_[ch].ch_count | ((value & 0x3f) << 8)) + 1;
                channels_[ch].type = static_cast<DMAType>(value >> 6);

                if (Log::enabled) Log::write(LogComponent::IOPDMA,
                    "Channel %d count set to 0x%04x", ch, channels_[ch].ch_count);
                if (Log::enabled) Log::write(LogComponent::IOPDMA,
                    "Channel %d type set to %d", ch, static_cast<int>(channels_[ch].type));
            }
            first_ = !first_;
            break;
        }

        default:
            throw std::runtime_error("Unexpected write to DMA port.");
    }
}

uint8_t DMAController::read_port(int port)
{
    uint8_t value = 0;

    switch (static_cast<DMAPorts>(port)) {
        case DMAPorts::DmaStatus:
            // "The low 4 bits of this register indicate what channels have completed."
            value = static_cast<uint8_t>(
                (channels_[0].completed ? 0x01 : 0x00) |
                (channels_[1].completed ? 0x02 : 0x00) |
                (channels_[2].completed ? 0x04 : 0x00) |
                (channels_[3].completed ? 0x08 : 0x00));

            if (Log::enabled) Log::write(LogComponent::IOPDMA, "DMAStatus read %d", value);

            // TC Status bits are cleared after the status register is read.
            channels_[0].completed = false;
            channels_[1].completed = false;
            channels_[2].completed = false;
            channels_[3].completed = false;
            break;

        default:
            throw std::runtime_error("Unexpected read from DMA port.");
    }

    return value;
}

int DMAController::select_next_channel()
{
    int next_channel = -1;

    if (!rotating_priority_) {
        // Select the highest priority channel that has something to do.
        // Channel 0 has the highest priority.
        for (int i = 0; i < 4; i++) {
            if (channels_[i].enabled && channels_[i].device != nullptr && channels_[i].device->drq()) {
                next_channel = i;
                break;
            }
        }
    } else {
        // Select the next channel in the cycle, if it has something to do.
        for (int i = 0; i < 4; i++) {
            int j = (i + next_to_service_) % 4;
            if (channels_[j].enabled && channels_[j].device != nullptr && channels_[j].device->drq()) {
                next_channel = j;
                next_to_service_ = j + 1;
                break;
            }
        }
    }

    return next_channel;
}

} // namespace darkstar
