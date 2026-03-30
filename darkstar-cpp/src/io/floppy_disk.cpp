/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/

#include "io/floppy_disk.h"

#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace darkstar {

//
// Sector implementation
//
Sector::Sector(int sector_size, Format format)
    : format_(format)
    , data_(sector_size, 0)
{
}

Sector::Sector(int sector_size, Format format, uint8_t compressed_value)
    : format_(format)
    , data_(sector_size, compressed_value)
{
}

Sector::Sector(int sector_size, Format format, std::istream& s)
    : format_(format)
    , data_(sector_size, 0)
{
    s.read(reinterpret_cast<char*>(data_.data()), sector_size);
    if (s.gcount() != sector_size) {
        throw std::runtime_error("Short read in sector data.");
    }
}

void Sector::save(std::ostream& s) const {
    s.write(reinterpret_cast<const char*>(data_.data()),
            static_cast<std::streamsize>(data_.size()));
}

//
// Track implementation
//
constexpr int Track::kSectorSizes[];

Track::Track(Format format, int cylinder, int head, int sector_count, int sector_size)
    : format_(format)
    , cylinder_(cylinder)
    , head_(head)
    , sector_count_(sector_count)
    , sector_size_(sector_size)
{
    sectors_.resize(sector_count);
    sector_ordering_.reserve(sector_count);

    for (int i = 0; i < sector_count; i++) {
        sectors_[i] = std::make_unique<Sector>(sector_size, format);
        sector_ordering_.push_back(i + 1);  // 1:1 interleave
    }
}

Track::Track(std::istream& s) {
    bool b_cyl_map = false;
    bool b_head_map = false;

    uint8_t format_byte, cyl_byte, head_byte, sector_count_byte, sector_size_index;
    s.read(reinterpret_cast<char*>(&format_byte), 1);
    s.read(reinterpret_cast<char*>(&cyl_byte), 1);
    s.read(reinterpret_cast<char*>(&head_byte), 1);
    s.read(reinterpret_cast<char*>(&sector_count_byte), 1);
    s.read(reinterpret_cast<char*>(&sector_size_index), 1);

    format_ = static_cast<Format>(format_byte);
    cylinder_ = cyl_byte;
    head_ = head_byte;
    sector_count_ = sector_count_byte;

    // Sanity checks
    if (static_cast<int>(format_) > static_cast<int>(Format::MFM250) ||
        cylinder_ > 77 ||
        (head_ & 0x3f) > 1 ||
        sector_size_index > kSectorSizeCount - 1) {
        throw std::runtime_error("Invalid header data for track.");
    }

    sector_size_ = kSectorSizes[sector_size_index];

    b_cyl_map = (head_ & 0x80) != 0;
    b_head_map = (head_ & 0x40) != 0;

    // Head is just the first bit.
    head_ = head_ & 0x1;

    // Read sector numbering.
    sector_ordering_.reserve(sector_count_);
    for (int i = 0; i < sector_count_; i++) {
        uint8_t val;
        s.read(reinterpret_cast<char*>(&val), 1);
        sector_ordering_.push_back(val);
    }

    if (b_cyl_map || b_head_map) {
        throw std::runtime_error("IMD Cylinder and Head maps not supported.");
    }

    // Read sector data.
    sectors_.resize(sector_count_);
    for (int i = 0; i < sector_count_; i++) {
        uint8_t type_byte;
        s.read(reinterpret_cast<char*>(&type_byte), 1);
        SectorRecordType type = static_cast<SectorRecordType>(type_byte);

        int idx = sector_ordering_[i] - 1;

        switch (type) {
            case SectorRecordType::Unavailable:
                // Nothing, sector left null.
                break;

            case SectorRecordType::Normal:
            case SectorRecordType::NormalDeleted:
            case SectorRecordType::NormalError:
            case SectorRecordType::DeletedError:
                sectors_[idx] = std::make_unique<Sector>(sector_size_, format_, s);
                break;

            case SectorRecordType::Compressed:
            case SectorRecordType::CompressedDeleted:
            case SectorRecordType::CompressedError:
            case SectorRecordType::CompressedDeletedError: {
                uint8_t compressed_data;
                s.read(reinterpret_cast<char*>(&compressed_data), 1);
                sectors_[idx] = std::make_unique<Sector>(sector_size_, format_, compressed_data);
                break;
            }

            default:
                throw std::runtime_error("Unexpected IMD sector data type.");
        }
    }
}

