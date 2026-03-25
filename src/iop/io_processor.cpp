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

#include "io_processor.h"
#include "../dsystem.h"
#include "../cp/central_processor.h"

IOProcessor::IOProcessor(DSystem* system) : _system(system) {
    // Create I/O bus
    _io = new IOPIOBus();
    
    // Create memory bus with I/O bus
    _mem = new IOPMemoryBus(_io);
    
    // Create CPU with memory and I/O buses
    _cpu = new i8085(_mem, _io);
    
    // Create floppy drive
    _floppyDrive = new FloppyDrive();
    
    // Create peripheral devices
    _miscIO = new MiscIO(this);
    _floppyController = new FloppyController(_floppyDrive, system);
    _dma = new DMAController(this);
    _tty = new Printer();
    
    // Set up floppy controller interrupt callback
    _floppyController->SetInterruptCallback([this]() {
        _cpu->RaiseExternalInterrupt(InterruptType::RST7_5);
    });
    
    // Register DMA devices
    _dma->RegisterDevice(_floppyController, 0);                    // Floppy on channel 0
    _dma->RegisterDevice(_system->GetCP()->GetIO(), 1);            // CP on channel 1
    
    // Register I/O devices (matches C# IOProcessor constructor order)
    _io->RegisterDevice(_miscIO);
    _io->RegisterDevice(_floppyController);
    _io->RegisterDevice(_dma);
    _io->RegisterDevice(_system->GetCP()->GetIO());               // CP ports 0xeb/0xec/0xee/0xf8-0xff
    _io->RegisterDevice(_tty);
    
    Reset();
}

IOProcessor::~IOProcessor() {
    delete _tty;
    delete _dma;
    delete _floppyController;
    delete _miscIO;
    delete _floppyDrive;
    delete _cpu;
    delete _mem;
    delete _io;
}

void IOProcessor::Reset() {
    _keyboard = Keyboard();
    _mouse = Mouse();
    _beeper.Reset();
    
    _miscIO->Reset();
    _floppyController->Reset();
    _dma->Reset();
    _tty->Reset();
    
    _cpu->Reset();
    
    // Update host ID PROM
    _mem->UpdateHostIDProm();
}

int IOProcessor::Execute() {
    // Run DMA controller; if it asserted HRQ the bus is taken – CPU doesn't run.
    _dma->Execute();
    if (_dma->HRQ()) {
        return 4;   // A DMA cycle takes 4 clocks (matches C# behaviour)
    }
    // Execute one CPU instruction
    return _cpu->Execute();
}