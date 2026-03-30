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

#include "ethernet/ethernet_controller.h"
#include "ethernet/nethub_interface.h"
#ifdef ENABLE_ETHERNET
#include "ethernet/host_ethernet.h"
#endif

#include "core/configuration.h"
#include "core/log.h"
#include "core/scheduler.h"
#include "core/system.h"
#include "cp/task_type.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace darkstar {

EthernetController::EthernetController(DSystem& system)
    : system_(system)
{
    attach_host_ethernet();

    // Start the ethernet receiver poll event; this will run forever.
    system_.scheduler().schedule(
        kReceiverPollInterval,
        [this](uint64_t skew, void* ctx) { receiver_poll_callback(skew, ctx); });

    reset();
}

EthernetController::~EthernetController() {
    shutdown();
}

void EthernetController::shutdown() {
    if (host_interface_) {
        host_interface_->shutdown();
        host_interface_.reset();
    }
}

void EthernetController::reset() {
    turn_off_ = false;
    rx_even_len_ = false;
    rx_good_crc_ = false;
    rx_overrun_ = true;
    tx_underrun_ = false;
    tx_collision_ = true;
    rx_mode_ = true;
    enable_tx_ = false;
    last_word_ = false;
    enable_rcv_ = false;
    local_loop_ = false;
    loop_back_ = false;
    defer_ = false;

    purge_ = false;
    tick_elapsed_ = false;
    out_attn_ = false;
    in_attn_ = false;
    last_r_word_ = false;
    transmitter_running_ = false;
    even_packet_length_ = false;

    output_data_ = 0;
    output_data_latched_ = false;

    // Clear FIFO
    std::queue<uint16_t>().swap(fifo_);
    fifo_head_ = 0;

    // Clear input packet
    std::queue<uint16_t>().swap(input_packet_);

    crc32_.reset();
}

void EthernetController::host_interface_changed() {
    // Shut down previous host interface (if any) and attach a new one.
    if (host_interface_) {
        host_interface_->shutdown();
        host_interface_.reset();
    }

    attach_host_ethernet();
}

int EthernetController::ether_disp() {
    //
    // Pin 139(YIODisp.1) : Hooked to "Attn," which indicates whether any
    //                       attention is needed by the receiver or transmitter.
    // Pin 39(YIODisp.0)  : Must be zero for the transmitting inner loop uCode.
    //                       It is also used to determine if the Option card is
    //                       plugged in.
    //
    int value = 0;

    if (turn_off_) {
        // Ethernet is not turned off; returned value is based on whether
        // the transmitter or receiver hardware has a status to report.
        value = (out_attn_ || in_attn_) ? 1 : 0;
    }

    return value;
}

uint16_t EthernetController::e_status() {
    uint16_t value = static_cast<uint16_t>(
        ~((turn_off_       ? 0x0001 : 0x00) |
          (rx_even_len_    ? 0x0002 : 0x00) |
          (rx_good_crc_    ? 0x0004 : 0x00) |
          (rx_overrun_     ? 0x0008 : 0x00) |
          (rx_good_align_  ? 0x0010 : 0x00) |
          (!tx_underrun_   ? 0x0020 : 0x00) |
          (tx_collision_   ? 0x0040 : 0x00) |
          (rx_mode_        ? 0x0080 : 0x00) |
          (enable_tx_      ? 0x0100 : 0x00) |
          (last_word_      ? 0x0200 : 0x00) |
          (enable_rcv_     ? 0x0400 : 0x00) |
          (local_loop_     ? 0x0800 : 0x00) |
          (loop_back_      ? 0x1000 : 0x00)));

    return value;
}

void EthernetController::eo_ctl(uint16_t value) {
    // EOCtl:             Bit(etc)
    // ----------------------------
    // EnableTrn          15
    // LastWord           14
    // Defer              13
    enable_tx_ = (value & 0x1) != 0;
    last_word_ = (value & 0x2) != 0;
    defer_ = (value & 0x4) != 0;

    out_attn_ = false;
    tx_underrun_ = false;
    tx_collision_ = true;

    if (Log::enabled) {
        Log::write(LogComponent::EthernetControl,
                   "EOCtl<- 0x%04x: enableTx %d lastWord %d defer %d",
                   value, enable_tx_, last_word_, defer_);
    }

    // Writing EOCtl resets the defer clock
    tick_elapsed_ = false;
    system_.scheduler().cancel(defer_event_);

    if (defer_) {
        // Queue up an event 51.2uS in the future
        defer_event_ = system_.scheduler().schedule(
            kDeferDelay,
            [this](uint64_t skew, void* ctx) { defer_callback(skew, ctx); });
    } else {
        // Start the transmitter running if need be
        if (enable_tx_ && !transmitter_running_ && !last_word_) {
            crc32_.reset();
            start_transmitter();
        }
    }

    if (!enable_tx_) {
        std::queue<uint16_t>().swap(fifo_);
        fifo_head_ = 0;
        stop_transmitter();
    }

    update_wakeup();

    if (Log::enabled) {
        Log::write(LogComponent::EthernetControl, "EOCtl end.");
    }
}

