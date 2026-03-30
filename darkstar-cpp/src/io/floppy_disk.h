/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace darkstar {

// 00 = 500 kbps FM
// 01 = 300 kbps FM
// 02 = 250 kbps FM
// 03 = 500 kbps MFM
// 04 = 300 kbps MFM
// 05 = 250 kbps MFM
enum class Format {
    FM500 = 0,
    FM300 = 1,
    FM250 = 2,
    MFM500 = 3,
    MFM300 = 4,
    MFM250 = 5,
};

enum class SectorRecordType {
    Unavailable = 0,
    Normal = 1,
    Compressed = 2,
    NormalDeleted = 3,
    CompressedDeleted = 4,
    NormalError = 5,
    CompressedError = 6,
    DeletedError = 7,
    CompressedDeletedError = 8,
};

class Sector {
public:
    Sector(int sector_size, Format format);
    Sector(int sector_size, Format format, uint8_t compressed_value);
    Sector(int sector_size, Format format, std::istream& s);

    void save(std::ostream& s) const;

    Format format() const { return format_; }
    std::vector<uint8_t>& data() { return data_; }
    const std::vector<uint8_t>& data() const { return data_; }

private:
    Format format_;
    std::vector<uint8_t> data_;
};

class Track {
public:
    // Create a new empty track.
    Track(Format format, int cylinder, int head, int sector_count, int sector_size);

    // Create from IMD stream.
    explicit Track(std::istream& s);

    void save(std::ostream& s) const;

    int cylinder() const { return cylinder_; }
    int head() const { return head_; }
    int sector_count() const { return sector_count_; }
    int sector_size() const { return sector_size_; }
    Format format() const { return format_; }

    Sector* read_sector(int sector);

private:
    uint8_t get_imd_sector_size() const;

    Format format_;
    int cylinder_;
    int head_;
    int sector_count_;
    int sector_size_;

    std::vector<int> sector_ordering_;
    std::vector<std::unique_ptr<Sector>> sectors_;

    static constexpr int kSectorSizes[] = {128, 256, 512, 1024, 2048, 4096, 8192};
    static constexpr int kSectorSizeCount = 7;
};

class FloppyDisk {
public:
    explicit FloppyDisk(const std::string& image_path);

    const std::string& description() const { return imd_header_; }
    bool is_single_sided() const { return is_single_sided_; }
    bool is_write_protected() const { return is_write_protected_; }
    void set_write_protected(bool wp) { is_write_protected_ = wp; }
    const std::string& image_path() const { return image_path_; }
    bool is_modified() const { return is_modified_; }
    void set_modified() { is_modified_ = true; }

    void save();

    Sector* get_sector(int cylinder, int head, int sector);
    Track* get_track(int cylinder, int head);
    void format_track(Format format, int cylinder, int head, int sector_count, int sector_size);

private:
    void load_imd(std::istream& s);
    void save_imd(std::ostream& s) const;
    static std::string read_imd_header(std::istream& s);
    static void write_imd_header(std::ostream& s, const std::string& header);

    std::string imd_header_;
    bool is_single_sided_;
    bool is_write_protected_;
    std::string image_path_;
    bool is_modified_;

    // tracks_[head][cylinder], 2 heads, 77 cylinders max
    std::unique_ptr<Track> tracks_[2][77];
};

} // namespace darkstar
