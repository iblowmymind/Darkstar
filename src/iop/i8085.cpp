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
#include "iop/i8085.h"
#include <stdexcept>
#include <cstdio>
i8085::i8085(I8085MemoryBus* mem, I8085IOBus* io)
    : _mem(mem), _io(io)
{
    InitializeParityTable();
    InitializeInstructionTables();
    Reset();
}
void i8085::Reset()
{
    _pc = 0;
    _sp = 0;
    _r.AF = 0;
    _r.BC = 0;
    _r.DE = 0;
    _r.HL = 0;
    _interruptMask = 0;
    _halted = false;
}
void i8085::RaiseExternalInterrupt(InterruptType type)
{
    switch (type) {
    case InterruptType::RST7_5:
        _interruptMask |= static_cast<uint8_t>(P7_5);
        break;
    case InterruptType::RST6_5:
        _interruptMask |= static_cast<uint8_t>(P6_5);
        break;
    case InterruptType::RST5_5:
        _interruptMask |= static_cast<uint8_t>(P5_5);
        break;
    default:
        break;
    }
}
void i8085::ClearExternalInterrupt(InterruptType type)
{
    switch (type) {
    case InterruptType::RST6_5:
        _interruptMask &= static_cast<uint8_t>(~P6_5);
        break;
    case InterruptType::RST5_5:
        _interruptMask &= static_cast<uint8_t>(~P5_5);
        break;
    default:
        break;
    }
}
int i8085::Execute()
{
    if (_halted)
        return 4;
    // Handle pending interrupts if globally enabled
    if ((_interruptMask & IE) != 0) {
        if ((_interruptMask & P7_5) != 0 && (_interruptMask & I7_5) == 0) {
            Restore(0x3c);
            _interruptMask &= static_cast<uint8_t>(~IE);
            _interruptMask &= static_cast<uint8_t>(~P7_5);
        } else if ((_interruptMask & P6_5) != 0 && (_interruptMask & I6_5) == 0) {
            Restore(0x34);
            _interruptMask &= static_cast<uint8_t>(~IE);
        } else if ((_interruptMask & P5_5) != 0 && (_interruptMask & I5_5) == 0) {
            Restore(0x2c);
            _interruptMask &= static_cast<uint8_t>(~IE);
        }
    }
    const InstructionData& id = _instructionData[_mem->ReadByte(_pc++)];
    uint16_t arg = 0;
    if (id.size == 2) {
        arg = _mem->ReadByte(_pc);
        _pc++;
    } else if (id.size == 3) {
        arg = _mem->ReadWord(_pc);
        _pc += 2;
    }
    bool alt = id.executor(this, id.opcode, arg);
    return alt ? id.cycles2 : id.cycles1;
}
std::string i8085::Disassemble(uint16_t address)
{
    const InstructionData& id = _instructionData[_mem->ReadByte(address++)];
    char buf[64];
    if (id.size == 1) {
        return id.mnemonic;
    } else if (id.size == 2) {
        snprintf(buf, sizeof(buf), id.mnemonic, _mem->ReadByte(address));
    } else {
        snprintf(buf, sizeof(buf), id.mnemonic, _mem->ReadWord(address));
    }
    return buf;
}
uint8_t  i8085::A() const  { return _r.A; }
uint8_t  i8085::F() const  { return _r.F; }
uint8_t  i8085::B() const  { return _r.B; }
uint8_t  i8085::C() const  { return _r.C; }
uint8_t  i8085::D() const  { return _r.D; }
uint8_t  i8085::E() const  { return _r.E; }
uint8_t  i8085::H() const  { return _r.H; }
uint8_t  i8085::L() const  { return _r.L; }
uint16_t i8085::GetAF() const { return _r.AF; }
uint16_t i8085::GetBC() const { return _r.BC; }
uint16_t i8085::GetDE() const { return _r.DE; }
uint16_t i8085::GetHL() const { return _r.HL; }
// ---- Helper routines ----
void i8085::Push(uint16_t v)
{
    _sp -= 2;
    _mem->WriteWord(_sp, v);
}
uint16_t i8085::Pop()
{
    uint16_t v = _mem->ReadWord(_sp);
    _sp += 2;
    return v;
}
void i8085::Restore(uint16_t addr)
{
    Push(_pc);
    _pc = addr;
}
void i8085::InitializeParityTable()
{
    for (int i = 0; i < 256; i++) {
        int bits = 0;
        for (int j = i; j != 0; j &= j - 1)
            bits++;
        _parityTable[i] = (bits & 1) == 0;
    }
}
// ---- Instruction implementations ----
bool i8085::ADD(uint8_t op, uint16_t arg)
{
    int src;
    switch (op & 0x7) {
    case 0: src = _r.B; break;
    case 1: src = _r.C; break;
    case 2: src = _r.D; break;
    case 3: src = _r.E; break;
    case 4: src = _r.H; break;
    case 5: src = _r.L; break;
    case 6: src = _mem->ReadByte(_r.HL); break;
    default: src = _r.A; break;
    }
    int temp = _r.A + src;
    _r.set_F_CY((temp & 0x100) != 0);
    _r.set_F_AC((((_r.A & 0x0f) + (src & 0x0f)) & 0x10) != 0);
    _r.A = static_cast<uint8_t>(temp);
    _r.set_F_Z(_r.A == 0);
    _r.set_F_S((_r.A & 0x80) != 0);
    _r.set_F_P(_parityTable[_r.A]);
    return false;
}
bool i8085::ADC(uint8_t op, uint16_t arg)
{
    int src;
    switch (op & 0x7) {
    case 0: src = _r.B; break;
    case 1: src = _r.C; break;
    case 2: src = _r.D; break;
    case 3: src = _r.E; break;
    case 4: src = _r.H; break;
    case 5: src = _r.L; break;
    case 6: src = _mem->ReadByte(_r.HL); break;
    default: src = _r.A; break;
    }
    int cy = _r.F_CY() ? 1 : 0;
    _r.set_F_AC((((_r.A & 0x0f) + (src & 0x0f) + cy) & 0x10) != 0);
    int temp = _r.A + src + cy;
    _r.set_F_CY((temp & 0x100) != 0);
    _r.A = static_cast<uint8_t>(temp);
    _r.set_F_Z(_r.A == 0);
    _r.set_F_S((_r.A & 0x80) != 0);
    _r.set_F_P(_parityTable[_r.A]);
    return false;
}
bool i8085::ACI(uint8_t op, uint16_t arg)
{
    int cy = _r.F_CY() ? 1 : 0;
    _r.set_F_AC((((_r.A & 0x0f) + (arg & 0x0f) + cy) & 0x10) != 0);
    int temp = _r.A + arg + cy;
    _r.set_F_CY((temp & 0x100) != 0);
    _r.A = static_cast<uint8_t>(temp);
    _r.set_F_Z(_r.A == 0);
    _r.set_F_S((_r.A & 0x80) != 0);
    _r.set_F_P(_parityTable[_r.A]);
    return false;
}
bool i8085::ADI(uint8_t op, uint16_t arg)
{
    _r.set_F_AC((((_r.A & 0x0f) + (arg & 0x0f)) & 0x10) != 0);
    int temp = _r.A + arg;
    _r.set_F_CY((temp & 0x100) != 0);
    _r.A = static_cast<uint8_t>(temp);
    _r.set_F_Z(_r.A == 0);
    _r.set_F_S((_r.A & 0x80) != 0);
    _r.set_F_P(_parityTable[_r.A]);
    return false;
}
bool i8085::ANA(uint8_t op, uint16_t arg)
{
    int src;
    switch (op & 0x7) {
    case 0: src = _r.B; break;
    case 1: src = _r.C; break;
    case 2: src = _r.D; break;
    case 3: src = _r.E; break;
    case 4: src = _r.H; break;
    case 5: src = _r.L; break;
    case 6: src = _mem->ReadByte(_r.HL); break;
    default: src = _r.A; break;
    }
    _r.A &= static_cast<uint8_t>(src);
    _r.set_F_CY(false);
    _r.set_F_AC(false);
    _r.set_F_Z(_r.A == 0);
    _r.set_F_S((_r.A & 0x80) != 0);
    _r.set_F_P(_parityTable[_r.A]);
    return false;
}
bool i8085::ANI(uint8_t op, uint16_t arg)
{
    _r.A &= static_cast<uint8_t>(arg);
    _r.set_F_CY(false);
    _r.set_F_AC(false);
    _r.set_F_Z(_r.A == 0);
    _r.set_F_S((_r.A & 0x80) != 0);
    _r.set_F_P(_parityTable[_r.A]);
    return false;
}
bool i8085::CALL(uint8_t op, uint16_t arg)
{
    Push(_pc);
    _pc = arg;
    return false;
}
bool i8085::CALLC(uint8_t op, uint16_t arg)
{
    bool call = false;
    switch ((op & 0x38) >> 3) {
    case 0: call = !_r.F_Z(); break;   // CNZ
    case 1: call =  _r.F_Z(); break;   // CZ
    case 2: call = !_r.F_CY(); break;  // CNC
    case 3: call =  _r.F_CY(); break;  // CC
    case 4: call = !_r.F_P(); break;   // CPO
    case 5: call =  _r.F_P(); break;   // CPE
    case 6: call = !_r.F_S(); break;   // CP
    case 7: call =  _r.F_S(); break;   // CM
    }
    if (call) { Push(_pc); _pc = arg; }
    return call;
}
bool i8085::CMA(uint8_t op, uint16_t arg)
{
    _r.A = ~_r.A;
    return false;
}
bool i8085::CMC(uint8_t op, uint16_t arg)
{
    _r.set_F_CY(!_r.F_CY());
    return false;
}
bool i8085::CMP(uint8_t op, uint16_t arg)
{
    int src;
    switch (op & 0x7) {
    case 0: src = _r.B; break;
    case 1: src = _r.C; break;
    case 2: src = _r.D; break;
    case 3: src = _r.E; break;
    case 4: src = _r.H; break;
    case 5: src = _r.L; break;
    case 6: src = _mem->ReadByte(_r.HL); break;
    default: src = _r.A; break;
    }
    int temp = _r.A - src;
    _r.set_F_CY(temp < 0);
    _r.set_F_AC(static_cast<int8_t>((_r.A & 0x0f) - (src & 0x0f)) < 0);
    _r.set_F_Z(static_cast<uint8_t>(temp) == 0);
    _r.set_F_S((temp & 0x80) != 0);
    _r.set_F_P(_parityTable[static_cast<uint8_t>(temp)]);
    return false;
}
bool i8085::CPI(uint8_t op, uint16_t arg)
{
    int temp = _r.A - static_cast<uint8_t>(arg);
    _r.set_F_CY(temp < 0);
    _r.set_F_AC(static_cast<int8_t>((_r.A & 0x0f) - (arg & 0x0f)) < 0);
    _r.set_F_Z(static_cast<uint8_t>(temp) == 0);
    _r.set_F_S((temp & 0x80) != 0);
    _r.set_F_P(_parityTable[static_cast<uint8_t>(temp)]);
    return false;
}
bool i8085::DAA(uint8_t op, uint16_t arg)
{
    // Decimal adjust accumulator
    uint8_t a = _r.A;
    bool cy = _r.F_CY();
    if ((a & 0xf) > 9 || _r.F_AC()) {
        a += 6;
    }
    if (((a >> 4) & 0xf) > 9 || cy) {
        a += 0x60;
        cy = true;
    }
    _r.set_F_CY(cy);
    _r.set_F_Z(a == 0);
    _r.set_F_S((a & 0x80) != 0);
    _r.set_F_P(_parityTable[a]);
    _r.A = a;
    return false;
}
bool i8085::DAD(uint8_t op, uint16_t arg)
{
    uint32_t addend;
    switch ((op & 0x30) >> 4) {
    case 0: addend = _r.BC; break;
    case 1: addend = _r.DE; break;
    case 2: addend = _r.HL; break;
    default: addend = _sp; break;
    }
    uint32_t result = _r.HL + addend;
    _r.set_F_CY(result > 0xffff);
    _r.HL = static_cast<uint16_t>(result);
    return false;
}
bool i8085::DCR(uint8_t op, uint16_t arg)
{
    uint8_t res;
    switch ((op & 0x38) >> 3) {
    case 0: res = --_r.B; break;
    case 1: res = --_r.C; break;
    case 2: res = --_r.D; break;
    case 3: res = --_r.E; break;
    case 4: res = --_r.H; break;
    case 5: res = --_r.L; break;
    case 6: res = static_cast<uint8_t>(_mem->ReadByte(_r.HL) - 1); _mem->WriteByte(_r.HL, res); break;
    default: res = --_r.A; break;
    }
    _r.set_F_Z(res == 0);
    _r.set_F_S((res & 0x80) != 0);
    _r.set_F_P(_parityTable[res]);
    _r.set_F_AC((res & 0xf) == 0xf);
    return false;
}
bool i8085::DCX(uint8_t op, uint16_t arg)
{
    switch ((op & 0x30) >> 4) {
    case 0: _r.BC--; break;
    case 1: _r.DE--; break;
    case 2: _r.HL--; break;
    default: _sp--; break;
    }
    return false;
}
bool i8085::DI(uint8_t op, uint16_t arg)
{
    _interruptMask &= static_cast<uint8_t>(~IE);
    return false;
}
bool i8085::EI(uint8_t op, uint16_t arg)
{
    _interruptMask |= static_cast<uint8_t>(IE);
    return false;
}
bool i8085::HLT(uint8_t op, uint16_t arg)
{
    _halted = true;
    return false;
}
bool i8085::IN(uint8_t op, uint16_t arg)
{
    _r.A = _io->In(static_cast<uint8_t>(arg));
    return false;
}
bool i8085::INR(uint8_t op, uint16_t arg)
{
    uint8_t res;
    switch ((op & 0x38) >> 3) {
    case 0: res = ++_r.B; break;
    case 1: res = ++_r.C; break;
    case 2: res = ++_r.D; break;
    case 3: res = ++_r.E; break;
    case 4: res = ++_r.H; break;
    case 5: res = ++_r.L; break;
    case 6: res = static_cast<uint8_t>(_mem->ReadByte(_r.HL) + 1); _mem->WriteByte(_r.HL, res); break;
    default: res = ++_r.A; break;
    }
    _r.set_F_Z(res == 0);
    _r.set_F_S((res & 0x80) != 0);
    _r.set_F_P(_parityTable[res]);
    _r.set_F_AC((res & 0xf) == 0);
    return false;
}
bool i8085::Invalid(uint8_t op, uint16_t arg)
{
    // Log and ignore invalid opcodes
    fprintf(stderr, "Invalid 8085 opcode 0x%02x at PC=0x%04x\n", op, static_cast<unsigned>(_pc - 1));
    return false;
}
bool i8085::INX(uint8_t op, uint16_t arg)
{
    switch ((op & 0x30) >> 4) {
    case 0: _r.BC++; break;
    case 1: _r.DE++; break;
    case 2: _r.HL++; break;
    default: _sp++; break;
    }
    return false;
}
bool i8085::JMP(uint8_t op, uint16_t arg)
{
    bool test = false;
    switch (op & 0x3f) {
    case 0x02: test = !_r.F_Z(); break;   // JNZ
    case 0x03: test = true; break;          // JMP
    case 0x0a: test =  _r.F_Z(); break;   // JZ
    case 0x12: test = !_r.F_CY(); break;  // JNC
    case 0x1a: test =  _r.F_CY(); break;  // JC
    case 0x22: test = !_r.F_P(); break;   // JPO
    case 0x2a: test =  _r.F_P(); break;   // JPE
    case 0x32: test = !_r.F_S(); break;   // JP
    case 0x3a: test =  _r.F_S(); break;   // JM
    default: break;
    }
    if (test) _pc = arg;
    return test;
}
bool i8085::LDA(uint8_t op, uint16_t arg)
{
    _r.A = _mem->ReadByte(arg);
    return false;
}
bool i8085::LDAX(uint8_t op, uint16_t arg)
{
    switch ((op & 0x10) >> 4) {
    case 0: _r.A = _mem->ReadByte(_r.BC); break;
    default: _r.A = _mem->ReadByte(_r.DE); break;
    }
    return false;
}
bool i8085::LHLD(uint8_t op, uint16_t arg)
{
    _r.HL = _mem->ReadWord(arg);
    return false;
}
bool i8085::LXI(uint8_t op, uint16_t arg)
{
    switch ((op & 0x30) >> 4) {
    case 0: _r.BC = arg; break;
    case 1: _r.DE = arg; break;
    case 2: _r.HL = arg; break;
    default: _sp = arg; break;
    }
    return false;
}
bool i8085::MOV(uint8_t op, uint16_t arg)
{
    int src = op & 0x7;
    int dst = (op >> 3) & 0x7;
    int val;
    switch (src) {
    case 0: val = _r.B; break;
    case 1: val = _r.C; break;
    case 2: val = _r.D; break;
    case 3: val = _r.E; break;
    case 4: val = _r.H; break;
    case 5: val = _r.L; break;
    case 6: val = _mem->ReadByte(_r.HL); break;
    default: val = _r.A; break;
    }
    switch (dst) {
    case 0: _r.B = static_cast<uint8_t>(val); break;
    case 1: _r.C = static_cast<uint8_t>(val); break;
    case 2: _r.D = static_cast<uint8_t>(val); break;
    case 3: _r.E = static_cast<uint8_t>(val); break;
    case 4: _r.H = static_cast<uint8_t>(val); break;
    case 5: _r.L = static_cast<uint8_t>(val); break;
    case 6: _mem->WriteByte(_r.HL, static_cast<uint8_t>(val)); break;
    default: _r.A = static_cast<uint8_t>(val); break;
    }
    return false;
}
bool i8085::MVI(uint8_t op, uint16_t arg)
{
    switch ((op & 0x38) >> 3) {
    case 0: _r.B = static_cast<uint8_t>(arg); break;
    case 1: _r.C = static_cast<uint8_t>(arg); break;
    case 2: _r.D = static_cast<uint8_t>(arg); break;
    case 3: _r.E = static_cast<uint8_t>(arg); break;
    case 4: _r.H = static_cast<uint8_t>(arg); break;
    case 5: _r.L = static_cast<uint8_t>(arg); break;
    case 6: _mem->WriteByte(_r.HL, static_cast<uint8_t>(arg)); break;
    default: _r.A = static_cast<uint8_t>(arg); break;
    }
    return false;
}
bool i8085::NOP(uint8_t op, uint16_t arg)
{
    return false;
}
bool i8085::OUT(uint8_t op, uint16_t arg)
{
    _io->Out(static_cast<uint8_t>(arg), _r.A);
    return false;
}
bool i8085::ORA(uint8_t op, uint16_t arg)
{
    int src;
    switch (op & 0x7) {
    case 0: src = _r.B; break;
    case 1: src = _r.C; break;
    case 2: src = _r.D; break;
    case 3: src = _r.E; break;
    case 4: src = _r.H; break;
    case 5: src = _r.L; break;
    case 6: src = _mem->ReadByte(_r.HL); break;
    default: src = _r.A; break;
    }
    _r.A |= static_cast<uint8_t>(src);
    _r.set_F_CY(false);
    _r.set_F_AC(false);
    _r.set_F_Z(_r.A == 0);
    _r.set_F_S((_r.A & 0x80) != 0);
    _r.set_F_P(_parityTable[_r.A]);
    return false;
}
bool i8085::ORI(uint8_t op, uint16_t arg)
{
    _r.A |= static_cast<uint8_t>(arg);
    _r.set_F_CY(false);
    _r.set_F_AC(false);
    _r.set_F_Z(_r.A == 0);
    _r.set_F_S((_r.A & 0x80) != 0);
    _r.set_F_P(_parityTable[_r.A]);
    return false;
}
bool i8085::PCHL(uint8_t op, uint16_t arg)
{
    _pc = _r.HL;
    return false;
}
bool i8085::POP(uint8_t op, uint16_t arg)
{
    switch ((op & 0x30) >> 4) {
    case 0: _r.BC = Pop(); break;
    case 1: _r.DE = Pop(); break;
    case 2: _r.HL = Pop(); break;
    default: _r.AF = Pop(); break;
    }
    return false;
}
bool i8085::PUSH(uint8_t op, uint16_t arg)
{
    switch ((op & 0x30) >> 4) {
    case 0: Push(_r.BC); break;
    case 1: Push(_r.DE); break;
    case 2: Push(_r.HL); break;
    default: Push(_r.AF); break;
    }
    return false;
}
bool i8085::RAL(uint8_t op, uint16_t arg)
{
    bool newCarry = (_r.A & 0x80) != 0;
    _r.A = static_cast<uint8_t>((_r.A << 1) | (_r.F_CY() ? 1 : 0));
    _r.set_F_CY(newCarry);
    return false;
}
bool i8085::RAR(uint8_t op, uint16_t arg)
{
    bool newCarry = (_r.A & 0x01) != 0;
    _r.A = static_cast<uint8_t>((_r.A >> 1) | (_r.F_CY() ? 0x80 : 0));
    _r.set_F_CY(newCarry);
    return false;
}
bool i8085::RET(uint8_t op, uint16_t arg)
{
    _pc = Pop();
    return false;
}
bool i8085::RETC(uint8_t op, uint16_t arg)
{
    bool ret = false;
    switch ((op & 0x38) >> 3) {
    case 0: ret = !_r.F_Z(); break;   // RNZ
    case 1: ret =  _r.F_Z(); break;   // RZ
    case 2: ret = !_r.F_CY(); break;  // RNC
    case 3: ret =  _r.F_CY(); break;  // RC
    case 4: ret = !_r.F_P(); break;   // RPO
    case 5: ret =  _r.F_P(); break;   // RPE
    case 6: ret = !_r.F_S(); break;   // RP
    case 7: ret =  _r.F_S(); break;   // RM
    }
    if (ret) _pc = Pop();
    return ret;
}
bool i8085::RIM(uint8_t op, uint16_t arg)
{
    _r.A = _interruptMask;
    return false;
}
bool i8085::RLC(uint8_t op, uint16_t arg)
{
    _r.set_F_CY((_r.A & 0x80) != 0);
    _r.A = static_cast<uint8_t>((_r.A << 1) | (_r.F_CY() ? 1 : 0));
    return false;
}
bool i8085::RRC(uint8_t op, uint16_t arg)
{
    _r.set_F_CY((_r.A & 0x01) != 0);
    _r.A = static_cast<uint8_t>((_r.A >> 1) | (_r.F_CY() ? 0x80 : 0));
    return false;
}
bool i8085::RST(uint8_t op, uint16_t arg)
{
    Restore(static_cast<uint16_t>(op & 0x38));
    return false;
}
bool i8085::SBB(uint8_t op, uint16_t arg)
{
    int src;
    switch (op & 0x7) {
    case 0: src = _r.B; break;
    case 1: src = _r.C; break;
    case 2: src = _r.D; break;
    case 3: src = _r.E; break;
    case 4: src = _r.H; break;
    case 5: src = _r.L; break;
    case 6: src = _mem->ReadByte(_r.HL); break;
    default: src = _r.A; break;
    }
    int cy = _r.F_CY() ? 1 : 0;
    _r.set_F_AC(static_cast<int8_t>((_r.A & 0x0f) - (src & 0x0f) - cy) < 0);
    int temp = _r.A - src - cy;
    _r.set_F_CY(temp < 0);
    _r.A = static_cast<uint8_t>(temp);
    _r.set_F_Z(_r.A == 0);
    _r.set_F_S((_r.A & 0x80) != 0);
    _r.set_F_P(_parityTable[_r.A]);
    return false;
}
bool i8085::SBI(uint8_t op, uint16_t arg)
{
    int cy = _r.F_CY() ? 1 : 0;
    _r.set_F_AC(static_cast<int8_t>((_r.A & 0x0f) - (arg & 0x0f) - cy) < 0);
    int temp = _r.A - static_cast<uint8_t>(arg) - cy;
    _r.set_F_CY(temp < 0);
    _r.A = static_cast<uint8_t>(temp);
    _r.set_F_Z(_r.A == 0);
    _r.set_F_S((_r.A & 0x80) != 0);
    _r.set_F_P(_parityTable[_r.A]);
    return false;
}
bool i8085::SHLD(uint8_t op, uint16_t arg)
{
    _mem->WriteWord(arg, _r.HL);
    return false;
}
bool i8085::SIM(uint8_t op, uint16_t arg)
{
    if ((_r.A & 0x8) != 0) {
        // MSE bit set: update interrupt mask bits (bits 0-2)
        _interruptMask = static_cast<uint8_t>((_interruptMask & 0xf8) | (_r.A & 0x7));
    }
    if ((_r.A & 0x10) != 0) {
        // R7.5: clear RST7.5 pending flip-flop
        _interruptMask &= static_cast<uint8_t>(~P7_5);
    }
    return false;
}
bool i8085::SPHL(uint8_t op, uint16_t arg)
{
    _sp = _r.HL;
    return false;
}
bool i8085::STA(uint8_t op, uint16_t arg)
{
    _mem->WriteByte(arg, _r.A);
    return false;
}
bool i8085::STAX(uint8_t op, uint16_t arg)
{
    switch ((op & 0x10) >> 4) {
    case 0: _mem->WriteByte(_r.BC, _r.A); break;
    default: _mem->WriteByte(_r.DE, _r.A); break;
    }
    return false;
}
bool i8085::STC(uint8_t op, uint16_t arg)
{
    _r.set_F_CY(true);
    return false;
}
bool i8085::SUB(uint8_t op, uint16_t arg)
{
    int src;
    switch (op & 0x7) {
    case 0: src = _r.B; break;
    case 1: src = _r.C; break;
    case 2: src = _r.D; break;
    case 3: src = _r.E; break;
    case 4: src = _r.H; break;
    case 5: src = _r.L; break;
    case 6: src = _mem->ReadByte(_r.HL); break;
    default: src = _r.A; break;
    }
    _r.set_F_AC(static_cast<int8_t>((_r.A & 0x0f) - (src & 0x0f)) < 0);
    int temp = _r.A - src;
    _r.set_F_CY(temp < 0);
    _r.A = static_cast<uint8_t>(temp);
    _r.set_F_Z(_r.A == 0);
    _r.set_F_S((_r.A & 0x80) != 0);
    _r.set_F_P(_parityTable[_r.A]);
    return false;
}
bool i8085::SUI(uint8_t op, uint16_t arg)
{
    _r.set_F_AC(static_cast<int8_t>((_r.A & 0x0f) - (arg & 0x0f)) < 0);
    int temp = _r.A - static_cast<uint8_t>(arg);
    _r.set_F_CY(temp < 0);
    _r.A = static_cast<uint8_t>(temp);
    _r.set_F_Z(_r.A == 0);
    _r.set_F_S((_r.A & 0x80) != 0);
    _r.set_F_P(_parityTable[_r.A]);
    return false;
}
bool i8085::XCHG(uint8_t op, uint16_t arg)
{
    uint16_t tmp = _r.HL;
    _r.HL = _r.DE;
    _r.DE = tmp;
    return false;
}
bool i8085::XRA(uint8_t op, uint16_t arg)
{
    int src;
    switch (op & 0x7) {
    case 0: src = _r.B; break;
    case 1: src = _r.C; break;
    case 2: src = _r.D; break;
    case 3: src = _r.E; break;
    case 4: src = _r.H; break;
    case 5: src = _r.L; break;
    case 6: src = _mem->ReadByte(_r.HL); break;
    default: src = _r.A; break;
    }
    _r.A ^= static_cast<uint8_t>(src);
    _r.set_F_CY(false);
    _r.set_F_AC(false);
    _r.set_F_Z(_r.A == 0);
    _r.set_F_S((_r.A & 0x80) != 0);
    _r.set_F_P(_parityTable[_r.A]);
    return false;
}
bool i8085::XRI(uint8_t op, uint16_t arg)
{
    _r.A ^= static_cast<uint8_t>(arg);
    _r.set_F_CY(false);
    _r.set_F_AC(false);
    _r.set_F_Z(_r.A == 0);
    _r.set_F_S((_r.A & 0x80) != 0);
    _r.set_F_P(_parityTable[_r.A]);
    return false;
}
bool i8085::XTHL(uint8_t op, uint16_t arg)
{
    uint16_t tmp = Pop();
    Push(_r.HL);
    _r.HL = tmp;
    return false;
}
// ---- Instruction table initialization (all 256 opcodes) ----

