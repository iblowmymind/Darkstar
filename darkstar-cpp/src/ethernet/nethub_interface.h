/*
    BSD 2-Clause License

    Copyright Dr. Hans-Walter Latz 2020 and Living Computer Museum + Labs 2018
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

#include "ethernet/packet_interface.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace darkstar {

/// Packet interface for accessing a Dodo NetHub as network device for Darkstar.
class NethubInterface : public IPacketInterface {
public:
    /// This is the name of the NetHub network-device in the configuration
    /// dialog / configuration file.
    static constexpr const char* NETHUB_NAME = "[[ Dodo-Nethub ]]";

    NethubInterface();
    ~NethubInterface() override;

    void register_receive_callback(ReceivePacketDelegate callback) override;
    void send(const uint16_t* packet, size_t word_count) override;
    void shutdown() override;

private:
    void stop_receiver_thread();
    void packet_receiver();
    int get_len_prefix();
    uint8_t get_byte();
    static uint64_t read_address(const uint8_t* data, int offset);

    int socket_fd_ = -1;

    uint64_t local_address_ = 0;
    uint64_t broadcast_address_ = 0;

    ReceivePacketDelegate receiver_;
    std::thread receiver_thread_;
    std::atomic<bool> shutdown_flag_{false};

    // Send buffer - assumes single processing thread
    std::array<uint8_t, 1026> send_buffer_;
};

} // namespace darkstar
