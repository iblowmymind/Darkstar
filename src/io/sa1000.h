/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
*/
#pragma once
#include <cstdint>
#include <vector>
#include <string>

// SA1000 hard drive image (raw sector layout: 1024 bytes/sector)
class SA1000Drive {
public:
    SA1000Drive();

    void Reset();

    bool IsLoaded() const { return _loaded; }

    // Load a drive image from a memory buffer
    bool Load(const std::vector<uint8_t>& data);

    // Get image data (for saving)
    const std::vector<uint8_t>& GetData() const { return _data; }
    bool IsModified() const { return _modified; }

    // SA1000 geometry
    static constexpr int BYTES_PER_SECTOR = 1024;
    static constexpr int SECTORS_PER_TRACK = 32;
    static constexpr int HEADS = 5;
    static constexpr int CYLINDERS_10MB  = 306;
    static constexpr int CYLINDERS_40MB  = 823;
    static constexpr int CYLINDERS_80MB  = 1024;

    int GetCylinders() const { return _cylinders; }

    // Read/write a sector (raw linear sector index)
    bool ReadSector(int cylinder, int head, int sector, uint8_t* buf);
    bool WriteSector(int cylinder, int head, int sector, const uint8_t* buf);

private:
    int SectorOffset(int cylinder, int head, int sector) const;

    std::vector<uint8_t> _data;
    bool _loaded{false};
    bool _modified{false};
    int _cylinders{CYLINDERS_40MB};
};
