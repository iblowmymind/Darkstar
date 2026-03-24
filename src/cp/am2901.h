/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
*/
#pragma once
#include <cstdint>

// AMD AM2901 4-bit slice ALU (used as the 16-bit ALU in the Star CP).
// Ported from D/CP/AM2901.cs
class AM2901 {
public:
    AM2901();

    void Reset();

    // Execute one ALU operation.
    // src     : source select  (3 bits)
    // func    : ALU function   (3 bits)
    // dest    : destination    (3 bits)
    // rAddr   : R register address (4 bits)
    // sAddr   : S register address (4 bits; used as Q/RAM address)
    // d       : direct data input  (16 bits)
    // cin     : carry in
    // Returns the ALU output F (16-bit)
    uint16_t Execute(int src, int func, int dest, int rAddr, int sAddr, uint16_t d, bool cin);

    // Flag accessors
    bool CarryOut()  const { return _carryOut; }
    bool Overflow()  const { return _overflow; }
    bool Zero()      const { return _zero; }
    bool Sign()      const { return _sign; }

    // Register file (16 x 16-bit)
    uint16_t R(int i) const { return _r[i & 0xF]; }

    // Q register
    uint16_t Q() const { return _q; }

    // The output F stored for use in NIA / branch decisions
    uint16_t F() const { return _f; }

    // Shifter output (OE) – used by some CP microinstructions
    uint16_t OE() const { return _oe; }

private:
    uint16_t _r[16]{};
    uint16_t _q{};
    uint16_t _f{};
    uint16_t _oe{};

    bool _carryOut{false};
    bool _overflow{false};
    bool _zero{false};
    bool _sign{false};
};
