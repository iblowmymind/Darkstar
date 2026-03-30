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

enum class AluSourcePair {
    AQ = 0,
    AB = 1,
    ZQ = 2,
    ZB = 3,
    ZA = 4,
    DA = 5,
    DQ = 6,
    D0 = 7,
};

enum class AluFunction {
    RplusS      = 0,
    SminusR     = 1,
    RminusS     = 2,
    RorS        = 3,
    RandS       = 4,
    notRandS    = 5,
    RxorS       = 6,
    notRxorS    = 7,
};

enum class FunctionSelectFY {
    DispBr = 0,
    fyNorm = 1,
    IOOut  = 2,
    Byte   = 3,
};

enum class FunctionSelectFZ {
    fzNorm = 0,
    Nibble = 1,
    Uaddr  = 2,
    IOXIn  = 3,
};

enum class XFunction {
    pCallRet0       = 0x0,
    pCallRet1       = 0x1,
    pCallRet2       = 0x2,
    pCallRet3       = 0x3,
    pCallRet4       = 0x4,
    pCallRet5       = 0x5,
    pCallRet6       = 0x6,
    pCallRet7       = 0x7,
    Noop            = 0x8,
    LoadRH          = 0x9,
    shift           = 0xa,
    cycle           = 0xb,
    LoadCinFrompc16 = 0xc,
    LoadMap         = 0xd,
    pop             = 0xe,
    push            = 0xf,
};

enum class YNormFunction {
    ExitKern    = 0x0,
    EnterKern   = 0x1,
    ClrIntErr   = 0x2,
    IBDisp      = 0x3,
    MesaIntRq   = 0x4,
    LoadstackP  = 0x5,
    LoadIB      = 0x6,
    cycle       = 0x7,
    Noop        = 0x8,
    LoadMap     = 0x9,
    Refresh     = 0xa,
    push        = 0xb,
    ClrDPRq     = 0xc,
    ClrIOPRq    = 0xd,
    ClrRefRq    = 0xe,
    ClrKFlags   = 0xf,
};

enum class YDispBrFunction {
    NegBr       = 0x0,
    ZeroBr      = 0x1,
    NZeroBr     = 0x2,
    MesaIntBr   = 0x3,
    PgCarryBr   = 0x4,
    CarryBr     = 0x5,
    XRefBr      = 0x6,
    NibCarryBr  = 0x7,
    XDisp       = 0x8,
    YDisp       = 0x9,
    XC2npcDisp  = 0xa,
    YIODisp     = 0xb,
    XwdDisp     = 0xc,
    XHDisp      = 0xd,
    XLDisp      = 0xe,  // AKA XDirtyDisp
    PgCrOvDisp  = 0xf,
};

enum class YIOOutFunction {
    IOPOData    = 0x0,
    IOPCtl      = 0x1,
    KOData      = 0x2,
    KCtl        = 0x3,
    EOData      = 0x4,
    EICtl       = 0x5,
    DCtlFifo    = 0x6,
    DCtl        = 0x7,
    DBorder     = 0x8,
    PCtl        = 0x9,
    MCtl        = 0xa,
    Invalid0    = 0xb,
    EOCtl       = 0xc,
    KCmd        = 0xd,
    Invalid1    = 0xe,
    POData      = 0xf,
};

enum class ZNormFunction {
    Refresh         = 0x0,
    LoadIBPtr1      = 0x1,
    LoadIBPtr0      = 0x2,
    LoadCinFrompc16 = 0x3,
    LoadBank        = 0x4,
    pop             = 0x5,
    push            = 0x6,
    AltUaddr        = 0x7,
    Noop0           = 0x8,
    Noop1           = 0x9,
    Noop2           = 0xa,
    Noop3           = 0xb,
    LRot0           = 0xc,
    LRot12          = 0xd,
    LRot8           = 0xe,
    LRot4           = 0xf,
};

// For Zap Rowsdower
enum class ZIOXIn {
    ReadEIdata      = 0x0,
    ReadEStatus     = 0x1,
    ReadKIData      = 0x2,
    ReadKStatus     = 0x3,
    KStrobe         = 0x4,
    ReadMStatus     = 0x5,
    ReadKTest       = 0x6,
    EStrobe         = 0x7,
    ReadIOPIData    = 0x8,
    ReadIOPStatus   = 0x9,
    ReadErrnIBnStkp = 0xa,
    ReadRH          = 0xb,
    ReadibNA        = 0xc,
    Readib          = 0xd,
    ReadibLow       = 0xe,
    ReadibHigh      = 0xf,
};

enum class StackTestType {
    None,
    Underflow,
    Overflow,
    Underflow2,
};

/// Decodes a single microcode word.
class Microinstruction {
public:
    Microinstruction() = default;
    explicit Microinstruction(uint64_t word);

    std::string to_string() const;
    std::string disassemble(int cycle) const;

    // 2901 A reg addr, U addr [0-3]
    int rA = 0;
    // 2901 B reg addr, RH addr
    int rB = 0;
    // 2901 alu Source operand pair
    AluSourcePair aS = AluSourcePair::AQ;
    // 2901 alu Function
    AluFunction aF = AluFunction::RplusS;
    // 2901 alu Destination/shift control
    int aD = 0;
    // Even Parity
    bool ep = false;
    // 2901 Carry In, Shift Ends, writeSU (if enSU = 1)
    bool Cin = false;
    // enable SU reg file
    bool enSU = false;
    // MAR<- (if c1), MDR<- (if c2), <-MD (if c3)
    bool mem = false;
    // Function field selector for Y
    FunctionSelectFY fSfY = FunctionSelectFY::DispBr;
    // Function field selector for Z
    FunctionSelectFZ fSfZ = FunctionSelectFZ::fzNorm;
    // X Function
    XFunction fX = XFunction::pCallRet0;
    // Y Function
    int fY = 0;
    // Z Function
    int fZ = 0;
    // Next Instruction Address
    int INIA = 0;

    // Precomputed metadata
    bool Cycle = false;
    bool Shift = false;
    bool AluNeedsXBus = false;
    int AluDestination = 0;
    bool SURead = false;
    bool SUWrite = false;
    bool LoadMap = false;
    bool ABypass = false;
    bool LoadStackP = false;
    bool Push = false;
    bool Pop = false;
    bool DoublePop = false;
    bool StackOperation = false;
    StackTestType StackTest = StackTestType::None;
    bool AlwaysIBDisp = false;
    bool LoadIB = false;
    bool LoadIBPtr1 = false;
    int UAddress = 0;
    uint8_t Byte = 0;
    int LinkAddress = -1;
    bool MarMapMDR = false;
    bool LateLRotN = false;

private:
    std::string get_carry_mod() const;
    std::string disassemble_xbus_source(int cycle) const;
};

} // namespace darkstar
