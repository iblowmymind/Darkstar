/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>

namespace darkstar {

// Forward declarations
class Scheduler;
class CentralProcessor;
class IOProcessor;
class MemoryController;
class DisplayController;
class ShugartController;
class SA1000Drive;
class EthernetController;
class FrameTimer;
class IDisplayDevice;

using StepCallbackDelegate = std::function<bool()>;
using ErrorCallbackDelegate = std::function<void(const std::exception&)>;

struct SystemExecutionContext {
    StepCallbackDelegate step_callback_8085;
    StepCallbackDelegate step_callback_cp;
    StepCallbackDelegate step_callback_mesa;
    ErrorCallbackDelegate error_callback;

    SystemExecutionContext(StepCallbackDelegate s8085,
                          StepCallbackDelegate scp,
                          StepCallbackDelegate smesa,
                          ErrorCallbackDelegate err)
        : step_callback_8085(std::move(s8085))
        , step_callback_cp(std::move(scp))
        , step_callback_mesa(std::move(smesa))
        , error_callback(std::move(err)) {}
};

class DSystem {
public:
    DSystem();
    ~DSystem();

    void reset();
    void shutdown(bool commit_disks);

    bool is_executing() const;

    void start_execution(std::shared_ptr<SystemExecutionContext> context);
    void stop_execution();

    void attach_display(IDisplayDevice* display);

    // Accessors
    Scheduler& scheduler() { return *scheduler_; }
    IOProcessor& iop() { return *iop_; }
    CentralProcessor& cp() { return *cp_; }
    MemoryController& memory_controller() { return *memory_controller_; }
    DisplayController& display_controller() { return *display_controller_; }
    ShugartController& shugart_controller() { return *shugart_controller_; }
    SA1000Drive& hard_drive() { return *hard_drive_; }
    EthernetController& ethernet_controller() { return *ethernet_controller_; }
    IDisplayDevice* display() { return display_; }

    // Callback for UI state updates
    std::function<void()> on_execution_state_changed;

private:
    void execution_worker(std::shared_ptr<SystemExecutionContext> context);
    void debug_execution_worker(std::shared_ptr<SystemExecutionContext> context);

    // Hardware components
    std::unique_ptr<Scheduler> scheduler_;
    std::unique_ptr<CentralProcessor> cp_;
    std::unique_ptr<IOProcessor> iop_;
    std::unique_ptr<MemoryController> memory_controller_;
    std::unique_ptr<DisplayController> display_controller_;
    std::unique_ptr<SA1000Drive> hard_drive_;
    std::unique_ptr<ShugartController> shugart_controller_;
    std::unique_ptr<EthernetController> ethernet_controller_;

    // Display
    IDisplayDevice* display_ = nullptr;

    // Execution state
    std::thread execution_thread_;
    std::atomic<bool> abort_execution_{false};
    int cp_cycles_ = 0;
    double elapsed_cycles_ = 0.0;
    std::shared_ptr<SystemExecutionContext> current_context_;

    // Frame timer
    std::unique_ptr<FrameTimer> frame_timer_;

    // Constants
    static constexpr double kCpCyclesPer8085Cycle = 2.43 * 2.0;
    static constexpr double kCpCyclesPerField = 7299270.1 / 38.7;
};

} // namespace darkstar
