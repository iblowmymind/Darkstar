/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
*/
#include "sa1000.h"
#include <cstring>

SA1000Drive::SA1000Drive() {
    Reset();
}

void SA1000Drive::Reset() {
    _modified = false;
}

bool SA1000Drive::Load(const std::vector<uint8_t>& data) {
    _data = data;
    // Determine geometry from image size
    size_t totalSectors = data.size() / BYTES_PER_SECTOR;
    size_t sectorsPerCylinder = HEADS * SECTORS_PER_TRACK;
    _cylinders = (int)(totalSectors / sectorsPerCylinder);
    if (_cylinders <= 0) _cylinders = CYLINDERS_40MB;
    _loaded = true;
    return true;
}

int SA1000Drive::SectorOffset(int cylinder, int head, int sector) const {
    return ((cylinder * HEADS + head) * SECTORS_PER_TRACK + sector) * BYTES_PER_SECTOR;
}

bool SA1000Drive::ReadSector(int cylinder, int head, int sector, uint8_t* buf) {
    if (!_loaded) return false;
    int offset = SectorOffset(cylinder, head, sector);
    if (offset + BYTES_PER_SECTOR > (int)_data.size()) {
        memset(buf, 0, BYTES_PER_SECTOR);
        return false;
    }
    memcpy(buf, _data.data() + offset, BYTES_PER_SECTOR);
    return true;
}

bool SA1000Drive::WriteSector(int cylinder, int head, int sector, const uint8_t* buf) {
    if (!_loaded) return false;
    int offset = SectorOffset(cylinder, head, sector);
    if (offset + BYTES_PER_SECTOR > (int)_data.size()) return false;
    memcpy(_data.data() + offset, buf, BYTES_PER_SECTOR);
    _modified = true;
    return true;
}
