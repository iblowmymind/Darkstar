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

#include "iop_memory_bus.h"
#include "../types.h"
#include <fstream>
#include <iostream>

IOPMemoryBus::IOPMemoryBus(I8085IOBus* ioBus) : _io(ioBus) {
    // Initialize ROM and RAM to zero
    _rom.fill(0);
    _ram.fill(0);
    _hostIdProm.fill(0);
    
    LoadPROMs();
    LoadHostIDProm();
}

uint8_t IOPMemoryBus::ReadByte(uint16_t address) {
    if (address < 0x2000) {
        // ROM space: 0x0000-0x1FFF
        return _rom[address];
    } else if (address >= 0x2000 && address < 0x8000) {
        // RAM space: 0x2000-0x7FFF (we only use up to 0x5FFF)
        if (address < 0x8000) {
            return _ram[address - 0x2000];
        }
    } else if (address >= 0x80B0 && address <= 0x80BF) {
        // Host Address PROM: 0x80B0-0x80BF
        return _hostIdProm[address - 0x80B0];
    } else if (address >= 0x8001 && address <= 0x80FF) {
        // Memory-mapped I/O: 0x8001-0x80FF
        return _io->In(static_cast<uint8_t>(address));
    }
    
    // Unmapped address space returns 0xFF
    return 0xFF;
}

void IOPMemoryBus::WriteByte(uint16_t address, uint8_t b) {
    if (address < 0x2000) {
        // ROM space: writes ignored
        return;
    } else if (address >= 0x2000 && address < 0x8000) {
        // RAM space: 0x2000-0x7FFF (we only use up to 0x5FFF)
        if (address < 0x8000) {
            _ram[address - 0x2000] = b;
        }
    } else if (address >= 0x80B0 && address <= 0x80BF) {
        // Host Address PROM: writable
        _hostIdProm[address - 0x80B0] = b;
    } else if (address >= 0x8001 && address <= 0x80FF) {
        // Memory-mapped I/O: 0x8001-0x80FF
        _io->Out(static_cast<uint8_t>(address), b);
    }
    // Other writes ignored
}

uint16_t IOPMemoryBus::ReadWord(uint16_t address) {
    // Little-endian: low byte first
    uint8_t lo = ReadByte(address);
    uint8_t hi = ReadByte(address + 1);
    return lo | (static_cast<uint16_t>(hi) << 8);
}

void IOPMemoryBus::WriteWord(uint16_t address, uint16_t w) {
    // Little-endian: low byte first
    WriteByte(address, static_cast<uint8_t>(w & 0xFF));
    WriteByte(address + 1, static_cast<uint8_t>(w >> 8));
}

void IOPMemoryBus::LoadPROMs() {
    // Rev 3.1 PROM layout (matches the original C# implementation):
    //   U129 - 537P03029.bin - $0000
    //   U130 - 537P03030.bin - $0800
    //   U131 - 537P03700.bin - $1000
    //   U132 - 537P03032.bin - $1800
    LoadPROM("537P03029.bin", 0x0000);
    LoadPROM("537P03030.bin", 0x0800);
    LoadPROM("537P03700.bin", 0x1000);
    LoadPROM("537P03032.bin", 0x1800);
}

void IOPMemoryBus::LoadPROM(const std::string& promName, uint16_t address) {
    std::string path = "IOP/PROM/" + promName;
    std::ifstream file(path, std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Warning: Could not load PROM file: " << path << std::endl;
        // Fill with 0xFF as default
        for (uint16_t i = 0; i < 0x800 && (address + i) < 0x2000; i++) {
            _rom[address + i] = 0xFF;
        }
        return;
    }

    // Read PROM data (each PROM is exactly 0x800 bytes)
    char byte;
    uint16_t offset = 0;
    while (file.get(byte) && offset < 0x800 && (address + offset) < 0x2000) {
        _rom[address + offset] = static_cast<uint8_t>(byte);
        offset++;
    }

    // Fill remainder with 0xFF if file was shorter than expected
    while (offset < 0x800 && (address + offset) < 0x2000) {
        _rom[address + offset] = 0xFF;
        offset++;
    }
}

void IOPMemoryBus::LoadHostIDProm() {
    UpdateHostIDProm();
}

void IOPMemoryBus::UpdateHostIDProm() {
    // Set initial values to 0xFF
    _hostIdProm.fill(0xFF);
    
    // Use Configuration::HostID if available
    uint64_t hostId = Configuration::HostID;
    
    // Encode the Host ID in the PROM (implementation depends on Star requirements)
    // For now, store the lower 48 bits (6 bytes) in the first 6 locations
    for (int i = 0; i < 6; i++) {
        SetIDPromByte(i, static_cast<uint8_t>((hostId >> (i * 8)) & 0xFF));
    }
}

uint8_t IOPMemoryBus::GetIDPromByte(int byteNumber) const {
    if (byteNumber >= 0 && byteNumber < 16) {
        return _hostIdProm[byteNumber];
    }
    return 0xFF;
}

void IOPMemoryBus::SetIDPromByte(int byteNumber, uint8_t value) {
    if (byteNumber >= 0 && byteNumber < 16) {
        _hostIdProm[byteNumber] = value;
    }
}

uint8_t IOPMemoryBus::RotateLeft(uint8_t value) {
    return (value << 1) | (value >> 7);
}