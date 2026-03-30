/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/
#include "core/system.h"

#include "core/configuration.h"
#include "core/high_res_timer.h"
#include "core/scheduler.h"
#include "cp/central_processor.h"
#include "display/display_controller.h"
#include "ethernet/ethernet_controller.h"
#include "io/sa1000.h"
#include "io/shugart_controller.h"
#include "iop/io_processor.h"
#include "memory/memory_controller.h"

#include <iostream>

namespace darkstar {

DSystem::DSystem() {
    scheduler_ = std::make_unique<Scheduler>();
    cp_ = std::make_unique<CentralProcessor>(this);
    iop_ = std::make_unique<IOProcessor>(this);
    memory_controller_ = std::make_unique<MemoryController>();
    display_controller_ = std::make_unique<DisplayController>(this);
    hard_drive_ = std::make_unique<SA1000Drive>(this);
    shugart_controller_ = std::make_unique<ShugartController>(this, hard_drive_.get());
    ethernet_controller_ = std::make_unique<EthernetController>(this);

    try {
        frame_timer_ = std::make_unique<FrameTimer>(38.7);
    } catch (...) {
        // Not supported on this platform
        frame_timer_ = nullptr;
    }
}

DSystem::~DSystem() {
    stop_execution();
}

void DSystem::reset() {
    bool was_executing = is_executing();

    std::shared_ptr<SystemExecutionContext> context;
    if (was_executing) {
        context = current_context_;
        stop_execution();
    }

    cp_->reset();
    iop_->reset();
    display_controller_->reset();
    ethernet_controller_->reset();
    hard_drive_->reset();
    shugart_controller_->reset();

    cp_cycles_ = 0;
    elapsed_cycles_ = 0.0;

    if (was_executing && context) {
        start_execution(context);
    }
}

void DSystem::shutdown(bool commit_disks) {
    std::cout << "Saving disk images and shutting down. Please wait..." << std::endl;
    hard_drive_->save();
    iop_->floppy_controller().drive().unload_disk();
    ethernet_controller_->shutdown();
}

bool DSystem::is_executing() const {
    return execution_thread_.joinable();
}

void DSystem::attach_display(IDisplayDevice* display) {
    display_ = display;
}

void DSystem::start_execution(std::shared_ptr<SystemExecutionContext> context) {
    stop_execution();

    current_context_ = context;
    abort_execution_ = false;

    bool debug_mode = context->step_callback_8085 &&
                      context->step_callback_cp &&
                      context->step_callback_mesa;

    if (debug_mode) {
        execution_thread_ = std::thread(
            &DSystem::debug_execution_worker, this, context);
    } else {
        execution_thread_ = std::thread(
            &DSystem::execution_worker, this, context);
    }

    if (on_execution_state_changed) {
        on_execution_state_changed();
    }
}

void DSystem::stop_execution() {
    if (execution_thread_.joinable()) {
        abort_execution_ = true;
        execution_thread_.join();
        current_context_ = nullptr;

        if (on_execution_state_changed) {
            on_execution_state_changed();
        }
    }
}

void DSystem::execution_worker(std::shared_ptr<SystemExecutionContext> context) {
    abort_execution_ = false;

    while (!abort_execution_) {
        try {
            // Clock the IOP first - returns number of 8085 clock cycles consumed
            int i8085_cycles = iop_->execute();

            // Calculate corresponding CP cycles (7.3MHz vs 3.0MHz)
            cp_cycles_ = static_cast<int>(kCpCyclesPer8085Cycle) * i8085_cycles;

            cp_->execute_instruction(cp_cycles_);

            if (Configuration::throttle_speed) {
                elapsed_cycles_ += cp_cycles_;

                if (elapsed_cycles_ > kCpCyclesPerField) {
                    elapsed_cycles_ -= kCpCyclesPerField;

                    if (frame_timer_) {
                        frame_timer_->wait_for_frame();
                    }
                }
            }
        } catch (const std::exception& e) {
            if (context->error_callback) {
                context->error_callback(e);
            }
            abort_execution_ = true;
        }
    }

    if (on_execution_state_changed) {
        on_execution_state_changed();
    }
}

void DSystem::debug_execution_worker(std::shared_ptr<SystemExecutionContext> context) {
    abort_execution_ = false;
    bool iop_abort = false;

    while (!abort_execution_) {
        try {
            if (cp_cycles_ == 0) {
                if (iop_abort) {
                    abort_execution_ = true;
                } else {
                    int i8085_cycles = iop_->execute();
                    cp_cycles_ = static_cast<int>(kCpCyclesPer8085Cycle) * i8085_cycles;

                    if (context->step_callback_8085()) {
                        iop_abort = true;
                    }
                }
            }

            cp_->execute_instruction(1);
            cp_cycles_--;
            elapsed_cycles_++;

            if (context->step_callback_cp()) {
                abort_execution_ = true;
            }

            if (cp_->ib_dispatch() && context->step_callback_mesa()) {
                abort_execution_ = true;
            }
        } catch (const std::exception& e) {
            if (context->error_callback) {
                context->error_callback(e);
            }
            abort_execution_ = true;
        }
    }

    cp_cycles_ = 0;

    if (on_execution_state_changed) {
        on_execution_state_changed();
    }
}

} // namespace darkstar
