/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/

#include "cp/microinstruction.h"
#include <cstdio>
#include <stdexcept>

namespace darkstar {

Microinstruction::Microinstruction(uint64_t word) {
    rA   = static_cast<int>((word & 0xf00000000000ULL) >> 44);
    rB   = static_cast<int>((word & 0x0f0000000000ULL) >> 40);
    aS   = static_cast<AluSourcePair>((word & 0x00e000000000ULL) >> 37);
    aF   = static_cast<AluFunction>((word & 0x001c00000000ULL) >> 34);
    aD   = static_cast<int>((word & 0x000300000000ULL) >> 32);
    ep   = (word & 0x000080000000ULL) != 0;
    Cin  = (word & 0x000040000000ULL) != 0;
    enSU = (word & 0x000020000000ULL) != 0;
    mem  = (word & 0x000010000000ULL) != 0;
    fSfY = static_cast<FunctionSelectFY>((word & 0x00000c000000ULL) >> 26);
    fSfZ = static_cast<FunctionSelectFZ>((word & 0x000003000000ULL) >> 24);
    fX   = static_cast<XFunction>((word & 0x000000f00000ULL) >> 20);
    fY   = static_cast<int>((word & 0x0000000f0000ULL) >> 16);
    fZ   = static_cast<int>((word & 0x00000000f000ULL) >> 12);
    INIA = static_cast<int>((word & 0x000000000fffULL));

    // Precomputed metadata
    Cycle = (fX == XFunction::cycle) ||
            (fSfY == FunctionSelectFY::fyNorm && static_cast<YNormFunction>(fY) == YNormFunction::cycle);
    Shift = (fX == XFunction::shift) || Cycle;

    AluNeedsXBus = (aS == AluSourcePair::D0 || aS == AluSourcePair::DA || aS == AluSourcePair::DQ);

    AluDestination = aD | (Shift ? 0x4 : 0x0);

    SURead = enSU && !Cin;
    SUWrite = enSU && Cin;

    LoadMap = fX == XFunction::LoadMap ||
              (fSfY == FunctionSelectFY::fyNorm &&
               static_cast<YNormFunction>(fY) == YNormFunction::LoadMap);

    ABypass = AluDestination == 0x2;

    LoadStackP = (fSfY == FunctionSelectFY::fyNorm &&
                  static_cast<YNormFunction>(fY) == YNormFunction::LoadstackP);

    LoadIBPtr1 = (fSfZ == FunctionSelectFZ::fzNorm &&
                  static_cast<ZNormFunction>(fZ) == ZNormFunction::LoadIBPtr1);

    AlwaysIBDisp = (fSfY == FunctionSelectFY::fyNorm &&
                    static_cast<YNormFunction>(fY) == YNormFunction::IBDisp) &&
                   LoadIBPtr1;

    LoadIB = fSfY == FunctionSelectFY::fyNorm &&
             static_cast<YNormFunction>(fY) == YNormFunction::LoadIB;

    UAddress = (rA << 4) | fZ;

    switch (fX) {
        case XFunction::pCallRet0:
        case XFunction::pCallRet1:
        case XFunction::pCallRet2:
        case XFunction::pCallRet3:
        case XFunction::pCallRet4:
        case XFunction::pCallRet5:
        case XFunction::pCallRet6:
        case XFunction::pCallRet7:
            LinkAddress = static_cast<int>(fX);
            break;
        default:
            LinkAddress = -1;
            break;
    }

    MarMapMDR = mem || LoadMap;

    LateLRotN = !ABypass && fSfZ == FunctionSelectFZ::fzNorm;

    if (fSfY == FunctionSelectFY::Byte) {
        Byte = static_cast<uint8_t>((fY << 4) | fZ);
    } else if (fSfZ == FunctionSelectFZ::Nibble) {
        Byte = static_cast<uint8_t>(fZ);
    } else {
        Byte = 0;
    }

    bool fx_pop = (fX == XFunction::pop);
    bool fz_pop = (fSfZ == FunctionSelectFZ::fzNorm &&
                   static_cast<ZNormFunction>(fZ) == ZNormFunction::pop);

    Pop = fx_pop || fz_pop;

    // Special case: both fxPop and fzPop specified
    DoublePop = fx_pop && fz_pop;

    Push = (fX == XFunction::push) ||
           (fSfY == FunctionSelectFY::fyNorm && static_cast<YNormFunction>(fY) == YNormFunction::push) ||
           (fSfZ == FunctionSelectFZ::fzNorm && static_cast<ZNormFunction>(fZ) == ZNormFunction::push);

    StackOperation = Pop || Push;

    // Precompute stack test type
    if (fx_pop && fz_pop && Push) {
        StackTest = StackTestType::Underflow2;
    } else if (Push && fz_pop) {
        StackTest = StackTestType::Overflow;
    } else if (fx_pop && Push) {
        StackTest = StackTestType::Underflow;
    } else {
        StackTest = StackTestType::None;
    }
}

std::string Microinstruction::to_string() const {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "rA=%x rB=%x aS=%d aF=%d aD=%d ep=%d Cin=%d enSU=%d mem=%d fSY=%d fSZ=%d fX=%d fY=%x fZ=%x INIA=%03x",
        rA, rB, static_cast<int>(aS), static_cast<int>(aF), aD,
        ep ? 1 : 0, Cin ? 1 : 0, enSU ? 1 : 0, mem ? 1 : 0,
        static_cast<int>(fSfY), static_cast<int>(fSfZ),
        static_cast<int>(fX), fY, fZ, INIA);
    return std::string(buf);
}

