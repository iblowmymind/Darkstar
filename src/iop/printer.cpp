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

#include "printer.h"

// Port definitions
const std::vector<int> Printer::_readPorts = {0x88, 0x89};
const std::vector<int> Printer::_writePorts = {0x88, 0x89};

Printer::Printer() {
    Reset();
}

void Printer::Reset() {
    _rxRequest = true;
    _txRequest = true;
    _txData = 0;
}

const std::vector<int>& Printer::ReadPorts() const {
    return _readPorts;
}

const std::vector<int>& Printer::WritePorts() const {
    return _writePorts;
}

uint8_t Printer::ReadPort(int port) {
    switch (port) {
    case 0x88:
        // TX data (loopback)
        return _txData;
        
    case 0x89:
        // Status register
        _rxRequest = true; // Reading status sets RX request
        return 0x00;
    }
    
    return 0;
}

void Printer::WritePort(int port, uint8_t data) {
    switch (port) {
    case 0x88:
        // TX data
        _txData = data;
        _rxRequest = false;
        break;
        
    case 0x89:
        // Control register - ignored
        break;
    }
}