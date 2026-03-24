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
    IMPLIED WARRANTIES OF MERCHANTABILITY AND SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once
#include "iop_device.h"
#include <cstdint>
#include <vector>

class Printer : public IIOPDevice {
public:
    Printer();
    void Reset();
    bool RxRequest() const { return _rxRequest; }
    bool TxRequest() const { return _txRequest; }
    const std::vector<int>& ReadPorts()  const override;
    const std::vector<int>& WritePorts() const override;
    uint8_t ReadPort(int port) override;
    void    WritePort(int port, uint8_t data) override;

private:
    bool    _rxRequest{true};
    bool    _txRequest{true};
    uint8_t _txData{0};
    static const std::vector<int> _readPorts;
    static const std::vector<int> _writePorts;
};