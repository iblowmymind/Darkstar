/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>
#include <string>

namespace darkstar {

enum class MacroType {
    Mesa,
    Lisp,
};

enum class MacroOperand {
    None,
    Byte,
    SignedByte,
    Pair,
    TwoByte,
    Word,
    ThreeByte,
};

/// Provides facilities for interpreting Mesa/Lisp bytecodes
struct MacroInstruction {
    std::string mnemonic;
    MacroOperand operand;

    MacroInstruction() : mnemonic("INVALID"), operand(MacroOperand::None) {}
    MacroInstruction(uint8_t opcode, const char* mnem, MacroOperand op)
        : mnemonic(mnem), operand(op) {}

    static const MacroInstruction& get_instruction(MacroType type, uint8_t opcode);

private:
    static const MacroInstruction mesa_instruction_table[256];
    static const MacroInstruction lisp_instruction_table[256];
    static const MacroInstruction invalid;
};

} // namespace darkstar
