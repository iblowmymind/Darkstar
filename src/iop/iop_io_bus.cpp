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

#include "iop_io_bus.h"

IOPIOBus::IOPIOBus() {
    // Initialize all dispatch entries to nullptr
    _writeDispatch.fill(nullptr);
    _readDispatch.fill(nullptr);
}

void IOPIOBus::RegisterDevice(IIOPDevice* device) {
    if (!device) return;
    
    // Register device for all its read ports
    for (int port : device->ReadPorts()) {
        if (port >= 0 && port < 256) {
            _readDispatch[port] = device;
        }
    }
    
    // Register device for all its write ports
    for (int port : device->WritePorts()) {
        if (port >= 0 && port < 256) {
            _writeDispatch[port] = device;
        }
    }
}

void IOPIOBus::Out(uint8_t port, uint8_t val) {
    IIOPDevice* device = _writeDispatch[port];
    if (device) {
        device->WritePort(port, val);
    }
    // If no device registered for this port, the write is ignored
}

uint8_t IOPIOBus::In(uint8_t port) {
    IIOPDevice* device = _readDispatch[port];
    if (device) {
        return device->ReadPort(port);
    }
    // If no device registered for this port, return 0
    return 0;
}