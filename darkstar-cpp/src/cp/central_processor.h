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
    DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#pragma once

#include <cstdint>
#include <cstring>

#include "cp/am2901.h"
#include "cp/microinstruction.h"
#include "cp/task_type.h"
#include "iop/iop_device.h"
#include "iop/dma_interface.h"

namespace darkstar {

class DSystem;

// From SysDefs.asm:
// Bits 0:5 - (IOPWait', SwTAddr', IOPattn, CPDmaMode, CPDmaIn)
// (in the usual Xerox reverse order)
enum class CPControlFlags {
    IOPWait_  = 0x80,
    SwTAddr_  = 0x40,
    IOPattn   = 0x20,
    CPDmaMode = 0x10,
    CPDmaIn   = 0x08,
};

// From IOP schematics, p 15-17:
enum class CPStatusFlags {
    CPAttn          = 0x80,
    EmuWake         = 0x40,
    IOPAttn_        = 0x20,
    CPDmaMode_      = 0x10,
    CPDmaIn_        = 0x08,
    CPInIntReq_     = 0x04,
    CPOutIntReq_    = 0x02,
    CPDmaComplete_  = 0x01,
};

enum class IOPCtlFlags {
    EmuWake   = 0x8,
    CPAttn    = 0x4,
    WakeMode0 = 0x2,
    WakeMode1 = 0x1,
};

enum class IOPStatusFlags {
    IOPAttn    = 0x20,
    EmuWake_   = 0x10,
    CPAttn_    = 0x08,
    WakeMode0_ = 0x04,
    WakeMode1_ = 0x02,
    IOPReq     = 0x01,
};

enum class ClickType {
    Ethernet0 = 0,
    Disk,
    IOP,
    Ethernet1,
    Display,
};

/// IB state, corresponding to the value of iBPtr
enum class IBState {
    Full  = 2,
    Word  = 3,
    Byte  = 1,
    Empty = 0,
};

/// The Dandelion Central Processor.
/// Implements the microcode execution engine, task management,
/// and IOP communication channel.
class CentralProcessor : public IIOPDevice, public IDMAInterface {
public:
    explicit CentralProcessor(DSystem& system);

    void reset();

    // Microcode execution
    void execute_instruction(int cycles);

    // Task management
    void wake_task(TaskType task);
    void sleep_task(TaskType task);

    // Accessors
    uint64_t* microcode_ram() { return microcode_; }
    const uint64_t* microcode_ram() const { return microcode_; }

    uint8_t* rh() { return rh_; }
    const uint8_t* rh() const { return rh_; }

    uint16_t* u() { return u_; }
    const uint16_t* u() const { return u_; }

    TaskType current_task() const { return current_task_; }

    int* tpc() { return tpc_; }
    const int* tpc() const { return tpc_; }

    int nia_modifier() const { return nia_modifier_; }

    AM2901& alu() { return alu_; }
    const AM2901& alu() const { return alu_; }

    int cycle() const { return cycle_; }

    int stack_p() const { return stack_p_; }

    uint8_t ib_front() const { return ib_front_; }

    uint8_t* ib() { return ib_; }
    const uint8_t* ib() const { return ib_; }

    IBState ib_ptr() const { return ib_ptr_; }

    bool pc16() const { return pc16_; }

    /// Whether the processor is waiting to be awoken by the IOP.
    bool iop_wait() const { return iop_wait_; }

    /// Used by debuggers to allow stepping macrocode
    bool ib_dispatch() {
        bool value = ib_dispatch_;
        ib_dispatch_ = false;
        return value;
    }

    // IIOPDevice interface
    const int* read_ports() const override;
    int read_port_count() const override;
    const int* write_ports() const override;
    int write_port_count() const override;
    void write_port(int port, uint8_t value) override;
    uint8_t read_port(int port) override;

    // IDMAInterface interface
    bool drq() const override;
    uint8_t dma_read() override;
    void dma_write(uint8_t value) override;
    void dma_complete() override;

private:
    // IOP port enums
    enum class PortReadRegister {
        CPDataIn = 0xeb,
        CPStatus = 0xec,
        CPCS0    = 0xf8,
        CPCS1    = 0xf9,
        CPCS2    = 0xfa,
        CPCS3    = 0xfb,
        CPCS4    = 0xfc,
        CPCS5    = 0xfd,
        CPCS6    = 0xfe,
        CPCS7    = 0xff,
    };

    enum class PortWriteRegister {
        CPDataOut       = 0xeb,
        CPControl       = 0xec,
        CPClrDmaComplete = 0xee,
        CPCSa           = 0xf8,
        CPCSb           = 0xf9,
        CPCSc           = 0xfa,
        CPCSd           = 0xfb,
        CPCSe           = 0xfc,
        CPCSf           = 0xfd,
        TPCHigh         = 0xfe,
        TPCLow          = 0xff,
    };

    // From the IOP schematic:
    //  - 00 = Disabled (no wakeups)
    //  - 01 = Input (wakeup when Input from IOP is available)
    //  - 10 = Output (wakeup when IOP is ready for data from CP)
    //  - 11 = Always wake up
    enum class IOPTaskWakeMode {
        Disabled = 0,
        Input,
        Output,
        Always,
    };

    enum class ErrorTrap {
        ControlStoreParity  = 0,
        EmulatorMemoryError = 1,
        StackOverUnderflow  = 2,
        IBEmpty             = 3,
    };

