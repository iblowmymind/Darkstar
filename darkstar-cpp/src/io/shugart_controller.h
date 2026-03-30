/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>
#include <queue>

namespace darkstar {

class DSystem;
class SA1000Drive;

class ShugartController {
public:
    ShugartController(DSystem& system, SA1000Drive& drive);

    void reset();

    void set_k_ctl(uint16_t value);
    void set_k_cmd(uint16_t value);
    void clr_k_flags();
    void k_strobe();

    uint16_t read_k_status();
    uint16_t read_k_test();

    void set_k_o_data(uint16_t value);
    uint16_t read_k_i_data();

    void signal_disk_word_ready();
    void signal_seek_complete();

private:
    enum class ServiceRequest {
        FirmwareEnable = 0,
        SeekComplete = 1,
        IndexFound = 2,
        SectorFound = 3,
        ReadWordReady = 4,
        WriteWordNeeded = 5,
        NoWakeup0 = 6,
        NoWakeup1 = 7,
    };

    enum class TransferType {
        None,
        Read,
        Write,
        Verify,
    };

    enum class WriteState {
        Invalid = 0,
        AutoPreamble,
        Preamble,
        AddressMark,
        Data,
        CRC,
        Complete,
    };

    enum class VerifyState {
        Invalid = 0,
        WaitForAddressMark,
        Data,
        CRC,
    };

    enum class ReadState {
        Invalid = 0,
        WaitForAddressMark,
        Data,
        CRC,
    };

    bool seek_complete_status() const;
    void reset_transfer();

    DSystem& system_;
    SA1000Drive& drive_;

    // KCtl bits
    bool write_enable_ = false;
    int wakeup_control_ = 0;
    bool write_crc_ = false;
    bool transfer_enable_ = false;
    bool firmware_enable_ = false;
    bool direction_in_ = false;
    bool step_ = false;
    bool reduce_iw_ = false;
    bool fault_clear_ = false;
    bool drive_select_ = false;
    int head_select_ = 0;

    ServiceRequest wakeup_request_ = ServiceRequest::NoWakeup0;

    // KStatus bits
    bool verify_error_ = false;
    bool crc_error_ = false;
    bool overrun_ = false;
    bool write_fault_ = false;
    bool sa1000_ = true;
    bool sector_found_ = false;
    bool index_found_ = false;

    // Read/Write state machine
    int transfer_count_ = 0;
    uint32_t read_data_ = 0;
    bool read_word_ready_ = false;

    // Write pipeline
    std::queue<uint16_t> write_pipeline_;

    WriteState write_state_ = WriteState::AutoPreamble;
    VerifyState verify_state_ = VerifyState::WaitForAddressMark;
    ReadState read_state_ = ReadState::WaitForAddressMark;
    TransferType transfer_ = TransferType::None;

    // Address marks
    static constexpr uint32_t kHeaderAddressMark = 0x1a141;
    static constexpr uint32_t kLabelDataAddressMark = 0x1a143;

    // CRC (not actually CRC) value
    static constexpr uint32_t kFakeCRCValue = 0x2beef;

    // Debug metadata
    int debug_sector_ = 0;
    int debug_field_ = 0;
};

} // namespace darkstar