void EthernetController::ei_ctl(uint16_t value) {
    // EICtl:             Bit(xerox order)
    // ------------------------------------
    // EnableRcv          15
    // TurnOff'           14
    // LocalLoop          13
    // LoopBack           12
    enable_rcv_ = (value & 0x1) != 0;
    turn_off_ = (value & 0x2) == 0;
    local_loop_ = (value & 0x4) != 0;
    loop_back_ = (value & 0x8) != 0;

    if (!enable_rcv_) {
        // Reset receive state
        rx_mode_ = true;
        receiver_state_ = ReceiverState::Preamble;
        in_attn_ = false;
        last_r_word_ = false;

        rx_good_crc_ = true;
        rx_good_align_ = true;
        rx_overrun_ = true;
        rx_even_len_ = true;

        if (!loop_back_) {
            std::queue<uint16_t>().swap(fifo_);
            fifo_head_ = 0;
        }

        std::queue<uint16_t>().swap(input_packet_);
        crc32_.reset();
    }

    update_wakeup();

    if (Log::enabled) {
        Log::write(LogComponent::EthernetControl,
                   "EICtl<- 0x%04x enablerx %d turnOff' %d localLoop %d loopBack %d.",
                   value, enable_rcv_, turn_off_, local_loop_, loop_back_);
    }
}

void EthernetController::eo_data(uint16_t value) {
    if (Log::enabled) {
        Log::write(LogComponent::EthernetControl, "EOData<- 0x%04x.", value);
    }

    output_data_ = value;
    output_data_latched_ = true;
}

void EthernetController::e_strobe(int cycle) {
    if (Log::enabled) {
        Log::write(LogComponent::EthernetControl, "EStrobe.");
    }

    if ((cycle == 1 || cycle == 3) && !last_word_) {
        // Strobe output data into FIFO.

        if (!output_data_latched_) {
            if (Log::enabled) {
                Log::write(LogType::Error, LogComponent::EthernetControl,
                           "EStrobe: no data latched.");
            }
            out_attn_ = false;
        }

        // Move data from output data word into the FIFO.
        if (fifo_.size() < 16) {
            fifo_.push(output_data_);
            output_data_latched_ = false;

            if (Log::enabled) {
                Log::write(LogComponent::EthernetControl,
                           "EStrobe: loaded word 0x%04x into FIFO. FIFO count is now %zu",
                           output_data_, fifo_.size());
            }

            out_attn_ = false;
            update_wakeup();
        } else {
            fifo_.pop();
            fifo_.push(output_data_);
            if (Log::enabled) {
                Log::write(LogType::Error, LogComponent::EthernetControl,
                           "EStrobe: FIFO full, dropping word.");
            }
        }
    } else if (cycle == 2) {
        if (Log::enabled) {
            Log::write(LogComponent::EthernetControl,
                       "EStrobe: (cycle 2) flushing received data.");
        }

        // Throw out input data and stop the receiver
        std::queue<uint16_t>().swap(input_packet_);
        std::queue<uint16_t>().swap(fifo_);
        fifo_head_ = 0;
        in_attn_ = false;
        last_r_word_ = false;
        rx_mode_ = true;
        stop_receiver();
        update_wakeup();
    }
}

uint16_t EthernetController::ei_data(int cycle) {
    uint16_t value = 0;

    // Read from the input/output FIFO.
    if (!fifo_.empty()) {
        // If cycle == 2 we dequeue the next item from the FIFO;
        // otherwise the last-dequeued item is returned.
        if (cycle == 2) {
            fifo_head_ = fifo_.front();
            fifo_.pop();
        }

        value = fifo_head_;

        if (Log::enabled) {
            Log::write(LogComponent::EthernetReceive,
                       "   <-EIData: Returning FIFO word 0x%04x. FIFO count is now %zu",
                       value, fifo_.size());
        }
    } else {
        if (Log::enabled) {
            Log::write(LogType::Error, LogComponent::EthernetReceive,
                       "   <-EIData: FIFO empty.");
        }
    }

    if (fifo_.empty() && (last_r_word_ || loop_back_ || local_loop_)) {
        {
            std::lock_guard<std::mutex> lock(reader_lock_);
            receiver_running_ = false;
        }

        last_r_word_ = false;

        // Let microcode know the packet is done.
        in_attn_ = true;

        // Update CRC and other final status flags.
        rx_mode_ = false;
        rx_good_crc_ = (loop_back_ || local_loop_) ? (crc32_.checksum() == kGoodCRC) : true;
        rx_even_len_ = (loop_back_ || local_loop_) ? true : even_packet_length_;

        if (Log::enabled) {
            Log::write(LogComponent::EthernetReceive,
                       "   <-EIData: completing transfer.");
        }
    }

    update_wakeup();

    return value;
}

