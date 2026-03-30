/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/

#include "iop/floppy_controller.h"
#include "iop/io_processor.h"
#include "iop/dma_controller.h"
#include "io/floppy_drive.h"
#include "io/floppy_disk.h"
#include "core/system.h"
#include "core/scheduler.h"
#include "core/log.h"

#include <stdexcept>

namespace darkstar {

// Port tables
const int FloppyController::kReadPorts[] = {
    0x84, // FDCStatus
    0x85, // FDCTrack
    0x86, // FDCSector
    0x87, // FDCData
    0xe8, // ExtFDCStatusReg
};

const int FloppyController::kWritePorts[] = {
    0x84, // FDCCommand
    0x85, // FDCTrack
    0x86, // FDCSector
    0x87, // FDCData
    0xe8, // ExtFDCState
};

FloppyController::FloppyController(FloppyDrive& drive, DSystem& system)
    : drive_(drive)
    , system_(system)
    , fdc_track_(0)
    , fdc_sector_(0)
    , fdc_data_(0)
    , sector_buffer_(nullptr)
    , sector_data_index_(0)
    , write_track_data_index_(0)
    , write_track_side_(false)
    , crc_error_(false)
    , busy_(false)
    , head_loaded_(false)
    , seek_error_(false)
    , record_type_write_fault_(false)
    , rnf_(false)
    , lost_data_(false)
    , drq_(false)
    , drq_counter_(16)
    , index_reset_(false)
    , fdc_enabled_(false)
    , interrupt_pending_(false)
    , last_command_(FDCCommand::Restore)
    , ext_state_(0)
    , seek_destination_(0)
    , step_direction_(StepDirection::In)
    , double_density_(false)
    , side_select_(false)
    , command_abort_(false)
    , pending_seek_params_(nullptr)
{
}

// IIOPDevice interface
const int* FloppyController::read_ports() const { return kReadPorts; }
int FloppyController::read_port_count() const { return 5; }
const int* FloppyController::write_ports() const { return kWritePorts; }
int FloppyController::write_port_count() const { return 5; }

// IDMAInterface
bool FloppyController::drq() const {
    if (drq_) {
        drq_counter_--;
        if (drq_counter_ == 0) {
            return true;
        }
        return false;
    }
    return false;
}

uint8_t FloppyController::dma_read() {
    // Return the next byte from the sector buffer.
    uint8_t dma_read_val = 0;

    if (sector_buffer_ && sector_data_index_ < static_cast<int>(sector_buffer_->size())) {
        dma_read_val = (*sector_buffer_)[sector_data_index_];
    } else {
        if (Log::enabled) Log::write(LogComponent::IOPFloppy, "DMA sector read past end of sector.");
    }

    drq_counter_ = 16;
    sector_data_index_++;

    return dma_read_val;
}

void FloppyController::dma_write(uint8_t value) {
    if (last_command_ == FDCCommand::WriteSectorSingle) {
        if (sector_buffer_ && sector_data_index_ < static_cast<int>(sector_buffer_->size())) {
            (*sector_buffer_)[sector_data_index_] = value;
        } else {
            if (Log::enabled) Log::write(LogComponent::IOPFloppy, "DMA sector write past end of sector.");
        }
        sector_data_index_++;
        drq_counter_ = 16;
    } else if (last_command_ == FDCCommand::WriteTrack) {
        throw std::runtime_error("Unexpected Write Track w/DMA transfer.");
    } else {
        throw std::runtime_error("DMA write with unexpected command");
    }
}

void FloppyController::dma_complete() {
    if (Log::enabled) Log::write(LogComponent::IOPFloppy, "DMA transfer completed.");
    finish_data_transfer();
}

void FloppyController::write_port(int port, uint8_t value) {
    if (Log::enabled) Log::write(LogComponent::IOPFloppy, "FDC port %02x write %02x", port, value);

    switch (static_cast<FDCPorts>(port)) {
        case FDCPorts::ExtFDCState:
            ext_state_ = value;
            if (Log::enabled) Log::write(LogComponent::IOPFloppy, "FDC Ext state %02x", value);

            drive_.set_drive_select((ext_state_ & kFDCDriveSelect) != 0);

            double_density_ = (ext_state_ & kFDCDensity) != 0;
            side_select_ = (ext_state_ & kFDCSide) != 0;

            if ((ext_state_ & kFDCEnableFDC) != 0) {
                enable_fdc();
            } else {
                disable_fdc();
            }
            break;

        case FDCPorts::FDCTrack:
            fdc_track_ = value;
            break;

        case FDCPorts::FDCSector:
            fdc_sector_ = value;
            break;

        case FDCPorts::FDCData:
            fdc_data_ = value;

            if (drq_) {
                if (last_command_ == FDCCommand::WriteTrack) {
                    if (drive_.index()) {
                        finish_write_track();
                    } else {
                        if (write_track_data_index_ < static_cast<int>(write_track_buffer_.size())) {
                            write_track_buffer_[write_track_data_index_] = fdc_data_;
                            write_track_data_index_++;
                        }
                    }
                } else if (last_command_ == FDCCommand::WriteSectorSingle) {
                    if (sector_buffer_ && sector_data_index_ < static_cast<int>(sector_buffer_->size()) - 1) {
                        (*sector_buffer_)[sector_data_index_] = fdc_data_;
                        sector_data_index_++;
                        if (sector_data_index_ > static_cast<int>(sector_buffer_->size()) - 1) {
                            finish_data_transfer();
                        }
                    }
                }
            }
            break;

        case FDCPorts::FDCCommand:
            do_command(value);
            break;

        default:
            break;
    }
}

uint8_t FloppyController::read_port(int port) {
    uint8_t value = 0;

    switch (static_cast<FDCPorts>(port)) {
        case FDCPorts::FDCTrack:
            value = fdc_track_;
            break;

        case FDCPorts::FDCSector:
            value = fdc_sector_;
            break;

        case FDCPorts::FDCData:
            if (drq_ && sector_buffer_) {
                if (sector_data_index_ < static_cast<int>(sector_buffer_->size()) - 1) {
                    fdc_data_ = (*sector_buffer_)[sector_data_index_];
                    sector_data_index_++;
                    if (sector_data_index_ > static_cast<int>(sector_buffer_->size()) - 1) {
                        finish_data_transfer();
                    }
                }
            }
            value = fdc_data_;
            break;

        case FDCPorts::FDCStatus:
            value = read_status();
            break;

        case FDCPorts::ExtFDCStatusReg:
            value = read_ext_status();
            break;

        default:
            break;
    }

    return value;
}

void FloppyController::reset() {
    ext_state_ = 0;
    last_command_ = FDCCommand::Restore;
    fdc_enabled_ = false;
    sector_data_index_ = 0;
    drq_counter_ = 16;
    index_reset_ = false;
    double_density_ = false;
    side_select_ = false;

    drive_.reset();
    reset_flags();
}

void FloppyController::reset_flags() {
    fdc_data_ = 0;
    fdc_sector_ = 0;
    fdc_track_ = 0;
    command_abort_ = false;

    crc_error_ = false;
    busy_ = false;
    head_loaded_ = false;
    seek_error_ = false;
    record_type_write_fault_ = false;
    rnf_ = false;
    lost_data_ = false;
    drq_ = false;

    clear_interrupt();
}

uint8_t FloppyController::read_status() {
    clear_interrupt();

    uint8_t value = 0;

    switch (last_command_) {
        // Type I Commands
        case FDCCommand::Restore:
        case FDCCommand::Seek:
        case FDCCommand::StepNoUpdate:
        case FDCCommand::StepUpdate:
        case FDCCommand::StepInNoUpdate:
        case FDCCommand::StepInUpdate:
        case FDCCommand::StepOutNoUpdate:
        case FDCCommand::StepOutUpdate:
            value = static_cast<uint8_t>(
                (not_ready() ? 0x80 : 0x00) |
                (write_protect() ? 0x40 : 0x00) |
                (head_loaded_ ? 0x20 : 0x00) |
                (seek_error_ ? 0x10 : 0x00) |
                (crc_error_ ? 0x08 : 0x00) |
                (track0() ? 0x04 : 0x00) |
                (index() ? 0x02 : 0x00) |
                (busy_ ? 0x01 : 0x00));
            break;

        case FDCCommand::ReadAddress:
            value = static_cast<uint8_t>(
                (not_ready() ? 0x80 : 0x00) |
                (rnf_ ? 0x10 : 0x00) |
                (crc_error_ ? 0x08 : 0x00) |
                (lost_data_ ? 0x04 : 0x00) |
                (drq_ ? 0x02 : 0x00) |
                (busy_ ? 0x01 : 0x00));
            break;

        case FDCCommand::ReadTrack:
            value = static_cast<uint8_t>(
                (not_ready() ? 0x80 : 0x00) |
                (lost_data_ ? 0x04 : 0x00) |
                (drq_ ? 0x02 : 0x00) |
                (busy_ ? 0x01 : 0x00));
            break;

        case FDCCommand::ReadSectorMultiple:
        case FDCCommand::ReadSectorSingle:
            value = static_cast<uint8_t>(
                (not_ready() ? 0x80 : 0x00) |
                (record_type_write_fault_ ? 0x20 : 0x00) |
                (rnf_ ? 0x10 : 0x00) |
                (crc_error_ ? 0x08 : 0x00) |
                (lost_data_ ? 0x04 : 0x00) |
                (drq_ ? 0x02 : 0x00) |
                (busy_ ? 0x01 : 0x00));
            break;

        case FDCCommand::WriteSectorMultiple:
        case FDCCommand::WriteSectorSingle:
            value = static_cast<uint8_t>(
                (not_ready() ? 0x80 : 0x00) |
                (write_protect() ? 0x40 : 0x00) |
                (record_type_write_fault_ ? 0x20 : 0x00) |
                (rnf_ ? 0x10 : 0x00) |
                (crc_error_ ? 0x08 : 0x00) |
                (lost_data_ ? 0x04 : 0x00) |
                (drq_ ? 0x02 : 0x00) |
                (busy_ ? 0x01 : 0x00));
            break;

        case FDCCommand::WriteTrack:
            value = static_cast<uint8_t>(
                (not_ready() ? 0x80 : 0x00) |
                (write_protect() ? 0x40 : 0x00) |
                (record_type_write_fault_ ? 0x20 : 0x00) |
                (lost_data_ ? 0x04 : 0x00) |
                (drq_ ? 0x02 : 0x00) |
                (busy_ ? 0x01 : 0x00));
            break;

        default:
            break;
    }

    return value;
}

uint8_t FloppyController::read_ext_status() {
    bool double_sided = false;
    bool sa800 = false;
    bool disk_change = false;

    if (drive_.drive_select()) {
        double_sided = !drive_.is_single_sided();
        sa800 = !drive_.is_loaded();
        disk_change = drive_.disk_change();
    }

    uint8_t ext_status = static_cast<uint8_t>(
        (disk_change ? 0x80 : 0x00) |
        (system_.iop().dma_controller().tc() ? 0x40 : 0x00) |
        (double_sided ? 0x20 : 0x00) |
        (sa800 ? 0x10 : 0x00));

    return ext_status;
}

void FloppyController::enable_fdc() {
    if (fdc_enabled_) {
        return;
    }

    fdc_enabled_ = true;
    if (Log::enabled) Log::write(LogComponent::IOPFloppy, "FDC enabled.");

    // "A logic low on the [-MR] input resets the device and loads HEX 03
    //  into the command register. When -MR is brought to a logic high, a RESTORE
    //  command is executed."
    do_command(static_cast<int>(FDCCommand::Restore) << 4);

    // If DriveSelect is high, set INDEX high and schedule reset after short duration.
    if (drive_.drive_select()) {
        index_reset_ = true;

        system_.scheduler().schedule(kResetIndexDuration,
            [this](uint64_t, void*) {
                index_reset_ = false;
                if (Log::enabled) Log::write(LogComponent::IOPFloppy, "Resetting INDEX signal after FDC reset.");
            });
    }
}

void FloppyController::disable_fdc() {
    if (!fdc_enabled_) {
        return;
    }

    fdc_enabled_ = false;
    if (Log::enabled) Log::write(LogComponent::IOPFloppy, "FDC disabled. Resetting FDC.");

    reset_flags();
    ext_state_ = 0;
    last_command_ = FDCCommand::Restore;
}

void FloppyController::do_command(int command_data) {
    clear_interrupt();
    command_abort_ = false;

    FDCCommand command = static_cast<FDCCommand>(command_data >> 4);

    if (command != FDCCommand::ForceInterrupt) {
        if (busy_) {
            if (Log::enabled) Log::write(LogComponent::IOPFloppy, "FDC busy, command aborted.");
            return;
        }
        last_command_ = command;
    } else {
        last_command_ = FDCCommand::Restore;
    }

    int data = command_data & 0x1f;

    switch (command) {
        case FDCCommand::Restore:
            fdc_track_ = static_cast<uint8_t>(drive_.track());
            seek(0, Type1CommandParams(data));
            break;

        case FDCCommand::Seek:
            seek(fdc_data_, Type1CommandParams(data));
            break;

        case FDCCommand::StepNoUpdate:
        case FDCCommand::StepUpdate:
            step(StepDirection::Last, Type1CommandParams(data));
            break;

        case FDCCommand::StepInNoUpdate:
        case FDCCommand::StepInUpdate:
            step(StepDirection::In, Type1CommandParams(data));
            break;

        case FDCCommand::StepOutNoUpdate:
        case FDCCommand::StepOutUpdate:
            step(StepDirection::Out, Type1CommandParams(data));
            break;

        case FDCCommand::ReadSectorSingle:
            sector_transfer(Type2CommandParams(data), true);
            break;

        case FDCCommand::WriteSectorSingle:
            sector_transfer(Type2CommandParams(data), false);
            break;

        case FDCCommand::ForceInterrupt:
            force_interrupt(data);
            break;

        case FDCCommand::WriteTrack:
            write_track();
            break;

        default:
            throw std::runtime_error("FDC command not implemented.");
    }
}

void FloppyController::seek(int track, Type1CommandParams p) {
    seek_destination_ = track;
    seek_error_ = false;

    // Allocate params on heap so they survive the scheduled callback.
    delete pending_seek_params_;
    pending_seek_params_ = new Type1CommandParams(p);

    system_.scheduler().schedule(kCommandBeginNsec, pending_seek_params_,
        [this](uint64_t skew_nsec, void* context) {
            seek_callback(skew_nsec, context);
        });
}

void FloppyController::step(StepDirection direction, Type1CommandParams p) {
    if (direction != StepDirection::Last) {
        step_direction_ = direction;
    }

    seek_error_ = false;

    if (!busy_) {
        system_.scheduler().schedule(kCommandBeginNsec,
            [this, p](uint64_t, void*) {
                if (command_abort_) {
                    return;
                }

                if (!busy_) {
                    busy_ = true;

                    system_.scheduler().schedule(kStepTimeNsec,
                        [this, p](uint64_t, void*) {
                            switch (step_direction_) {
                                case StepDirection::Out:
                                    drive_.seek_to(drive_.track() - 1);
                                    if (p.update) {
                                        fdc_track_--;
                                    }
                                    break;

                                case StepDirection::In:
                                    drive_.seek_to(drive_.track() + 1);
                                    if (p.update) {
                                        fdc_track_++;
                                    }
                                    break;

                                default:
                                    break;
                            }

                            if (p.verify && drive_.is_loaded() && (fdc_track_ != drive_.track())) {
                                seek_error_ = true;
                            }

                            head_loaded_ = p.head_load;
                            raise_interrupt();
                            busy_ = false;
                        });
                }
            });
    }
}

void FloppyController::sector_transfer(Type2CommandParams p, bool read) {
    system_.scheduler().schedule(kCommandBeginNsec,
        [this, p, read](uint64_t, void*) {
            Track* t = drive_.disk()->get_track(drive_.track(), p.side_select ? 1 : 0);

            rnf_ = (fdc_track_ != drive_.track()) ||
                   (t == nullptr) ||
                   (t != nullptr && fdc_sector_ > t->sector_count());

            if (t != nullptr) {
                if ((t->format() != Format::FM500 && t->format() != Format::MFM500) ||
                    ((t->format() == Format::FM500) ^ !double_density_)) {
                    crc_error_ = true;
                }
            }

            bool wp = !read && drive_.is_write_protected();

            if (!not_ready() && !rnf_ && !crc_error_ && !wp) {
                Sector* sec = drive_.disk()->get_sector(drive_.track(), p.side_select ? 1 : 0, fdc_sector_ - 1);
                if (sec) {
                    sector_buffer_ = &sec->data();
                    sector_data_index_ = 0;
                    busy_ = true;
                    drq_ = true;
                    drq_counter_ = 16;

                    if (!read) {
                        drive_.disk()->set_modified();
                    }
                }
            } else {
                sector_buffer_ = nullptr;
                busy_ = false;
            }
        });
}

void FloppyController::write_track() {
    system_.scheduler().schedule(kCommandBeginNsec,
        [this](uint64_t, void*) {
            if (!not_ready() && !rnf_ && !drive_.is_write_protected()) {
                write_track_buffer_.resize(0x10000, 0);
                write_track_data_index_ = 0;
                write_track_side_ = side_select_;
                busy_ = true;
                drq_ = true;
                drq_counter_ = 16;

                drive_.disk()->set_modified();
            } else {
                write_track_buffer_.clear();
                busy_ = false;
            }
        });
}

void FloppyController::force_interrupt(int flags) {
    busy_ = false;
    command_abort_ = true;
    if (Log::enabled) Log::write(LogComponent::IOPFloppy, "Force Interrupt 0x%02x", flags);
}

void FloppyController::seek_callback(uint64_t /*skew_nsec*/, void* context) {
    if (command_abort_) {
        return;
    }

    busy_ = true;

    if (fdc_track_ == seek_destination_) {
        busy_ = false;

        Type1CommandParams* p = static_cast<Type1CommandParams*>(context);
        if (p->verify && drive_.is_loaded() && (fdc_track_ != drive_.track())) {
            seek_error_ = true;
        }

        head_loaded_ = p->head_load;
        raise_interrupt();
    } else {
        if (fdc_track_ < seek_destination_) {
            fdc_track_++;
            drive_.seek_to(drive_.track() + 1);
        } else {
            fdc_track_--;
            drive_.seek_to(drive_.track() - 1);
        }

        system_.scheduler().schedule(kStepTimeNsec, context,
            [this](uint64_t skew_nsec, void* ctx) {
                seek_callback(skew_nsec, ctx);
            });
    }
}

void FloppyController::finish_data_transfer() {
    drq_ = false;
    busy_ = false;
    sector_buffer_ = nullptr;
    sector_data_index_ = 0;

    raise_interrupt();
}

void FloppyController::finish_write_track() {
    // Process write track data to determine track format.
    bool formatted_sectors[256] = {false};
    int sector_size = 0;
    TrackParseState state = TrackParseState::Gap4;

    for (int i = 0; i < write_track_data_index_; i++) {
        uint8_t b = write_track_buffer_[i];
        switch (state) {
            case TrackParseState::Gap4:
                if ((b != kGap4MFM && b != kGap4FM) ||
                    ((b == kGap4MFM) ^ double_density_)) {
                    throw std::runtime_error("Unexpected Gap4 value in WriteTrack");
                }
                state = TrackParseState::IndexMark;
                break;

            case TrackParseState::IndexMark:
                if (b == kSoftIndex) {
                    state = TrackParseState::IDRecordMark;
                }
                break;

            case TrackParseState::IDRecordMark:
                if (b == kSectorIDRecord) {
                    uint8_t track_val = write_track_buffer_[++i];
                    uint8_t head = write_track_buffer_[++i];
                    uint8_t sector = write_track_buffer_[++i];
                    uint8_t sector_length = write_track_buffer_[++i];

                    if (track_val != fdc_track_) {
                        throw std::runtime_error("WriteTrack: track != current track");
                    }

                    if ((head == 0) ^ !write_track_side_) {
                        throw std::runtime_error("WriteTrack: invalid head value");
                    }

                    int current_sector_size = 0;
                    switch (sector_length) {
                        case 0: current_sector_size = 128; break;
                        case 1: current_sector_size = 256; break;
                        case 2: current_sector_size = 512; break;
                        case 3: current_sector_size = 1024; break;
                        default:
                            throw std::runtime_error("WriteTrack: unexpected sector size.");
                    }

                    if (sector_size == 0) {
                        sector_size = current_sector_size;
                    } else if (sector_size != current_sector_size) {
                        throw std::runtime_error("WriteTrack: multiple sector sizes per track not supported.");
                    }

                    if (!formatted_sectors[sector - 1]) {
                        formatted_sectors[sector - 1] = true;
                    } else {
                        throw std::runtime_error("WriteTrack: Duplicate sector");
                    }

                    state = TrackParseState::DataRecordMark;
                }
                break;

            case TrackParseState::DataRecordMark:
                if (b == kDataRecord) {
                    int data_length = 0;
                    while (i < write_track_data_index_ && write_track_buffer_[++i] != kCRC) {
                        data_length++;
                    }

                    if (data_length != sector_size) {
                        throw std::runtime_error("WriteTrack: data record length != sector size");
                    }

                    state = TrackParseState::IDRecordMark;
                }
                break;
        }
    }

    // Count sectors and ensure no gaps.
    int sector_count = 0;
    bool found_gap = false;
    for (int i = 0; i < 256; i++) {
        if (!formatted_sectors[i]) {
            found_gap = true;
        } else {
            if (found_gap) {
                throw std::runtime_error("WriteTrack: discontinuous sector IDs");
            }
            sector_count++;
        }
    }

    // Format the track with the given parameters.
    drive_.disk()->format_track(
        double_density_ ? Format::MFM500 : Format::FM500,
        fdc_track_,
        write_track_side_ ? 1 : 0,
        sector_count,
        sector_size);

    drq_ = false;
    busy_ = false;
    write_track_buffer_.clear();
    write_track_data_index_ = 0;

    raise_interrupt();
}

void FloppyController::clear_interrupt() {
    interrupt_pending_ = false;
}

void FloppyController::raise_interrupt() {
    interrupt_pending_ = true;
    system_.iop().cpu().raise_external_interrupt(InterruptType::RST7_5);
}

bool FloppyController::not_ready() const {
    return drive_.drive_select() ? !drive_.is_loaded() : true;
}

bool FloppyController::write_protect() const {
    return drive_.is_loaded() && drive_.is_write_protected();
}

bool FloppyController::track0() const {
    return drive_.track0();
}

bool FloppyController::index() const {
    return drive_.index() || index_reset_;
}

} // namespace darkstar
