/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
*/
#pragma once
#include <cstdint>
#include <cstring>

// ---------------------------------------------------------------------------
// Enumerations – ported from D/CP/Microinstruction.cs
// ---------------------------------------------------------------------------

enum class AluSourcePair : int {
    AQ = 0, AB = 1, ZQ = 2, ZB = 3, ZA = 4, DA = 5, DQ = 6, D0 = 7,
};

enum class AluFunction : int {
    RplusS   = 0, SminusR  = 1, RminusS  = 2,
    RorS     = 3, RandS    = 4, notRandS = 5,
    RxorS    = 6, notRxorS = 7,
};

enum class FunctionSelectFY : int {
    DispBr = 0, fyNorm = 1, IOOut = 2, Byte = 3,
};

enum class FunctionSelectFZ : int {
    fzNorm = 0, Nibble = 1, Uaddr = 2, IOXIn = 3,
};

enum class XFunction : int {
    pCallRet0 = 0, pCallRet1 = 1, pCallRet2 = 2, pCallRet3 = 3,
    pCallRet4 = 4, pCallRet5 = 5, pCallRet6 = 6, pCallRet7 = 7,
    Noop      = 8, LoadRH   = 9, shift    = 0xa, cycle    = 0xb,
    LoadCinFrompc16 = 0xc, LoadMap = 0xd, pop = 0xe, push = 0xf,
};

enum class YNormFunction : int {
    ExitKern    = 0x0, EnterKern  = 0x1, ClrIntErr   = 0x2, IBDisp      = 0x3,
    MesaIntRq   = 0x4, LoadstackP = 0x5, LoadIB      = 0x6, cycle       = 0x7,
    Noop        = 0x8, LoadMap    = 0x9, Refresh     = 0xa, push        = 0xb,
    ClrDPRq     = 0xc, ClrIOPRq   = 0xd, ClrRefRq   = 0xe, ClrKFlags   = 0xf,
};

enum class YDispBrFunction : int {
    NegBr     = 0x0, ZeroBr   = 0x1, NZeroBr    = 0x2, MesaIntBr = 0x3,
    PgCarryBr = 0x4, CarryBr  = 0x5, XRefBr     = 0x6, NibCarryBr = 0x7,
    XDisp     = 0x8, YDisp    = 0x9, XC2npcDisp = 0xa, YIODisp   = 0xb,
    XwdDisp   = 0xc, XHDisp   = 0xd, XLDisp     = 0xe, PgCrOvDisp = 0xf,
};

enum class YIOOutFunction : int {
    IOPOData = 0x0, IOPCtl   = 0x1, KOData   = 0x2, KCtl     = 0x3,
    EOData   = 0x4, EICtl    = 0x5, DCtlFifo = 0x6, DCtl     = 0x7,
    DBorder  = 0x8, PCtl     = 0x9, MCtl     = 0xa, Invalid0 = 0xb,
    EOCtl    = 0xc, KCmd     = 0xd, Invalid1 = 0xe, POData   = 0xf,
};

enum class ZNormFunction : int {
    Refresh    = 0x0, LoadIBPtr1 = 0x1, LoadIBPtr0 = 0x2,
    LoadCinFrompc16 = 0x3, LoadBank = 0x4, pop      = 0x5, push = 0x6,
    AltUaddr   = 0x7, Noop0  = 0x8, Noop1 = 0x9, Noop2 = 0xa, Noop3 = 0xb,
    LRot0      = 0xc, LRot12 = 0xd, LRot8 = 0xe, LRot4 = 0xf,
};

enum class ZIOXIn : int {
    ReadEIdata  = 0x0, ReadEStatus   = 0x1, ReadKIData     = 0x2, ReadKStatus = 0x3,
    KStrobe     = 0x4, ReadMStatus   = 0x5, ReadKTest      = 0x6, EStrobe     = 0x7,
    ReadIOPIData= 0x8, ReadIOPStatus = 0x9, ReadErrnIBnStkp= 0xa, ReadRH      = 0xb,
    ReadibNA    = 0xc, Readib        = 0xd, ReadibLow      = 0xe, ReadibHigh  = 0xf,
};

enum class StackTestType : int {
    None = 0, Underflow, Overflow, Underflow2,
};

enum class NiaModiferType : int {
    Normal = 0, pCallRet, Dispatch, SJump,
};

enum class ErrorTrap : int {
    IBEmpty = 0,
    StackUnderflow,
    StackOverflow,
    EmulatorMemoryError,
    UnimplementedInstruction,
};

// ---------------------------------------------------------------------------
// Microinstruction – decoded from a 48-bit microcode word.
// Ported from D/CP/Microinstruction.cs (the constructor + readonly fields).
// ---------------------------------------------------------------------------
struct Microinstruction {
    // ---- Raw fields -------------------------------------------------------
    int            rA;       // AM2901 A reg address
    int            rB;       // AM2901 B reg address / RH address
    AluSourcePair  aS;       // ALU source operand pair
    AluFunction    aF;       // ALU function
    int            aD;       // ALU destination/shift control (2 bits)
    bool           ep;       // Even parity
    bool           Cin;      // Carry in / shift ends / write SU
    bool           enSU;     // Enable SU register file
    bool           mem;      // MAR<-(c1) / MDR<-(c2) / <-MD(c3)
    FunctionSelectFY fSfY;
    FunctionSelectFZ fSfZ;
    XFunction      fX;
    int            fY;
    int            fZ;
    int            INIA;     // Next instruction address (12 bits)

