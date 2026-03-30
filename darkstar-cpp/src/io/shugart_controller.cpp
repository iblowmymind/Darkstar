/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/

#include "io/shugart_controller.h"
#include "io/sa1000.h"
#include "core/log.h"
#include "core/system.h"
#include "cp/task_type.h"

#include <stdexcept>

namespace darkstar {

// Forward declarations for CP methods we need
// The DSystem header provides cp() accessor; CentralProcessor must have
// wake_task() and sleep_task() methods.

ShugartController::ShugartController(DSystem& system, SA1000Drive& drive)
    : system_(system)
    , drive_(drive)
{
    write_pipeline_ = std::queue<uint16_t>();
}

void ShugartController::reset() {
    write_enable_ = false;
    wakeup_control_ = 0;
    write_crc_ = false;
    transfer_enable_ = false;
    firmware_enable_ = false;
    direction_in_ = false;
    step_ = false;
    reduce_iw_ = false;
    fault_clear_ = false;
    drive_select_ = false;
    head_select_ = 0;
    wakeup_request_ = ServiceRequest::NoWakeup0;

    verify_error_ = false;
    crc_error_ = false;
    overrun_ = false;
    write_fault_ = false;
    sa1000_ = true;
    sector_found_ = false;
    index_found_ = false;
    read_word_ready_ = false;

    while (!write_pipeline_.empty()) write_pipeline_.pop();

    reset_transfer();
}

void ShugartController::set_k_ctl(uint16_t value) {
    write_enable_ = (value & 0x0001) != 0;
    wakeup_control_ = (value & 0x0006) >> 1;
    write_crc_ = (value & 0x0008) != 0;
    transfer_enable_ = (value & 0x0010) != 0;
    firmware_enable_ = (value & 0x0020) != 0;
    direction_in_ = (value & 0x0040) != 0;
    step_ = (value & 0x0080) != 0;
    reduce_iw_ = (value & 0x0100) != 0;
    fault_clear_ = (value & 0x0200) != 0;
    drive_select_ = (value & 0x0400) != 0;
    head_select_ = (value & 0xf800) >> 11;

    wakeup_request_ = static_cast<ServiceRequest>(
        wakeup_control_ | (transfer_enable_ ? 0x4 : 0x0));

    // WriteFault cleared by de-selecting the drive.
    if (!drive_select_) {
        write_fault_ = false;
    }

    // Step the drive
    drive_.step(direction_in_, step_);

    // Select the head
    drive_.set_head(head_select_);

    // Handle wakeup requests
    bool wake = false;
    switch (wakeup_request_) {
        case ServiceRequest::FirmwareEnable:
            wake = firmware_enable_;
            break;
        case ServiceRequest::SeekComplete:
            wake = seek_complete_status();
            break;
        case ServiceRequest::IndexFound:
            wake = index_found_;
            break;
        case ServiceRequest::SectorFound:
            wake = sector_found_;
            break;
        default:
            wake = false;
            break;
    }

    if (transfer_enable_) {
        if (write_enable_ && wakeup_request_ == ServiceRequest::WriteWordNeeded) {
            transfer_ = TransferType::Write;
        } else if (!write_enable_ && wakeup_request_ == ServiceRequest::WriteWordNeeded) {
            transfer_ = TransferType::Verify;
        } else if (!write_enable_ && transfer_enable_ && wakeup_request_ == ServiceRequest::ReadWordReady) {
            transfer_ = TransferType::Read;
        } else {
            // Unexpected combination - log but don't crash in release
            if (Log::enabled) Log::write(LogType::Error, LogComponent::ShugartControl,
                "Unexpected combination of service request and write enable flags");
        }
    } else {
        reset_transfer();
    }

    if (wake) {
        system_.cp().wake_task(TaskType::Disk);
    } else {
        system_.cp().sleep_task(TaskType::Disk);
    }
}

void ShugartController::set_k_cmd(uint16_t value) {
    if (Log::enabled) Log::write(LogType::Verbose, LogComponent::ShugartControl,
        "KCmd<-0x%04x unimplemented.", value);
}

void ShugartController::clr_k_flags() {
    if (wakeup_request_ != ServiceRequest::FirmwareEnable &&
        wakeup_request_ != ServiceRequest::SeekComplete) {
        system_.cp().sleep_task(TaskType::Disk);
    }

    verify_error_ = false;
    crc_error_ = false;
    overrun_ = false;
    index_found_ = false;
    sector_found_ = false;

    if (Log::enabled) Log::write(LogType::Verbose, LogComponent::ShugartControl, "ClrKFlags");
}

void ShugartController::k_strobe() {
    if (Log::enabled) Log::write(LogType::Warning, LogComponent::ShugartControl,
        "KStrobe unimplemented");
}

uint16_t ShugartController::read_k_status() {
    // All status bits are inverted on the X bus.
    uint16_t value = static_cast<uint16_t>(~(
        (verify_error_ ? 0x0001 : 0x0000) |
        (crc_error_ ? 0x0002 : 0x0000) |
        (overrun_ ? 0x0004 : 0x0000) |
        (write_fault_ ? 0x0008 : 0x0000) |
        (!drive_.is_ready() ? 0x0010 : 0x0000) |
        (sa1000_ ? 0x0020 : 0x0000) |
        (!sector_found_ ? 0x0040 : 0x0000) |
        (index_found_ ? 0x0080 : 0x0000) |
        (firmware_enable_ ? 0x0100 : 0x0000) |
        (drive_.track0() ? 0x0200 : 0x0000) |
        (seek_complete_status() ? 0x0400 : 0x0000) |
        ((~head_select_ & 0x1f) << 11)
    ));

    return value;
}

uint16_t ShugartController::read_k_test() {
    int value = 0;

    switch (drive_.type()) {
        case DriveType::SA1004:
            value = 0;
            break;
        case DriveType::Q2040:
            value = 0x40;
            break;
        case DriveType::Q2080:
            value = ((~head_select_) & 0x10) == 0 ? 0x40 : 0;
            break;
        default:
            break;
    }

    return static_cast<uint16_t>(value);
}

void ShugartController::set_k_o_data(uint16_t value) {
    if (write_pipeline_.size() > 1) {
        if (Log::enabled) Log::write(LogType::Error, LogComponent::ShugartControl,
            "KOData<- : Not ready for write! (c/h/s/f %d/%d/%d/%d)",
            drive_.cylinder(), drive_.head(), debug_sector_, debug_field_);
    } else {
        write_pipeline_.push(value);
        if (Log::enabled) Log::write(LogComponent::ShugartControl,
            "KOData<- : 0x%04x latched", value);
    }

    if (wakeup_request_ == ServiceRequest::WriteWordNeeded) {
        system_.cp().sleep_task(TaskType::Disk);
    }
}

uint16_t ShugartController::read_k_i_data() {
    if (!read_word_ready_) {
        if (Log::enabled) Log::write(LogType::Error, LogComponent::ShugartControl,
            "<-KIData : Not ready for read! (c/h/s/f %d/%d/%d/%d)",
            drive_.cylinder(), drive_.head(), debug_sector_, debug_field_);
        overrun_ = true;
    }

    if (wakeup_request_ == ServiceRequest::ReadWordReady) {
        system_.cp().sleep_task(TaskType::Disk);
    }

    read_word_ready_ = false;

    return static_cast<uint16_t>(read_data_);
}

void ShugartController::signal_disk_word_ready() {
    if (drive_.index()) {
        index_found_ = true;

        if (wakeup_request_ == ServiceRequest::IndexFound) {
            system_.cp().wake_task(TaskType::Disk);
        }

        debug_sector_ = -1;
        debug_field_ = -1;
    }

    // Check for overrun on reads
    if (read_word_ready_ &&
        transfer_enable_ &&
        transfer_ == TransferType::Read &&
        read_state_ == ReadState::Data) {
        if (Log::enabled) Log::write(LogType::Error, LogComponent::ShugartControl,
            "Read data: overrun.");
        overrun_ = true;
    }

    read_word_ready_ = true;
    read_data_ = drive_.read_data();

    // Check for Header Address Mark
    if (read_data_ == kHeaderAddressMark) {
        sector_found_ = true;

        debug_sector_++;
        debug_field_ = 0;
    }

    if (read_data_ == kLabelDataAddressMark) {
        debug_field_++;
    }

    // Run the transfer state machine
    if (transfer_enable_) {
        bool wake = false;
        switch (transfer_) {
            case TransferType::Write:
                switch (write_state_) {
                    case WriteState::AutoPreamble:
                        drive_.write_data(0);
                        transfer_count_++;
                        if (transfer_count_ > 1) {
                            transfer_count_ = 0;
                            write_state_ = WriteState::Preamble;
                            wake = wakeup_request_ == ServiceRequest::WriteWordNeeded;
                            if (Log::enabled) Log::write(LogComponent::ShugartControl,
                                "Preamble word needed.");
                        }
                        break;

                    case WriteState::Preamble:
                        if (!write_pipeline_.empty()) {
                            drive_.write_data(write_pipeline_.front());
                            write_pipeline_.pop();
                        } else {
                            if (Log::enabled) Log::write(LogType::Error, LogComponent::ShugartControl,
                                "Write preamble: overrun.");
                            overrun_ = true;
                        }
                        wake = wakeup_request_ == ServiceRequest::WriteWordNeeded;
                        transfer_count_++;
                        if (transfer_count_ > 4) {
                            transfer_count_ = 0;
                            write_state_ = WriteState::AddressMark;
                            if (Log::enabled) Log::write(LogComponent::ShugartControl,
                                "AM word needed.");
                        } else {
                            if (Log::enabled) Log::write(LogComponent::ShugartControl,
                                "Preamble word needed.");
                        }
                        break;

                    case WriteState::AddressMark: {
                        uint16_t am_word = 0;
                        if (!write_pipeline_.empty()) {
                            am_word = write_pipeline_.front();
                            write_pipeline_.pop();
                        } else {
                            if (Log::enabled) Log::write(LogType::Error, LogComponent::ShugartControl,
                                "Write am data: overrun.");
                            overrun_ = true;
                        }
                        drive_.write_address_mark(am_word);
                        if (Log::enabled) Log::write(LogComponent::ShugartControl,
                            "KOData<- : Address mark is 0x%04x", am_word);
                        wake = wakeup_request_ == ServiceRequest::WriteWordNeeded;
                        write_state_ = WriteState::Data;
                        break;
                    }

                    case WriteState::Data:
                        if (write_crc_) {
                            wake = wakeup_request_ == ServiceRequest::WriteWordNeeded;
                            write_state_ = WriteState::CRC;
                            transfer_count_ = 0;
                            drive_.write_crc(0xbeef);
                        } else {
                            wake = wakeup_request_ == ServiceRequest::WriteWordNeeded;
                            transfer_count_++;
                            if (!write_pipeline_.empty()) {
                                drive_.write_data(write_pipeline_.front());
                                write_pipeline_.pop();
                            } else {
                                if (Log::enabled) Log::write(LogType::Error, LogComponent::ShugartControl,
                                    "Write data: overrun.");
                                overrun_ = true;
                            }
                            if (Log::enabled) Log::write(LogComponent::ShugartControl,
                                "Data word needed.");
                        }
                        break;

                    case WriteState::CRC:
                        drive_.write_crc(0xdead);
                        transfer_count_++;
                        wake = wakeup_request_ == ServiceRequest::WriteWordNeeded;
                        if (transfer_count_ > 1) {
                            write_state_ = WriteState::Complete;
                        }
                        break;

                    case WriteState::Complete:
                        break;

                    default:
                        break;
                }
                break;

            case TransferType::Verify:
                switch (verify_state_) {
                    case VerifyState::WaitForAddressMark:
                        if (read_data_ == kHeaderAddressMark ||
                            read_data_ == kLabelDataAddressMark) {
                            if (Log::enabled) Log::write(LogComponent::ShugartControl,
                                "Address Mark 0x%05x found at word 0x%04x, waking microcode.",
                                read_data_, drive_.word_index());
                            wake = true;
                            verify_state_ = VerifyState::Data;
                            crc_error_ = true;

                            if (write_pipeline_.empty()) {
                                write_pipeline_.push(0);
                            }
                        }
                        break;

                    case VerifyState::Data: {
                        uint16_t write_data = 0;
                        if (!write_pipeline_.empty()) {
                            write_data = write_pipeline_.front();
                            write_pipeline_.pop();
                        }

                        if ((read_data_ & 0x20000) != 0) {
                            if (read_data_ != kFakeCRCValue) {
                                if (Log::enabled) Log::write(LogType::Error, LogComponent::ShugartControl,
                                    "Verify CRC: 0x%05x != 0x%05x", kFakeCRCValue, read_data_);
                                crc_error_ = true;
                            } else {
                                crc_error_ = false;
                            }
                            verify_state_ = VerifyState::CRC;
                        } else {
                            if (write_data != static_cast<uint16_t>(read_data_)) {
                                if (Log::enabled) Log::write(LogComponent::ShugartControl,
                                    "Verify data: 0x%04x != 0x%04x", write_data,
                                    static_cast<uint16_t>(read_data_));
                                verify_error_ = true;
                            }
                        }
                        wake = true;
                        break;
                    }

                    case VerifyState::CRC:
                        if (!write_pipeline_.empty()) {
                            write_pipeline_.pop();
                        }
                        wake = true;
                        break;

                    default:
                        break;
                }
                break;

            case TransferType::Read:
                switch (read_state_) {
                    case ReadState::WaitForAddressMark:
                        if (read_data_ == kHeaderAddressMark ||
                            read_data_ == kLabelDataAddressMark) {
                            if (Log::enabled) Log::write(LogComponent::ShugartControl,
                                "Address Mark 0x%05x found at word 0x%04x, waking microcode.",
                                drive_.read_data(), drive_.word_index());
                            wake = true;
                            read_state_ = ReadState::Data;
                        }
                        break;

                    case ReadState::Data:
                        if ((read_data_ & 0x20000) != 0) {
                            if (read_data_ != kFakeCRCValue) {
                                if (Log::enabled) Log::write(LogType::Error, LogComponent::ShugartControl,
                                    "Read CRC: 0x%05x != 0x%05x", kFakeCRCValue, read_data_);
                                crc_error_ = true;
                            }
                            read_state_ = ReadState::CRC;
                        }
                        wake = true;
                        break;

                    case ReadState::CRC:
                        if (!write_pipeline_.empty()) {
                            write_pipeline_.pop();
                        }
                        wake = true;
                        break;

                    default:
                        break;
                }
                break;

            default:
                break;
        }

        if (wake) {
            system_.cp().wake_task(TaskType::Disk);
        }
    }
}

void ShugartController::signal_seek_complete() {
    if (wakeup_request_ == ServiceRequest::SeekComplete && seek_complete_status()) {
        system_.cp().wake_task(TaskType::Disk);
    }
}

bool ShugartController::seek_complete_status() const {
    return drive_.is_ready() && drive_select_ && drive_.seek_complete();
}

void ShugartController::reset_transfer() {
    transfer_ = TransferType::None;
    write_state_ = WriteState::AutoPreamble;
    verify_state_ = VerifyState::WaitForAddressMark;
    read_state_ = ReadState::WaitForAddressMark;
    transfer_count_ = 0;
    while (!write_pipeline_.empty()) write_pipeline_.pop();
}

} // namespace darkstar
