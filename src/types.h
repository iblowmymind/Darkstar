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

#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>

// -------------------------------------------------------------------------
// Integer type aliases
// -------------------------------------------------------------------------
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

// -------------------------------------------------------------------------
// Time conversion constants  (from D/Conversion.cs)
// -------------------------------------------------------------------------
namespace Conversion
{
    static constexpr uint64_t MsecToNsec = 1000000ULL;
    static constexpr double   NsecToMsec = 0.000001;
    static constexpr uint64_t UsecToNsec = 1000ULL;
    static constexpr double   UsecToSec  = 0.000001;
}

// -------------------------------------------------------------------------
// TaskType  (from D/CP/CentralProcessor.cs)
// -------------------------------------------------------------------------
enum class TaskType : int
{
    Emulator = 0,
    Display,
    Ethernet,
    Refresh,
    Disk,
    IOP,
    IOPcs,   // used as an address register when reading/writing control store
    Kernel,

    // Sentinel – must stay last
    TaskCount
};

// -------------------------------------------------------------------------
// ClickType  (from D/CP/CentralProcessor.cs)
// -------------------------------------------------------------------------
enum class ClickType : int
{
    Ethernet0 = 0,
    Disk,
    IOP,
    Ethernet1,
    Display
};

// -------------------------------------------------------------------------
// IBState  (from D/CP/CentralProcessor.cs)
// -------------------------------------------------------------------------
enum class IBState : int
{
    Empty = 0,
    Byte  = 1,
    Full  = 2,
    Word  = 3,
};

// -------------------------------------------------------------------------
// TODPowerUpSetMode  (referenced by Configuration.cs)
// -------------------------------------------------------------------------
enum class TODPowerUpSetMode : int
{
    HostTimeY2K = 0,
    HostTime,
    SpecificDateAndTime,
    SpecificDate,
    NoChange,
};

// -------------------------------------------------------------------------
// AltBootValues  (referenced by Configuration.cs)
// -------------------------------------------------------------------------
enum class AltBootValues : int
{
    None = -1,
    DiagnosticRigid = 0,
    Rigid,
    Floppy,
    Ethernet,
    DiagnosticEthernet,
    DiagnosticFloppy,
    AlternateEthernet,
    DiagnosticTrident1,
    DiagnosticTrident2,
    DiagnosticTrident3,
    HeadCleaning
};

// -------------------------------------------------------------------------
// Platform type  (from D/Configuration.cs)
// -------------------------------------------------------------------------
enum class PlatformType : int
{
    Windows = 0,
    Unix,
};

// -------------------------------------------------------------------------
// Configuration  (simplified; mirrors D/Configuration.cs static class)
// -------------------------------------------------------------------------
namespace Configuration
{
    // Memory size in KW.  Default 768 KW.
    extern uint32_t MemorySize;

    extern uint64_t HostID;
    extern bool     ThrottleSpeed;
    extern uint32_t DisplayScale;
    extern bool     SlowPhosphor;
    extern bool     FullScreenStretch;

    extern const char* HardDriveImage;
    extern const char* FloppyDriveImage;
    extern const char* HostPacketInterfaceName;
    extern const char* NetHubHost;
    extern uint16_t    NetHubPort;

    extern TODPowerUpSetMode TODSetMode;
    extern AltBootValues     AltBootMode;
    extern PlatformType      Platform;

    extern bool Start;
}

// -------------------------------------------------------------------------
// Forward declarations used across multiple translation units
// -------------------------------------------------------------------------
class DSystem;
class CentralProcessor;
class MemoryController;
class Memory;
class Scheduler;
class SchedulerEvent;
class DisplayController;
