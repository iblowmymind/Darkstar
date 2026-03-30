/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>

namespace darkstar {

class I8085MemoryBus {
public:
    virtual ~I8085MemoryBus() = default;
    virtual uint8_t read_byte(uint16_t address) = 0;
    virtual void write_byte(uint16_t address, uint8_t b) = 0;
    virtual uint16_t read_word(uint16_t address) = 0;
    virtual void write_word(uint16_t address, uint16_t w) = 0;
};

} // namespace darkstar