void Track::save(std::ostream& s) const {
    uint8_t bytes[5];
    bytes[0] = static_cast<uint8_t>(format_);
    bytes[1] = static_cast<uint8_t>(cylinder_);
    bytes[2] = static_cast<uint8_t>(head_);
    bytes[3] = static_cast<uint8_t>(sector_count_);
    bytes[4] = get_imd_sector_size();
    s.write(reinterpret_cast<const char*>(bytes), 5);

    // Write sector numbering.
    for (int i = 0; i < sector_count_; i++) {
        uint8_t val = static_cast<uint8_t>(sector_ordering_[i]);
        s.write(reinterpret_cast<const char*>(&val), 1);
    }

    // Write sector data.
    for (int i = 0; i < sector_count_; i++) {
        int idx = sector_ordering_[i] - 1;
        Sector* sector = (idx >= 0 && idx < static_cast<int>(sectors_.size()))
            ? sectors_[idx].get() : nullptr;

        if (sector == nullptr) {
            uint8_t unavail = static_cast<uint8_t>(SectorRecordType::Unavailable);
            s.write(reinterpret_cast<const char*>(&unavail), 1);
        } else {
            uint8_t normal = static_cast<uint8_t>(SectorRecordType::Normal);
            s.write(reinterpret_cast<const char*>(&normal), 1);
            sector->save(s);
        }
    }
}

Sector* Track::read_sector(int sector) {
    if (sector >= 0 && sector < static_cast<int>(sectors_.size())) {
        return sectors_[sector].get();
    }
    return nullptr;
}

uint8_t Track::get_imd_sector_size() const {
    for (int i = 0; i < kSectorSizeCount; i++) {
        if (sector_size_ == kSectorSizes[i]) {
            return static_cast<uint8_t>(i);
        }
    }
    throw std::runtime_error("No IMD sector size match.");
}

//
// FloppyDisk implementation
//
FloppyDisk::FloppyDisk(const std::string& image_path)
    : is_single_sided_(true)
    , is_write_protected_(false)
    , image_path_(image_path)
    , is_modified_(false)
{
    std::ifstream fs(image_path, std::ios::binary);
    if (!fs) {
        throw std::runtime_error("Failed to open floppy disk image.");
    }
    load_imd(fs);
}

void FloppyDisk::save() {
    std::ofstream fs(image_path_, std::ios::binary);
    if (!fs) {
        throw std::runtime_error("Failed to open floppy disk image for writing.");
    }
    save_imd(fs);
}

Sector* FloppyDisk::get_sector(int cylinder, int head, int sector) {
    Track* t = get_track(cylinder, head);
    if (t) {
        return t->read_sector(sector);
    }
    return nullptr;
}

Track* FloppyDisk::get_track(int cylinder, int head) {
    if (head >= 0 && head < 2 && cylinder >= 0 && cylinder < 77) {
        return tracks_[head][cylinder].get();
    }
    return nullptr;
}

void FloppyDisk::format_track(Format format, int cylinder, int head,
                               int sector_count, int sector_size) {
    if (head >= 0 && head < 2 && cylinder >= 0 && cylinder < 77) {
        tracks_[head][cylinder] = std::make_unique<Track>(
            format, cylinder, head, sector_count, sector_size);
    }
}

void FloppyDisk::load_imd(std::istream& s) {
    imd_header_ = read_imd_header(s);

    while (s.peek() != std::char_traits<char>::eof()) {
        auto t = std::make_unique<Track>(s);

        if (t->cylinder() < 0 || t->cylinder() > 76) {
            throw std::runtime_error("Invalid cylinder value in IMD file.");
        }

        if (t->head() < 0 || t->head() > 1) {
            throw std::runtime_error("Invalid head value in IMD file.");
        }

        if (tracks_[t->head()][t->cylinder()] != nullptr) {
            throw std::runtime_error("Duplicate head/track in IMD file.");
        }

        if (t->head() != 0) {
            is_single_sided_ = false;
        }

        int h = t->head();
        int c = t->cylinder();
        tracks_[h][c] = std::move(t);
    }
}

void FloppyDisk::save_imd(std::ostream& s) const {
    write_imd_header(s, imd_header_);

    for (int cylinder = 0; cylinder < 77; cylinder++) {
        for (int head = 0; head < 2; head++) {
            if (tracks_[head][cylinder] != nullptr) {
                tracks_[head][cylinder]->save(s);
            }
        }
    }
}

std::string FloppyDisk::read_imd_header(std::istream& s) {
    std::string header;
    while (s.peek() != std::char_traits<char>::eof()) {
        uint8_t b;
        s.read(reinterpret_cast<char*>(&b), 1);
        if (b == 0x1a) {
            break;
        }
        header += static_cast<char>(b);
    }
    return header;
}

void FloppyDisk::write_imd_header(std::ostream& s, const std::string& header) {
    s.write(header.data(), static_cast<std::streamsize>(header.size()));
    uint8_t terminator = 0x1a;
    s.write(reinterpret_cast<const char*>(&terminator), 1);
}

} // namespace darkstar
