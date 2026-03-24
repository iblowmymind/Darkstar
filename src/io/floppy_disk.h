/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
*/
#pragma once
#include <cstdint>
#include <vector>
#include <string>

// Floppy disk image formats
enum class FloppyDiskFormat {
    IMD,   // ImageDisk format
    Raw,   // Raw sector dump
};

class FloppyDisk {
public:
    FloppyDisk() = default;
    explicit FloppyDisk(const std::vector<uint8_t>& data);

    bool IsLoaded() const { return _loaded; }
    bool IsModified() const { return _modified; }

    // Read a sector: returns true on success
    bool ReadSector(int cylinder, int head, int sector, uint8_t* buf, int bufLen);
    // Write a sector: returns true on success
    bool WriteSector(int cylinder, int head, int sector, const uint8_t* buf, int bufLen);

    // Number of cylinders/heads/sectors
    int Cylinders() const { return _cylinders; }
    int Heads() const { return _heads; }
    int Sectors() const { return _sectors; }

    void GetImageData(std::vector<uint8_t>& out) const;

    // Mark the image as modified (called after write operations).
    void SetModified() { _modified = true; }

    // Format a track (stub – geometric metadata only; content is written via WriteSector).
    void FormatTrack(int /*format*/, int /*cylinder*/, int /*head*/, int /*sectorCount*/, int /*sectorSize*/) {}

private:
    bool ParseIMD(const std::vector<uint8_t>& data);

    struct SectorData {
        int cylinder{0};
        int head{0};
        int sector{0};
        int sectorSize{512};
        std::vector<uint8_t> data;
    };

    std::vector<SectorData> _sectors_data;
    int _cylinders{0};
    int _heads{0};
    int _sectors{0};
    bool _loaded{false};
    bool _modified{false};
};