std::string Microinstruction::get_carry_mod() const {
    bool add = (aF == AluFunction::RplusS);
    bool sub = (aF == AluFunction::RminusS || aF == AluFunction::SminusR);

    if (Cin && add) {
        return "+1";
    } else if (!Cin && sub) {
        return "-1";
    }
    return "";
}

std::string Microinstruction::disassemble_xbus_source(int cycle) const {
    std::string xBus;
    char buf[64];

    if (fSfY == FunctionSelectFY::Byte) {
        std::snprintf(buf, sizeof(buf), "byte(%02x)", (fY << 4) | fZ);
        xBus = buf;
    }

    if (fSfZ == FunctionSelectFZ::Nibble && fSfY != FunctionSelectFY::Byte) {
        std::snprintf(buf, sizeof(buf), "nibble(%x)", fZ);
        xBus = buf;
    } else if (fSfZ == FunctionSelectFZ::IOXIn) {
        switch (static_cast<ZIOXIn>(fZ)) {
            case ZIOXIn::ReadEIdata:      xBus += "EIData"; break;
            case ZIOXIn::ReadEStatus:     xBus += "EStatus"; break;
            case ZIOXIn::ReadKIData:      xBus += "KIData"; break;
            case ZIOXIn::ReadKStatus:     xBus += "KStatus"; break;
            case ZIOXIn::ReadMStatus:     xBus += "MStatus"; break;
            case ZIOXIn::ReadKTest:       xBus += "KTest"; break;
            case ZIOXIn::ReadIOPIData:    xBus += "IOPIData"; break;
            case ZIOXIn::ReadIOPStatus:   xBus += "IOPStatus"; break;
            case ZIOXIn::ReadErrnIBnStkp: xBus += "ErrnIBnStkP"; break;
            case ZIOXIn::ReadRH:
                std::snprintf(buf, sizeof(buf), "RH%x", rB);
                xBus += buf;
                break;
            case ZIOXIn::ReadibNA:   xBus += "ibNA"; break;
            case ZIOXIn::ReadibLow:  xBus += "ibLow"; break;
            case ZIOXIn::ReadibHigh: xBus += "ibHigh"; break;
            default:
                std::snprintf(buf, sizeof(buf), "ZIOXIn(%d)", fZ);
                xBus += buf;
                break;
        }
    }

    if (enSU && !Cin) {
        switch (static_cast<int>(fSfZ)) {
            case 0:
            case 1:
                xBus += "STK";
                break;
            case 2:
            case 3:
                std::snprintf(buf, sizeof(buf), "U%02x", (rA << 4) | fZ);
                xBus += buf;
                break;
        }
    }

    if (mem && cycle == 3) {
        xBus += "<-MD";
    }

    return xBus;
}

