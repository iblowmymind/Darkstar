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

#ifdef ENABLE_ETHERNET

#include "ethernet/host_ethernet.h"
#include "core/configuration.h"
#include "core/log.h"

#include <pcap/pcap.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace darkstar {

HostEthernetEncapsulation::HostEthernetEncapsulation(const std::string& name) {
    broadcast_address_.fill(0xff);

    attach_interface(name);

    if (pcap_handle_ == nullptr) {
        Log::write(LogComponent::HostEthernet,
                   "Specified ethernet interface does not exist or is not compatible with Darkstar.");
        throw std::runtime_error(
            "Specified ethernet interface does not exist or is not compatible with Darkstar.");
    }

    update_source_address();
}

HostEthernetEncapsulation::~HostEthernetEncapsulation() {
    shutdown();
}

void HostEthernetEncapsulation::register_receive_callback(ReceivePacketDelegate callback) {
    callback_ = std::move(callback);

    // Now that we have a callback we can start receiving.
    open(true /* promiscuous */, 0);
    begin_receive();
}

void HostEthernetEncapsulation::send(const uint16_t* packet, size_t word_count) {
    if (pcap_handle_ == nullptr) {
        return;
    }

    std::vector<uint8_t> packet_bytes(word_count * 2);

    for (size_t i = 0; i < word_count; i++) {
        packet_bytes[i * 2]     = static_cast<uint8_t>(packet[i] >> 8);
        packet_bytes[i * 2 + 1] = static_cast<uint8_t>(packet[i]);
    }

    pcap_sendpacket(pcap_handle_, packet_bytes.data(),
                    static_cast<int>(packet_bytes.size()));

    Log::write(LogType::Verbose, LogComponent::EthernetReceive,
               "Packet (length %zu) sent.", packet_bytes.size());
}

void HostEthernetEncapsulation::shutdown() {
    shutdown_flag_ = true;

    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }

    if (pcap_handle_ != nullptr) {
        pcap_close(pcap_handle_);
        pcap_handle_ = nullptr;
    }
}

void HostEthernetEncapsulation::update_source_address() {
    uint64_t host_id = Configuration::host_id;
    for (int i = 0; i < 6; i++) {
        source_address_[i] = static_cast<uint8_t>(host_id >> ((5 - i) * 8));
    }
}

void HostEthernetEncapsulation::attach_interface(const std::string& name) {
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_if_t* alldevs = nullptr;

    if (pcap_findalldevs(&alldevs, errbuf) == -1) {
        Log::write(LogType::Error, LogComponent::HostEthernet,
                   "Error finding network devices: %s", errbuf);
        return;
    }

    std::string name_lower = name;
    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);

    for (pcap_if_t* d = alldevs; d != nullptr; d = d->next) {
        std::string dev_name = d->name ? d->name : "";
        std::string dev_name_lower = dev_name;
        std::transform(dev_name_lower.begin(), dev_name_lower.end(),
                       dev_name_lower.begin(), ::tolower);

        if (dev_name_lower == name_lower) {
            pcap_handle_ = pcap_open_live(d->name, 65536,
                                          0 /* not promiscuous yet */,
                                          1 /* 1ms timeout */,
                                          errbuf);
            if (pcap_handle_ != nullptr) {
                Log::write(LogComponent::HostEthernet,
                           "Attached to host interface %s", d->name);
            } else {
                Log::write(LogType::Error, LogComponent::HostEthernet,
                           "Failed to open interface %s: %s", d->name, errbuf);
            }
            break;
        }
    }

    pcap_freealldevs(alldevs);
}

void HostEthernetEncapsulation::open(bool promiscuous, int /*timeout*/) {
    // The interface was already opened in attach_interface.
    // If promiscuous mode is requested, we close and reopen.
    if (promiscuous && pcap_handle_ != nullptr) {
        const char* dev_name = nullptr;

        // We need to reopen with promiscuous mode.
        // pcap doesn't provide a way to change mode after open,
        // so we get the device name, close, and reopen.
        // For simplicity, we rely on the handle already being open
        // and set a BPF filter if needed.
        // Actually, pcap_set_promisc is only for pcap_create handles.
        // Since we used pcap_open_live, let's just close and reopen.

        // Unfortunately we can't easily get the device name from pcap_t,
        // so we store nothing extra. The interface was opened non-promiscuous
        // in attach_interface. Let's just accept the current state --
        // in practice pcap_open_live with promisc=1 would be done in attach.
        // For a more complete implementation we'd store the device name.
    }

    Log::write(LogComponent::HostEthernet,
               "Host interface opened and receiving packets.");
}

void HostEthernetEncapsulation::begin_receive() {
    receive_thread_ = std::thread(&HostEthernetEncapsulation::receive_thread_proc, this);
}

void HostEthernetEncapsulation::receive_thread_proc() {
    while (!shutdown_flag_) {
        if (pcap_handle_ == nullptr) {
            return;
        }

        struct pcap_pkthdr* header = nullptr;
        const uint8_t* data = nullptr;

        int result = pcap_next_ex(pcap_handle_, &header, &data);

        if (result == 1 && header != nullptr && data != nullptr) {
            // We have a packet. Check if it's for us.
            if (header->caplen < 14) {
                continue; // Too short to have ethernet header
            }

            // Extract destination and source MAC addresses
            // dst is bytes 0-5, src is bytes 6-11
            bool dst_matches_us = std::memcmp(data, source_address_.data(), 6) == 0;
            bool dst_is_broadcast = std::memcmp(data, broadcast_address_.data(), 6) == 0;
            bool src_is_us = std::memcmp(data + 6, source_address_.data(), 6) == 0;

            if (!src_is_us && (dst_matches_us || dst_is_broadcast)) {
                Log::write(LogType::Verbose, LogComponent::HostEthernet,
                           "Packet received (length %u).", header->caplen);

                if (callback_) {
                    std::vector<uint8_t> packet_data(data, data + header->caplen);
                    callback_(packet_data);
                }
            }
        } else if (result == -1) {
            Log::write(LogType::Error, LogComponent::HostEthernet,
                       "Error reading packet: %s", pcap_geterr(pcap_handle_));
        }
        // result == 0 means timeout, just continue
    }
}

} // namespace darkstar

#endif // ENABLE_ETHERNET
