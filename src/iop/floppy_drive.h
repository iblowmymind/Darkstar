#pragma once
#include <cstdint>
#include "../io/floppy_disk.h"

// Track encoding formats used by the floppy controller
enum class Format {
    FM500  = 0,
    MFM500 = 3,
};

class FloppyDrive {
public:
    FloppyDisk* Disk{nullptr};
    int Track{0};
    bool IsLoaded{false};
    bool IsWriteProtected{false};
    bool IsSingleSided{true};
    bool Track0{true};
    bool Index{false};
    bool DiskChange{false};
    bool DriveSelect{false};

    void Reset()         { Track = 0; Track0 = true; }
    void SeekTo(int t)   { Track = t; Track0 = (t == 0); }
    void LoadDisk(FloppyDisk* d)  { Disk = d; IsLoaded = (d != nullptr); DiskChange = true; }
    void UnloadDisk()             { Disk = nullptr; IsLoaded = false; DiskChange = true; }
};