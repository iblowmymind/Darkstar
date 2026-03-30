/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/

#include "io/sa1000.h"
#include "io/shugart_controller.h"
#include "core/conversion.h"
#include "core/log.h"
#include "core/scheduler.h"
#include "core/system.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <filesystem>

namespace darkstar {

SA1000Drive::SA1000Drive(DSystem& system)
    : system_(system)
    , type_(DriveType::Invalid)
    , seek_duration_(static_cast<uint64_t>(25.0 * Conversion::MsecToNsec))
{
    new_disk(type_, std::string());

    // Queue up the event that rotates our virtual disk. This runs continuously.
    system_.scheduler().schedule(
        kDiskWordDelay,
        [this](uint64_t skew, void* ctx) { disk_word_callback(skew, ctx); });

    reset();
}

void SA1000Drive::reset() {
    cylinder_ = 0;
    head_ = 0;
    word_index_ = 0;
    index_ = false;
    seek_complete_ = true;
    last_step_ = false;
}

void SA1000Drive::save() {
    if (!disk_image_path_.empty()) {
        save(disk_image_path_);
    }
}

void SA1000Drive::save(const std::string& path) {
    // Write to a temporary file first, then replace original.
    std::string temp_path = path + ".tmp";

    {
        std::ofstream fs(temp_path, std::ios::binary);
        if (!fs) {
            throw std::runtime_error("Failed to open temp file for disk save.");
        }

        // Format: 1st byte = drive type, then track data (3 bytes per word).
        uint8_t type_byte = static_cast<uint8_t>(type_);
        fs.write(reinterpret_cast<const char*>(&type_byte), 1);

        for (int cyl = 0; cyl < geometry_.cylinders; cyl++) {
            for (int h = 0; h < geometry_.heads; h++) {
                for (int word = 0; word < kWordsPerTrack; word++) {
                    uint32_t val = track_word(cyl, h, word);
                    uint8_t bytes[3];
                    bytes[0] = static_cast<uint8_t>(val & 0xff);
                    bytes[1] = static_cast<uint8_t>((val >> 8) & 0xff);
                    bytes[2] = static_cast<uint8_t>((val >> 16) & 0xff);
                    fs.write(reinterpret_cast<const char*>(bytes), 3);
                }
            }
        }
    }

    // Move temp file to final location.
    std::filesystem::rename(temp_path, path);
    disk_image_path_ = path;
}

void SA1000Drive::load(const std::string& path) {
    try {
        std::ifstream fs(path, std::ios::binary);
        if (!fs) {
            throw std::runtime_error("Failed to open disk image file.");
        }

        uint8_t type_byte;
        fs.read(reinterpret_cast<char*>(&type_byte), 1);

        if (type_byte < static_cast<int>(DriveType::SA1004) ||
            type_byte > static_cast<int>(DriveType::Q2080)) {
            throw std::runtime_error("Unsupported drive type.");
        }

        type_ = static_cast<DriveType>(type_byte);
        new_disk(type_, path);

        uint8_t buffer[4] = {0};
        for (int cyl = 0; cyl < geometry_.cylinders; cyl++) {
            for (int h = 0; h < geometry_.heads; h++) {
                for (int word = 0; word < kWordsPerTrack; word++) {
                    fs.read(reinterpret_cast<char*>(buffer), 3);
                    if (fs.gcount() < 3) {
                        throw std::runtime_error("Short read on disk image load.");
                    }
                    buffer[3] = 0;
                    uint32_t val = static_cast<uint32_t>(buffer[0]) |
                                   (static_cast<uint32_t>(buffer[1]) << 8) |
                                   (static_cast<uint32_t>(buffer[2]) << 16);
                    track_word(cyl, h, word) = val;
                }
            }
        }
    }
    catch (...) {
        type_ = DriveType::Invalid;
        new_disk(type_, std::string());
        throw;
    }
}

void SA1000Drive::new_disk(DriveType type, const std::string& path) {
    switch (type) {
        case DriveType::Invalid:
        case DriveType::SA1004:
            geometry_ = Geometry::sa1004();
            break;
        case DriveType::Q2040:
            geometry_ = Geometry::q2040();
            break;
        case DriveType::Q2080:
            geometry_ = Geometry::q2080();
            break;
    }

    tracks_.assign(
        static_cast<size_t>(geometry_.cylinders) * geometry_.heads * kWordsPerTrack, 0);
    type_ = type;
    disk_image_path_ = path;
}

void SA1000Drive::set_head(int head) {
    head_ = head % geometry_.heads;
}

void SA1000Drive::step(bool direction_in, bool step_signal) {
    if (step_signal && !last_step_) {
        if (step_count_ == 0) {
            direction_in_ = direction_in;
        }

        time_since_last_step_ = 0;
        step_count_++;
        seek_complete_ = false;

        if (Log::enabled) Log::write(LogComponent::ShugartControl, "Buffering step.");
    }

    last_step_ = step_signal;
}

uint32_t SA1000Drive::read_data() const {
    return current_word_;
}

void SA1000Drive::write_data(uint16_t data) {
    track_word(cylinder_, head_, word_index_) = data;

    if (Log::enabled) Log::write(LogComponent::ShugartControl,
        "Wrote 0x%04x to c/h/w %d/%d/%d", data, cylinder_, head_, word_index_);
}

void SA1000Drive::write_address_mark(uint16_t data) {
    track_word(cylinder_, head_, word_index_) = static_cast<uint32_t>(data | 0x10000);

    if (Log::enabled) Log::write(LogComponent::ShugartControl,
        "Wrote Address Mark 0x%04x to c/h/w %d/%d/%d", data, cylinder_, head_, word_index_);
}

void SA1000Drive::write_crc(uint16_t data) {
    track_word(cylinder_, head_, word_index_) = static_cast<uint32_t>(data | 0x20000);

    if (Log::enabled) Log::write(LogComponent::ShugartControl,
        "Wrote CRC 0x%04x to c/h/w %d/%d/%d", data, cylinder_, head_, word_index_);
}

uint32_t SA1000Drive::debug_read(int cyl, int head, int word) const {
    return track_word(cyl, head, word);
}

void SA1000Drive::disk_word_callback(uint64_t skew_nsec, void* /*context*/) {
    // Rotate the disk one word.
    spin_disk();

    // Let the controller know a new word is ready.
    system_.shugart_controller().signal_disk_word_ready();

    // Deal with buffered seeks.
    time_since_last_step_ += 370;

    if (time_since_last_step_ > 35000 && step_count_ > 0) {
        seek(step_count_, direction_in_);
        step_count_ = 0;
    }

    // Queue this event up again.
    system_.scheduler().schedule(
        kDiskWordDelay - skew_nsec,
        [this](uint64_t skew, void* ctx) { disk_word_callback(skew, ctx); });
}

void SA1000Drive::seek(int count, bool direction_in) {
    destination_cylinder_ = cylinder_ + count * (direction_in ? 1 : -1);

    destination_cylinder_ = std::max(0, destination_cylinder_);
    destination_cylinder_ = std::min(geometry_.cylinders - 1, destination_cylinder_);

    system_.scheduler().schedule(
        seek_duration_,
        [this](uint64_t skew, void* ctx) { seek_complete_callback(skew, ctx); });
}

void SA1000Drive::seek_complete_callback(uint64_t /*skew_nsec*/, void* /*context*/) {
    cylinder_ = destination_cylinder_;
    seek_complete_ = true;

    system_.shugart_controller().signal_seek_complete();

    if (Log::enabled) Log::write(LogComponent::ShugartControl,
        "Seek to %d complete.", cylinder_);
}

void SA1000Drive::spin_disk() {
    current_word_ = track_word(cylinder_, head_, word_index_);

    word_index_++;

    if (word_index_ == kWordsPerTrack) {
        word_index_ = 0;
        index_ = true;
    } else {
        index_ = false;
    }
}

uint32_t& SA1000Drive::track_word(int cyl, int head, int word) {
    return tracks_[static_cast<size_t>(cyl) * geometry_.heads * kWordsPerTrack +
                   static_cast<size_t>(head) * kWordsPerTrack +
                   word];
}

uint32_t SA1000Drive::track_word(int cyl, int head, int word) const {
    return tracks_[static_cast<size_t>(cyl) * geometry_.heads * kWordsPerTrack +
                   static_cast<size_t>(head) * kWordsPerTrack +
                   word];
}

} // namespace darkstar
