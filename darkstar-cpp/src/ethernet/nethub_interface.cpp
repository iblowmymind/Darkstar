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

#include "ethernet/nethub_interface.h"
#include "core/configuration.h"
#include "core/log.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
using ssize_t = int;
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace darkstar {

namespace {

#ifdef _WIN32
void close_socket(int fd) {
    closesocket(fd);
}

void init_sockets() {
    static bool initialized = false;
    if (!initialized) {
        WSADATA wsa_data;
        WSAStartup(MAKEWORD(2, 2), &wsa_data);
        initialized = true;
    }
}
#else
void close_socket(int fd) {
    ::close(fd);
}

void init_sockets() {
    // No-op on POSIX
}
#endif

} // anonymous namespace

NethubInterface::NethubInterface() {
    init_sockets();

    local_address_ = Configuration::host_id;
    broadcast_address_ = 0x0000FFFFFFFFFFFFULL;

    // Connect to the NetHub via TCP
    struct addrinfo hints;
    struct addrinfo* result = nullptr;

    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    std::string port_str = std::to_string(Configuration::nethub_port);

    int status = getaddrinfo(Configuration::nethub_host.c_str(),
                             port_str.c_str(), &hints, &result);
    if (status != 0 || result == nullptr) {
        throw std::runtime_error("Failed to resolve NetHub host address.");
    }

    socket_fd_ = static_cast<int>(
        socket(result->ai_family, result->ai_socktype, result->ai_protocol));
    if (socket_fd_ < 0) {
        freeaddrinfo(result);
        throw std::runtime_error("Failed to create socket for NetHub connection.");
    }

    if (connect(socket_fd_, result->ai_addr,
                static_cast<int>(result->ai_addrlen)) < 0) {
        close_socket(socket_fd_);
        socket_fd_ = -1;
        freeaddrinfo(result);
        throw std::runtime_error("Failed to connect to NetHub.");
    }

    freeaddrinfo(result);

    // Disable Nagle's algorithm for low latency
    int flag = 1;
    setsockopt(socket_fd_, IPPROTO_TCP, TCP_NODELAY,
               reinterpret_cast<const char*>(&flag), sizeof(flag));
}

NethubInterface::~NethubInterface() {
    shutdown();
}

void NethubInterface::register_receive_callback(ReceivePacketDelegate callback) {
    stop_receiver_thread();
    receiver_ = std::move(callback);
    shutdown_flag_ = false;
    receiver_thread_ = std::thread(&NethubInterface::packet_receiver, this);
}

void NethubInterface::send(const uint16_t* packet, size_t word_count) {
    if (socket_fd_ < 0) {
        return;
    }

    int byte_len = static_cast<int>(word_count * 2);

    // Build the NetHub transmission packet
    int src = 0;
    int dst = 0;

    // First word is the packet length (big-endian)
    send_buffer_[dst++] = static_cast<uint8_t>((byte_len >> 8) & 0xFF);
    send_buffer_[dst++] = static_cast<uint8_t>(byte_len & 0xFF);

    // Then the packet itself
    int limit = std::min(byte_len + 2, static_cast<int>(send_buffer_.size()));
    while (dst < limit) {
        uint16_t w = packet[src++];
        send_buffer_[dst++] = static_cast<uint8_t>((w >> 8) & 0xFF);
        send_buffer_[dst++] = static_cast<uint8_t>(w & 0xFF);
    }

    // Transmit it
    int total_sent = 0;
    while (total_sent < dst) {
        ssize_t sent = ::send(socket_fd_,
                              reinterpret_cast<const char*>(send_buffer_.data() + total_sent),
                              dst - total_sent, 0);
        if (sent <= 0) {
            // Send failed, ignore
            break;
        }
        total_sent += static_cast<int>(sent);
    }
}

void NethubInterface::shutdown() {
    shutdown_flag_ = true;

    if (socket_fd_ >= 0) {
#ifdef _WIN32
        ::shutdown(socket_fd_, SD_BOTH);
#else
        ::shutdown(socket_fd_, SHUT_RDWR);
#endif
        close_socket(socket_fd_);
        socket_fd_ = -1;
    }

    stop_receiver_thread();
    receiver_ = nullptr;
}

void NethubInterface::stop_receiver_thread() {
    shutdown_flag_ = true;
    if (receiver_thread_.joinable()) {
        receiver_thread_.join();
    }
}

void NethubInterface::packet_receiver() {
    std::array<uint8_t, 1024> data;

    while (!shutdown_flag_) {
        if (socket_fd_ < 0) {
            return;
        }

        // Wait for next packet from the nethub
        int byte_len;
        try {
            byte_len = get_len_prefix();
        } catch (...) {
            return; // Connection closed or error
        }

        // Read the packet content up to the buffer size
        int pos = 0;
        while (pos < byte_len && pos < static_cast<int>(data.size())) {
            try {
                data[pos++] = get_byte();
            } catch (...) {
                return;
            }
        }

        // Swallow exceeding bytes in the packet
        while (pos < byte_len) {
            pos++;
            try {
                get_byte();
            } catch (...) {
                return;
            }
        }

        int actual_len = std::min(byte_len, static_cast<int>(data.size()));

        // Check if it is for us
        if (actual_len >= 12) {
            uint64_t dst_address = read_address(data.data(), 0);
            // uint64_t src_address = read_address(data.data(), 6); // available for logging

            if (dst_address == local_address_ || dst_address == broadcast_address_) {
                // Pass it to the ethernet controller
                if (receiver_) {
                    std::vector<uint8_t> packet_data(data.data(),
                                                     data.data() + actual_len);
                    receiver_(packet_data);
                }
            }
        }
    }
}

int NethubInterface::get_len_prefix() {
    int b1 = get_byte() & 0xFF;
    int b2 = get_byte() & 0xFF;
    return (b1 << 8) | b2;
}

uint8_t NethubInterface::get_byte() {
    uint8_t byte_val;
    ssize_t result = recv(socket_fd_, reinterpret_cast<char*>(&byte_val), 1, 0);
    if (result <= 0) {
        throw std::runtime_error("Connection closed or read error.");
    }
    return byte_val;
}

uint64_t NethubInterface::read_address(const uint8_t* data, int offset) {
    uint64_t addr = 0;
    for (int i = 0; i < 6; i++) {
        addr <<= 8;
        addr |= static_cast<uint64_t>(data[offset + i] & 0xFF);
    }
    return addr;
}

} // namespace darkstar
