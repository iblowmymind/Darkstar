/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace darkstar {

class DSystem;
struct Event;

enum class DriveType {
    Invalid = 0,
    SA1004 = 1,
    Q2040 = 2,
    Q2080 = 3,
};

struct Geometry {
    int cylinders;
    int heads;

    Geometry() : cylinders(0), heads(0) {}
    Geometry(int c, int h) : cylinders(c), heads(h) {}

    static Geometry sa1004() { return Geometry(256, 4); }
    static Geometry q2040() { return Geometry(512, 8); }
    static Geometry q2080() { return Geometry(1172, 7); }
};

class SA1000Drive {
public:
    explicit SA1000Drive(DSystem& system);

    void reset();
    void save();
    void save(const std::string& path);
    void load(const std::string& path);
    void new_disk(DriveType type, const std::string& path);

    // Accessors
    const std::string& image_path() const { return disk_image_path_; }
    DriveType type() const { return type_; }
    Geometry geometry() const { return geometry_; }
    int words_per_track() const { return kWordsPerTrack; }
    int cylinder() const { return cylinder_; }
    int head() const { return head_; }
    bool track0() const { return cylinder_ == 0; }
    bool index() const { return index_; }
    bool is_ready() const { return type_ != DriveType::Invalid; }
    bool seek_complete() const { return seek_complete_; }
    int word_index() const { return word_index_; }

    void set_head(int head);
    void step(bool direction_in, bool step_signal);

    uint32_t read_data() const;
    void write_data(uint16_t data);
    void write_address_mark(uint16_t data);
    void write_crc(uint16_t data);

    uint32_t debug_read(int cyl, int head, int word) const;

private:
    void disk_word_callback(uint64_t skew_nsec, void* context);
    void seek(int count, bool direction_in);
    void seek_complete_callback(uint64_t skew_nsec, void* context);
    void spin_disk();

    // Track access helpers (3D array flattened)
    uint32_t& track_word(int cyl, int head, int word);
    uint32_t track_word(int cyl, int head, int word) const;

    static constexpr int kWordsPerTrack = 5325;

    Geometry geometry_;
    DriveType type_;
    std::vector<uint32_t> tracks_;

    int word_index_ = 0;
    uint32_t current_word_ = 0;

    int cylinder_ = 0;
    int head_ = 0;

    bool index_ = false;

    // Seek timing
    uint64_t seek_duration_;
    int destination_cylinder_ = 0;
    bool seek_complete_ = true;

    // Step buffering
    bool last_step_ = false;
    int step_count_ = 0;
    int time_since_last_step_ = 0;
    bool direction_in_ = false;

    // Disk word timing: ~3.6us -> 3699ns
    static constexpr uint64_t kDiskWordDelay = 3699;

    std::string disk_image_path_;

    DSystem& system_;
};

} // namespace darkstar
