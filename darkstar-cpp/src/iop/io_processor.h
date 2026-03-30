/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include "iop/i8085.h"
#include "iop/i8085_io_bus.h"
#include "iop/i8085_memory_bus.h"
#include "iop/dma_interface.h"
#include "iop/iop_device.h"

#include <cstdint>
#include <memory>

namespace darkstar {

class DSystem;
class IOPMemoryBus;
class IOPIOBus;
class MiscIO;
class FloppyController;
class DMAController;
class Keyboard;
class Mouse;
class Printer;
class Beeper;
class FloppyDrive;
class TODClock;

class IOProcessor {
public:
    explicit IOProcessor(DSystem* system);
    ~IOProcessor();

    void reset();
    int execute();

    i8085& cpu() { return *cpu_; }
    const i8085& cpu() const { return *cpu_; }

    I8085MemoryBus& memory() { return *mem_; }
    MiscIO& misc_io() { return *misc_io_; }
    FloppyController& floppy_controller() { return *floppy_controller_; }
    DMAController& dma_controller() { return *dma_; }
    Keyboard& keyboard() { return *keyboard_; }
    Mouse& mouse() { return *mouse_; }
    Printer& printer() { return *printer_; }
    Beeper& beeper() { return *beeper_; }
    TODClock& tod_clock() { return *tod_clock_; }

private:
    DSystem* system_;

    std::unique_ptr<IOPIOBus> io_;
    std::unique_ptr<IOPMemoryBus> mem_;
    std::unique_ptr<i8085> cpu_;

    std::unique_ptr<MiscIO> misc_io_;
    std::unique_ptr<FloppyController> floppy_controller_;
    std::unique_ptr<DMAController> dma_;
    std::unique_ptr<Keyboard> keyboard_;
    std::unique_ptr<Mouse> mouse_;
    std::unique_ptr<Printer> printer_;
    std::unique_ptr<Beeper> beeper_;
    std::unique_ptr<TODClock> tod_clock_;
    std::unique_ptr<FloppyDrive> floppy_drive_;
};

} // namespace darkstar
