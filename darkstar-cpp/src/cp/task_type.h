/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>

namespace darkstar {

enum class TaskType {
    Emulator = 0,
    Display,
    Ethernet,
    Refresh,
    Disk,
    IOP,
    IOPcs,
    Kernel,
};

} // namespace darkstar