    enum class NiaModifierType {
        Normal,
        IBDispatch,
        IBRefillTrap,
    };

    // Task switching
    void task_switch();
    void do_task_switch(TaskType new_task);
    bool wake_status(TaskType task) const;

    // Error handling
    void signal_error_trap(ErrorTrap err);

    // IB management
    void decrement_ib_ptr();

    // IOP communication helpers (in central_processor_io.cpp)
    uint8_t read_cp_out_buffer();
    void write_cp_in_buffer(uint8_t value);
    void write_iop_microcode_word(int b, uint8_t value);
    uint8_t read_iop_microcode_word(int b);
    void write_cp_ctl(uint8_t value);
    uint8_t read_cp_status();
    void write_iop_ctl(uint8_t value);
    uint8_t read_iop_status();
    uint8_t read_iop_data();
    void write_iop_data(uint8_t value);
    void update_iop_task_wakeup();

    // The D System we belong to
    DSystem& system_;

    // Task/Temporary Program Counters
    int tpc_[8] = {};

    // Task/Temporary Condition bits (NIA modifiers). Only 4 bits.
    int tc_[8] = {};

    // Task wakeups
    bool wakeup_[8] = {};

    // Current task
    TaskType current_task_ = TaskType::Kernel;

    // Microcode store
    uint64_t microcode_[4096] = {};

    // Microcode decode cache
    Microinstruction microcode_cache_[4096] = {};

    // 2901 ALU
    AM2901 alu_;

    // RH registers, 8 bit
    uint8_t rh_[16] = {};

    // Link registers, 4 bit
    // See section 2.5.4 of the HW ref;
    // Link is addressed by fX and is written with the low nibble of NIAX when
    // fX is in 0..7 and NIA[7] = 0;
    // A Link register is or'd into the low nibble of INIA when fX is in 0..7 and
    // NIA[7] = 1.
    int link_[8] = {};

    // U registers
    uint16_t u_[256] = {};

    // Instruction buffer (IB)
    uint8_t ib_front_ = 0;
    uint8_t ib_[2] = {};
    IBState ib_ptr_ = IBState::Empty;
    bool ib_empty_cancel_ = false;

    // Table of values for next ibPtr value when decrementing ibPtr.
    static constexpr IBState kNextIBPtr[4] = {
        IBState::Empty,   // 0 (Empty) -> Empty
        IBState::Empty,   // 1 (Byte)  -> Empty
        IBState::Word,    // 2 (Full)  -> Word
        IBState::Byte,    // 3 (Word)  -> Byte
    };

    // Stack pointer, 4 bits
    int stack_p_ = 0;

    // pc16 register, 1 bit
    bool pc16_ = false;

    // Bus data
    uint16_t x_bus_ = 0;
    uint16_t y_bus_ = 0;

    // NIA modifier for branch/dispatch
    int nia_modifier_ = 0;
    NiaModifierType nia_modifier_type_ = NiaModifierType::Normal;

    // AltUAddress flag
    bool alt_u_addr_ = false;

    // Interrupt flags
    bool m_int_ = false;

    // Error state
    // The EKErr register, names the type of error:
    //   0 - control store parity error
    //   1 - Emulator memory error
    //   2 - stackPointer overflow or underflow
    //   3 - IB-Empty error
    int ek_err_ = 0;
    bool emulator_error_trap_ = false;
    int emulator_error_trap_click_count_ = 0;

    // Whether a PageCross branch occurred during the last MAR<- operation.
    bool mar_page_cross_br_ = false;

    // Cycle / Click / Round data
    int cycle_ = 1;             // c1 ... c3
    ClickType click_ = ClickType::Ethernet0;  // 0 ... 4

    // Whether to exit the Kernel task at the end of this click
    bool exit_kernel_ = false;

    // Debugging flag: Indicates that an IBDispatch has occurred
    bool ib_dispatch_ = false;

    //
    // IOP communication state (in central_processor_io.cpp)
    //

    // Control data, IOP
    bool cp_dma_complete_ = false;
    bool iop_wait_ = true;       // Waiting for IOP to wake us
    bool sw_t_addr_ = false;
    bool iop_attn_ = false;
    bool cp_dma_mode_ = false;
    bool cp_dma_in_ = false;

    // Control data, CP
    bool wake_mode1_ = false;
    bool wake_mode0_ = false;
    bool cp_attn_ = false;
    bool emu_wake_ = false;
    IOPTaskWakeMode wake_mode_ = IOPTaskWakeMode::Disabled;

    // Status data
    bool cp_out_int_req_ = true;
    bool cp_in_int_req_ = true;
    bool out_latched_ = false;       // Data from CP->IOP latched
    bool in_latched_ = false;        // Data from IOP->CP latched
    bool iop_req_ = false;

    // CP<->IOP data buffers
    uint8_t cp_out_data_ = 0;       // OUT from IOP (CP reads)
    uint8_t cp_in_data_ = 0;        // IN from CP (IOP reads)

    // Used as TPC address when IOP is writing control store or modifying TPC values.
    int tpc_addr_ = 0;

    // Temporary used when loading TPC values; stores high bits of new TPC address.
    int tpc_temp_ = 0;

    // IOP port data
    static const int kReadPorts[];
    static const int kWritePorts[];
    static constexpr int kReadPortCount = 10;
    static constexpr int kWritePortCount = 11;
};

} // namespace darkstar
