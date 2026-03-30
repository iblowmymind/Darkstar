/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace darkstar {

class I8085MemoryBus;
class I8085IOBus;

enum class InterruptType {
    RST7_5,
    RST6_5,
    RST5_5,
    TRAP,
    INTR
};

// RegisterFile uses a union for register pair access.
// On little-endian systems (x86/ARM), the low byte of a 16-bit value
// is at the lower address.  For the 8085:
//   AF: F is low byte, A is high byte
//   BC: C is low byte, B is high byte
//   DE: E is low byte, D is high byte
//   HL: L is low byte, H is high byte
struct RegisterFile {
    union {
        uint16_t AF;
        struct { uint8_t F; uint8_t A; };
    };
    union {
        uint16_t BC;
        struct { uint8_t C; uint8_t B; };
    };
    union {
        uint16_t DE;
        struct { uint8_t E; uint8_t D; };
    };
    union {
        uint16_t HL;
        struct { uint8_t L; uint8_t H; };
    };

    // Flag accessors (bits of F register)
    bool f_cy() const  { return (F & 0x01) != 0; }
    void set_f_cy(bool v) { F = v ? (F | 0x01) : (F & 0xFE); }

    bool f_p() const   { return (F & 0x04) != 0; }
    void set_f_p(bool v)  { F = v ? (F | 0x04) : (F & 0xFB); }

    bool f_ac() const  { return (F & 0x10) != 0; }
    void set_f_ac(bool v) { F = v ? (F | 0x10) : (F & 0xEF); }

    bool f_z() const   { return (F & 0x40) != 0; }
    void set_f_z(bool v)  { F = v ? (F | 0x40) : (F & 0xBF); }

    bool f_s() const   { return (F & 0x80) != 0; }
    void set_f_s(bool v)  { F = v ? (F | 0x80) : (F & 0x7F); }
};

class i8085 {
public:
    i8085(I8085MemoryBus& mem, I8085IOBus& io);

    void reset();
    int execute();

    void raise_external_interrupt(InterruptType type);
    void clear_external_interrupt(InterruptType type);

    std::string disassemble(uint16_t address);

    // Register accessors
    bool halted() const { return halted_; }
    uint8_t a() const { return r_.A; }
    uint8_t f() const { return r_.F; }
    uint8_t b() const { return r_.B; }
    uint8_t c() const { return r_.C; }
    uint8_t d() const { return r_.D; }
    uint8_t e() const { return r_.E; }
    uint8_t h() const { return r_.H; }
    uint8_t l() const { return r_.L; }
    uint16_t pc() const { return pc_; }
    uint16_t sp() const { return sp_; }
    uint16_t af() const { return r_.AF; }
    uint16_t bc() const { return r_.BC; }
    uint16_t de() const { return r_.DE; }
    uint16_t hl() const { return r_.HL; }

private:
    using Executor = std::function<bool(uint8_t op, uint16_t arg)>;

    struct InstructionData {
        uint8_t opcode;
        const char* mnemonic;
        uint16_t size;
        int cycles1;
        int cycles2;
        Executor executor;

        InstructionData() : opcode(0), mnemonic(""), size(1), cycles1(4), cycles2(4) {}
        InstructionData(uint8_t op, const char* mn, uint16_t sz, int c1, int c2, Executor ex)
            : opcode(op), mnemonic(mn), size(sz), cycles1(c1), cycles2(c2), executor(std::move(ex)) {}
        InstructionData(uint8_t op, const char* mn, uint16_t sz, int c, Executor ex)
            : opcode(op), mnemonic(mn), size(sz), cycles1(c), cycles2(c), executor(std::move(ex)) {}
    };

    void initialize_instruction_tables();
    void initialize_parity_table();

    // Instruction implementations
    bool ADD(uint8_t op, uint16_t arg);
    bool ADC(uint8_t op, uint16_t arg);
    bool ACI(uint8_t op, uint16_t arg);
    bool ADI(uint8_t op, uint16_t arg);
    bool ANA(uint8_t op, uint16_t arg);
    bool ANI(uint8_t op, uint16_t arg);
    bool CALL(uint8_t op, uint16_t arg);
    bool CALLC(uint8_t op, uint16_t arg);
    bool CMA(uint8_t op, uint16_t arg);
    bool CMC(uint8_t op, uint16_t arg);
    bool CMP(uint8_t op, uint16_t arg);
    bool CPI(uint8_t op, uint16_t arg);
    bool DAA(uint8_t op, uint16_t arg);
    bool DAD(uint8_t op, uint16_t arg);
    bool DCR(uint8_t op, uint16_t arg);
    bool DCX(uint8_t op, uint16_t arg);
    bool DI(uint8_t op, uint16_t arg);
    bool EI(uint8_t op, uint16_t arg);
    bool HLT(uint8_t op, uint16_t arg);
    bool IN(uint8_t op, uint16_t arg);
    bool INR(uint8_t op, uint16_t arg);
    bool Invalid(uint8_t op, uint16_t arg);
    bool INX(uint8_t op, uint16_t arg);
    bool JMP(uint8_t op, uint16_t arg);
    bool LDA(uint8_t op, uint16_t arg);
    bool LDAX(uint8_t op, uint16_t arg);
    bool LHLD(uint8_t op, uint16_t arg);
    bool LXI(uint8_t op, uint16_t arg);
    bool MOV(uint8_t op, uint16_t arg);
    bool MVI(uint8_t op, uint16_t arg);
    bool NOP(uint8_t op, uint16_t arg);
    bool OUT(uint8_t op, uint16_t arg);
    bool ORA(uint8_t op, uint16_t arg);
    bool ORI(uint8_t op, uint16_t arg);
    bool PCHL(uint8_t op, uint16_t arg);
    bool POP(uint8_t op, uint16_t arg);
    bool PUSH(uint8_t op, uint16_t arg);
    bool RAL(uint8_t op, uint16_t arg);
    bool RAR(uint8_t op, uint16_t arg);
    bool RET(uint8_t op, uint16_t arg);
    bool RETC(uint8_t op, uint16_t arg);
    bool RIM(uint8_t op, uint16_t arg);
    bool RLC(uint8_t op, uint16_t arg);
    bool RRC(uint8_t op, uint16_t arg);
    bool RST(uint8_t op, uint16_t arg);
    bool SBB(uint8_t op, uint16_t arg);
    bool SBI(uint8_t op, uint16_t arg);
    bool SHLD(uint8_t op, uint16_t arg);
    bool SIM(uint8_t op, uint16_t arg);
    bool SPHL(uint8_t op, uint16_t arg);
    bool STA(uint8_t op, uint16_t arg);
    bool STAX(uint8_t op, uint16_t arg);
    bool STC(uint8_t op, uint16_t arg);
    bool SUB(uint8_t op, uint16_t arg);
    bool SUI(uint8_t op, uint16_t arg);
    bool XCHG(uint8_t op, uint16_t arg);
    bool XRA(uint8_t op, uint16_t arg);
    bool XRI(uint8_t op, uint16_t arg);
    bool XTHL(uint8_t op, uint16_t arg);

    // Helper routines
    void push_word(uint16_t v);
    uint16_t pop_word();
    void restore(uint16_t addr);

    // Processor registers
    uint16_t pc_ = 0;
    uint16_t sp_ = 0;
    RegisterFile r_;

    // Memory and I/O bus
    I8085MemoryBus& mem_;
    I8085IOBus& io_;

    // Interrupt mask and control bits
    uint8_t interrupt_mask_ = 0;

    // Interrupt mask bit constants
    static constexpr int kI5_5 = 0x01;
    static constexpr int kI6_5 = 0x02;
    static constexpr int kI7_5 = 0x04;
    static constexpr int kIE   = 0x08;
    static constexpr int kP5_5 = 0x10;
    static constexpr int kP6_5 = 0x20;
    static constexpr int kP7_5 = 0x40;
    static constexpr int kSID  = 0x80;

    bool halted_ = false;

    bool parity_table_[256];
    InstructionData instruction_data_[256];
};

} // namespace darkstar
