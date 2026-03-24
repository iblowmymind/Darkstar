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
#include <cstdint>
#include <vector>

class IOProcessor;

enum class DMAType : int {
    Verify  = 0,
    Write   = 1,
    Read    = 2,
    Invalid = 3,
};

class IDMAInterface {
public:
    virtual ~IDMAInterface() = default;
    virtual bool    DRQ() = 0;
    virtual void    DMAWrite(uint8_t value) = 0;
    virtual uint8_t DMARead() = 0;
    virtual void    DMAComplete() = 0;
};

struct DMAChannel {
    DMAChannel();
    void Reset();
    bool         Enabled{false};
    bool         Completed{false};
    uint16_t     ChAddr{0};
    int          ChCount{-1};
    DMAType      Type{DMAType::Invalid};
    IDMAInterface* Device{nullptr};
};

class DMAController : public IIOPDevice {
public:
    explicit DMAController(IOProcessor* iop);
    void RegisterDevice(IDMAInterface* device, int channel);
    DMAChannel* GetChannel(int i) { return &_channels[i]; }
    bool HRQ() const { return _hrq; }
    bool TC() const;
    void Reset();
    void Execute();

    const std::vector<int>& ReadPorts()  const override;
    const std::vector<int>& WritePorts() const override;
    void    WritePort(int port, uint8_t value) override;
    uint8_t ReadPort(int port) override;

private:
    int SelectNextChannel();
    IOProcessor* _iop;
    DMAChannel   _channels[4];
    bool  _rotatingPriority{false};
    bool  _extendedWrite{false};
    bool  _tcStop{false};
    bool  _autoLoad{false};
    bool  _first{true};
    int   _nextToService{0};
    int   _lastSelectedChannel{-1};
    bool  _hrq{false};
    static const std::vector<int> _readPorts;
    static const std::vector<int> _writePorts;
};