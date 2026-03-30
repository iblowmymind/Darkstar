/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>

namespace darkstar {

class IDMAInterface {
public:
    virtual ~IDMAInterface() = default;
    virtual bool drq() const = 0;
    virtual uint8_t dma_read() = 0;
    virtual void dma_write(uint8_t value) = 0;
    virtual void dma_complete() = 0;
};

} // namespace darkstar
