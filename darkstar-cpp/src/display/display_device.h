/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>

namespace darkstar {

class IDisplayDevice {
public:
    virtual ~IDisplayDevice() = default;
    virtual void draw_scanline(int scanline, const uint16_t* data, int word_count, bool invert) = 0;
    virtual void render() = 0;
    virtual void clear() = 0;
};

} // namespace darkstar
