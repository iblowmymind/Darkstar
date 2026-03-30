/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/

#include "memory/memory.h"
#include "core/configuration.h"
#include "core/log.h"

namespace darkstar {

Memory::Memory() {
    reset();
}

void Memory::reset() {
    size_t required_size = static_cast<size_t>(Configuration::memory_size) * 1024;

    if (memory_.size() != required_size) {
        memory_.assign(required_size, 0);
        ecc_.assign(required_size, 0);
    } else {
        std::fill(memory_.begin(), memory_.end(), static_cast<uint16_t>(0));
        std::fill(ecc_.begin(), ecc_.end(), static_cast<uint8_t>(0));
    }
}

uint16_t Memory::read_word(int address, bool& valid) const {
    if (address >= 0 && address < static_cast<int>(memory_.size())) {
        uint16_t mem_word = memory_[address];
        valid = ecc_[address] == calculate_ecc_syndrome(mem_word);
        return mem_word;
    } else {
        valid = false;
        return 0;
    }
}

void Memory::write_word(int address, uint16_t value) {
    if (address >= 0 && address < static_cast<int>(memory_.size())) {
        write_ecc(address, value);
        memory_[address] = value;
    } else {
        if (Log::enabled) Log::write(LogComponent::MemoryAccess,
            "Write to nonexistent memory address 0x%x", address);
    }
}

void Memory::set_check_bits(int invert_bits) {
    mctl_invert_ = invert_bits;
}

uint8_t Memory::calculate_ecc_syndrome(uint16_t /*word*/) const {
    // Not actually calculating ECC syndrome - stubbed to always return 0.
    return 0;
}

void Memory::write_ecc(int address, uint16_t value) {
    ecc_[address] = static_cast<uint8_t>(calculate_ecc_syndrome(value) ^ mctl_invert_);
}

} // namespace darkstar
