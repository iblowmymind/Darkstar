/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>
#include "cp/microinstruction.h"

namespace darkstar {

/// This implements an abstraction of the AMD 2901 as seen by the Central Processor --
/// that is: as a 16-bit ALU + register file, rather than four 4-bit ALUs hooked together.
class AM2901 {
public:
    AM2901();

    uint16_t* r() { return r_; }
    const uint16_t* r() const { return r_; }
    uint16_t q() const { return q_; }

    /// Executes the ALU operation specified by the given microinstruction.
    uint16_t execute(const Microinstruction& i, uint16_t d, bool carry_in, bool load_mar);

    /// Executes with all condition flags calculated, even for logical operations.
    /// Significantly slower than execute().
    uint16_t execute_accurate(const Microinstruction& i, uint16_t d, bool carry_in, bool load_mar);

    // Flags
    bool zero = false;
    bool neg = false;
    bool nib_carry = false;
    bool pg_carry = false;
    bool carry_out = false;
    bool overflow = false;

    // Output
    uint16_t y = 0;

private:
    // Registers
    uint16_t r_[16] = {};
    uint16_t q_ = 0;

    // Carry/overflow lookup tables (16x16x2)
    static bool overflow_table_[16][16][2];
    static bool carry_table_arithmetic_[16][16][2];
    static bool carry_table_or_[16][16][2];
    static bool carry_table_and_[16][16][2];
    static bool carry_table_not_xor_[16][16][2];
    static bool overflow_not_xor_[16][16][2];

    static bool tables_built_;
    static void build_tables();

    static bool calc_overflow(int r, int s, int c_in);
    static bool calc_carry_arithmetic(int r, int s, int c_in);
    static bool calc_carry_or(int r, int s, int c_in);
    static bool calc_carry_and(int r, int s, int c_in);
    static bool calc_carry_not_xor(int r, int s, int c_in);
    static bool calc_overflow_not_xor(int r, int s, int c_in);

    // Common source selection
    void select_sources(const Microinstruction& i, uint16_t d, int& r, int& s);

    // Common destination/shift writeback
    void write_destination(const Microinstruction& i, int f, bool carry_in, int c_in);

    // Common loadMAR upper-byte fixup
    void apply_load_mar(const Microinstruction& i, int& f, int r, int s);
};

} // namespace darkstar
