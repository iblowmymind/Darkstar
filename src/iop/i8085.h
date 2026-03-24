/*
    BSD 2-Clause License

    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this
      list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once
#include "i8085_memory_bus.h"
#include "i8085_io_bus.h"
#include <cstdint>
#include <functional>
#include <string>

enum class InterruptType {
    RST7_5,
    RST6_5,
    RST5_5,
    TRAP,
    INTR,
};

class i8085 {
public:
    i8085(I8085MemoryBus* mem, I8085IOBus* io);
    void Reset();
    void RaiseExternalInterrupt(InterruptType type);
    void ClearExternalInterrupt(InterruptType type);
    int  Execute();
    std::string Disassemble(uint16_t address);

    bool    Halted() const { return _halted; }
    uint8_t A() const;
    uint8_t F() const;
    uint8_t B() const;
    uint8_t C() const;
    uint8_t D() const;
    uint8_t E() const;
    uint8_t H() const;
    uint8_t L() const;
    uint16_t PC() const { return _pc; }
    uint16_t SP() const { return _sp; }
    uint16_t GetAF() const;
    uint16_t GetBC() const;
    uint16_t GetDE() const;
    uint16_t GetHL() const;

private:
    // Register file using union for 16-bit pairs
    // Little-endian layout (matches C# [StructLayout(LayoutKind.Explicit)])
    // AF: F is low byte, A is high byte
    // BC: C is low byte, B is high byte  
    // DE: E is low byte, D is high byte
    // HL: L is low byte, H is high byte
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

        bool F_CY() const { return (F & 0x01) != 0; }
        bool F_P()  const { return (F & 0x04) != 0; }
        bool F_AC() const { return (F & 0x10) != 0; }
        bool F_Z()  const { return (F & 0x40) != 0; }
        bool F_S()  const { return (F & 0x80) != 0; }
        void set_F_CY(bool v) { F = v ? (F | 0x01) : (F & 0xfe); }
        void set_F_P (bool v) { F = v ? (F | 0x04) : (F & 0xfb); }
        void set_F_AC(bool v) { F = v ? (F | 0x10) : (F & 0xef); }
        void set_F_Z (bool v) { F = v ? (F | 0x40) : (F & 0xbf); }
        void set_F_S (bool v) { F = v ? (F | 0x80) : (F & 0x7f); }
    };

    // Instruction dispatch table
    struct InstructionData {
        uint8_t     opcode;
        const char* mnemonic;
        uint16_t    size;
        int         cycles1;
        int         cycles2;
        std::function<bool(i8085*, uint8_t, uint16_t)> executor;
    };

    // All 256 instruction implementations
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
    bool INX(uint8_t op, uint16_t arg);
    bool Invalid(uint8_t op, uint16_t arg);
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

    void Push(uint16_t v);
    uint16_t Pop();
    void Restore(uint16_t addr);
    void InitializeInstructionTables();
    void InitializeParityTable();

    uint16_t _pc{0};
    uint16_t _sp{0};
    RegisterFile _r{};
    uint8_t  _interruptMask{0};
    bool     _halted{false};
    bool     _parityTable[256]{};
    InstructionData _instructionData[256]{};

    I8085MemoryBus* _mem;
    I8085IOBus*     _io;

    // Interrupt mask bits (matches C# constants)
    static constexpr int I5_5 = 0x01;
    static constexpr int I6_5 = 0x02;
    static constexpr int I7_5 = 0x04;
    static constexpr int IE   = 0x08;
    static constexpr int P5_5 = 0x10;
    static constexpr int P6_5 = 0x20;
    static constexpr int P7_5 = 0x40;
    static constexpr int SID  = 0x80;
};