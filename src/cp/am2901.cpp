/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
*/
#include "am2901.h"

AM2901::AM2901() {
    Reset();
}

void AM2901::Reset() {
    for (auto& r : _r) r = 0;
    _q = 0;
    _f = 0;
    _oe = 0;
    _carryOut = false;
    _overflow = false;
    _zero = false;
    _sign = false;
}

uint16_t AM2901::Execute(int src, int func, int dest, int rAddr, int sAddr, uint16_t d, bool cin) {
    // Source operand selection (I2..I0 in AM2901 nomenclature)
    uint16_t A = _r[rAddr & 0xF];
    uint16_t B = _r[sAddr & 0xF];

    uint16_t R_op, S_op;
    switch (src & 0x7) {
        case 0: R_op = A;    S_op = _q; break;  // AQ
        case 1: R_op = A;    S_op = B;  break;  // AB
        case 2: R_op = 0;    S_op = _q; break;  // ZQ
        case 3: R_op = 0;    S_op = B;  break;  // ZB
        case 4: R_op = 0;    S_op = A;  break;  // ZA
        case 5: R_op = d;    S_op = A;  break;  // DA
        case 6: R_op = d;    S_op = _q; break;  // DQ
        case 7: R_op = d;    S_op = 0;  break;  // DZ
        default: R_op = 0;   S_op = 0;  break;
    }

    // ALU function (I5..I3)
    uint32_t result = 0;
    switch (func & 0x7) {
        case 0: result = (uint32_t)R_op + (uint32_t)S_op + (cin ? 1u : 0u); break;  // ADD
        case 1: result = (uint32_t)S_op + (~(uint32_t)R_op & 0xFFFF) + (cin ? 1u : 0u); break; // SUBR
        case 2: result = (uint32_t)R_op + (~(uint32_t)S_op & 0xFFFF) + (cin ? 1u : 0u); break; // SUBS
        case 3: result = (uint32_t)R_op | (uint32_t)S_op; break;   // OR
        case 4: result = (uint32_t)R_op & (uint32_t)S_op; break;   // AND
        case 5: result = ~((uint32_t)R_op) & (uint32_t)S_op; break;// NOTRS
        case 6: result = (uint32_t)R_op ^ (uint32_t)S_op; break;   // EXOR
        case 7: result = ~((uint32_t)R_op ^ (uint32_t)S_op); break;// EXNOR
        default: break;
    }

    _f = (uint16_t)(result & 0xFFFF);
    _carryOut = (result >> 16) != 0;
    _zero = (_f == 0);
    _sign = (_f & 0x8000) != 0;
    _overflow = (((R_op ^ result) & (S_op ^ result) & 0x8000) != 0) ||
                (func == 1 && (R_op & 0x8000) != (result & 0x8000)); // simplified

    // Destination selection (I8..I6)
    _oe = _f;
    switch (dest & 0x7) {
        case 0: // QREG: F→Q
            _q = _f;
            break;
        case 1: // NOP
            break;
        case 2: // RAMA: F→RAM[A], F/2→Q (right shift)
            _r[rAddr & 0xF] = _f;
            _q = (uint16_t)((_q >> 1) | ((_q & 1) << 15)); // rotate Q
            break;
        case 3: // RAMF: F→RAM[B]
            _r[sAddr & 0xF] = _f;
            break;
        case 4: // RAMQD: right-shift RAM[B] and Q (with MSB from sign)
            _r[sAddr & 0xF] = (uint16_t)((_f >> 1) | (_sign ? 0x8000u : 0u));
            _q = (uint16_t)((_q >> 1) | ((_f & 1) << 15));
            _oe = _r[sAddr & 0xF];
            break;
        case 5: // RAMD: right-shift RAM[B]
            _r[sAddr & 0xF] = (uint16_t)((_f >> 1) | (_sign ? 0x8000u : 0u));
            _oe = _r[sAddr & 0xF];
            break;
        case 6: // RAMQU: left-shift RAM[B] and Q
            _r[sAddr & 0xF] = (uint16_t)((_f << 1) | (_carryOut ? 1u : 0u));
            _q = (uint16_t)((_q << 1) | ((_f >> 15) & 1));
            _oe = _r[sAddr & 0xF];
            break;
        case 7: // RAMU: left-shift RAM[B]
            _r[sAddr & 0xF] = (uint16_t)((_f << 1) | (_carryOut ? 1u : 0u));
            _oe = _r[sAddr & 0xF];
            break;
        default:
            break;
    }

    return _oe;
}
