/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/

#include "iop/iop_memory_bus.h"
#include "core/configuration.h"
#include "core/log.h"

#include <cstring>
#include <fstream>
#include <stdexcept>
#include <filesystem>

namespace darkstar {

IOPMemoryBus::IOPMemoryBus(I8085IOBus& io_bus)
    : io_(io_bus)
{
    std::memset(rom_, 0, sizeof(rom_));
    std::memset(ram_, 0, sizeof(ram_));
    std::memset(host_id_prom_, 0, sizeof(host_id_prom_));

    load_proms();
    load_host_id_prom();
}

uint8_t IOPMemoryBus::read_byte(uint16_t address) {
    if (address < 0x2000) {
        return rom_[address];
    } else if (address < 0x6000) {
        return ram_[address];
    } else if (address > 0x80af && address < 0x80c0) {
        // Host Address PROM: 12 nybbles of 48-bit Ethernet host address + checksum.
        return host_id_prom_[address - 0x80b0];
    } else if (address > 0x8000 && address < 0x8100) {
        // Memory-mapped I/O ports
        if (Log::enabled) Log::write(LogComponent::IOPMemory, "Memory-mapped I/O read from %04x", address);
        return io_.in(static_cast<uint8_t>(address));
    } else {
        if (Log::enabled) Log::write(LogComponent::IOPMemory, "Read from nonexistent memory at %04x", address);
        return 0xff;
    }
}

void IOPMemoryBus::write_byte(uint16_t address, uint8_t b) {
    if (address < 0x2000) {
        // ROM, not writeable.
    } else if (address < 0x6000) {
        ram_[address] = b;
    } else if (address > 0x8000 && address < 0x8100) {
        // Memory-mapped I/O ports
        if (Log::enabled) Log::write(LogComponent::IOPMemory, "Memory-mapped I/O write at %04x", address);
        io_.out(static_cast<uint8_t>(address), b);
    } else {
        if (Log::enabled) Log::write(LogComponent::IOPMemory, "Write to nonexistent memory at %04x", address);
    }
}

uint16_t IOPMemoryBus::read_word(uint16_t address) {
    return static_cast<uint16_t>(read_byte(address) |
           (read_byte(static_cast<uint16_t>(address + 1)) << 8));
}

void IOPMemoryBus::write_word(uint16_t address, uint16_t w) {
    write_byte(address, static_cast<uint8_t>(w));
    write_byte(static_cast<uint16_t>(address + 1), static_cast<uint8_t>(w >> 8));
}

void IOPMemoryBus::update_host_id_prom() {
    load_host_id_prom();
}

void IOPMemoryBus::load_proms() {
    // Loads PROMs into ROM space.  The files for rev 3.1 are:
    //  U129 - 537P03029 - $0000
    //  U130 - 537P03030 - $0800
    //  U131 - 537P03700 - $1000
    //  U132 - 537P03032 - $1800
    load_prom("537P03029.bin", 0x0000);
    load_prom("537P03030.bin", 0x0800);
    load_prom("537P03700.bin", 0x1000);
    load_prom("537P03032.bin", 0x1800);
}

void IOPMemoryBus::load_prom(const std::string& prom_name, uint16_t address) {
    std::string prom_path;

    if (!StartupOptions::rom_path.empty()) {
        prom_path = (std::filesystem::path(StartupOptions::rom_path) / prom_name).string();
    } else {
        prom_path = (std::filesystem::path("IOP") / "PROM" / prom_name).string();
    }

    std::ifstream prom_stream(prom_path, std::ios::binary);
    if (!prom_stream) {
        throw std::runtime_error("PROM file " + prom_name + " was not found at " + prom_path);
    }

    prom_stream.seekg(0, std::ios::end);
    auto size = prom_stream.tellg();
    prom_stream.seekg(0, std::ios::beg);

    if (size != 0x800) {
        throw std::runtime_error("PROM file " + prom_name + " has unexpected size");
    }

    prom_stream.read(reinterpret_cast<char*>(&rom_[address]), 0x800);
}

void IOPMemoryBus::load_host_id_prom() {
    // Copy data from the current emulator config into the HostID prom array.
    for (int i = 0; i < 6; i++) {
        uint8_t val = static_cast<uint8_t>(Configuration::host_id >> ((5 - i) * 8));
        set_id_prom_byte(i, val);
    }

    // Calculate the checksum of the PROM.
    // Looking at the PROM as 8 bytes (big-endian) rather than 16 nibbles:
    // Bytes 0 & 1 XOR'd together, then cyclic XOR with bytes 2-5.
    // Checksum in byte 6, complement in byte 7.
    uint8_t checksum = rotate_left(static_cast<uint8_t>(get_id_prom_byte(0) ^ get_id_prom_byte(1)));

    for (int i = 2; i < 6; i++) {
        checksum ^= get_id_prom_byte(i);
        checksum = rotate_left(checksum);
    }

    set_id_prom_byte(6, checksum);
    set_id_prom_byte(7, static_cast<uint8_t>(~checksum));
}

uint8_t IOPMemoryBus::get_id_prom_byte(int byte_number) const {
    return static_cast<uint8_t>((host_id_prom_[byte_number * 2] & 0xf) |
                                (host_id_prom_[byte_number * 2 + 1] << 4));
}

void IOPMemoryBus::set_id_prom_byte(int byte_number, uint8_t value) {
    host_id_prom_[byte_number * 2] = static_cast<uint8_t>(value & 0xf);
    host_id_prom_[byte_number * 2 + 1] = static_cast<uint8_t>(value >> 4);
}

uint8_t IOPMemoryBus::rotate_left(uint8_t value) {
    return static_cast<uint8_t>((value << 1) | ((value & 0x80) != 0 ? 1 : 0));
}

} // namespace darkstar