void EthernetController::defer_callback(uint64_t /*skew_nsec*/, void* /*context*/) {
    tick_elapsed_ = true;
    update_wakeup();
    tick_elapsed_ = false;

    // Start the transmitter.
    if (enable_tx_ && !transmitter_running_) {
        if (Log::enabled) {
            Log::write(LogComponent::EthernetControl,
                       "Defer complete, starting transmitter.");
        }
        start_transmitter();
    }
}

void EthernetController::start_transmitter() {
    if (!transmitter_running_) {
        // First abort any transmit clock that may be running.
        system_.scheduler().cancel(transmit_event_);

        // Schedule the transmission callback.
        transmit_event_ = system_.scheduler().schedule(
            kIpgInterval,
            [this](uint64_t skew, void* ctx) { transmit_callback(skew, ctx); });

        // Clear the output packet.
        std::queue<uint16_t>().swap(output_packet_);

        transmitter_running_ = true;
    } else {
        throw std::logic_error("Transmitter already running.");
    }
}

void EthernetController::stop_transmitter() {
    system_.scheduler().cancel(transmit_event_);
    transmitter_running_ = false;
}

void EthernetController::transmit_word(uint16_t word) {
    if (local_loop_ || loop_back_) {
        // Loop back to FIFO through the receiver.
        // Append this word to the input packet.
        input_packet_.push(word);

        // Ensure the receiver is running.
        run_receiver();
    } else {
        // Append to outgoing packet.
        output_packet_.push(word);
    }
}

void EthernetController::complete_transmission() {
    // A properly formed packet generated by the microcode should begin with
    // the standard ethernet SFD of 3 words of 0x5555 and 1 word of 0x55d5.
    // This must be stripped before we send it to the host device.
    if (output_packet_.size() < 4) {
        if (Log::enabled) {
            Log::write(LogComponent::EthernetTransmit, "Malformed packet: too short.");
        }
        return;
    }

    bool bad_sfd = false;
    for (int i = 0; i < 4; i++) {
        uint16_t sfd_word = output_packet_.front();
        output_packet_.pop();

        if (i < 3) {
            bad_sfd = (sfd_word != 0x5555);
        } else {
            bad_sfd = (sfd_word != 0x55d5);
        }
    }

    if (bad_sfd) {
        if (Log::enabled) {
            Log::write(LogComponent::EthernetTransmit, "Malformed packet: Invalid SFD.");
        }
        return;
    }

    if (!output_packet_.empty() && host_interface_) {
        if (Log::enabled) {
            Log::write(LogComponent::EthernetTransmit, "Transmitting completed packet.");
        }

        // Convert queue to array for sending
        size_t count = output_packet_.size();
        std::vector<uint16_t> packet_words(count);
        for (size_t i = 0; i < count; i++) {
            packet_words[i] = output_packet_.front();
            output_packet_.pop();
        }

        host_interface_->send(packet_words.data(), packet_words.size());
    }
}

void EthernetController::transmit_callback(uint64_t /*skew_nsec*/, void* /*context*/) {
    // Pull the next word from the FIFO, if available.
    if (!fifo_.empty()) {
        uint16_t next_word = fifo_.front();
        fifo_.pop();
        if (Log::enabled) {
            Log::write(LogComponent::EthernetTransmit,
                       "Transmitting word 0x%04x", next_word);
        }
        transmit_word(next_word);
    } else if (!last_word_) {
        // No data available in FIFO and LastWord is not set: Underrun.
        tx_underrun_ = true;
        if (Log::enabled) {
            Log::write(LogType::Error, LogComponent::EthernetTransmit,
                       "Transmit underrun.");
        }
    }

    if (last_word_ && fifo_.empty()) {
        // If LastWord is set and the FIFO is empty, that will be the last
        // word in the packet. Shut things down.
        transmitter_running_ = false;
        out_attn_ = true;
        if (Log::enabled) {
            Log::write(LogComponent::EthernetTransmit,
                       "Last word. Stopping transmission.");
        }

        // Transmit completed packet over real ethernet.
        complete_transmission();
    } else if (tx_underrun_) {
        transmitter_running_ = false;
        if (Log::enabled) {
            Log::write(LogComponent::EthernetTransmit,
                       "Underrun. Stopping transmission.");
        }
    } else {
        // Still going, schedule the next callback.
        transmit_event_ = system_.scheduler().schedule(
            kTransmitInterval,
            [this](uint64_t skew, void* ctx) { transmit_callback(skew, ctx); });
    }

    // Update wakeups
    update_wakeup();
}