#define E(fn) [](i8085* cpu, uint8_t o, uint16_t a) -> bool { return cpu->fn(o, a); }

void i8085::InitializeInstructionTables()
{
    // Default all to Invalid
    for (int i = 0; i < 256; i++)
        _instructionData[i] = { static_cast<uint8_t>(i), "INVALID", 1, 4, 4, E(Invalid) };

    _instructionData[0x00] = {0x00,"NOP",1,4,4,E(NOP)};
    _instructionData[0x01] = {0x01,"LXI B,$%04x",3,10,10,E(LXI)};
    _instructionData[0x02] = {0x02,"STAX B",1,7,7,E(STAX)};
    _instructionData[0x03] = {0x03,"INX B",1,6,6,E(INX)};
    _instructionData[0x04] = {0x04,"INR B",1,4,4,E(INR)};
    _instructionData[0x05] = {0x05,"DCR B",1,4,4,E(DCR)};
    _instructionData[0x06] = {0x06,"MVI B,$%02x",2,7,7,E(MVI)};
    _instructionData[0x07] = {0x07,"RLC",1,4,4,E(RLC)};
    _instructionData[0x08] = {0x08,"INVALID",1,4,4,E(Invalid)};
    _instructionData[0x09] = {0x09,"DAD B",1,10,10,E(DAD)};
    _instructionData[0x0a] = {0x0a,"LDAX B",1,7,7,E(LDAX)};
    _instructionData[0x0b] = {0x0b,"DCX B",1,6,6,E(DCX)};
    _instructionData[0x0c] = {0x0c,"INR C",1,4,4,E(INR)};
    _instructionData[0x0d] = {0x0d,"DCR C",1,4,4,E(DCR)};
    _instructionData[0x0e] = {0x0e,"MVI C,$%02x",2,7,7,E(MVI)};
    _instructionData[0x0f] = {0x0f,"RRC",1,4,4,E(RRC)};

    _instructionData[0x10] = {0x10,"INVALID",1,4,4,E(Invalid)};
    _instructionData[0x11] = {0x11,"LXI D,$%04x",3,10,10,E(LXI)};
    _instructionData[0x12] = {0x12,"STAX D",1,7,7,E(STAX)};
    _instructionData[0x13] = {0x13,"INX D",1,6,6,E(INX)};
    _instructionData[0x14] = {0x14,"INR D",1,4,4,E(INR)};
    _instructionData[0x15] = {0x15,"DCR D",1,4,4,E(DCR)};
    _instructionData[0x16] = {0x16,"MVI D,$%02x",2,7,7,E(MVI)};
    _instructionData[0x17] = {0x17,"RAL",1,4,4,E(RAL)};
    _instructionData[0x18] = {0x18,"INVALID",1,4,4,E(Invalid)};
    _instructionData[0x19] = {0x19,"DAD D",1,10,10,E(DAD)};
    _instructionData[0x1a] = {0x1a,"LDAX D",1,7,7,E(LDAX)};
    _instructionData[0x1b] = {0x1b,"DCX D",1,6,6,E(DCX)};
    _instructionData[0x1c] = {0x1c,"INR E",1,4,4,E(INR)};
    _instructionData[0x1d] = {0x1d,"DCR E",1,4,4,E(DCR)};
    _instructionData[0x1e] = {0x1e,"MVI E,$%02x",2,7,7,E(MVI)};
    _instructionData[0x1f] = {0x1f,"RAR",1,4,4,E(RAR)};

    _instructionData[0x20] = {0x20,"RIM",1,4,4,E(RIM)};
    _instructionData[0x21] = {0x21,"LXI H,$%04x",3,10,10,E(LXI)};
    _instructionData[0x22] = {0x22,"SHLD $%04x",3,16,16,E(SHLD)};
    _instructionData[0x23] = {0x23,"INX H",1,6,6,E(INX)};
    _instructionData[0x24] = {0x24,"INR H",1,4,4,E(INR)};
    _instructionData[0x25] = {0x25,"DCR H",1,4,4,E(DCR)};
    _instructionData[0x26] = {0x26,"MVI H,$%02x",2,7,7,E(MVI)};
    _instructionData[0x27] = {0x27,"DAA",1,4,4,E(DAA)};
    _instructionData[0x28] = {0x28,"INVALID",1,4,4,E(Invalid)};
    _instructionData[0x29] = {0x29,"DAD H",1,10,10,E(DAD)};
    _instructionData[0x2a] = {0x2a,"LHLD $%04x",3,16,16,E(LHLD)};
    _instructionData[0x2b] = {0x2b,"DCX H",1,6,6,E(DCX)};
    _instructionData[0x2c] = {0x2c,"INR L",1,4,4,E(INR)};
    _instructionData[0x2d] = {0x2d,"DCR L",1,4,4,E(DCR)};
    _instructionData[0x2e] = {0x2e,"MVI L,$%02x",2,7,7,E(MVI)};
    _instructionData[0x2f] = {0x2f,"CMA",1,4,4,E(CMA)};

    _instructionData[0x30] = {0x30,"SIM",1,4,4,E(SIM)};
    _instructionData[0x31] = {0x31,"LXI SP,$%04x",3,10,10,E(LXI)};
    _instructionData[0x32] = {0x32,"STA $%04x",3,13,13,E(STA)};
    _instructionData[0x33] = {0x33,"INX SP",1,6,6,E(INX)};
    _instructionData[0x34] = {0x34,"INR M",1,10,10,E(INR)};
    _instructionData[0x35] = {0x35,"DCR M",1,10,10,E(DCR)};
    _instructionData[0x36] = {0x36,"MVI M,$%02x",2,10,10,E(MVI)};
    _instructionData[0x37] = {0x37,"STC",1,4,4,E(STC)};
    _instructionData[0x38] = {0x38,"INVALID",2,4,4,E(Invalid)};
    _instructionData[0x39] = {0x39,"DAD SP",1,10,10,E(DAD)};
    _instructionData[0x3a] = {0x3a,"LDA $%04x",3,13,13,E(LDA)};
    _instructionData[0x3b] = {0x3b,"DCX SP",1,6,6,E(DCX)};
    _instructionData[0x3c] = {0x3c,"INR A",1,4,4,E(INR)};
    _instructionData[0x3d] = {0x3d,"DCR A",1,4,4,E(DCR)};
    _instructionData[0x3e] = {0x3e,"MVI A,$%02x",2,7,7,E(MVI)};
    _instructionData[0x3f] = {0x3f,"CMC",1,4,4,E(CMC)};

    // MOV 0x40-0x7f (except 0x76=HLT)
    for (int op = 0x40; op <= 0x7f; op++) {
        if (op == 0x76) continue;
        int c = ((op & 7) == 6 || ((op >> 3) & 7) == 6) ? 7 : 4;
        _instructionData[op] = {(uint8_t)op,"MOV",1,c,c,E(MOV)};
    }
    _instructionData[0x76] = {0x76,"HLT",1,5,5,E(HLT)};

    // Arithmetic/logic 0x80-0xbf
    for (int op = 0x80; op <= 0x87; op++) _instructionData[op] = {(uint8_t)op,"ADD",1,(op==0x86?7:4),(op==0x86?7:4),E(ADD)};
    for (int op = 0x88; op <= 0x8f; op++) _instructionData[op] = {(uint8_t)op,"ADC",1,(op==0x8e?7:4),(op==0x8e?7:4),E(ADC)};
    for (int op = 0x90; op <= 0x97; op++) _instructionData[op] = {(uint8_t)op,"SUB",1,(op==0x96?7:4),(op==0x96?7:4),E(SUB)};
    for (int op = 0x98; op <= 0x9f; op++) _instructionData[op] = {(uint8_t)op,"SBB",1,(op==0x9e?7:4),(op==0x9e?7:4),E(SBB)};
    for (int op = 0xa0; op <= 0xa7; op++) _instructionData[op] = {(uint8_t)op,"ANA",1,(op==0xa6?7:4),(op==0xa6?7:4),E(ANA)};
    for (int op = 0xa8; op <= 0xaf; op++) _instructionData[op] = {(uint8_t)op,"XRA",1,(op==0xae?7:4),(op==0xae?7:4),E(XRA)};
    for (int op = 0xb0; op <= 0xb7; op++) _instructionData[op] = {(uint8_t)op,"ORA",1,(op==0xb6?7:4),(op==0xb6?7:4),E(ORA)};
    for (int op = 0xb8; op <= 0xbf; op++) _instructionData[op] = {(uint8_t)op,"CMP",1,(op==0xbe?7:4),(op==0xbe?7:4),E(CMP)};

    _instructionData[0xc0] = {0xc0,"RNZ",1,12,6,E(RETC)};
    _instructionData[0xc1] = {0xc1,"POP B",1,10,10,E(POP)};
    _instructionData[0xc2] = {0xc2,"JNZ $%04x",3,10,7,E(JMP)};
    _instructionData[0xc3] = {0xc3,"JMP $%04x",3,10,10,E(JMP)};
    _instructionData[0xc4] = {0xc4,"CNZ $%04x",3,18,9,E(CALLC)};
    _instructionData[0xc5] = {0xc5,"PUSH B",1,12,12,E(PUSH)};
    _instructionData[0xc6] = {0xc6,"ADI $%02x",2,7,7,E(ADI)};
    _instructionData[0xc7] = {0xc7,"RST 0",1,12,12,E(RST)};
    _instructionData[0xc8] = {0xc8,"RZ",1,12,6,E(RETC)};
    _instructionData[0xc9] = {0xc9,"RET",1,10,10,E(RET)};
    _instructionData[0xca] = {0xca,"JZ $%04x",3,10,7,E(JMP)};
    _instructionData[0xcb] = {0xcb,"INVALID",1,4,4,E(Invalid)};
    _instructionData[0xcc] = {0xcc,"CZ $%04x",3,18,9,E(CALLC)};
    _instructionData[0xcd] = {0xcd,"CALL $%04x",3,18,18,E(CALL)};
    _instructionData[0xce] = {0xce,"ACI $%02x",2,7,7,E(ACI)};
    _instructionData[0xcf] = {0xcf,"RST 1",1,12,12,E(RST)};

    _instructionData[0xd0] = {0xd0,"RNC",1,12,6,E(RETC)};
    _instructionData[0xd1] = {0xd1,"POP D",1,10,10,E(POP)};
    _instructionData[0xd2] = {0xd2,"JNC $%04x",3,10,7,E(JMP)};
    _instructionData[0xd3] = {0xd3,"OUT $%02x",2,10,10,E(OUT)};
    _instructionData[0xd4] = {0xd4,"CNC $%04x",3,18,9,E(CALLC)};
    _instructionData[0xd5] = {0xd5,"PUSH D",1,12,12,E(PUSH)};
    _instructionData[0xd6] = {0xd6,"SUI $%02x",2,7,7,E(SUI)};
    _instructionData[0xd7] = {0xd7,"RST 2",1,12,12,E(RST)};
    _instructionData[0xd8] = {0xd8,"RC",1,12,6,E(RETC)};
    _instructionData[0xd9] = {0xd9,"INVALID",1,10,10,E(Invalid)};
    _instructionData[0xda] = {0xda,"JC $%04x",3,10,7,E(JMP)};
    _instructionData[0xdb] = {0xdb,"IN $%02x",2,10,10,E(IN)};
    _instructionData[0xdc] = {0xdc,"CC $%04x",3,18,9,E(CALLC)};
    _instructionData[0xdd] = {0xdd,"INVALID",1,4,4,E(Invalid)};
    _instructionData[0xde] = {0xde,"SBI $%02x",2,7,7,E(SBI)};
    _instructionData[0xdf] = {0xdf,"RST 3",1,12,12,E(RST)};

    _instructionData[0xe0] = {0xe0,"RPO",1,12,6,E(RETC)};
    _instructionData[0xe1] = {0xe1,"POP H",1,10,10,E(POP)};
    _instructionData[0xe2] = {0xe2,"JPO $%04x",3,10,7,E(JMP)};
    _instructionData[0xe3] = {0xe3,"XTHL",1,16,16,E(XTHL)};
    _instructionData[0xe4] = {0xe4,"CPO $%04x",3,18,9,E(CALLC)};
    _instructionData[0xe5] = {0xe5,"PUSH H",1,12,12,E(PUSH)};
    _instructionData[0xe6] = {0xe6,"ANI $%02x",2,7,7,E(ANI)};
    _instructionData[0xe7] = {0xe7,"RST 4",1,12,12,E(RST)};
    _instructionData[0xe8] = {0xe8,"RPE",1,12,6,E(RETC)};
    _instructionData[0xe9] = {0xe9,"PCHL",1,6,6,E(PCHL)};
    _instructionData[0xea] = {0xea,"JPE $%04x",3,10,7,E(JMP)};
    _instructionData[0xeb] = {0xeb,"XCHG",1,4,4,E(XCHG)};
    _instructionData[0xec] = {0xec,"CPE $%04x",3,18,9,E(CALLC)};
    _instructionData[0xed] = {0xed,"INVALID",1,4,4,E(Invalid)};
    _instructionData[0xee] = {0xee,"XRI $%02x",2,7,7,E(XRI)};
    _instructionData[0xef] = {0xef,"RST 5",1,12,12,E(RST)};

    _instructionData[0xf0] = {0xf0,"RP",1,12,6,E(RETC)};
    _instructionData[0xf1] = {0xf1,"POP PSW",1,10,10,E(POP)};
    _instructionData[0xf2] = {0xf2,"JP $%04x",3,10,7,E(JMP)};
    _instructionData[0xf3] = {0xf3,"DI",1,4,4,E(DI)};
    _instructionData[0xf4] = {0xf4,"CP $%04x",3,18,9,E(CALLC)};
    _instructionData[0xf5] = {0xf5,"PUSH PSW",1,12,12,E(PUSH)};
    _instructionData[0xf6] = {0xf6,"ORI $%02x",2,7,7,E(ORI)};
    _instructionData[0xf7] = {0xf7,"RST 6",1,12,12,E(RST)};
    _instructionData[0xf8] = {0xf8,"RM",1,12,6,E(RETC)};
    _instructionData[0xf9] = {0xf9,"SPHL",1,6,6,E(SPHL)};
    _instructionData[0xfa] = {0xfa,"JM $%04x",3,10,7,E(JMP)};
    _instructionData[0xfb] = {0xfb,"EI",1,4,4,E(EI)};
    _instructionData[0xfc] = {0xfc,"CM $%04x",3,18,9,E(CALLC)};
    _instructionData[0xfd] = {0xfd,"INVALID",3,4,4,E(Invalid)};
    _instructionData[0xfe] = {0xfe,"CPI $%02x",2,7,7,E(CPI)};
    _instructionData[0xff] = {0xff,"RST 7",1,12,12,E(RST)};
}

#undef E
