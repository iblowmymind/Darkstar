/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#include "cp/central_processor.h"
#include "core/log.h"
#include "core/scheduler.h"
#include "core/system.h"
#include "display/display_controller.h"
#include "ethernet/ethernet_controller.h"
#include "io/shugart_controller.h"
#include "memory/memory_controller.h"

#include <stdexcept>

namespace darkstar {

CentralProcessor::CentralProcessor(DSystem& system)
    : system_(system)
{
    reset();
}

void CentralProcessor::reset() {
    current_task_ = TaskType::Kernel;
    cycle_ = 1;
    click_ = ClickType::Ethernet0;
    iop_wait_ = true;

    m_int_ = false;
    ek_err_ = 0;
    emulator_error_trap_ = false;
    emulator_error_trap_click_count_ = 0;

    std::memset(microcode_, 0, sizeof(microcode_));
    std::memset(tpc_, 0, sizeof(tpc_));
    std::memset(tc_, 0, sizeof(tc_));
    std::memset(wakeup_, 0, sizeof(wakeup_));
    std::memset(rh_, 0, sizeof(rh_));
    std::memset(u_, 0, sizeof(u_));
    std::memset(link_, 0, sizeof(link_));
    std::memset(ib_, 0, sizeof(ib_));

    for (int i = 0; i < 4096; i++) {
        microcode_cache_[i] = Microinstruction();
    }

    tpc_addr_ = 0;
    stack_p_ = 0;
    ib_ptr_ = IBState::Empty;
    ib_front_ = 0;
    ib_empty_cancel_ = false;
    pc16_ = false;
    nia_modifier_ = 0;
    nia_modifier_type_ = NiaModifierType::Normal;
    mar_page_cross_br_ = false;
    alt_u_addr_ = false;

    sw_t_addr_ = false;
    iop_attn_ = false;
    cp_dma_mode_ = false;
    cp_dma_in_ = false;
    cp_dma_complete_ = false;
    wake_mode0_ = false;
    wake_mode1_ = false;
    cp_attn_ = false;
    emu_wake_ = false;
    cp_out_int_req_ = true;
    cp_in_int_req_ = true;
    iop_req_ = false;
    in_latched_ = false;
    out_latched_ = false;

    exit_kernel_ = false;
}

void CentralProcessor::execute_instruction(int cycles) {
    for (int c = 0; c < cycles; c++) {
        system_.scheduler().clock();

        if (iop_wait_) {
            continue;
        }

        Microinstruction& instruction = microcode_cache_[tpc_[static_cast<int>(current_task_)]];

        bool c_in = instruction.Cin;
        bool invert_pc16 = false;

        bool page_cross_cancel = mar_page_cross_br_;
        mar_page_cross_br_ = false;

        int nia_modifier = nia_modifier_;
        nia_modifier_ = 0;

        bool alt_u_addr = alt_u_addr_;
        alt_u_addr_ = false;

        uint16_t last_y_bus = y_bus_;

        bool ib_empty_cancel = ib_empty_cancel_;
        ib_empty_cancel_ = false;

        NiaModifierType nia_modifier_type = nia_modifier_type_;
        nia_modifier_type_ = NiaModifierType::Normal;

        // Byte/Nibble constants
        x_bus_ = instruction.Byte;

        // Decode XBus sources (fSfZ)
        switch (instruction.fSfZ) {
            case FunctionSelectFZ::fzNorm:
                switch (static_cast<ZNormFunction>(instruction.fZ)) {
                    case ZNormFunction::LoadIBPtr1:
                        if (instruction.AlwaysIBDisp || instruction.LoadIB) {
                            // Modified later
                        } else {
                            if (ib_ptr_ != IBState::Byte) {
                                ib_ptr_ = IBState::Byte;
                                ib_front_ = ib_[1];
                            }
                        }
                        break;

                    case ZNormFunction::LoadIBPtr0:
                        if (ib_ptr_ != IBState::Word) {
                            ib_ptr_ = IBState::Word;
                            ib_front_ = ib_[0];
                        }
                        break;

                    case ZNormFunction::LoadCinFrompc16:
                        if (c_in) {
                            c_in = pc16_;
                        }
                        invert_pc16 = true;
                        break;

                    case ZNormFunction::AltUaddr:
                        alt_u_addr_ = true;
                        break;

                    case ZNormFunction::LRot0:
                        if (instruction.ABypass) {
                            x_bus_ = alu_.r()[instruction.rA];
                        }
                        break;

                    case ZNormFunction::LRot12:
                        if (instruction.ABypass) {
                            x_bus_ = static_cast<uint16_t>((alu_.r()[instruction.rA] << 12) | (alu_.r()[instruction.rA] >> 4));
                        }
                        break;

                    case ZNormFunction::LRot8:
                        if (instruction.ABypass) {
                            x_bus_ = static_cast<uint16_t>((alu_.r()[instruction.rA] << 8) | (alu_.r()[instruction.rA] >> 8));
                        }
                        break;

                    case ZNormFunction::LRot4:
                        if (instruction.ABypass) {
                            x_bus_ = static_cast<uint16_t>((alu_.r()[instruction.rA] << 4) | (alu_.r()[instruction.rA] >> 12));
                        }
                        break;

                    default:
                        break;
                }
                break;

            case FunctionSelectFZ::IOXIn:
                switch (static_cast<ZIOXIn>(instruction.fZ)) {
                    case ZIOXIn::ReadEIdata:
                        x_bus_ = system_.ethernet_controller().ei_data(cycle_);
                        break;
                    case ZIOXIn::ReadEStatus:
                        x_bus_ = system_.ethernet_controller().e_status();
                        break;
                    case ZIOXIn::ReadKIData:
                        x_bus_ = system_.shugart_controller().read_k_i_data();
                        break;
                    case ZIOXIn::ReadKStatus:
                        x_bus_ = system_.shugart_controller().read_k_status();
                        break;
                    case ZIOXIn::KStrobe:
                        system_.shugart_controller().k_strobe();
                        break;
                    case ZIOXIn::ReadMStatus:
                        x_bus_ = system_.memory_controller().m_status();
                        break;
                    case ZIOXIn::ReadKTest:
                        x_bus_ = system_.shugart_controller().read_k_test();
                        break;
                    case ZIOXIn::EStrobe:
                        system_.ethernet_controller().e_strobe(cycle_);
                        break;
                    case ZIOXIn::ReadIOPIData:
                        x_bus_ = read_iop_data();
                        break;
                    case ZIOXIn::ReadIOPStatus:
                        x_bus_ = read_iop_status();
                        break;
                    case ZIOXIn::ReadErrnIBnStkp:
                        x_bus_ = static_cast<uint16_t>(
                            (ek_err_ << 6) |
                            ((~static_cast<int>(ib_ptr_) & 0x3) << 4) |
                            (~stack_p_ & 0xf));
                        break;
                    case ZIOXIn::ReadRH:
                        x_bus_ = rh_[instruction.rB];
                        break;
                    case ZIOXIn::ReadibNA:
                        if (ib_ptr_ == IBState::Empty) {
                            signal_error_trap(ErrorTrap::IBEmpty);
                            ib_empty_cancel_ = cycle_ == 1;
                        } else {
                            x_bus_ = ib_front_;
                        }
                        break;
                    case ZIOXIn::Readib:
                        if (ib_ptr_ == IBState::Empty) {
                            signal_error_trap(ErrorTrap::IBEmpty);
                            ib_empty_cancel_ = cycle_ == 1;
                        } else {
                            x_bus_ = ib_front_;
                            ib_front_ = ib_[static_cast<int>(ib_ptr_) & 0x1];
                            decrement_ib_ptr();
                        }
                        break;
                    case ZIOXIn::ReadibLow:
                        if (ib_ptr_ == IBState::Empty) {
                            signal_error_trap(ErrorTrap::IBEmpty);
                            ib_empty_cancel_ = cycle_ == 1;
                        } else {
                            x_bus_ = static_cast<uint16_t>(ib_front_ & 0xf);
                        }
                        break;
                    case ZIOXIn::ReadibHigh:
                        if (ib_ptr_ == IBState::Empty) {
                            signal_error_trap(ErrorTrap::IBEmpty);
                            ib_empty_cancel_ = cycle_ == 1;
                        } else {
                            x_bus_ = static_cast<uint16_t>(ib_front_ >> 4);
                        }
                        break;
                }
                break;

            default:
                break;
        }

        // fX Cin<-pc16
        if (instruction.fX == XFunction::LoadCinFrompc16) {
            c_in = pc16_;
            invert_pc16 = true;
        }

        // SU read
        if (instruction.SURead) {
            switch (static_cast<int>(instruction.fSfZ)) {
                case 0:
                case 1:
                    x_bus_ = u_[stack_p_];
                    break;
                case 2:
                case 3:
                    if (alt_u_addr) {
                        x_bus_ = u_[(instruction.rA << 4) | (last_y_bus & 0xf)];
                    } else {
                        x_bus_ = u_[instruction.UAddress];
                    }
                    break;
            }
        }

        // <-MD in C3
        if (instruction.mem && cycle_ == 3) {
            bool valid = false;
            x_bus_ = system_.memory_controller().read_md(current_task_, valid);
            if (!valid) {
                signal_error_trap(ErrorTrap::EmulatorMemoryError);
            }
        }

        // ALU execution
        y_bus_ = alu_.execute(instruction, x_bus_, c_in, (instruction.mem && cycle_ == 1));

        // Handle MAR<-, Map<-, MDR<-
        if (instruction.MarMapMDR) {
            switch (cycle_) {
                case 1:
                    if (instruction.LoadMap) {
                        int map_addr = 0x10000 + ((rh_[instruction.rB] << 16 | y_bus_) >> 8);
                        system_.memory_controller().load_mar(map_addr);
                    } else {
                        system_.memory_controller().load_mar(((rh_[instruction.rB] & 0xf) << 16) | y_bus_);
                    }
                    if (instruction.mem && (alu_.pg_carry ^ ((static_cast<int>(instruction.aF) & 0x1) == 1))) {
                        nia_modifier_ |= 0x2;
                        mar_page_cross_br_ = true;
                    }
                    break;
                case 2:
                    if (!page_cross_cancel && !ib_empty_cancel) {
                        system_.memory_controller().load_mdr(y_bus_);
                    }
                    break;
                case 3:
                    break;
            }
        }

        // Late LRotn
        if (instruction.LateLRotN) {
            switch (static_cast<ZNormFunction>(instruction.fZ)) {
                case ZNormFunction::LRot0:
                    x_bus_ = y_bus_;
                    break;
                case ZNormFunction::LRot12:
                    x_bus_ = static_cast<uint16_t>((y_bus_ << 12) | (y_bus_ >> 4));
                    break;
                case ZNormFunction::LRot8:
                    x_bus_ = static_cast<uint16_t>((y_bus_ << 8) | (y_bus_ >> 8));
                    break;
                case ZNormFunction::LRot4:
                    x_bus_ = static_cast<uint16_t>((y_bus_ << 4) | (y_bus_ >> 12));
                    break;
                default:
                    break;
            }
        }

        // Load RH
        if (instruction.fX == XFunction::LoadRH) {
            rh_[instruction.rB] = static_cast<uint8_t>(x_bus_);
        }

        // fY functions
        switch (instruction.fSfY) {
            case FunctionSelectFY::fyNorm:
                switch (static_cast<YNormFunction>(instruction.fY)) {
                    case YNormFunction::ExitKern:
                        if (cycle_ == 1) {
                            exit_kernel_ = true;
                        }
                        break;
                    case YNormFunction::ClrIntErr:
                        m_int_ = false;
                        ek_err_ = 0;
                        break;
                    case YNormFunction::IBDisp:
                        if (!page_cross_cancel) {
                            if ((ib_ptr_ != IBState::Full || m_int_) && !instruction.AlwaysIBDisp) {
                                if (m_int_) {
                                    nia_modifier_ |= (ib_ptr_ == IBState::Empty || ib_ptr_ == IBState::Full) ? 0x600 : 0x700;
                                } else {
                                    nia_modifier_ |= (ib_ptr_ == IBState::Empty) ? 0x400 : 0x500;
                                }
                                nia_modifier_type_ = NiaModifierType::IBRefillTrap;
                            } else {
                                nia_modifier_ |= ib_front_;
                                nia_modifier_type_ = NiaModifierType::IBDispatch;
                                ib_front_ = ib_[static_cast<int>(ib_ptr_) & 0x1];
                                decrement_ib_ptr();
                            }
                        }
                        break;
                    case YNormFunction::MesaIntRq:
                        m_int_ = true;
                        break;
                    case YNormFunction::LoadIB:
                        if (instruction.LoadIBPtr1) {
                            if (ib_ptr_ != IBState::Empty) {
                                ib_[0] = static_cast<uint8_t>(x_bus_ >> 8);
                                ib_[1] = static_cast<uint8_t>(x_bus_);
                                ib_ptr_ = IBState::Full;
                            } else {
                                ib_ptr_ = IBState::Byte;
                                ib_front_ = static_cast<uint8_t>(x_bus_);
                            }
                        } else {
                            ib_[1] = static_cast<uint8_t>(x_bus_);
                            if (ib_ptr_ != IBState::Empty) {
                                ib_[0] = static_cast<uint8_t>(x_bus_ >> 8);
                                ib_ptr_ = IBState::Full;
                            } else {
                                ib_front_ = static_cast<uint8_t>(x_bus_ >> 8);
                                ib_ptr_ = IBState::Word;
                            }
                        }
                        break;
                    case YNormFunction::ClrDPRq:
                        system_.display_controller().clr_dp_rq();
                        break;
                    case YNormFunction::ClrIOPRq:
                        sleep_task(TaskType::IOP);
                        break;
                    case YNormFunction::ClrRefRq:
                        sleep_task(TaskType::Refresh);
                        break;
                    case YNormFunction::ClrKFlags:
                        system_.shugart_controller().clr_k_flags();
                        break;
                    default:
                        break;
                }
                break;

            case FunctionSelectFY::DispBr:
                switch (static_cast<YDispBrFunction>(instruction.fY)) {
                    case YDispBrFunction::NegBr:
                        if (alu_.neg) nia_modifier_ |= 1;
                        break;
                    case YDispBrFunction::ZeroBr:
                        if (alu_.zero) nia_modifier_ |= 1;
                        break;
                    case YDispBrFunction::NZeroBr:
                        if (!alu_.zero) nia_modifier_ |= 1;
                        break;
                    case YDispBrFunction::MesaIntBr:
                        if (m_int_) nia_modifier_ |= 1;
                        break;
                    case YDispBrFunction::PgCarryBr:
                        if (alu_.pg_carry) nia_modifier_ |= 1;
                        break;
                    case YDispBrFunction::CarryBr:
                        if (alu_.carry_out) nia_modifier_ |= 1;
                        break;
                    case YDispBrFunction::XRefBr:
                        nia_modifier_ |= (x_bus_ & 0x10) >> 4;
                        break;
                    case YDispBrFunction::NibCarryBr:
                        if (alu_.nib_carry) nia_modifier_ |= 1;
                        break;
                    case YDispBrFunction::XDisp:
                        nia_modifier_ |= (x_bus_ & 0xf);
                        break;
                    case YDispBrFunction::YDisp:
                        nia_modifier_ |= (y_bus_ & 0xf);
                        break;
                    case YDispBrFunction::XC2npcDisp:
                        nia_modifier_ |= (x_bus_ & 0xc) | (cycle_ == 2 ? 0x2 : 0x0) | (pc16_ ? 0x0 : 0x1);
                        break;
                    case YDispBrFunction::YIODisp:
                        nia_modifier_ |= (y_bus_ & 0xc) | (system_.ethernet_controller().ether_disp());
                        break;
                    case YDispBrFunction::XwdDisp:
                        nia_modifier_ |= (x_bus_ & 0x60) >> 5;
                        break;
                    case YDispBrFunction::XHDisp:
                        nia_modifier_ |= ((x_bus_ & 0x8000) >> 15) | ((x_bus_ & 0x0800) >> 10);
                        break;
                    case YDispBrFunction::XLDisp:
                        nia_modifier_ |= (x_bus_ & 0x1) | ((x_bus_ & 0x80) >> 6);
                        break;
                    case YDispBrFunction::PgCrOvDisp:
                        nia_modifier_ |= (alu_.pg_carry ^ ((static_cast<int>(instruction.aF) & 0x1) == 1) ? 0x2 : 0x0) | (alu_.overflow ? 0x1 : 0x0);
                        break;
                }
                break;

            case FunctionSelectFY::IOOut:
                switch (static_cast<YIOOutFunction>(instruction.fY)) {
                    case YIOOutFunction::IOPOData:
                        write_iop_data(static_cast<uint8_t>(x_bus_));
                        break;
                    case YIOOutFunction::IOPCtl:
                        write_iop_ctl(static_cast<uint8_t>(x_bus_));
                        break;
                    case YIOOutFunction::KOData:
                        system_.shugart_controller().set_k_o_data(x_bus_);
                        break;
                    case YIOOutFunction::KCtl:
                        system_.shugart_controller().set_k_ctl(x_bus_);
                        break;
                    case YIOOutFunction::EOData:
                        system_.ethernet_controller().eo_data(x_bus_);
                        break;
                    case YIOOutFunction::EICtl:
                        system_.ethernet_controller().ei_ctl(x_bus_);
                        break;
                    case YIOOutFunction::DCtlFifo:
                        system_.display_controller().set_d_ctl_fifo(y_bus_);
                        break;
                    case YIOOutFunction::DCtl:
                        system_.display_controller().set_d_ctl(x_bus_);
                        break;
                    case YIOOutFunction::DBorder:
                        system_.display_controller().set_d_border(y_bus_);
                        break;
                    case YIOOutFunction::PCtl:
                        if ((x_bus_ & 0x1) != 0) {
                            wake_task(TaskType::Refresh);
                        } else {
                            sleep_task(TaskType::Refresh);
                        }
                        break;
                    case YIOOutFunction::MCtl:
                        system_.memory_controller().set_m_ctl(y_bus_);
                        break;
                    case YIOOutFunction::EOCtl:
                        system_.ethernet_controller().eo_ctl(x_bus_);
                        break;
                    case YIOOutFunction::KCmd:
                        system_.shugart_controller().set_k_cmd(x_bus_);
                        break;
                    case YIOOutFunction::Invalid0:
                    case YIOOutFunction::Invalid1:
                        break;
                    default:
                        break;
                }
                break;

            default:
                break;
        }

        // SU reg write
        if (instruction.SUWrite) {
            switch (static_cast<int>(instruction.fSfZ)) {
                case 0:
                case 1:
                    u_[stack_p_] = y_bus_;
                    break;
                case 2:
                case 3:
                    if (alt_u_addr) {
                        u_[(instruction.rA << 4) | (last_y_bus & 0xf)] = y_bus_;
                    } else {
                        u_[instruction.UAddress] = y_bus_;
                    }
                    break;
            }
        }

        // pc16 inversion
        if (invert_pc16) {
            pc16_ = !pc16_;
        }

        // Stack modifications
        if (instruction.LoadStackP) {
            stack_p_ = (y_bus_ & 0xf);
        }

        if (instruction.StackOperation) {
            switch (instruction.StackTest) {
                case StackTestType::None:
                    if (instruction.Push) {
                        if (stack_p_ == 0xf) {
                            signal_error_trap(ErrorTrap::StackOverUnderflow);
                        }
                        stack_p_ = (stack_p_ + 1) & 0xf;
                    } else if (instruction.DoublePop) {
                        if (stack_p_ < 2) {
                            signal_error_trap(ErrorTrap::StackOverUnderflow);
                        }
                        stack_p_ = (stack_p_ - 1) & 0xf;
                    } else if (instruction.Pop) {
                        if (stack_p_ == 0) {
                            signal_error_trap(ErrorTrap::StackOverUnderflow);
                        }
                        stack_p_ = (stack_p_ - 1) & 0xf;
                    }
                    break;
                case StackTestType::Underflow:
                    if (stack_p_ == 0x0) {
                        signal_error_trap(ErrorTrap::StackOverUnderflow);
                    }
                    break;
                case StackTestType::Overflow:
                    if (stack_p_ == 0xf) {
                        signal_error_trap(ErrorTrap::StackOverUnderflow);
                    }
                    break;
                case StackTestType::Underflow2:
                    if (stack_p_ < 2) {
                        signal_error_trap(ErrorTrap::StackOverUnderflow);
                    }
                    break;
            }
        }

        // Calculate NIA
        int nia = instruction.INIA;

        if (ek_err_ > 0 && nia_modifier_type == NiaModifierType::IBRefillTrap) {
            nia_modifier_type = NiaModifierType::Normal;
            nia_modifier = 0;
        }

        switch (nia_modifier_type) {
            case NiaModifierType::Normal:
                nia |= nia_modifier;
                break;
            case NiaModifierType::IBDispatch:
                nia = (nia & 0xf0f) | nia_modifier;
                ib_dispatch_ = true;
                break;
            case NiaModifierType::IBRefillTrap:
                nia = (nia & 0x0ff) | nia_modifier;
                break;
        }

        tpc_[static_cast<int>(current_task_)] = nia;

        // Link register handling
        if (instruction.LinkAddress != -1) {
            if ((nia & 0x10) == 0) {
                link_[instruction.LinkAddress] = nia & 0xf;
            } else {
                nia_modifier_ |= link_[instruction.LinkAddress];
            }
        }

        cycle_++;
        if (cycle_ > 3) {
            cycle_ = 1;
            task_switch();
        }
    }
}

void CentralProcessor::wake_task(TaskType task) {
    wakeup_[static_cast<int>(task)] = true;
}

void CentralProcessor::sleep_task(TaskType task) {
    wakeup_[static_cast<int>(task)] = false;
}

void CentralProcessor::task_switch() {
    click_ = static_cast<ClickType>((static_cast<int>(click_) + 1) % 5);

    if (exit_kernel_ || current_task_ != TaskType::Kernel) {
        if (exit_kernel_) {
            sleep_task(TaskType::Kernel);
            exit_kernel_ = false;
        }

        switch (click_) {
            case ClickType::Ethernet0:
            case ClickType::Ethernet1:
                do_task_switch(TaskType::Ethernet);
                break;
            case ClickType::Disk:
                do_task_switch(TaskType::Disk);
                break;
            case ClickType::IOP:
                do_task_switch(TaskType::IOP);
                break;
            case ClickType::Display:
                if (system_.display_controller().display_on()) {
                    do_task_switch(TaskType::Display);
                } else {
                    do_task_switch(TaskType::Refresh);
                }
                break;
        }

        exit_kernel_ = false;
    }

    if (emulator_error_trap_ && current_task_ == TaskType::Emulator) {
        emulator_error_trap_click_count_--;
        if (emulator_error_trap_click_count_ == 0) {
            emulator_error_trap_ = false;
            tpc_[static_cast<int>(current_task_)] = 0;
        }
    }
}

void CentralProcessor::do_task_switch(TaskType new_task) {
    TaskType next_task;

    if (wake_status(TaskType::Kernel)) {
        next_task = TaskType::Kernel;
    } else {
        next_task = wake_status(new_task) ? new_task : TaskType::Emulator;
    }

    if (next_task == current_task_) {
        return;
    }

    tc_[static_cast<int>(current_task_)] = nia_modifier_ & 0xf;
    nia_modifier_ = tc_[static_cast<int>(next_task)];
    current_task_ = next_task;
}

bool CentralProcessor::wake_status(TaskType task) const {
    return wakeup_[static_cast<int>(task)];
}

void CentralProcessor::signal_error_trap(ErrorTrap err) {
    if (static_cast<int>(err) < ek_err_ || (ek_err_ == 0 && !emulator_error_trap_)) {
        ek_err_ = static_cast<int>(err);
    }

    if (!emulator_error_trap_) {
        emulator_error_trap_ = true;
        switch (err) {
            case ErrorTrap::ControlStoreParity:
                emulator_error_trap_click_count_ = cycle_ == 1 ? 1 : 2;
                break;
            case ErrorTrap::EmulatorMemoryError:
                emulator_error_trap_click_count_ = 2;
                break;
            case ErrorTrap::StackOverUnderflow:
                emulator_error_trap_click_count_ = 2;
                break;
            case ErrorTrap::IBEmpty:
                emulator_error_trap_click_count_ = cycle_ == 1 ? 1 : 2;
                break;
        }
    }
}

void CentralProcessor::decrement_ib_ptr() {
    ib_ptr_ = kNextIBPtr[static_cast<int>(ib_ptr_)];
}

} // namespace darkstar