void EthernetController::on_host_packet_received(const std::vector<uint8_t>& data) {
    // NOTE: This may run on a receiver thread, not the main emulator thread.
    std::lock_guard<std::mutex> lock(reader_lock_);

    if (!enable_rcv_ || !turn_off_) {
        // Receiver is off, just drop the packet.
        if (Log::enabled) {
            Log::write(LogComponent::EthernetReceive,
                       "Ethernet receiver is off; dropping this packet.");
        }
        std::queue<std::vector<uint8_t>>().swap(pending_packets_);
    } else if (pending_packets_.size() < 1) {
        // Place the packet into the queue
        pending_packets_.push(data);

        if (Log::enabled) {
            Log::write(LogComponent::EthernetReceive,
                       "Packet (length %zu) added to pending buffer.", data.size());
        }
    } else {
        // Too many queued-up packets, drop this one.
        if (Log::enabled) {
            Log::write(LogComponent::EthernetReceive,
                       "Pending buffer full; dropping this packet.");
        }
    }
}

void EthernetController::stop_receiver() {
    system_.scheduler().cancel(receive_event_);

    std::lock_guard<std::mutex> lock(reader_lock_);
    receiver_running_ = false;
}

void EthernetController::run_receiver() {
    rx_mode_ = false;

    if (!receiver_running_ && enable_rcv_) {
        // For loopback cases we delay the receive operation to ensure no overlap
        // between transmit and receive on loopback.
        receive_event_ = system_.scheduler().schedule(
            (local_loop_ || loop_back_) ? kReceiveIntervalLoopback : kReceiveInterval,
            [this](uint64_t skew, void* ctx) { receive_callback(skew, ctx); });

        std::lock_guard<std::mutex> lock(reader_lock_);
        receiver_running_ = true;
    }
}

void EthernetController::receiver_poll_callback(uint64_t /*skew_nsec*/, void* /*context*/) {
    if (!enable_rcv_ || !turn_off_ || enable_tx_ || transmitter_running_ ||
        local_loop_ || loop_back_) {
        // Receiver is off, we're currently transmitting, or we're in loopback
        // mode -- do nothing.
    } else {
        // See if there's a packet to pick up.
        std::vector<uint8_t> packet_data;
        bool have_packet = false;

        {
            std::lock_guard<std::mutex> lock(reader_lock_);
            if (!receiver_running_ && !pending_packets_.empty()) {
                packet_data = std::move(pending_packets_.front());
                pending_packets_.pop();
                have_packet = true;
            }
        }

        if (have_packet) {
            // Read the data into the receiver input queue.
            size_t length = packet_data.size();
            size_t even_length = length - (length % 2);

            size_t pos = 0;
            while (pos < even_length) {
                uint16_t word = static_cast<uint16_t>(
                    (packet_data[pos] << 8) | packet_data[pos + 1]);
                input_packet_.push(word);
                pos += 2;
            }

            // If we have a byte left, enqueue it now.
            if (pos < length) {
                input_packet_.push(static_cast<uint16_t>(packet_data[pos] << 8));
            }

            std::queue<uint16_t>().swap(fifo_);
            fifo_head_ = 0;

            // Skip the preamble state (only used in loopback)
            receiver_state_ = ReceiverState::Data;

            // Set the packet even/odd byte length flag
            even_packet_length_ = (length % 2) == 0;

            // Alert the microcode to the presence of input data and start
            // processing.
            run_receiver();

            if (Log::enabled) {
                Log::write(LogComponent::EthernetReceive,
                           "Receive: Incoming packet queued into input buffer.");
            }
        }
    }

    // Schedule the next poll callback.
    system_.scheduler().schedule(
        kReceiverPollInterval,
        [this](uint64_t skew, void* ctx) { receiver_poll_callback(skew, ctx); });
}

