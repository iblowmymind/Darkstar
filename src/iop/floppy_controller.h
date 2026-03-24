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
#include "dma_controller.h"
#include "floppy_drive.h"
#include <cstdint>
#include <vector>
#include <functional>

class DSystem;

class FloppyController : public IIOPDevice, public IDMAInterface {
public:
    FloppyController(FloppyDrive* drive, DSystem* system);

    FloppyDrive* Drive() { return _drive; }
    bool Interrupt() const { return _interruptPending; }
    void SetInterruptCallback(std::function<void()> cb) { _raiseInterruptCallback = cb; }

    // IDMAInterface
    bool    DRQ() override;
    void    DMAWrite(uint8_t value) override;
    uint8_t DMARead() override;
    void    DMAComplete() override;

    // IIOPDevice
    const std::vector<int>& ReadPorts()  const override;
    const std::vector<int>& WritePorts() const override;
    void    WritePort(int port, uint8_t value) override;
    uint8_t ReadPort(int port) override;

    void Reset();

private:
    void RaiseInterrupt();
    void SeekCallback(uint64_t skewNsec, void* context);
    void StepCallback(uint64_t skewNsec, void* context);
    void SectorTransferCallback(uint64_t skewNsec, void* context);
    void WriteTrackCallback(uint64_t skewNsec, void* context);
    void ResetIndexCallback(uint64_t skewNsec, void* context);
    void FinishWriteTrack();
    
    FloppyDrive* _drive;
    DSystem* _system;
    std::function<void()> _raiseInterruptCallback;
    
    bool _interruptPending{false};
    uint8_t _status{0};
    uint8_t _track{0};
    uint8_t _sector{0};
    uint8_t _data{0};
    uint8_t _extStatus{0};
    uint8_t _extState{0};
    
    bool _busy{false};
    bool _drq{false};
    int  _drqCounter{0};
    
    std::vector<uint8_t> _trackBuffer;
    int _trackBufferIndex{0};
    
    static const std::vector<int> _readPorts;
    static const std::vector<int> _writePorts;
};