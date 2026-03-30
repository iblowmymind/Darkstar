/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>

namespace darkstar {

struct Conversion {
    static constexpr uint64_t MsecToNsec = 1000000ULL;
    static constexpr double NsecToMsec = 0.000001;
    static constexpr uint64_t UsecToNsec = 1000ULL;
    static constexpr double UsecToSec = 0.000001;
};

} // namespace darkstar
