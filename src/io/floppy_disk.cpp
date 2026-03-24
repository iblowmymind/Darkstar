/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
*/
#include "floppy_disk.h"
#include <cstring>
#include <algorithm>

FloppyDisk::FloppyDisk(const std::vector<uint8_t>& data) {
    if (data.size() > 4 && data[0] == 'I' && data[1] == 'M' && data[2] == 'D') {
        _loaded = ParseIMD(data);
    } else {
        // Raw format: assume 77 cylinders, 2 heads, 26 sectors, 128 bytes/sector
        _cylinders = 77;
        _heads = 2;
        _sectors = 26;
        int sectorSize = 128;
        int total = _cylinders * _heads * _sectors;
        _sectors_data.resize(total);
        for (int c = 0; c < _cylinders; c++) {
            for (int h = 0; h < _heads; h++) {
                for (int s = 0; s < _sectors; s++) {
                    int idx = (c * _heads + h) * _sectors + s;
                    _sectors_data[idx].cylinder = c;
                    _sectors_data[idx].head = h;
                    _sectors_data[idx].sector = s + 1;
                    _sectors_data[idx].sectorSize = sectorSize;
                    _sectors_data[idx].data.resize(sectorSize, 0);
                    int offset = idx * sectorSize;
                    if (offset + sectorSize <= (int)data.size()) {
                        std::copy(data.begin() + offset,
                                  data.begin() + offset + sectorSize,
                                  _sectors_data[idx].data.begin());
                    }
                }
            }
        }
        _loaded = true;
    }
}

bool FloppyDisk::ParseIMD(const std::vector<uint8_t>& data) {
    // Skip ASCII header (up to 0x1A)
    size_t pos = 0;
    while (pos < data.size() && data[pos] != 0x1A) pos++;
    if (pos >= data.size()) return false;
    pos++; // skip 0x1A

    int maxCyl = 0, maxHead = 0, maxSec = 0;

    while (pos < data.size()) {
        if (pos + 5 > data.size()) break;
        uint8_t mode = data[pos++];
        (void)mode;
        uint8_t cyl = data[pos++];
        uint8_t head = data[pos++];
        uint8_t nsec = data[pos++];
        uint8_t secsize = data[pos++];

        int sectorBytes = 128 << secsize;
        bool hasCylMap = (head & 0x80) != 0;
        bool hasSideMap = (head & 0x40) != 0;
        head &= 0x3F;

        std::vector<uint8_t> secmap(nsec);
        for (int i = 0; i < nsec; i++) secmap[i] = data[pos++];

        std::vector<uint8_t> cylmap, sidemap;
        if (hasCylMap) { cylmap.resize(nsec); for (int i=0;i<nsec;i++) cylmap[i]=data[pos++]; }
        if (hasSideMap) { sidemap.resize(nsec); for (int i=0;i<nsec;i++) sidemap[i]=data[pos++]; }

        if (cyl > maxCyl) maxCyl = cyl;
        if (head > maxHead) maxHead = head;
        if (nsec > maxSec) maxSec = nsec;

        for (int s = 0; s < nsec; s++) {
            if (pos >= data.size()) break;
            uint8_t rectype = data[pos++];
            SectorData sd;
            sd.cylinder = cyl;
            sd.head = head;
            sd.sector = secmap[s];
            sd.sectorSize = sectorBytes;
            sd.data.resize(sectorBytes, 0);
            if (rectype == 0) {
                // sector not available
            } else if (rectype == 1) {
                // normal sector
                for (int b = 0; b < sectorBytes && pos < (int)data.size(); b++)
                    sd.data[b] = data[pos++];
            } else if (rectype == 2) {
                // compressed
                uint8_t fill = data[pos++];
                std::fill(sd.data.begin(), sd.data.end(), fill);
            }
            _sectors_data.push_back(std::move(sd));
        }
    }

    _cylinders = maxCyl + 1;
    _heads = maxHead + 1;
    _sectors = maxSec;
    return true;
}

bool FloppyDisk::ReadSector(int cylinder, int head, int sector, uint8_t* buf, int bufLen) {
    for (auto& sd : _sectors_data) {
        if (sd.cylinder == cylinder && sd.head == head && sd.sector == sector) {
            int n = std::min(bufLen, (int)sd.data.size());
            memcpy(buf, sd.data.data(), n);
            return true;
        }
    }
    return false;
}

bool FloppyDisk::WriteSector(int cylinder, int head, int sector, const uint8_t* buf, int bufLen) {
    for (auto& sd : _sectors_data) {
        if (sd.cylinder == cylinder && sd.head == head && sd.sector == sector) {
            int n = std::min(bufLen, (int)sd.data.size());
            memcpy(sd.data.data(), buf, n);
            _modified = true;
            return true;
        }
    }
    return false;
}

void FloppyDisk::GetImageData(std::vector<uint8_t>& out) const {
    // Return raw dump
    for (const auto& sd : _sectors_data)
        out.insert(out.end(), sd.data.begin(), sd.data.end());
}
