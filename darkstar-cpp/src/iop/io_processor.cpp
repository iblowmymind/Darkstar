/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/

#include "iop/io_processor.h"
#include "iop/iop_io_bus.h"
#include "iop/iop_memory_bus.h"
#include "iop/misc_io.h"
#include "iop/floppy_controller.h"
#include "iop/dma_controller.h"
#include "iop/keyboard.h"
#include "iop/mouse.h"
#include "iop/printer.h"
#include "iop/beeper.h"
#include "iop/tod_clock.h"
#include "io/floppy_drive.h"
#include "core/system.h"
#include "cp/central_processor.h"

namespace darkstar {

IOProcessor::IOProcessor(DSystem* system)
    : system_(system)
{
    io_ = std::make_unique<IOPIOBus>();
    mem_ = std::make_unique<IOPMemoryBus>(*io_);
    cpu_ = std::make_unique<i8085>(*mem_, *io_);

    keyboard_ = std::make_unique<Keyboard>();
    mouse_ = std::make_unique<Mouse>();

    // 8" floppy drive used by the IOP
    floppy_drive_ = std::make_unique<FloppyDrive>(*system_);

    // Create devices
    misc_io_ = std::make_unique<MiscIO>(*this);
    floppy_controller_ = std::make_unique<FloppyController>(*floppy_drive_, *system_);
    dma_ = std::make_unique<DMAController>(*this);
    printer_ = std::make_unique<Printer>();
    beeper_ = std::make_unique<Beeper>();
    tod_clock_ = std::make_unique<TODClock>();

    // Register DMA devices with controller
    dma_->register_device(floppy_controller_.get(), 0);  // Floppy, DMA Channel 0
    dma_->register_device(&system_->cp(), 1);             // CP, DMA Channel 1

    // Register IO devices
    io_->register_device(misc_io_.get());
    io_->register_device(floppy_controller_.get());
    io_->register_device(dma_.get());
    io_->register_device(&system_->cp());
    io_->register_device(printer_.get());

    reset();
}

IOProcessor::~IOProcessor() = default;

void IOProcessor::reset() {
    cpu_->reset();
    misc_io_->reset();
    dma_->reset();
    floppy_controller_->reset();
}

int IOProcessor::execute() {
    // Run the DMA controller, see if it has anything to do this cycle.
    dma_->execute();

    if (dma_->hrq()) {
        // DMA transfer in progress, CPU doesn't get to run.
        return 4;  // A DMA cycle takes 4 clocks.
    } else {
        // Run the CPU for one instruction.
        return cpu_->execute();
    }
}

} // namespace darkstar
