/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/

#include "iop/misc_io.h"
#include "iop/io_processor.h"
#include "iop/keyboard.h"
#include "iop/mouse.h"
#include "iop/beeper.h"
#include "iop/dma_controller.h"
#include "iop/floppy_controller.h"
#include "core/log.h"

#include <stdexcept>

namespace darkstar {

const int MiscIO::kReadPorts[] = {
    0xd0,   // DMA Test Register
    0xe9,   // Interrupt request bits (read)
    0xea,   // Keyboard data latch
    0xed,   // Mouse X counter
    0xee,   // Mouse Y counter
    0xef,   // Miscellaneous input
};

const int MiscIO::kWritePorts[] = {
    0x8d,   // i8253 Timer Control 1 (Tone frequency)
    0x8f,   // i8253 Timer Mode
    0xd0,   // DMA Test Register
    0xe9,   // KB, MP, TOD clocks (write)
    0xea,   // Clear TOD interrupt (write)
    0xed,   // Clear Mouse X,Y counters
    0xef,   // KB, MP, TOD control (write)
};

MiscIO::MiscIO(IOProcessor& iop)
    : iop_(iop)
    , mpanel_blank_(true)
    , mpanel_value_(0)
    , alt_boot_counter_(0)
    , alt_boot_(AltBootValues::None)
    , last_clock_flags_(0)
    , dma_test_value_(0)
{
    // Default to no alt boot
    alt_boot_ = AltBootValues::None;
    reset();
}

void MiscIO::set_alt_boot(AltBootValues value)
{
    alt_boot_ = value;
    alt_boot_counter_ = static_cast<int>(value);
}

void MiscIO::reset()
{
    mpanel_value_ = 0;
    mpanel_blank_ = true;
    last_clock_flags_ = 0;
    dma_test_value_ = 0;
    alt_boot_counter_ = static_cast<int>(alt_boot_);
    tod_clock_.reset();
}

const int* MiscIO::read_ports() const { return kReadPorts; }
int MiscIO::read_port_count() const { return 6; }
const int* MiscIO::write_ports() const { return kWritePorts; }
int MiscIO::write_port_count() const { return 7; }

void MiscIO::write_port(int port, uint8_t value)
{
    switch (port) {
        case 0x8d:
            // i8253 Timer channel #1 - used to set the Keyboard bell (tone) frequency.
            iop_.beeper().load_period(value);
            break;

        case 0x8f:
            // i8253 Timer Mode.
            if (Log::enabled) Log::write(LogComponent::IOPMisc, "Misc IO port Timer Mode written %02x", value);
            break;

        case 0xd0:
            // DMA Test Register
            dma_test_value_ = value;
            if (Log::enabled) Log::write(LogComponent::IOPMisc, "Misc IO port DMATest write %02x", value);
            break;

        case 0xe9:     // KB, MP, TOD clocks
            do_misc_clock(value);
            break;

        case 0xea:     // Clear TOD interrupt
            tod_clock_.clear_interrupt();
            if (Log::enabled) Log::write(LogComponent::IOPMisc, "Misc IO TOD interrupt clear.");
            break;

        case 0xed:     // Clear Mouse X,Y counters
            iop_.mouse().clear();
            break;

        case 0xef:     // KB, MP, TOD control
        {
            // Control bits:
            // 0x40 - pReadKBData - read KB data
            // 0x20 - KBTone - KB speaker bit
            // 0x10 - KBDiag - Set KB Diag mode
            // 0x08 - BlankMPanel - Blank MPanel bit
            // 0x04 - ReadTimeMode - Read TOD mode bit
            // 0x02 - ClearTimeMode - Clear TOD mode bit
            // 0x01 - SetTimeMode - Set TOD mode bit
            mpanel_blank_ = (value & 0x08) != 0;
            if (mp_changed) mp_changed();

            if ((value & 0x40) != 0) {
                // Prime the next byte of keyboard data.
                iop_.keyboard().next_data();
                if (Log::enabled) Log::write(LogComponent::IOPMisc, "Misc IO Keyboard data clock.");
            }

            if ((value & 0x20) != 0) {
                iop_.beeper().enable_tone();
            } else {
                iop_.beeper().disable_tone();
            }

            if ((value & 0x10) != 0) {
                iop_.keyboard().enable_diagnostic_mode();
                if (Log::enabled) Log::write(LogComponent::IOPMisc, "Misc IO Keyboard diagnostic mode entered.");
            }

            if ((value & 0x04) != 0) {
                tod_clock_.set_mode(TODAccessMode::Read);
            }

            if ((value & 0x02) != 0) {
                tod_clock_.set_mode(TODAccessMode::Clear);
            }

            if ((value & 0x01) != 0) {
                tod_clock_.set_mode(TODAccessMode::Set);
            }
            break;
        }

        default:
            throw std::runtime_error("Unexpected write to MiscIO port.");
    }
}

uint8_t MiscIO::read_port(int port)
{
    uint8_t value = 0;

    switch (port) {
        case 0xd0:
            // DMA Test Register: just return whatever value was written.
            value = dma_test_value_;
            if (Log::enabled) Log::write(LogComponent::IOPMisc, "Misc IO port DMATest read %02x", value);
            break;

        case 0xef:
        {
            // MiscInput1: AltBoot,TimeData,PowerFailed,TODInt,CSParError,MouseSw1,Sw2,Sw3
            if (alt_boot_counter_ > 0) {
                value = kAltBoot;
                alt_boot_counter_--;
            } else {
                value = 0;
            }

            // OR in other bits
            value = static_cast<uint8_t>(value |
                tod_clock_.read_clock_bit() |
                (tod_clock_.power_loss() ? 0x20 : 0x0) |
                (tod_clock_.interrupt() ? 0x10 : 0x0) |
                static_cast<int>(iop_.mouse().buttons()) |
                kCSParity /* active low, we don't want parity errors */);

            if (Log::enabled) Log::write(LogComponent::IOPMisc, "Misc IO port MiscInput1 read %02x", value);
            break;
        }

        case 0xe9:
        {
            // Interrupt status register.
            // All signals are active low.
            value = static_cast<uint8_t>(~(
                (iop_.floppy_controller().interrupt() ? 0x80 : 0x00) |
                (iop_.keyboard().data_ready() ? 0x40 : 0x00)));

            break;
        }

        case 0xea:
        {
            // Keyboard data latch. Data is inverted.
            value = static_cast<uint8_t>(~iop_.keyboard().read_data());
            if (Log::enabled) Log::write(LogComponent::IOPMisc, "Misc IO port Keyboard Data read %02x", value);
            break;
        }

        case 0xed:
            // Mouse X counter
            value = static_cast<uint8_t>(iop_.mouse().mouse_x());
            break;

        case 0xee:
            // Mouse Y counter
            value = static_cast<uint8_t>(iop_.mouse().mouse_y());
            break;

        default:
            value = 0;
            break;
    }

    return value;
}

void MiscIO::do_misc_clock(uint8_t clock_flags)
{
    // On a 1->0 transition for a clock bit we will take the appropriate action.
    for (int clock_flag = 0x1; clock_flag < 0x100; clock_flag <<= 1) {
        if ((clock_flags & clock_flag) == 0 &&
            (last_clock_flags_ & clock_flag) != 0) {

            switch (clock_flag) {
                case kClrMPanel:
                    mpanel_value_ = 0;
                    if (mp_changed) mp_changed();
                    break;

                case kIncMPanel:
                    mpanel_value_ = (mpanel_value_ + 1) % 10000;
                    if (mp_changed) mp_changed();
                    break;

                case kTODRead:
                    tod_clock_.clock_bit(TODClockType::Read);
                    break;

                case kTODSetA:
                    tod_clock_.clock_bit(TODClockType::SetA);
                    break;

                case kTODSetB:
                    tod_clock_.clock_bit(TODClockType::SetB);
                    break;

                case kTODSetC:
                    tod_clock_.clock_bit(TODClockType::SetC);
                    break;

                case kTODSetD:
                    tod_clock_.clock_bit(TODClockType::SetD);
                    break;
            }
        }
    }

    last_clock_flags_ = clock_flags;
}

} // namespace darkstar
