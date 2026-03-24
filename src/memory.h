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

#include "types.h"
#include <vector>
#include <cstring>

// -------------------------------------------------------------------------
// Memory  (maps to D/Memory/Memory.cs)
// Encapsulates the physical memory with a minimal ECC implementation
// sufficient to allow diagnostics to pass.
// -------------------------------------------------------------------------
class Memory
{
public:
    Memory();
    ~Memory();

    void Reset();

    // Total number of 16-bit words in the memory array.
    int Size() const;

    // Read a 16-bit word. Sets valid=false if the address is out of range or
    // the ECC syndrome does not match (indicating uninitialised / corrupt data
    // written via an inverted check-bit mask).
    uint16_t ReadWord(int address, bool& valid) const;

    // Write a 16-bit word (also updates the ECC byte for that address).
    void WriteWord(int address, uint16_t value);

    // Set the inverted check-bit mask used by the ECC logic (MCtl bits [7:2]).
    void SetCheckBits(int invertBits);

private:
    uint8_t CalculateECCSyndrome(uint16_t word) const;
    void    WriteECC(int address, uint16_t value);

    uint16_t* _memory;
    uint8_t*  _ecc;
    int       _size;       // word count (= Configuration::MemorySize * 1024)
    int       _mctlInvert; // inverted check-bit mask set via MCtl
};

// -------------------------------------------------------------------------
// MemoryController  (maps to D/Memory/MemoryController.cs)
// -------------------------------------------------------------------------
class MemoryController
{
public:
    MemoryController();
    ~MemoryController();

    void Reset();

    // ---- Status / address registers ----
    uint16_t MStatus() const { return _mStatus; }
    int      MAR()     const { return _mar; }

    // Expose underlying Memory for display / DMA access.
    Memory* DebugMemory() { return _mem; }

    // Debugging accessor – last word latched by LoadMAR.
    uint16_t MD() const { return _md; }

    // ---- CP-facing operations (see HWRef figure 18) ----

    // MCtl<- : set syndrome inversion and optionally clear an error-log entry.
    void SetMCtl(uint16_t value);

    // MAR<- : load the memory address register and pre-fetch the word.
    void LoadMAR(int address);

    // MDR<- : write the word previously addressed by MAR.
    void LoadMDR(uint16_t value);

    // <-MD  : return the word fetched by the most-recent LoadMAR.
    //         Sets the appropriate bits in MStatus on an ECC error.
    uint16_t ReadMD(TaskType task, bool& valid);

private:
    int      _mar;
    uint16_t _md;
    bool     _mdValid;
    uint16_t _mStatus;

    Memory*  _mem;
};
