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
#include "i8085.h"
#include "iop_io_bus.h"
#include "iop_memory_bus.h"
#include "keyboard.h"
#include "mouse.h"
#include "beeper.h"
#include "misc_io.h"
#include "dma_controller.h"
#include "floppy_controller.h"
#include "printer.h"
#include "floppy_drive.h"
#include <cstdint>

class DSystem;

class IOProcessor {
public:
    explicit IOProcessor(DSystem* system);
    ~IOProcessor();
    void Reset();
    int  Execute();

    i8085*          CPU()             { return _cpu; }
    I8085MemoryBus* Memory()          { return _mem; }
    MiscIO*         GetMiscIO()       { return _miscIO; }
    FloppyController* GetFloppyController() { return _floppyController; }
    DMAController*  GetDMAController() { return _dma; }
    Keyboard*       GetKeyboard()     { return &_keyboard; }
    Mouse*          GetMouse()        { return &_mouse; }
    Printer*        GetPrinter()      { return _tty; }
    Beeper*         GetBeeper()       { return &_beeper; }

private:
    DSystem*          _system;
    IOPIOBus*         _io;
    IOPMemoryBus*     _mem;
    i8085*            _cpu;
    Keyboard          _keyboard;
    Mouse             _mouse;
    Beeper            _beeper;
    MiscIO*           _miscIO;
    FloppyController* _floppyController;
    DMAController*    _dma;
    Printer*          _tty;
    FloppyDrive*      _floppyDrive;
};