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

#include "ethernet/crc32.h"
#include "ethernet/packet_interface.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

namespace darkstar {

// Forward declarations
class DSystem;
struct Event;

/// EthernetController implements the Star's Ethernet controller.
/// At this time, no official documentation exists; only microcode listings
/// and the schematic drawings. The code is implemented based on study of the
/// schematics and diagnostic microcode.
class EthernetController {
public:
    explicit EthernetController(DSystem& system);
    ~EthernetController();

    void shutdown();
    void reset();
    void host_interface_changed();

    /// Returns the ethernet dispatch value for the CP.
    int ether_disp();

    /// Returns the ethernet status register.
    uint16_t e_status();

    /// Write to the output control register.
    void eo_ctl(uint16_t value);

    /// Write to the input control register.
    void ei_ctl(uint16_t value);

    /// Write output data.
    void eo_data(uint16_t value);

    /// Strobe data into/out of the FIFO.
    void e_strobe(int cycle);

    /// Read input data from the FIFO.
    uint16_t ei_data(int cycle);

private:
    enum class ReceiverState {
        Off,
        Preamble,
        Data
    };

    void defer_callback(uint64_t skew_nsec, void* context);
    void start_transmitter();
    void stop_transmitter();
    void transmit_word(uint16_t word);
    void complete_transmission();
    void transmit_callback(uint64_t skew_nsec, void* context);

    void on_host_packet_received(const std::vector<uint8_t>& data);
    void stop_receiver();
    void run_receiver();
    void receiver_poll_callback(uint64_t skew_nsec, void* context);
    void receive_callback(uint64_t skew_nsec, void* context);

    void update_wakeup();
    void attach_host_ethernet();

    DSystem& system_;

    // Output data
    bool output_data_latched_ = false;
    uint16_t output_data_ = 0;
    std::queue<uint16_t> fifo_;
    uint16_t fifo_head_ = 0;

    // Input data
    std::queue<uint16_t> input_packet_;
    std::queue<std::vector<uint8_t>> pending_packets_;

    // Defer timings
    bool tick_elapsed_ = false;

    // Attention flags
    bool out_attn_ = false;
    bool in_attn_ = false;

    // Additional flags
    bool last_r_word_ = false;
    bool even_packet_length_ = false;
    bool purge_ = false;

    // Status bits
    bool turn_off_ = false;
    bool rx_even_len_ = false;
    bool rx_good_crc_ = false;
    bool rx_overrun_ = true;
    bool rx_good_align_ = false;
    bool tx_underrun_ = false;
    bool tx_collision_ = true;
    bool rx_mode_ = true;
    bool enable_tx_ = false;
    bool last_word_ = false;
    bool enable_rcv_ = false;
    bool local_loop_ = false;
    bool loop_back_ = false;

    // EOCtl
    bool defer_ = false;

    // Defer event & timing -- 51.2uS
    Event* defer_event_ = nullptr;
    static constexpr uint64_t kDeferDelay = 51200; // 51.2 * 1000 nsec

    // Transmit event and timing -- 1600nS word interval
    static constexpr uint64_t kTransmitInterval = 1200;
    static constexpr uint64_t kIpgInterval = 9600; // 9.6 * 1000 nsec (inter-packet gap)
    bool transmitter_running_ = false;
    Event* transmit_event_ = nullptr;

    // Receive event, timing, and thread safety
    static constexpr uint64_t kReceiveInterval = 1600;
    static constexpr uint64_t kReceiveIntervalLoopback = 25600;
    bool receiver_running_ = false;
    ReceiverState receiver_state_ = ReceiverState::Off;
    Event* receive_event_ = nullptr;

    static constexpr uint64_t kReceiverPollInterval = 51200; // 51.2 * 1000 nsec

    std::mutex reader_lock_;

    // CRC32 generator
    CRC32 crc32_;
    static constexpr uint32_t kGoodCRC = 0x2144df1c;

    // Host ethernet
    std::unique_ptr<IPacketInterface> host_interface_;
    std::queue<uint16_t> output_packet_;
};

} // namespace darkstar
