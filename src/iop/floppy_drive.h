/*
    BSD 2-Clause License

    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this
      list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once
#include <cstdint>

// Forward declarations for floppy disk types (full implementation in later phase)
enum class Format { 
    FM500 = 0, 
    MFM500 = 3 
};

struct Sector { 
    uint8_t* Data{nullptr}; 
    int Size{0}; 
};

struct Track {
    int SectorCount{0};
    ::Format TrackFormat{::Format::FM500};
    Sector ReadSector(int idx) { return {}; } // stub
};

class FloppyDisk {
public:
    Track* GetTrack(int cylinder, int head) { return nullptr; }
    Sector* GetSector(int cylinder, int head, int sector) { return nullptr; }
    void SetModified() {}
    void FormatTrack(::Format format, int cylinder, int head, int sectorCount, int sectorSize) {}
    bool IsWriteProtected{false};
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
    
    void Reset() { Track = 0; Track0 = true; }
    void SeekTo(int t) { Track = t; Track0 = (t == 0); }
    void LoadDisk(FloppyDisk* d) { Disk = d; IsLoaded = (d != nullptr); DiskChange = true; }
    void UnloadDisk() { Disk = nullptr; IsLoaded = false; DiskChange = true; }
};