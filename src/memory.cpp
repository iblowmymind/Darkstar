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

#include "memory.h"
#include "types.h"
#include <cstring>
#include <cstdio>
#include <cassert>

// =========================================================================
// Memory
// =========================================================================

Memory::Memory()
    : _memory(nullptr)
    , _ecc(nullptr)
    , _size(0)
    , _mctlInvert(0)
{
    Reset();
}

Memory::~Memory()
{
    delete[] _memory;
    delete[] _ecc;
}

void Memory::Reset()
{
    int newSize = static_cast<int>(Configuration::MemorySize) * 1024;

    if (_size != newSize)
    {
        delete[] _memory;
        delete[] _ecc;
        _size   = newSize;
        _memory = new uint16_t[_size];
        _ecc    = new uint8_t[_size];
    }

    memset(_memory, 0, _size * sizeof(uint16_t));
    memset(_ecc,    0, _size * sizeof(uint8_t));
}

int Memory::Size() const
{
    return _size;
}

uint16_t Memory::ReadWord(int address, bool& valid) const
{
    if (address < _size)
    {
        uint16_t memWord = _memory[address];
        valid = (_ecc[address] == CalculateECCSyndrome(memWord));
        return memWord;
    }
    else
    {
        valid = false;
        return 0;
    }
}

void Memory::WriteWord(int address, uint16_t value)
{
    if (address < _size)
    {
        WriteECC(address, value);
        _memory[address] = value;
    }
    // Out-of-range writes are silently ignored (same as C# original).
}

void Memory::SetCheckBits(int invertBits)
{
    _mctlInvert = invertBits;
}

uint8_t Memory::CalculateECCSyndrome(uint16_t /*word*/) const
{
    // No actual ECC syndrome calculation is needed; there are no plans to
    // emulate faulty memory or hardware correction.
    return 0;
}

void Memory::WriteECC(int address, uint16_t value)
{
    _ecc[address] = static_cast<uint8_t>(CalculateECCSyndrome(value) ^ _mctlInvert);
}

// =========================================================================
// MemoryController
// =========================================================================

MemoryController::MemoryController()
    : _mar(0)
    , _md(0)
    , _mdValid(true)
    , _mStatus(0)
    , _mem(new Memory())
{
}

MemoryController::~MemoryController()
{
    delete _mem;
}

void MemoryController::Reset()
{
    _mStatus = 0;
    _mar     = 0;
    _md      = 0;
    _mdValid = true;

    _mem->Reset();
}

void MemoryController::SetMCtl(uint16_t value)
{
    // See HWRef, figure 18 (pg 55).
    // Bits [7:2] set the syndrome inversion mask for ECC testing.
    _mem->SetCheckBits((value & 0xff) >> 2);

    if ((value & 0x800) != 0)
    {
        // Clear error-log entry for the task specified by bits [10:8].
        int task = (value & 0x700) >> 8;
        _mStatus &= static_cast<uint16_t>(~(0x80 >> task));
    }
}

void MemoryController::LoadMAR(int address)
{
    _mar = address;

    // Pre-fetch so that the original data is available if MDR is written and
    // MD is read in the same click.
    _md = _mem->ReadWord(_mar, _mdValid);
}

void MemoryController::LoadMDR(uint16_t value)
{
    _mem->WriteWord(_mar, value);
}

uint16_t MemoryController::ReadMD(TaskType task, bool& valid)
{
    valid = _mdValid;

    if (!valid)
    {
        // Set error bit for the requesting task plus the double-bit error flag.
        _mStatus |= static_cast<uint16_t>(0x80 >> static_cast<int>(task));
        _mStatus |= 0x100; // double-bit error
    }
    else
    {
        // Clear single/double-bit error flags (bits [9:8] of MStatus).
        _mStatus &= 0xfcff;
    }

    return _md;
}
