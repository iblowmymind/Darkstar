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

#include "types.h"

// -------------------------------------------------------------------------
// Configuration global definitions  (from D/Configuration.cs)
// Default values mirror the C# static constructor.
// -------------------------------------------------------------------------
namespace Configuration
{
    uint32_t         MemorySize              = 768;
    uint64_t         HostID                  = 0x0000aa012345ULL;
    bool             ThrottleSpeed           = true;
    uint32_t         DisplayScale            = 1;
    bool             SlowPhosphor            = true;
    bool             FullScreenStretch       = false;
    const char*      HardDriveImage          = nullptr;
    const char*      FloppyDriveImage        = nullptr;
    const char*      HostPacketInterfaceName = "";
    const char*      NetHubHost              = "localhost";
    uint16_t         NetHubPort              = 3333;
    TODPowerUpSetMode TODSetMode             = TODPowerUpSetMode::HostTimeY2K;
    AltBootValues    AltBootMode             = AltBootValues::None;
    PlatformType     Platform                = PlatformType::Unix;
    bool             Start                   = false;
}