void EthernetController::receive_callback(uint64_t /*skew_nsec*/, void* /*context*/) {
    // Pull the next word from the input packet and run the state machine.
    if (!input_packet_.empty()) {
        switch (receiver_state_) {
            case ReceiverState::Preamble: {
                uint16_t word = input_packet_.front();
                input_packet_.pop();
                if (word == 0x55d5) { // end of preamble
                    if (Log::enabled) {
                        Log::write(LogComponent::EthernetReceive,
                                   "Receive: end of preamble, switching to Data state.");
                    }
                    receiver_state_ = ReceiverState::Data;
                }
                break;
            }

            case ReceiverState::Data:
                // Stuff into FIFO.
                if (fifo_.size() < 16) {
                    uint16_t next_word = input_packet_.front();
                    input_packet_.pop();
                    if (Log::enabled) {
                        Log::write(LogComponent::EthernetReceive,
                                   "Receive: Enqueuing Data word 0x%04x onto FIFO, %zu words left.",
                                   next_word, input_packet_.size());
                    }
                    fifo_.push(next_word);
                    crc32_.add_to_checksum(next_word);
                    update_wakeup();
                    if (Log::enabled) {
                        Log::write(LogComponent::EthernetReceive,
                                   "Packet CRC is now 0x%08x", crc32_.checksum());
                    }
                } else {
                    if (Log::enabled) {
                        Log::write(LogComponent::EthernetReceive,
                                   "Input FIFO full. Waiting.");
                    }
                }
                break;

            default:
                break;
        }
    }

    if (!input_packet_.empty()) {
        // Post next event if there are still words left.
        receive_event_ = system_.scheduler().schedule(
            kReceiveInterval,
            [this](uint64_t skew, void* ctx) { receive_callback(skew, ctx); });
    } else {
        // End of packet.
        last_r_word_ = true;

        // When the last word is read from the FIFO, we will update the final
        // status.

        update_wakeup();

        if (Log::enabled) {
            Log::write(LogComponent::EthernetReceive,
                       "Final Packet CRC is 0x%08x", crc32_.checksum());
        }
    }
}

void EthernetController::update_wakeup() {
    // See schematic, pg 2; ethernet requests (wakeups) generated by:
    // TxMode & BufIR & Defer' & LastWord'
    //     OR
    // Defer & TickElapsed
    //     OR
    // RcvMode & BufOR & Purge'
    //     OR
    // Attn
    bool tx_wakeup = enable_tx_ && fifo_.size() < 16 && !defer_ && !last_word_;
    bool defer_wakeup = defer_ && tick_elapsed_;
    bool rx_wakeup = !rx_mode_ && (fifo_.size() > 2 || last_r_word_) && !purge_;

    if (tx_wakeup || defer_wakeup || rx_wakeup || out_attn_ || in_attn_) {
        if (Log::enabled) {
            Log::write(LogComponent::EthernetControl,
                       "Waking Ethernet task (tx %d defer %d rx %d (fifo %zu lastRword %d) outAttn %d inAttn %d)",
                       tx_wakeup, defer_wakeup, rx_wakeup, fifo_.size(),
                       last_r_word_, out_attn_, in_attn_);
        }
        system_.cp().wake_task(TaskType::Ethernet);
    } else {
        if (Log::enabled) {
            Log::write(LogComponent::EthernetControl, "Sleeping Ethernet task.");
        }
        system_.cp().sleep_task(TaskType::Ethernet);
    }
}

void EthernetController::attach_host_ethernet() {
    // Attach real Ethernet device if user has specified one, otherwise leave
    // unattached; output data will go into a bit-bucket.
    try {
        if (Configuration::host_packet_interface_name == NethubInterface::NETHUB_NAME) {
            auto nethub = std::make_unique<NethubInterface>();
            nethub->register_receive_callback(
                [this](const std::vector<uint8_t>& data) {
                    on_host_packet_received(data);
                });
            host_interface_ = std::move(nethub);
        }
#ifdef ENABLE_ETHERNET
        else if (Configuration::host_raw_ethernet_interfaces_available &&
                 !Configuration::host_packet_interface_name.empty()) {
            auto host_eth = std::make_unique<HostEthernetEncapsulation>(
                Configuration::host_packet_interface_name);
            host_eth->register_receive_callback(
                [this](const std::vector<uint8_t>& data) {
                    on_host_packet_received(data);
                });
            host_interface_ = std::move(host_eth);
        }
#endif
    } catch (const std::exception& e) {
        host_interface_.reset();
        Log::write(LogType::Error, LogComponent::HostEthernet,
                   "Unable to configure network interface '%s': Error %s",
                   Configuration::host_packet_interface_name.c_str(),
                   e.what());
    }
}

} // namespace darkstar
