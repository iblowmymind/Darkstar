/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>
#include <vector>

namespace darkstar {

enum class BreakpointType : uint8_t {
    None = 0,
    Execution = 1,
    Read = 2,
    Write = 4,
};

inline BreakpointType operator|(BreakpointType a, BreakpointType b) {
    return static_cast<BreakpointType>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline BreakpointType operator&(BreakpointType a, BreakpointType b) {
    return static_cast<BreakpointType>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

inline BreakpointType& operator|=(BreakpointType& a, BreakpointType b) {
    a = a | b;
    return a;
}

enum class BreakpointProcessor {
    IOP = 0,
    CP,
    Mesa,
};

struct BreakpointEntry {
    BreakpointProcessor processor;
    BreakpointType type;
    int address;

    BreakpointEntry() : processor(BreakpointProcessor::IOP), type(BreakpointType::None), address(0) {}
    BreakpointEntry(BreakpointProcessor p, BreakpointType t, int addr)
        : processor(p), type(t), address(addr) {}
};

class BreakpointManager {
public:
    static BreakpointManager& instance();

    bool breakpoints_enabled() const { return enable_breakpoints_; }
    void set_breakpoints_enabled(bool enabled) { enable_breakpoints_ = enabled; }

    void set_breakpoint(const BreakpointEntry& entry);
    BreakpointType get_breakpoint(BreakpointProcessor processor, uint16_t address) const;
    bool test_breakpoint(const BreakpointEntry& entry) const;
    bool test_breakpoint(BreakpointProcessor processor, BreakpointType type, int address) const;
    std::vector<BreakpointEntry> enumerate_breakpoints() const;

private:
    BreakpointManager();
    const BreakpointType* get_breakpoints_for_processor(BreakpointProcessor processor) const;
    BreakpointType* get_breakpoints_for_processor(BreakpointProcessor processor);

    std::vector<BreakpointType> iop_breakpoints_;
    std::vector<BreakpointType> cp_breakpoints_;
    std::vector<BreakpointType> mesa_breakpoints_;
    bool enable_breakpoints_;
};

} // namespace darkstar
