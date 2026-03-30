/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

namespace darkstar {

enum class LogComponent : uint32_t {
    None            = 0,
    IOP             = 0x1,
    IOPMemory       = 0x2,
    IOPIO           = 0x4,
    IOPFloppy       = 0x8,
    IOPMisc         = 0x10,
    IOPDMA          = 0x20,
    IOPKeyboard     = 0x40,
    IOPPrinter      = 0x80,
    CPControl       = 0x100,
    CPExecution     = 0x200,
    CPMicrocodeLoad = 0x400,
    CPTPCLoad       = 0x800,
    CPTask          = 0x1000,
    CPMap           = 0x2000,
    CPError         = 0x4000,
    CPIB            = 0x8000,
    CPStack         = 0x10000,
    CPInst          = 0x20000,
    MemoryControl   = 0x100000,
    MemoryAccess    = 0x200000,
    DisplayControl  = 0x400000,
    ShugartControl  = 0x800000,
    EthernetControl = 0x1000000,
    HostEthernet    = 0x2000000,
    EthernetTransmit= 0x4000000,
    EthernetReceive = 0x8000000,
    EthernetPacket  = 0x10000000,
    Beeper          = 0x20000000,
    Configuration   = 0x40000000,
    All             = 0x7fffffff
};

inline LogComponent operator|(LogComponent a, LogComponent b) {
    return static_cast<LogComponent>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline LogComponent operator&(LogComponent a, LogComponent b) {
    return static_cast<LogComponent>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

enum class LogType : uint32_t {
    None    = 0,
    Normal  = 0x1,
    Warning = 0x2,
    Error   = 0x4,
    Verbose = 0x8,
    All     = 0x7fffffff
};

inline LogType operator|(LogType a, LogType b) {
    return static_cast<LogType>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline LogType operator&(LogType a, LogType b) {
    return static_cast<LogType>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

class Log {
public:
    static bool enabled;
    static LogComponent log_components;
    static LogType log_types;

    template<typename... Args>
    static void write(LogComponent component, const char* fmt, Args... args) {
        if (!enabled) return;
        if (static_cast<uint32_t>(component & log_components) == 0) return;
        std::fprintf(stderr, fmt, args...);
        std::fprintf(stderr, "\n");
    }

    template<typename... Args>
    static void write(LogType type, LogComponent component, const char* fmt, Args... args) {
        if (!enabled) return;
        if (static_cast<uint32_t>(type & log_types) == 0) return;
        if (static_cast<uint32_t>(component & log_components) == 0) return;
        std::fprintf(stderr, fmt, args...);
        std::fprintf(stderr, "\n");
    }
};

} // namespace darkstar
