/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>

namespace darkstar {

class DSystem;
class FloppyDisk;

class FloppyDrive {
public:
    explicit FloppyDrive(DSystem& system);

    void reset();

    FloppyDisk* disk() const { return disk_; }
    int track() const { return track_; }
    bool is_loaded() const { return disk_ != nullptr; }
    bool is_write_protected() const;
    bool is_single_sided() const { return single_sided_; }
    bool track0() const { return track_ == 0; }
    bool index() const { return index_; }
    bool disk_change() const { return disk_change_; }

    bool drive_select() const { return drive_select_; }
    void set_drive_select(bool value);

    void load_disk(FloppyDisk* disk);
    void unload_disk();
    void seek_to(int track);

private:
    void index_callback(uint64_t skew_nsec, void* context);

    DSystem& system_;

    bool single_sided_ = false;
    int track_ = 0;
    bool disk_change_ = false;
    bool drive_select_ = false;
    FloppyDisk* disk_ = nullptr;

    // Index signal and timing
    bool index_ = false;
    uint64_t index_interval_;   // 250ms
    uint64_t index_duration_;   // 10us
};

} // namespace darkstar