std::string Microinstruction::disassemble(int cycle) const {
    // Build ALU op
    std::string alu_r, alu_s;
    bool r_zero = false;
    bool s_zero = false;
    char buf[128];

    std::string x_bus_value = disassemble_xbus_source(cycle);

    switch (aS) {
        case AluSourcePair::AB:
            std::snprintf(buf, sizeof(buf), "R%x", rA); alu_r = buf;
            std::snprintf(buf, sizeof(buf), "R%x", rB); alu_s = buf;
            break;
        case AluSourcePair::AQ:
            std::snprintf(buf, sizeof(buf), "R%x", rA); alu_r = buf;
            alu_s = "Q";
            break;
        case AluSourcePair::ZA:
            alu_r = "0"; r_zero = true;
            std::snprintf(buf, sizeof(buf), "R%x", rA); alu_s = buf;
            break;
        case AluSourcePair::ZB:
            alu_r = "0"; r_zero = true;
            std::snprintf(buf, sizeof(buf), "R%x", rB); alu_s = buf;
            break;
        case AluSourcePair::ZQ:
            alu_r = "0"; r_zero = true;
            alu_s = "Q";
            break;
        case AluSourcePair::D0:
            alu_r = x_bus_value;
            alu_s = "0"; s_zero = true;
            break;
        case AluSourcePair::DA:
            alu_r = x_bus_value;
            std::snprintf(buf, sizeof(buf), "R%x", rA); alu_s = buf;
            break;
        case AluSourcePair::DQ:
            alu_r = x_bus_value;
            alu_s = "Q";
            break;
    }

    // Select operation
    std::string alu_op;
    switch (aF) {
        case AluFunction::RplusS:
            if (r_zero) alu_op = alu_s;
            else if (s_zero) alu_op = alu_r;
            else alu_op = alu_r + "+" + alu_s;
            break;
        case AluFunction::SminusR:
            if (r_zero) alu_op = alu_s;
            else if (s_zero) alu_op = "-" + alu_r;
            else alu_op = alu_s + "-" + alu_r;
            break;
        case AluFunction::RminusS:
            if (r_zero) alu_op = "-" + alu_s;
            else if (s_zero) alu_op = alu_r;
            else alu_op = alu_r + "-" + alu_s;
            break;
        case AluFunction::RorS:
            if (r_zero) alu_op = alu_s;
            else if (s_zero) alu_op = alu_r;
            else alu_op = alu_r + " or " + alu_s;
            break;
        case AluFunction::RandS:
            if (r_zero || s_zero) alu_op = "0";
            else alu_op = alu_r + " and " + alu_s;
            break;
        case AluFunction::notRandS:
            if (r_zero) alu_op = alu_s;
            else if (s_zero) alu_op = "0";
            else alu_op = "~" + alu_r + " and " + alu_s;
            break;
        case AluFunction::RxorS:
            if (r_zero) alu_op = alu_s;
            else if (s_zero) alu_op = alu_r;
            else alu_op = alu_r + " xor " + alu_s;
            break;
        case AluFunction::notRxorS:
            if (s_zero) alu_op = "~" + alu_r;
            else alu_op = "~" + alu_r + " xor " + alu_s;
            break;
    }

    // Select register writeback
    int write_fn = aD | (Shift ? 0x4 : 0x0);
    std::string reg_assignment;
    bool a_bypass = false;
    bool y_bus_is_source = false;
    bool alu_no_writeback = false;
    std::string carry_mod = get_carry_mod();

    switch (write_fn) {
        case 0:
            reg_assignment = "Q<- " + alu_op + carry_mod;
            y_bus_is_source = true;
            break;
        case 1:
            reg_assignment = alu_op + carry_mod;
            alu_no_writeback = true;
            break;
        case 2:
            std::snprintf(buf, sizeof(buf), "R%x<- ", rB);
            reg_assignment = std::string(buf) + alu_op + carry_mod;
            a_bypass = true;
            y_bus_is_source = true;
            break;
        case 3:
            std::snprintf(buf, sizeof(buf), "R%x<- ", rB);
            reg_assignment = std::string(buf) + alu_op + carry_mod;
            y_bus_is_source = true;
            break;
        case 4:
            std::snprintf(buf, sizeof(buf), "R%x<- %s ", rB, Cycle ? "DRShift1" : "DARShift1");
            reg_assignment = std::string(buf) + alu_op + carry_mod + (Cin ? " SE<-1" : "");
            y_bus_is_source = true;
            break;
        case 5:
            std::snprintf(buf, sizeof(buf), "R%x<- %s ", rB, Cycle ? "RRot1" : "RShift1");
            reg_assignment = std::string(buf) + alu_op + carry_mod + (!Cycle && Cin ? " SE<-1" : "");
            y_bus_is_source = true;
            break;
        case 6:
            std::snprintf(buf, sizeof(buf), "R%x<- %s ", rB, Cycle ? "DLShift1" : "DALShift1");
            reg_assignment = std::string(buf) + alu_op + carry_mod + (Cin ? " SE<-1" : "");
            y_bus_is_source = true;
            break;
        case 7:
            std::snprintf(buf, sizeof(buf), "R%x<- %s ", rB, Cycle ? "LRot1" : "LShift1");
            reg_assignment = std::string(buf) + alu_op + carry_mod + (!Cycle && Cin ? " SE<-1" : "");
            y_bus_is_source = true;
            break;
    }

    std::string y_bus_value;
    if (a_bypass) {
        std::snprintf(buf, sizeof(buf), "R%x, ", rA);
        y_bus_value = std::string(buf) + reg_assignment;
    } else {
        y_bus_value = reg_assignment;
    }

    std::string fx_func;
    bool x_bus_is_source = false;
    bool y_bus_branch = false;
    bool x_bus_branch = false;

    switch (fX) {
        case XFunction::pCallRet0: case XFunction::pCallRet1:
        case XFunction::pCallRet2: case XFunction::pCallRet3:
        case XFunction::pCallRet4: case XFunction::pCallRet5:
        case XFunction::pCallRet6: case XFunction::pCallRet7:
            std::snprintf(buf, sizeof(buf), "pCall/Ret%d ", static_cast<int>(fX));
            fx_func = buf;
            break;
        case XFunction::LoadRH:
            std::snprintf(buf, sizeof(buf), "RH%x<-", rB);
            fx_func = buf;
            x_bus_is_source = true;
            break;
        case XFunction::LoadCinFrompc16:
            fx_func = "SE<-pc16 ";
            break;
        case XFunction::LoadMap:
            std::snprintf(buf, sizeof(buf), "Map<- RH%x,,", rB);
            fx_func = buf;
            y_bus_is_source = true;
            break;
        case XFunction::pop:
            fx_func = "pop ";
            break;
        case XFunction::push:
            fx_func = "push ";
            break;
        default:
            break;
    }

    std::string fy_func;
    switch (fSfY) {
        case FunctionSelectFY::fyNorm:
            switch (static_cast<YNormFunction>(fY)) {
                case YNormFunction::ExitKern:   fy_func = "ExitKern "; break;
                case YNormFunction::EnterKern:  fy_func = "EnterKern "; break;
                case YNormFunction::ClrIntErr:  fy_func = "ClrIntErr "; break;
                case YNormFunction::IBDisp:     fy_func = "IBDisp "; break;
                case YNormFunction::MesaIntRq:  fy_func = "MesaIntRq "; break;
                case YNormFunction::LoadstackP:
                    fy_func = "stackP<-";
                    y_bus_is_source = true;
                    break;
                case YNormFunction::LoadIB:
                    fy_func = "IB<-";
                    x_bus_is_source = true;
                    break;
                case YNormFunction::LoadMap:
                    std::snprintf(buf, sizeof(buf), "Map<- RH%x,,", rB);
                    fy_func = buf;
                    break;
                case YNormFunction::Refresh:  fy_func = "Refresh "; break;
                case YNormFunction::push:     fy_func = "push "; break;
                case YNormFunction::ClrDPRq:  fy_func = "ClrDPRq "; break;
                case YNormFunction::ClrIOPRq: fy_func = "ClrIOPRq "; break;
                case YNormFunction::ClrRefRq: fy_func = "ClrRefRq "; break;
                case YNormFunction::ClrKFlags: fy_func = "ClrKFlags "; break;
                default: break;
            }
            break;

        case FunctionSelectFY::DispBr: {
            auto dbf = static_cast<YDispBrFunction>(fY);
            // Simple name output
            switch (dbf) {
                case YDispBrFunction::NegBr:      fy_func = "NegBr "; break;
                case YDispBrFunction::ZeroBr:     fy_func = "ZeroBr "; break;
                case YDispBrFunction::NZeroBr:    fy_func = "NZeroBr "; break;
                case YDispBrFunction::MesaIntBr:  fy_func = "MesaIntBr "; break;
                case YDispBrFunction::PgCarryBr:  fy_func = "PgCarryBr "; break;
                case YDispBrFunction::CarryBr:    fy_func = "CarryBr "; break;
                case YDispBrFunction::XRefBr:     fy_func = "XRefBr "; break;
                case YDispBrFunction::NibCarryBr: fy_func = "NibCarryBr "; break;
                case YDispBrFunction::XDisp:      fy_func = "XDisp "; break;
                case YDispBrFunction::YDisp:      fy_func = "YDisp "; break;
                case YDispBrFunction::XC2npcDisp: fy_func = "XC2npcDisp "; break;
                case YDispBrFunction::YIODisp:    fy_func = "YIODisp "; break;
                case YDispBrFunction::XwdDisp:    fy_func = "XwdDisp "; break;
                case YDispBrFunction::XHDisp:     fy_func = "XHDisp "; break;
                case YDispBrFunction::XLDisp:     fy_func = "XLDisp "; break;
                case YDispBrFunction::PgCrOvDisp: fy_func = "PgCrOvDisp "; break;
            }
            switch (dbf) {
                case YDispBrFunction::NegBr:
                case YDispBrFunction::ZeroBr:
                case YDispBrFunction::NibCarryBr:
                case YDispBrFunction::PgCarryBr:
                case YDispBrFunction::CarryBr:
                case YDispBrFunction::PgCrOvDisp:
                case YDispBrFunction::YDisp:
                case YDispBrFunction::YIODisp:
                    y_bus_branch = true;
                    break;
                case YDispBrFunction::XRefBr:
                case YDispBrFunction::XwdDisp:
                case YDispBrFunction::XHDisp:
                case YDispBrFunction::XLDisp:
                case YDispBrFunction::XDisp:
                case YDispBrFunction::XC2npcDisp:
                    x_bus_branch = true;
                    break;
                default:
                    break;
            }
            break;
        }

        case FunctionSelectFY::IOOut:
            if (fY != 0xb && fY != 0xe) {
                auto yio = static_cast<YIOOutFunction>(fY);
                switch (yio) {
                    case YIOOutFunction::IOPOData: fy_func = "IOPOData<-"; break;
                    case YIOOutFunction::IOPCtl:   fy_func = "IOPCtl<-"; break;
                    case YIOOutFunction::KOData:   fy_func = "KOData<-"; break;
                    case YIOOutFunction::KCtl:     fy_func = "KCtl<-"; break;
                    case YIOOutFunction::EOData:   fy_func = "EOData<-"; break;
                    case YIOOutFunction::EICtl:    fy_func = "EICtl<-"; break;
                    case YIOOutFunction::DCtlFifo: fy_func = "DCtlFifo<-"; break;
                    case YIOOutFunction::DCtl:     fy_func = "DCtl<-"; break;
                    case YIOOutFunction::DBorder:  fy_func = "DBorder<-"; break;
                    case YIOOutFunction::PCtl:     fy_func = "PCtl<-"; break;
                    case YIOOutFunction::MCtl:     fy_func = "MCtl<-"; break;
                    case YIOOutFunction::EOCtl:    fy_func = "EOCtl<-"; break;
                    case YIOOutFunction::KCmd:     fy_func = "KCmd<-"; break;
                    case YIOOutFunction::POData:   fy_func = "POData<-"; break;
                    default: break;
                }
                x_bus_is_source =
                    (yio == YIOOutFunction::IOPOData ||
                     yio == YIOOutFunction::IOPCtl ||
                     yio == YIOOutFunction::KOData ||
                     yio == YIOOutFunction::KCtl ||
                     yio == YIOOutFunction::EOData ||
                     yio == YIOOutFunction::EICtl ||
                     yio == YIOOutFunction::DCtl ||
                     yio == YIOOutFunction::PCtl ||
                     yio == YIOOutFunction::EOCtl ||
                     yio == YIOOutFunction::KCmd ||
                     yio == YIOOutFunction::POData);
                y_bus_is_source = (!x_bus_is_source && (fY != 0xb && fY != 0xe));
            }
            break;

        default:
            break;
    }

    std::string fz_func;
    if (fSfZ == FunctionSelectFZ::fzNorm) {
        switch (static_cast<ZNormFunction>(fZ)) {
            case ZNormFunction::Refresh:     fz_func = "Refresh "; break;
            case ZNormFunction::LoadIBPtr1:  fz_func = "IBPtr<-1 "; break;
            case ZNormFunction::LoadIBPtr0:  fz_func = "IBPtr<-0 "; break;
            case ZNormFunction::LoadCinFrompc16: fz_func = "SE<-pc16 "; break;
            case ZNormFunction::pop:         fz_func = "pop "; break;
            case ZNormFunction::push:        fz_func = "push "; break;
            case ZNormFunction::AltUaddr:    fz_func = "AltUaddr "; break;
            case ZNormFunction::LRot0:       fz_func = "LRot0 "; break;
            case ZNormFunction::LRot12:      fz_func = "LRot12 "; break;
            case ZNormFunction::LRot8:       fz_func = "LRot8 "; break;
            case ZNormFunction::LRot4:       fz_func = "LRot4 "; break;
            default: break;
        }
    }

    // SU reg write
    std::string su_write_str;
    bool su_write = enSU && Cin;
    if (su_write) {
        switch (static_cast<int>(fSfZ)) {
            case 0:
            case 1:
                su_write_str = "STK<-";
                break;
            case 2:
            case 3:
                std::snprintf(buf, sizeof(buf), "U%02x<-", (rA << 4) | fZ);
                su_write_str = buf;
                break;
        }
        y_bus_is_source = true;
    }

    if (!y_bus_is_source && !x_bus_is_source) {
        su_write_str = "Xbus<- ";
        y_bus_is_source = x_bus_value.empty();
    }

    // MAR or MDR writes
    std::string mem_write;
    if (mem) {
        if (cycle == 1)       mem_write = "MAR<- ";
        else if (cycle == 2)  mem_write = "MDR<- ";
        else if (cycle == -1) mem_write = "{MAR/MDR/MD} ";
        y_bus_is_source = true;
    }

    bool show_y = (y_bus_branch || y_bus_is_source || !alu_no_writeback);
    bool show_x = (x_bus_branch || !AluNeedsXBus || x_bus_is_source);

    std::snprintf(buf, sizeof(buf), " [%03x]", INIA);
    std::string result = fx_func + fy_func + fz_func + mem_write + su_write_str +
                         (show_x ? x_bus_value : "") +
                         (show_y ? y_bus_value : "") +
                         buf;
    return result;
}

} // namespace darkstar
