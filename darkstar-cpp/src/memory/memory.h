/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>
#include <vector>

namespace darkstar {

class Memory {
public:
    Memory();

    void reset();

    int size() const { return static_cast<int>(memory_.size()); }

    uint16_t read_word(int address, bool& valid) const;
    void write_word(int address, uint16_t value);

    void set_check_bits(int invert_bits);

private:
    uint8_t calculate_ecc_syndrome(uint16_t word) const;
    void write_ecc(int address, uint16_t value);

    std::vector<uint16_t> memory_;
    std::vector<uint8_t> ecc_;

    int mctl_invert_ = 0;
};

} // namespace darkstar
