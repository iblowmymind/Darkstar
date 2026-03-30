/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#include "debugger/breakpoint_manager.h"

namespace darkstar {

BreakpointManager& BreakpointManager::instance() {
    static BreakpointManager mgr;
    return mgr;
}

BreakpointManager::BreakpointManager()
    : iop_breakpoints_(0x10000, BreakpointType::None)
    , cp_breakpoints_(0x1000, BreakpointType::None)
    , mesa_breakpoints_(0x200000, BreakpointType::None)
    , enable_breakpoints_(true)
{
}

void BreakpointManager::set_breakpoint(const BreakpointEntry& entry) {
    BreakpointType* bkpts = get_breakpoints_for_processor(entry.processor);
    if (!bkpts) return;

    if (entry.type == BreakpointType::None) {
        bkpts[entry.address] = BreakpointType::None;
    } else {
        bkpts[entry.address] |= entry.type;
    }
}

BreakpointType BreakpointManager::get_breakpoint(BreakpointProcessor processor, uint16_t address) const {
    const BreakpointType* bkpts = get_breakpoints_for_processor(processor);
    if (!bkpts) return BreakpointType::None;
    return bkpts[address];
}

bool BreakpointManager::test_breakpoint(const BreakpointEntry& entry) const {
    if (!enable_breakpoints_) return false;
    const BreakpointType* bkpts = get_breakpoints_for_processor(entry.processor);
    if (!bkpts) return false;
    return (bkpts[entry.address] & entry.type) != BreakpointType::None;
}

bool BreakpointManager::test_breakpoint(BreakpointProcessor processor, BreakpointType type, int address) const {
    if (!enable_breakpoints_) return false;
    const BreakpointType* bkpts = get_breakpoints_for_processor(processor);
    if (!bkpts) return false;
    return (bkpts[address] & type) != BreakpointType::None;
}

std::vector<BreakpointEntry> BreakpointManager::enumerate_breakpoints() const {
    std::vector<BreakpointEntry> breakpoints;

    for (int i = 0; i < static_cast<int>(iop_breakpoints_.size()); i++) {
        if (iop_breakpoints_[i] != BreakpointType::None) {
            breakpoints.emplace_back(BreakpointProcessor::IOP, iop_breakpoints_[i], i);
        }
    }
    for (int i = 0; i < static_cast<int>(cp_breakpoints_.size()); i++) {
        if (cp_breakpoints_[i] != BreakpointType::None) {
            breakpoints.emplace_back(BreakpointProcessor::CP, cp_breakpoints_[i], i);
        }
    }
    for (int i = 0; i < static_cast<int>(mesa_breakpoints_.size()); i++) {
        if (mesa_breakpoints_[i] != BreakpointType::None) {
            breakpoints.emplace_back(BreakpointProcessor::Mesa, mesa_breakpoints_[i], i);
        }
    }

    return breakpoints;
}

const BreakpointType* BreakpointManager::get_breakpoints_for_processor(BreakpointProcessor processor) const {
    switch (processor) {
        case BreakpointProcessor::CP: return cp_breakpoints_.data();
        case BreakpointProcessor::IOP: return iop_breakpoints_.data();
        case BreakpointProcessor::Mesa: return mesa_breakpoints_.data();
    }
    return nullptr;
}

BreakpointType* BreakpointManager::get_breakpoints_for_processor(BreakpointProcessor processor) {
    switch (processor) {
        case BreakpointProcessor::CP: return cp_breakpoints_.data();
        case BreakpointProcessor::IOP: return iop_breakpoints_.data();
        case BreakpointProcessor::Mesa: return mesa_breakpoints_.data();
    }
    return nullptr;
}

} // namespace darkstar
