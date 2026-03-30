/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>
#include <vector>
#include "iop/iop_device.h"
#include "iop/dma_interface.h"

namespace darkstar {

class FloppyDrive;
class DSystem;
class IOProcessor;

/// Implements the WD FD1797 controller and the IOP's external floppy state registers.
/// It implements IDMAInterface so that DMA transfers can take place.
class FloppyController : public IIOPDevice, public IDMAInterface {
public:
    FloppyController(FloppyDrive& drive, DSystem& system);

    FloppyDrive& drive() { return drive_; }

    bool interrupt() const { return interrupt_pending_; }

    void reset();

    // IIOPDevice interface
    const int* read_ports() const override;
    int read_port_count() const override;
    const int* write_ports() const override;
    int write_port_count() const override;
    void write_port(int port, uint8_t value) override;
    uint8_t read_port(int port) override;

    // IDMAInterface
    bool drq() const override;
    uint8_t dma_read() override;
    void dma_write(uint8_t value) override;
    void dma_complete() override;

private:
    enum class FDCCommand {
        Restore = 0,
        Seek = 1,
        StepNoUpdate = 2,
        StepUpdate = 3,
        StepInNoUpdate = 4,
        StepInUpdate = 5,
        StepOutNoUpdate = 6,
        StepOutUpdate = 7,
        ReadSectorSingle = 8,
        ReadSectorMultiple = 9,
        WriteSectorSingle = 0xa,
        WriteSectorMultiple = 0xb,
        ReadAddress = 0xc,
        ForceInterrupt = 0x0d,
        ReadTrack = 0xe,
        WriteTrack = 0xf,
    };

    enum class FDCPorts {
        FDCCommand     = 0x84,     // Commands (write)
        FDCStatus      = 0x84,     // Status (read)
        FDCTrack       = 0x85,     // Track register (r/w)
        FDCSector      = 0x86,     // Sector register (r/w)
        FDCData        = 0x87,     // Data register (r/w)
        ExtFDCStatusReg = 0xe8,    // External status register (read)
        ExtFDCState    = 0xe8,     // External state register (write)
    };

    enum FDCStateFlags {
        kFDCEnableWaits = 0x80,
        kFDCPrecomp     = 0x40,
        kFDCSide        = 0x20,
        kFDCDensity     = 0x08,
        kFDCEnableFDC   = 0x04,
        kFDCDriveSelect = 0x01,
        kFDCNone        = 0x00,
    };

    enum class StepDirection {
        In = 0,
        Out = 1,
        Last = 2,
    };

    struct Type1CommandParams {
        Type1CommandParams() : update(false), head_load(false), verify(false) {}
        Type1CommandParams(int p)
            : update((p & 0x10) != 0)
            , head_load((p & 0x08) != 0)
            , verify((p & 0x04) != 0) {}
        bool update;
        bool head_load;
        bool verify;
    };

    struct Type2CommandParams {
        Type2CommandParams(int p)
            : sector_length((p & 0x8) != 0)
            , delay((p & 0x4) != 0)
            , side_select((p & 0x2) != 0)
            , data_address_mark((p & 0x1) != 0) {}
        bool sector_length;
        bool delay;
        bool side_select;
        bool data_address_mark;
    };

    enum class TrackParseState {
        Gap4,
        IndexMark,
        IDRecordMark,
        DataRecordMark,
    };

    enum WriteTrackMarkers {
        kGap4MFM       = 0x4e,
        kGap4FM        = 0xff,
        kSoftIndex     = 0xfc,
        kSectorIDRecord = 0xfe,
        kDataRecord    = 0xfb,
        kCRC           = 0xf7,
    };

    void reset_flags();
    uint8_t read_status();
    uint8_t read_ext_status();

    void enable_fdc();
    void disable_fdc();

    void do_command(int command_data);
    void seek(int track, Type1CommandParams p);
    void step(StepDirection direction, Type1CommandParams p);
    void sector_transfer(Type2CommandParams p, bool read);
    void write_track();
    void force_interrupt(int flags);

    void seek_callback(uint64_t skew_nsec, void* context);
    void finish_data_transfer();
    void finish_write_track();

    void clear_interrupt();
    void raise_interrupt();

    bool not_ready() const;
    bool write_protect() const;
    bool track0() const;
    bool index() const;

    FloppyDrive& drive_;
    DSystem& system_;

    // FDC data
    uint8_t fdc_track_;
    uint8_t fdc_sector_;
    uint8_t fdc_data_;

    // Sector data
    std::vector<uint8_t>* sector_buffer_;
    int sector_data_index_;

    // Track data (for WriteTrack)
    std::vector<uint8_t> write_track_buffer_;
    int write_track_data_index_;
    bool write_track_side_;

    // Status flags
    bool crc_error_;
    bool busy_;
    bool head_loaded_;
    bool seek_error_;
    bool record_type_write_fault_;
    bool rnf_;
    bool lost_data_;
    bool drq_;
    mutable int drq_counter_;

    // Overrides drive Index signal immediately after FDC reset.
    bool index_reset_;

    bool fdc_enabled_;
    bool interrupt_pending_;

    FDCCommand last_command_;
    uint8_t ext_state_;

    // Seek state
    int seek_destination_;
    StepDirection step_direction_;

    // Density and side select
    bool double_density_;
    bool side_select_;

    // Command execution state
    bool command_abort_;

    // Timing constants
    static constexpr uint64_t kCommandBeginNsec = 12 * 1000ULL;        // 12 usec
    static constexpr uint64_t kStepTimeNsec = 6 * 1000000ULL;          // 6 msec
    static constexpr uint64_t kResetIndexDuration = 10 * 1000000ULL;   // 10 msec

    // Allocated on heap so it survives scheduled callback
    Type1CommandParams* pending_seek_params_;

    static const int kReadPorts[];
    static const int kWritePorts[];
};

} // namespace darkstar
