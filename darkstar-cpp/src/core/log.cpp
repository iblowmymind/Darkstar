/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#include "core/log.h"

namespace darkstar {

bool Log::enabled = false;
LogComponent Log::log_components = LogComponent::None;
LogType Log::log_types = LogType::None;

} // namespace darkstar
