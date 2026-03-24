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
#include "iop_device.h"
#include "tod_clock.h"
#include "../types.h"
#include <cstdint>
#include <functional>
#include <vector>

class IOProcessor;

class MiscIO : public IIOPDevice {
public:
    explicit MiscIO(IOProcessor* iop);
    void Reset();

    const std::vector<int>& ReadPorts()  const override;
    const std::vector<int>& WritePorts() const override;
    void    WritePort(int port, uint8_t value) override;
    uint8_t ReadPort(int port) override;

    std::function<void()> MPChanged;

    AltBootValues AltBoot() const       { return _altBoot; }
    void          SetAltBoot(AltBootValues v);
    int           MPanelValue() const   { return _mPanelValue; }
    bool          MPanelBlank() const   { return _mPanelBlank; }
    TODClock&     GetTODClock()         { return _todClock; }

private:
    void DoMiscClock(uint8_t clockFlags);

    IOProcessor*  _iop;
    TODClock      _todClock;
    bool          _mPanelBlank{true};
    int           _mPanelValue{0};
    int           _altBootCounter{0};
    AltBootValues _altBoot{AltBootValues::None};
    int           _lastClockFlags{0};
    uint8_t       _dmaTestValue{0};

    static const std::vector<int> _readPorts;
    static const std::vector<int> _writePorts;
};