/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#include "cp/central_processor.h"
#include "core/log.h"

#include <stdexcept>

namespace darkstar {

// IOP port arrays
const int CentralProcessor::kReadPorts[] = {
    0xeb, 0xec, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

const int CentralProcessor::kWritePorts[] = {
    0xeb, 0xec, 0xee, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

const int* CentralProcessor::read_ports() const { return kReadPorts; }
int CentralProcessor::read_port_count() const { return kReadPortCount; }
const int* CentralProcessor::write_ports() const { return kWritePorts; }
int CentralProcessor::write_port_count() const { return kWritePortCount; }

void CentralProcessor::write_port(int port, uint8_t value) {
    switch (static_cast<PortWriteRegister>(port)) {
        case PortWriteRegister::CPDataOut:
            write_cp_in_buffer(value);
            break;
        case PortWriteRegister::CPControl:
            write_cp_ctl(value);
            break;
        case PortWriteRegister::CPClrDmaComplete:
            cp_dma_complete_ = false;
            break;
        case PortWriteRegister::CPCSa:
        case PortWriteRegister::CPCSb:
        case PortWriteRegister::CPCSc:
        case PortWriteRegister::CPCSd:
        case PortWriteRegister::CPCSe:
        case PortWriteRegister::CPCSf:
            write_iop_microcode_word(port - 0xf8, static_cast<uint8_t>(~value));
            break;
        case PortWriteRegister::TPCHigh:
            tpc_addr_ = value >> 5;
            tpc_temp_ = ((~value & 0x1f) << 7);
            break;
        case PortWriteRegister::TPCLow:
            tpc_[tpc_addr_] = tpc_temp_ | (~value & 0x7f);
            break;
        default:
            break;
    }
}

uint8_t CentralProcessor::read_port(int port) {
    uint8_t value = 0;
    switch (static_cast<PortReadRegister>(port)) {
        case PortReadRegister::CPDataIn:
            value = read_cp_out_buffer();
            break;
        case PortReadRegister::CPStatus:
            value = read_cp_status();
            break;
        case PortReadRegister::CPCS0:
        case PortReadRegister::CPCS1:
        case PortReadRegister::CPCS2:
        case PortReadRegister::CPCS3:
        case PortReadRegister::CPCS4:
        case PortReadRegister::CPCS5:
            value = read_iop_microcode_word(port - 0xf8);
            break;
        case PortReadRegister::CPCS6:
            value = static_cast<uint8_t>(~((~tc_[tpc_addr_] << 4) | ((tpc_[tpc_addr_] & 0xf00) >> 8)));
            break;
        case PortReadRegister::CPCS7:
            value = static_cast<uint8_t>(~tpc_[tpc_addr_]);
            break;
        default:
            break;
    }
    return value;
}

// IDMAInterface
bool CentralProcessor::drq() const {
    if (cp_dma_mode_) {
        return cp_dma_in_ ? out_latched_ : !in_latched_;
    }
    return false;
}

uint8_t CentralProcessor::dma_read() {
    return read_cp_out_buffer();
}

void CentralProcessor::dma_write(uint8_t value) {
    write_cp_in_buffer(value);
}

void CentralProcessor::dma_complete() {
    cp_dma_complete_ = true;
}

uint8_t CentralProcessor::read_cp_out_buffer() {
    out_latched_ = false;
    cp_in_int_req_ = true;
    update_iop_task_wakeup();
    return cp_in_data_;
}

void CentralProcessor::write_cp_in_buffer(uint8_t value) {
    cp_out_int_req_ = true;
    cp_out_data_ = value;
    in_latched_ = true;
    update_iop_task_wakeup();
}

void CentralProcessor::write_iop_microcode_word(int b, uint8_t value) {
    uint64_t word = microcode_[tpc_[6]];
    switch (b) {
        case 0:
            word = (word & 0x00ffffffffffULL) | (static_cast<uint64_t>(value) << 40);
            break;
        case 1:
            word = (word & 0xff00ffffffffULL) | (static_cast<uint64_t>(value) << 32);
            break;
        case 2:
            word = (word & 0xffff00ffffffULL) | (static_cast<uint64_t>(value) << 24);
            break;
        case 3: {
            uint64_t fy = (static_cast<uint64_t>(value) & 0xf0) >> 4;
            uint64_t inia = (static_cast<uint64_t>(value) & 0xf);
            word = (word & 0xfffffff0f0ffULL) | (fy << 16) | (inia << 8);
            break;
        }
        case 4: {
            uint64_t fx = (static_cast<uint64_t>(value) & 0xf0) >> 4;
            uint64_t inia = (static_cast<uint64_t>(value) & 0xf);
            word = (word & 0xffffff0fff0fULL) | (fx << 20) | (inia << 4);
            break;
        }
        case 5: {
            uint64_t fz = (static_cast<uint64_t>(value) & 0xf0) >> 4;
            uint64_t inia = (static_cast<uint64_t>(value) & 0xf);
            word = (word & 0xffffffff0ff0ULL) | (fz << 12) | inia;
            break;
        }
    }
    microcode_[tpc_[6]] = word;
    microcode_cache_[tpc_[6]] = Microinstruction(word);
}

uint8_t CentralProcessor::read_iop_microcode_word(int b) {
    uint64_t word = microcode_[tpc_[6]];
    switch (b) {
        case 0: return static_cast<uint8_t>(word >> 40);
        case 1: return static_cast<uint8_t>(word >> 32);
        case 2: return static_cast<uint8_t>(word >> 24);
        case 3: return static_cast<uint8_t>(((word >> 12) & 0xf0) | ((word >> 8) & 0xf));
        case 4: return static_cast<uint8_t>(((word >> 16) & 0xf0) | ((word >> 4) & 0xf));
        case 5: return static_cast<uint8_t>(((word >> 8) & 0xf0) | (word & 0xf));
    }
    return 0;
}

void CentralProcessor::write_cp_ctl(uint8_t value) {
    bool old_iop_wait = iop_wait_;
    iop_wait_ = (value & static_cast<int>(CPControlFlags::IOPWait_)) == 0;
    sw_t_addr_ = (value & static_cast<int>(CPControlFlags::SwTAddr_)) == 0;
    iop_attn_ = (value & static_cast<int>(CPControlFlags::IOPattn)) != 0;
    cp_dma_mode_ = (value & static_cast<int>(CPControlFlags::CPDmaMode)) != 0;
    cp_dma_in_ = (value & static_cast<int>(CPControlFlags::CPDmaIn)) != 0;

    if (old_iop_wait != iop_wait_) {
        for (int i = 0; i < 7; i++) {
            sleep_task(static_cast<TaskType>(i));
        }
        wake_task(TaskType::Kernel);
        current_task_ = TaskType::Kernel;
        emulator_error_trap_ = false;
        emulator_error_trap_click_count_ = 0;
    }
}

uint8_t CentralProcessor::read_cp_status() {
    return static_cast<uint8_t>(
        (cp_dma_complete_ ? static_cast<int>(CPStatusFlags::CPDmaComplete_) : 0) |
        (!cp_out_int_req_ ? static_cast<int>(CPStatusFlags::CPOutIntReq_) : 0) |
        (!cp_in_int_req_ ? static_cast<int>(CPStatusFlags::CPInIntReq_) : 0) |
        (!cp_dma_in_ ? static_cast<int>(CPStatusFlags::CPDmaIn_) : 0) |
        (!cp_dma_mode_ ? static_cast<int>(CPStatusFlags::CPDmaMode_) : 0) |
        (emu_wake_ ? static_cast<int>(CPStatusFlags::EmuWake) : 0) |
        (!cp_attn_ ? static_cast<int>(CPStatusFlags::CPAttn) : 0));
}

void CentralProcessor::write_iop_ctl(uint8_t value) {
    wake_mode1_ = (value & static_cast<int>(IOPCtlFlags::WakeMode1)) != 0;
    wake_mode0_ = (value & static_cast<int>(IOPCtlFlags::WakeMode0)) != 0;
    cp_attn_ = (value & static_cast<int>(IOPCtlFlags::CPAttn)) != 0;
    emu_wake_ = (value & static_cast<int>(IOPCtlFlags::EmuWake)) != 0;

    wake_mode_ = static_cast<IOPTaskWakeMode>((wake_mode0_ ? 0x2 : 0x0) | (wake_mode1_ ? 0x1 : 0x0));
    update_iop_task_wakeup();
}

uint8_t CentralProcessor::read_iop_status() {
    return static_cast<uint8_t>(
        (iop_req_ ? static_cast<int>(IOPStatusFlags::IOPReq) : 0) |
        (!wake_mode1_ ? static_cast<int>(IOPStatusFlags::WakeMode1_) : 0) |
        (!wake_mode0_ ? static_cast<int>(IOPStatusFlags::WakeMode0_) : 0) |
        (!cp_attn_ ? static_cast<int>(IOPStatusFlags::CPAttn_) : 0) |
        (!emu_wake_ ? static_cast<int>(IOPStatusFlags::EmuWake_) : 0) |
        (iop_attn_ ? static_cast<int>(IOPStatusFlags::IOPAttn) : 0));
}

uint8_t CentralProcessor::read_iop_data() {
    in_latched_ = false;
    cp_out_int_req_ = false;
    update_iop_task_wakeup();
    return cp_out_data_;
}

void CentralProcessor::write_iop_data(uint8_t value) {
    out_latched_ = true;
    cp_in_int_req_ = false;
    cp_in_data_ = value;
    update_iop_task_wakeup();
}

void CentralProcessor::update_iop_task_wakeup() {
    switch (wake_mode_) {
        case IOPTaskWakeMode::Always:
            iop_req_ = true;
            wake_task(TaskType::IOP);
            break;
        case IOPTaskWakeMode::Input:
            if (in_latched_) {
                iop_req_ = true;
                wake_task(TaskType::IOP);
            } else {
                iop_req_ = false;
                sleep_task(TaskType::IOP);
            }
            break;
        case IOPTaskWakeMode::Output:
            if (!out_latched_) {
                iop_req_ = true;
                wake_task(TaskType::IOP);
            } else {
                iop_req_ = false;
                sleep_task(TaskType::IOP);
            }
            break;
        case IOPTaskWakeMode::Disabled:
            iop_req_ = false;
            sleep_task(TaskType::IOP);
            break;
    }
}

} // namespace darkstar