    // ---- Pre-computed metadata --------------------------------------------
    bool   Cycle;
    bool   Shift;
    bool   AluNeedsXBus;
    int    AluDestination;
    bool   SURead;
    bool   SUWrite;
    bool   LoadMap;
    bool   ABypass;
    bool   LoadStackP;
    bool   Push;
    bool   Pop;
    bool   DoublePop;
    bool   StackOperation;
    StackTestType StackTest;
    bool   AlwaysIBDisp;
    bool   LoadIB;
    bool   LoadIBPtr1;
    int    UAddress;
    uint8_t Byte;
    int    LinkAddress;
    bool   MarMapMDR;
    bool   LateLRotN;

    // Default: no-op
    Microinstruction() { memset(this, 0, sizeof(*this)); LinkAddress = -1; }

    // Decode from a 48-bit microcode word
    explicit Microinstruction(uint64_t word) {
        rA    = (int)((word & 0xf00000000000ULL) >> 44);
        rB    = (int)((word & 0x0f0000000000ULL) >> 40);
        aS    = (AluSourcePair)(int)((word & 0x00e000000000ULL) >> 37);
        aF    = (AluFunction)(int)((word & 0x001c00000000ULL) >> 34);
        aD    = (int)((word & 0x000300000000ULL) >> 32);
        ep    = (word & 0x000080000000ULL) != 0;
        Cin   = (word & 0x000040000000ULL) != 0;
        enSU  = (word & 0x000020000000ULL) != 0;
        mem   = (word & 0x000010000000ULL) != 0;
        fSfY  = (FunctionSelectFY)(int)((word & 0x00000c000000ULL) >> 26);
        fSfZ  = (FunctionSelectFZ)(int)((word & 0x000003000000ULL) >> 24);
        fX    = (XFunction)(int)((word & 0x000000f00000ULL) >> 20);
        fY    = (int)((word & 0x0000000f0000ULL) >> 16);
        fZ    = (int)((word & 0x00000000f000ULL) >> 12);
        INIA  = (int)((word & 0x000000000fffULL));

        Cycle = (fX == XFunction::cycle) ||
                (fSfY == FunctionSelectFY::fyNorm && (YNormFunction)fY == YNormFunction::cycle);
        Shift = (fX == XFunction::shift) || Cycle;

        AluNeedsXBus = (aS == AluSourcePair::D0 || aS == AluSourcePair::DA || aS == AluSourcePair::DQ);
        AluDestination = aD | (Shift ? 0x4 : 0x0);

        SURead  = enSU && !Cin;
        SUWrite = enSU &&  Cin;

        LoadMap = (fX == XFunction::LoadMap) ||
                  (fSfY == FunctionSelectFY::fyNorm && (YNormFunction)fY == YNormFunction::LoadMap);

        ABypass = (AluDestination == 0x2);

        LoadStackP = (fSfY == FunctionSelectFY::fyNorm &&
                      (YNormFunction)fY == YNormFunction::LoadstackP);

        LoadIBPtr1 = (fSfZ == FunctionSelectFZ::fzNorm &&
                      (ZNormFunction)fZ == ZNormFunction::LoadIBPtr1);

        AlwaysIBDisp = (fSfY == FunctionSelectFY::fyNorm &&
                        (YNormFunction)fY == YNormFunction::IBDisp) && LoadIBPtr1;

        LoadIB = (fSfY == FunctionSelectFY::fyNorm &&
                  (YNormFunction)fY == YNormFunction::LoadIB);

        UAddress = (rA << 4) | fZ;

        // Byte/Nibble constant
        if (fSfY == FunctionSelectFY::Byte) {
            Byte = (uint8_t)((fY << 4) | fZ);
        } else if (fSfZ == FunctionSelectFZ::Nibble) {
            Byte = (uint8_t)fZ;
        } else {
            Byte = 0;
        }

        bool fxPop = (fX == XFunction::pop);
        bool fzPop = (fSfZ == FunctionSelectFZ::fzNorm &&
                      (ZNormFunction)fZ == ZNormFunction::pop);

        Pop    = fxPop || fzPop;
        DoublePop = fxPop && fzPop;
        Push   = (fX == XFunction::push) ||
                 (fSfY == FunctionSelectFY::fyNorm && (YNormFunction)fY == YNormFunction::push) ||
                 (fSfZ == FunctionSelectFZ::fzNorm && (ZNormFunction)fZ == ZNormFunction::push);

        StackOperation = Pop || Push;

        if (fxPop && fzPop && Push)       StackTest = StackTestType::Underflow2;
        else if (Push && fzPop)           StackTest = StackTestType::Overflow;
        else if (fxPop && Push)           StackTest = StackTestType::Underflow;
        else                              StackTest = StackTestType::None;

        switch (fX) {
        case XFunction::pCallRet0: case XFunction::pCallRet1:
        case XFunction::pCallRet2: case XFunction::pCallRet3:
        case XFunction::pCallRet4: case XFunction::pCallRet5:
        case XFunction::pCallRet6: case XFunction::pCallRet7:
            LinkAddress = (int)fX; break;
        default:
            LinkAddress = -1; break;
        }

        MarMapMDR  = mem || LoadMap;
        LateLRotN  = !ABypass && (fSfZ == FunctionSelectFZ::fzNorm);
    }
};
