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
#include "i8085_memory_bus.h"
#include "i8085_io_bus.h"
#include <cstdint>
#include <array>
#include <string>

class IOPMemoryBus : public I8085MemoryBus {
public:
    explicit IOPMemoryBus(I8085IOBus* ioBus);

    uint8_t  ReadByte(uint16_t address) override;
    void     WriteByte(uint16_t address, uint8_t b) override;
    uint16_t ReadWord(uint16_t address) override;
    void     WriteWord(uint16_t address, uint16_t w) override;

    void UpdateHostIDProm();

private:
    void LoadPROMs();
    void LoadPROM(const std::string& promName, uint16_t address);
    void LoadHostIDProm();
    uint8_t  GetIDPromByte(int byteNumber) const;
    void     SetIDPromByte(int byteNumber, uint8_t value);
    static uint8_t RotateLeft(uint8_t value);

    std::array<uint8_t, 0x2000> _rom{};
    std::array<uint8_t, 0x6000> _ram{};
    std::array<uint8_t, 16>     _hostIdProm{};
    I8085IOBus* _io;
};