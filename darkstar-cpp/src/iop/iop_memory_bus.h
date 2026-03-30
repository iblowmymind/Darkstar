/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include "iop/i8085_memory_bus.h"
#include "iop/i8085_io_bus.h"

#include <cstdint>
#include <string>

namespace darkstar {

/// Implements the IOP's memory bus.  Memory map:
/// $0000 - $1FFF : PROM (8K)
/// $2000 - $5FFF : RAM  (16K)
/// $80B0 - $80BF : Host Addr PROM (16 bytes)
/// $8000 - $80FF : Memory-mapped I/O (delegates to IOPIOBus)
class IOPMemoryBus : public I8085MemoryBus {
public:
    explicit IOPMemoryBus(I8085IOBus& io_bus);

    uint8_t read_byte(uint16_t address) override;
    void write_byte(uint16_t address, uint8_t b) override;
    uint16_t read_word(uint16_t address) override;
    void write_word(uint16_t address, uint16_t w) override;

    void update_host_id_prom();

private:
    void load_proms();
    void load_prom(const std::string& prom_name, uint16_t address);
    void load_host_id_prom();
    uint8_t get_id_prom_byte(int byte_number) const;
    void set_id_prom_byte(int byte_number, uint8_t value);
    static uint8_t rotate_left(uint8_t value);

    uint8_t rom_[0x2000];       // 8K ROM
    uint8_t ram_[0x6000];       // 24K allocated (16K used at $2000-$5FFF)
    uint8_t host_id_prom_[16];  // 16 nybbles: Ethernet MAC + checksum

    I8085IOBus& io_;
};

} // namespace darkstar
