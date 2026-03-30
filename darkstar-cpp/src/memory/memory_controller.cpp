/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/

#include "memory/memory_controller.h"
#include "memory/memory.h"
#include "core/log.h"
#include "cp/task_type.h"

namespace darkstar {

MemoryController::MemoryController()
    : mem_(std::make_unique<Memory>())
{
}

MemoryController::~MemoryController() = default;

void MemoryController::reset() {
    m_status_ = 0;
    mar_ = 0;
    md_ = 0;
    md_valid_ = true;

    mem_->reset();
}

void MemoryController::set_m_ctl(uint16_t value) {
    if (Log::enabled) Log::write(LogType::Verbose, LogComponent::MemoryControl,
        "MCtl<-0x%x", value);

    mem_->set_check_bits((value & 0xff) >> 2);

    if ((value & 0x800) != 0) {
        // Clear error log for the task specified by bits [5..7].
        int task = (value & 0x700) >> 8;
        m_status_ &= static_cast<uint16_t>(~(0x80 >> task));
    }
}

void MemoryController::load_mar(int address) {
    mar_ = address;

    // Pre-load the memory requested.
    md_ = mem_->read_word(mar_, md_valid_);

    if (Log::enabled) Log::write(LogType::Verbose, LogComponent::MemoryAccess,
        "MAR<-0x%05x", address);
}

void MemoryController::load_mdr(uint16_t value) {
    mem_->write_word(mar_, value);

    if (Log::enabled) {
        Log::write(LogType::Verbose, LogComponent::MemoryAccess,
            "MDR<-0x%04x", value);
        if (mar_ >= 0x10000 && mar_ < 0x20000) {
            Log::write(LogType::Verbose, LogComponent::CPMap,
                "MAP 0x%05x = %04x", mar_, value);
        }
    }
}

uint16_t MemoryController::read_md(TaskType task, bool& valid) {
    if (Log::enabled) Log::write(LogType::Verbose, LogComponent::MemoryAccess,
        "<-MD (0x%04x) valid %d", md_, md_valid_ ? 1 : 0);

    valid = md_valid_;

    if (!valid) {
        if (Log::enabled) Log::write(LogComponent::MemoryAccess,
            "Read from nonexistent memory address 0x%x", mar_);
        m_status_ |= static_cast<uint16_t>(0x80 >> static_cast<int>(task));
        m_status_ |= 0x100;  // double-bit error
    } else {
        // Clear single/double-bit errors (bits 6..7)
        m_status_ &= 0xfcff;
    }

    return md_;
}

} // namespace darkstar
