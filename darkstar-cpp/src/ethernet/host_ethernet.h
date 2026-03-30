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

#ifdef ENABLE_ETHERNET

#include "ethernet/packet_interface.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

// Forward declaration for pcap handle
struct pcap;
typedef struct pcap pcap_t;

namespace darkstar {

/// Represents a host ethernet interface descriptor.
struct EthernetInterface {
    std::string name;
    std::string description;

    std::string to_string() const {
        return name + " (" + description + ")";
    }
};

/// Implements the logic for sending and receiving emulated 10mbit ethernet
/// packets over an actual ethernet interface controlled by the host OS.
/// Uses libpcap to do the dirty work.
class HostEthernetEncapsulation : public IPacketInterface {
public:
    explicit HostEthernetEncapsulation(const std::string& name);
    ~HostEthernetEncapsulation() override;

    void register_receive_callback(ReceivePacketDelegate callback) override;
    void send(const uint16_t* packet, size_t word_count) override;
    void shutdown() override;

private:
    void update_source_address();
    void attach_interface(const std::string& name);
    void open(bool promiscuous, int timeout);
    void begin_receive();
    void receive_thread_proc();

    pcap_t* pcap_handle_ = nullptr;
    ReceivePacketDelegate callback_;

    std::array<uint8_t, 6> source_address_;
    std::array<uint8_t, 6> broadcast_address_;

    std::thread receive_thread_;
    std::atomic<bool> shutdown_flag_{false};
};

} // namespace darkstar

#endif // ENABLE_ETHERNET
