/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/

#include "io/floppy_drive.h"
#include "io/floppy_disk.h"
#include "core/conversion.h"
#include "core/log.h"
#include "core/scheduler.h"
#include "core/system.h"

#include <algorithm>

namespace darkstar {

FloppyDrive::FloppyDrive(DSystem& system)
    : system_(system)
    , index_interval_(250 * Conversion::MsecToNsec)
    , index_duration_(10 * Conversion::UsecToNsec)
{
    // Start the Index event rolling. This will run forever.
    system_.scheduler().schedule(
        index_interval_,
        [this](uint64_t skew, void* ctx) { index_callback(skew, ctx); });

    reset();
}

void FloppyDrive::reset() {
    track_ = 0;
    single_sided_ = false;
    disk_change_ = false;
    index_ = false;
    drive_select_ = false;
}

bool FloppyDrive::is_write_protected() const {
    return disk_ != nullptr ? disk_->is_write_protected() : false;
}

void FloppyDrive::set_drive_select(bool value) {
    drive_select_ = value;

    // The Disk Change signal is reset when Drive Select goes low.
    if (!drive_select_) {
        disk_change_ = false;
    }
}

void FloppyDrive::load_disk(FloppyDisk* disk) {
    disk_ = disk;
    single_sided_ = disk_->is_single_sided();
    disk_change_ = true;

    if (Log::enabled) Log::write(LogComponent::IOPFloppy,
        "Floppy disk image loaded. Description is:\n%s", disk->description().c_str());
}

void FloppyDrive::unload_disk() {
    if (disk_ != nullptr && disk_->is_modified()) {
        disk_->save();
    }

    disk_ = nullptr;
    disk_change_ = true;
}

void FloppyDrive::seek_to(int track) {
    track_ = std::max(0, track);
    track_ = std::min(76, track_);
}

void FloppyDrive::index_callback(uint64_t /*skew_nsec*/, void* /*context*/) {
    if (drive_select_ && is_loaded() && !index_) {
        // Raise the index signal, hold for a short period.
        index_ = true;
        system_.scheduler().schedule(
            index_duration_,
            [this](uint64_t skew, void* ctx) { index_callback(skew, ctx); });

        if (Log::enabled) Log::write(LogComponent::IOPFloppy,
            "Disk rotation complete, raising INDEX signal for 10us.");
    } else {
        // Reset the index signal, wait for a long period.
        index_ = false;
        system_.scheduler().schedule(
            index_interval_,
            [this](uint64_t skew, void* ctx) { index_callback(skew, ctx); });
    }
}

} // namespace darkstar
