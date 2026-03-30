/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/

#include "cp/am2901.h"
#include <stdexcept>

namespace darkstar {

bool AM2901::overflow_table_[16][16][2] = {};
bool AM2901::carry_table_arithmetic_[16][16][2] = {};
bool AM2901::carry_table_or_[16][16][2] = {};
bool AM2901::carry_table_and_[16][16][2] = {};
bool AM2901::carry_table_not_xor_[16][16][2] = {};
bool AM2901::overflow_not_xor_[16][16][2] = {};
bool AM2901::tables_built_ = false;

AM2901::AM2901() {
    if (!tables_built_) {
        build_tables();
        tables_built_ = true;
    }
}

void AM2901::select_sources(const Microinstruction& i, uint16_t d, int& r, int& s) {
    switch (i.aS) {
        case AluSourcePair::AQ:
            r = r_[i.rA]; s = q_;
            break;
        case AluSourcePair::AB:
            r = r_[i.rA]; s = r_[i.rB];
            break;
        case AluSourcePair::ZQ:
            r = 0; s = q_;
            break;
        case AluSourcePair::ZB:
            r = 0; s = r_[i.rB];
            break;
        case AluSourcePair::ZA:
            r = 0; s = r_[i.rA];
            break;
        case AluSourcePair::DA:
            r = d; s = r_[i.rA];
            break;
        case AluSourcePair::DQ:
            r = d; s = q_;
            break;
        case AluSourcePair::D0:
            r = d; s = 0;
            break;
        default:
            r = 0; s = 0;
            break;
    }
}

void AM2901::apply_load_mar(const Microinstruction& i, int& f, int r, int s) {
    switch (static_cast<AluFunction>(static_cast<int>(i.aF) | 0x3)) {
        case AluFunction::RorS: {
            f = (f & 0xff) | (r_[i.rB] & 0xff00);
            bool mid_carry = carry_table_or_[(r >> 8) & 0xf][(s >> 8) & 0xf][pg_carry ? 1 : 0];
            overflow = carry_out = carry_table_or_[(r >> 12) & 0xf][(s >> 12) & 0xf][mid_carry ? 1 : 0];
            break;
        }
        case AluFunction::notRxorS: {
            f = (f & 0xff) | ((~r_[i.rB]) & 0xff00);
            bool mid_carry = carry_table_not_xor_[(r >> 8) & 0xf][(s >> 8) & 0xf][pg_carry ? 1 : 0];
            carry_out = carry_table_not_xor_[(r >> 12) & 0xf][(s >> 12) & 0xf][mid_carry ? 1 : 0];
            overflow = overflow_not_xor_[(r >> 12) & 0xf][(s >> 12) & 0xf][mid_carry ? 1 : 0];
            break;
        }
        default:
            break;
    }
}

void AM2901::write_destination(const Microinstruction& i, int f, bool carry_in, int c_in) {
    switch (i.AluDestination) {
        case 0:
            q_ = static_cast<uint16_t>(f);
            y = static_cast<uint16_t>(f);
            break;

        case 1:
            y = static_cast<uint16_t>(f);
            break;

        case 2:
            y = r_[i.rA];
            r_[i.rB] = static_cast<uint16_t>(f);
            break;

        case 3:
            r_[i.rB] = static_cast<uint16_t>(f);
            y = static_cast<uint16_t>(f);
            break;

        case 4:
            y = static_cast<uint16_t>(f);
            if (i.Cycle) {
                // double-word right shift
                q_ = static_cast<uint16_t>((q_ >> 1) | ((~f & 0x1) << 15));
                f = static_cast<uint16_t>((f >> 1) | (carry_in ? 0x8000 : 0x0));
            } else {
                // double-word arithmetic right shift
                q_ = static_cast<uint16_t>((q_ >> 1) | ((~f & 0x1) << 15));
                f = static_cast<uint16_t>((f >> 1) | (carry_out ? 0x8000 : 0x0));
            }
            r_[i.rB] = static_cast<uint16_t>(f);
            break;

        case 5:
            y = static_cast<uint16_t>(f);
            if (i.Cycle) {
                // single-word right rotate
                f = static_cast<uint16_t>((f >> 1) | ((f & 0x1) << 15));
            } else {
                // single-word right shift w/carryIn to MSB
                f = static_cast<uint16_t>((f >> 1) | (carry_in ? 0x8000 : 0x0));
            }
            r_[i.rB] = static_cast<uint16_t>(f);
            break;

        case 6:
            y = static_cast<uint16_t>(f);
            // double-word left shift (identical for cycle and shift)
            f = static_cast<uint16_t>((f << 1) | ((q_ & 0x8000) >> 15));
            q_ = static_cast<uint16_t>((q_ << 1) | (1 - c_in));
            r_[i.rB] = static_cast<uint16_t>(f);
            break;

        case 7:
            y = static_cast<uint16_t>(f);
            if (i.Cycle) {
                // single-word left rotate
                f = static_cast<uint16_t>((f << 1) | ((f & 0x8000) >> 15));
            } else {
                // single-word left shift w/carryIn to LSB
                f = static_cast<uint16_t>((f << 1) | c_in);
            }
            r_[i.rB] = static_cast<uint16_t>(f);
            break;
    }
}

uint16_t AM2901::execute(const Microinstruction& i, uint16_t d, bool carry_in, bool load_mar) {
    int r, s;
    select_sources(i, d, r, s);

    int f;
    int c_in = carry_in ? 1 : 0;

    switch (i.aF) {
        case AluFunction::RplusS: {
            f = r + s + c_in;
            carry_out = (f > 0xffff);
            nib_carry = (r & 0xf) + (s & 0xf) + c_in > 0xf;
            pg_carry = (r & 0xff) + (s & 0xff) + c_in > 0xff;
            int cn = (r & 0xfff) + (s & 0xfff) + c_in > 0xfff ? 1 : 0;
            overflow = overflow_table_[r >> 12][s >> 12][cn];
            break;
        }

        case AluFunction::SminusR: {
            f = s + (~r & 0xffff) + c_in;
            carry_out = (f > 0xffff);
            nib_carry = ((~r & 0xf) + (s & 0xf) + c_in > 0xf);
            pg_carry = ((~r & 0xff) + (s & 0xff) + c_in > 0xff);
            int cn = (~r & 0xfff) + (s & 0xfff) + c_in > 0xfff ? 1 : 0;
            overflow = overflow_table_[(~r & 0xffff) >> 12][s >> 12][cn];
            break;
        }

        case AluFunction::RminusS: {
            f = r + (~s & 0xffff) + c_in;
            carry_out = (f > 0xffff);
            nib_carry = ((r & 0xf) + (~s & 0xf) + c_in > 0xf);
            pg_carry = ((r & 0xff) + (~s & 0xff) + c_in > 0xff);
            int cn = (r & 0xfff) + (~s & 0xfff) + c_in > 0xfff ? 1 : 0;
            overflow = overflow_table_[r >> 12][(~s & 0xffff) >> 12][cn];
            break;
        }

        case AluFunction::RorS:
            f = r | s;
            nib_carry = carry_table_or_[r & 0xf][s & 0xf][c_in];
            pg_carry = carry_table_or_[(r >> 4) & 0xf][(s >> 4) & 0xf][nib_carry ? 1 : 0];
            carry_out = false;
            overflow = false;
            break;

        case AluFunction::RandS:
            f = r & s;
            nib_carry = false;
            pg_carry = false;
            carry_out = false;
            overflow = false;
            break;

        case AluFunction::notRandS:
            f = (~r) & s;
            nib_carry = false;
            pg_carry = false;
            carry_out = false;
            overflow = false;
            break;

        case AluFunction::RxorS:
            f = r ^ s;
            nib_carry = false;
            pg_carry = false;
            carry_out = false;
            overflow = false;
            break;

        case AluFunction::notRxorS:
            f = (~r) ^ s;
            nib_carry = false;
            pg_carry = false;
            carry_out = false;
            overflow = false;
            break;

        default:
            f = 0;
            break;
    }

    // Clip F to 16 bits
    f = f & 0xffff;

    if (load_mar) {
        apply_load_mar(i, f, r, s);
    }

    zero = (f == 0);
    neg = ((f & 0x8000) != 0);

    write_destination(i, f, carry_in, c_in);

    return y;
}

uint16_t AM2901::execute_accurate(const Microinstruction& i, uint16_t d, bool carry_in, bool load_mar) {
    int r, s;
    select_sources(i, d, r, s);

    int f;
    int c_in = carry_in ? 1 : 0;

    switch (i.aF) {
        case AluFunction::RplusS: {
            f = r + s + c_in;
            nib_carry = carry_table_arithmetic_[r & 0xf][s & 0xf][c_in];
            pg_carry = carry_table_arithmetic_[(r >> 4) & 0xf][(s >> 4) & 0xf][nib_carry ? 1 : 0];
            bool mid_carry = carry_table_arithmetic_[(r >> 8) & 0xf][(s >> 8) & 0xf][pg_carry ? 1 : 0];
            carry_out = carry_table_arithmetic_[(r >> 12) & 0xf][(s >> 12) & 0xf][mid_carry ? 1 : 0];
            overflow = overflow_table_[r >> 12][s >> 12][mid_carry ? 1 : 0];
            break;
        }

        case AluFunction::SminusR: {
            f = s + (~r & 0xffff) + c_in;
            nib_carry = carry_table_arithmetic_[~r & 0xf][s & 0xf][c_in];
            pg_carry = carry_table_arithmetic_[(~r >> 4) & 0xf][(s >> 4) & 0xf][nib_carry ? 1 : 0];
            bool mid_carry = carry_table_arithmetic_[(~r >> 8) & 0xf][(s >> 8) & 0xf][pg_carry ? 1 : 0];
            carry_out = carry_table_arithmetic_[(~r >> 12) & 0xf][(s >> 12) & 0xf][mid_carry ? 1 : 0];
            overflow = overflow_table_[(~r & 0xffff) >> 12][s >> 12][mid_carry ? 1 : 0];
            break;
        }

        case AluFunction::RminusS: {
            f = r + (~s & 0xffff) + c_in;
            nib_carry = carry_table_arithmetic_[r & 0xf][~s & 0xf][c_in];
            pg_carry = carry_table_arithmetic_[(r >> 4) & 0xf][(~s >> 4) & 0xf][nib_carry ? 1 : 0];
            bool mid_carry = carry_table_arithmetic_[(r >> 8) & 0xf][(~s >> 8) & 0xf][pg_carry ? 1 : 0];
            carry_out = carry_table_arithmetic_[(r >> 12) & 0xf][(~s >> 12) & 0xf][mid_carry ? 1 : 0];
            overflow = overflow_table_[r >> 12][(~s & 0xffff) >> 12][mid_carry ? 1 : 0];
            break;
        }

        case AluFunction::RorS: {
            f = r | s;
            nib_carry = carry_table_or_[r & 0xf][s & 0xf][c_in];
            pg_carry = carry_table_or_[(r >> 4) & 0xf][(s >> 4) & 0xf][nib_carry ? 1 : 0];
            bool mid_carry = carry_table_or_[(r >> 8) & 0xf][(s >> 8) & 0xf][pg_carry ? 1 : 0];
            overflow = carry_out = carry_table_or_[(r >> 12) & 0xf][(s >> 12) & 0xf][mid_carry ? 1 : 0];
            break;
        }

        case AluFunction::RandS: {
            f = r & s;
            nib_carry = carry_table_and_[r & 0xf][s & 0xf][c_in];
            pg_carry = carry_table_and_[(r >> 4) & 0xf][(s >> 4) & 0xf][nib_carry ? 1 : 0];
            bool mid_carry = carry_table_and_[(r >> 8) & 0xf][(s >> 8) & 0xf][pg_carry ? 1 : 0];
            overflow = carry_out = carry_table_and_[(r >> 12) & 0xf][(s >> 12) & 0xf][mid_carry ? 1 : 0];
            break;
        }

        case AluFunction::notRandS: {
            f = (~r) & s;
            nib_carry = carry_table_and_[~r & 0xf][s & 0xf][c_in];
            pg_carry = carry_table_and_[(~r >> 4) & 0xf][(s >> 4) & 0xf][nib_carry ? 1 : 0];
            bool mid_carry = carry_table_and_[(~r >> 8) & 0xf][(s >> 8) & 0xf][pg_carry ? 1 : 0];
            overflow = carry_out = carry_table_and_[(~r >> 12) & 0xf][(s >> 12) & 0xf][mid_carry ? 1 : 0];
            break;
        }

        case AluFunction::RxorS: {
            f = r ^ s;
            nib_carry = carry_table_not_xor_[~r & 0xf][s & 0xf][c_in];
            pg_carry = carry_table_not_xor_[(~r >> 4) & 0xf][(s >> 4) & 0xf][nib_carry ? 1 : 0];
            bool mid_carry = carry_table_not_xor_[(~r >> 8) & 0xf][(s >> 8) & 0xf][pg_carry ? 1 : 0];
            carry_out = carry_table_not_xor_[(~r >> 12) & 0xf][(s >> 12) & 0xf][mid_carry ? 1 : 0];
            overflow = overflow_not_xor_[(~r >> 12) & 0xf][(s >> 12) & 0xf][mid_carry ? 1 : 0];
            break;
        }

        case AluFunction::notRxorS: {
            f = (~r) ^ s;
            nib_carry = carry_table_not_xor_[r & 0xf][s & 0xf][c_in];
            pg_carry = carry_table_not_xor_[(r >> 4) & 0xf][(s >> 4) & 0xf][nib_carry ? 1 : 0];
            bool mid_carry = carry_table_not_xor_[(r >> 8) & 0xf][(s >> 8) & 0xf][pg_carry ? 1 : 0];
            carry_out = carry_table_not_xor_[(r >> 12) & 0xf][(s >> 12) & 0xf][mid_carry ? 1 : 0];
            overflow = overflow_not_xor_[(r >> 12) & 0xf][(s >> 12) & 0xf][mid_carry ? 1 : 0];
            break;
        }

        default:
            f = 0;
            break;
    }

    // Clip F to 16 bits
    f = f & 0xffff;

    if (load_mar) {
        apply_load_mar(i, f, r, s);
    }

    zero = (f == 0);
    neg = ((f & 0x8000) != 0);

    write_destination(i, f, carry_in, c_in);

    return y;
}

// Table building functions

bool AM2901::calc_overflow(int r, int s, int c_in) {
    int p0 = (r | s) & 0x1;
    int p1 = ((r | s) & 0x2) >> 1;
    int p2 = ((r | s) & 0x4) >> 2;
    int p3 = ((r | s) & 0x8) >> 3;

    int g0 = (r & s & 0x1);
    int g1 = (r & s & 0x2) >> 1;
    int g2 = (r & s & 0x4) >> 2;
    int g3 = (r & s & 0x8) >> 3;

    int c4 = g3 | (p3 & g2) | (p3 & p2 & g1) | (p3 & p2 & p1 & g0) | (p3 & p2 & p1 & p0 & c_in);
    int c3 = g2 | (p2 & g1) | (p2 & p1 & g0) | (p2 & p1 & p0 & c_in);

    return (c3 ^ c4) != 0;
}

bool AM2901::calc_carry_arithmetic(int r, int s, int c_in) {
    int p0 = (r | s) & 0x1;
    int p1 = ((r | s) & 0x2) >> 1;
    int p2 = ((r | s) & 0x4) >> 2;
    int p3 = ((r | s) & 0x8) >> 3;

    int g0 = (r & s & 0x1);
    int g1 = (r & s & 0x2) >> 1;
    int g2 = (r & s & 0x4) >> 2;
    int g3 = (r & s & 0x8) >> 3;

    int c4 = g3 | (p3 & g2) | (p3 & p2 & g1) | (p3 & p2 & p1 & g0) | (p3 & p2 & p1 & p0 & c_in);

    return c4 != 0;
}

bool AM2901::calc_carry_or(int r, int s, int c_in) {
    int p0 = (r | s) & 0x1;
    int p1 = ((r | s) & 0x2) >> 1;
    int p2 = ((r | s) & 0x4) >> 2;
    int p3 = ((r | s) & 0x8) >> 3;

    int c4 = (~(p3 & p2 & p1 & p0) & 0x1) | c_in;

    return c4 != 0;
}

bool AM2901::calc_carry_and(int r, int s, int c_in) {
    int g0 = (r & s & 0x1);
    int g1 = (r & s & 0x2) >> 1;
    int g2 = (r & s & 0x4) >> 2;
    int g3 = (r & s & 0x8) >> 3;

    int c4 = g3 | g2 | g1 | g0 | c_in;

    return c4 != 0;
}

bool AM2901::calc_carry_not_xor(int r, int s, int c_in) {
    int p0 = (r | s) & 0x1;
    int p1 = ((r | s) & 0x2) >> 1;
    int p2 = ((r | s) & 0x4) >> 2;
    int p3 = ((r | s) & 0x8) >> 3;

    int g0 = (r & s & 0x1);
    int g1 = (r & s & 0x2) >> 1;
    int g2 = (r & s & 0x4) >> 2;
    int g3 = (r & s & 0x8) >> 3;

    int c4 = ~(g3 | (p3 & g2) | (p3 & p2 & g1) | (p3 & p2 & p1 & p0 & (g0 | ~c_in))) & 0x1;

    return c4 != 0;
}

bool AM2901::calc_overflow_not_xor(int r, int s, int c_in) {
    int p0 = (r | s) & 0x1;
    int p1 = ((r | s) & 0x2) >> 1;
    int p2 = ((r | s) & 0x4) >> 2;
    int p3 = ((r | s) & 0x8) >> 3;

    int g0 = (r & s & 0x1);
    int g1 = (r & s & 0x2) >> 1;
    int g2 = (r & s & 0x4) >> 2;
    int g3 = (r & s & 0x8) >> 3;

    int ovr = ((~p2 | (~g2 & ~p1) | (~g2 & ~g1 & ~p0) | (~g2 & ~g1 & ~g0 & c_in)) ^
        (~p3 | (~g3 & ~p2) | (~g3 & ~g2 & ~p1) | (~g3 & ~g2 & ~g1 & ~p0) | (~g3 & ~g2 & ~g1 & ~g0 & c_in))) & 0x1;

    return ovr != 0;
}

void AM2901::build_tables() {
    for (int r = 0; r < 16; r++) {
        for (int s = 0; s < 16; s++) {
            for (int c = 0; c < 2; c++) {
                overflow_table_[r][s][c] = calc_overflow(r, s, c);
                carry_table_arithmetic_[r][s][c] = calc_carry_arithmetic(r, s, c);
                carry_table_or_[r][s][c] = calc_carry_or(r, s, c);
                carry_table_and_[r][s][c] = calc_carry_and(r, s, c);
                carry_table_not_xor_[r][s][c] = calc_carry_not_xor(r, s, c);
                overflow_not_xor_[r][s][c] = calc_overflow_not_xor(r, s, c);
            }
        }
    }
}

} // namespace darkstar
