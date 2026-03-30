/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/

#include "display/display_controller.h"
#include "display/display_device.h"
#include "memory/memory.h"
#include "memory/memory_controller.h"
#include "core/conversion.h"
#include "core/log.h"
#include "core/scheduler.h"
#include "core/system.h"
#include "cp/task_type.h"

#include <cstring>

namespace darkstar {

DisplayController::DisplayController(DSystem& system)
    : system_(system)
    , horizontal_retrace_delay_(static_cast<uint64_t>(28.8 * Conversion::UsecToNsec))
    , lost_sync_interval_(static_cast<uint64_t>(52.91 * Conversion::MsecToNsec))
{
    lost_sync_event_ = system_.scheduler().schedule(
        lost_sync_interval_,
        [this](uint64_t skew, void* ctx) { lost_sync_callback(skew, ctx); });
}

void DisplayController::reset() {
    display_on_ = false;
    blank_ = false;
    picture_ = false;
    invert_ = false;
    odd_line_ = false;

    scanline_ = 0;

    while (!fifo_.empty()) fifo_.pop();

    if (system_.display() != nullptr) {
        system_.display()->clear();
    }
}

void DisplayController::clr_dp_rq() {
    system_.cp().sleep_task(TaskType::Display);
}

void DisplayController::set_d_ctl_fifo(uint16_t value) {
    if (fifo_.size() < 16) {
        fifo_.push(value);
    } else {
        if (Log::enabled) Log::write(LogType::Error, LogComponent::DisplayControl,
            "DCtlFIFO: FIFO overflow, word dropped.");
    }
}

void DisplayController::set_d_ctl(uint16_t value) {
    bool was_display_on = display_on_;
    display_on_ = (value & 0x01) != 0;
    blank_ = (value & 0x02) != 0;
    picture_ = (value & 0x04) != 0;
    invert_ = (value & 0x08) != 0;

    if ((value & 0x20) != 0) {
        // Vertical Sync -- back to the top of the screen
        scanline_ = 0;
        odd_line_ = (value & 0x10) != 0;
        sync_present_ = true;
        if (system_.display() != nullptr) {
            system_.display()->render();
        }
    }

    if ((value & 0x40) == 0) {
        // Clear control fifo
        while (!fifo_.empty()) fifo_.pop();
    }

    if (!was_display_on && display_on_) {
        // Kick off the horizontal retrace callback since we're turning the display on.
        system_.scheduler().schedule(
            horizontal_retrace_delay_,
            [this](uint64_t skew, void* ctx) { horizontal_retrace_callback(skew, ctx); });

        system_.scheduler().cancel(lost_sync_event_);
        lost_sync_event_ = system_.scheduler().schedule(
            lost_sync_interval_,
            [this](uint64_t skew, void* ctx) { lost_sync_callback(skew, ctx); });
    } else if (!display_on_) {
        system_.cp().sleep_task(TaskType::Display);
    }

    if (Log::enabled) Log::write(LogType::Verbose, LogComponent::DisplayControl,
        "DCtl<-0x%04x: On=%d Blank=%d Picture=%d Invert=%d Odd=%d",
        value, display_on_, blank_, picture_, invert_, odd_line_);
}

void DisplayController::set_d_border(uint16_t value) {
    display_border_ = value;
    if (Log::enabled) Log::write(LogType::Verbose, LogComponent::DisplayControl,
        "DBorder<-0x%04x", value);
}

void DisplayController::horizontal_retrace_callback(uint64_t /*skew_nsec*/, void* /*context*/) {
    int visible_offset = odd_line_ ? 37 : 36;
    int effective_scanline = scanline_ - visible_offset;

    // Render this scanline.
    if (blank_) {
        // Blank scanline
        std::memset(scanline_data_, 0, sizeof(scanline_data_));
    } else {
        if (picture_) {
            // Normal line: 32 bits border, 1024 bits display, 32 bits border
            int pattern_byte = (effective_scanline & 0x2) == 0
                ? display_border_ & 0xff
                : display_border_ >> 8;
            uint16_t pattern_word = static_cast<uint16_t>(pattern_byte | (pattern_byte << 8));

            scanline_data_[0] = pattern_word;
            scanline_data_[1] = pattern_word;

            if (!fifo_.empty()) {
                uint16_t fifo_word = fifo_.front();
                fifo_.pop();
                int last_word = fifo_word >> 10;
                int line_number = fifo_word & 0x3ff;
                bool valid = false;

                for (int word = 0; word < 64; word++) {
                    scanline_data_[word + 2] = system_.memory_controller().debug_memory().read_word(
                        (line_number << 6) | word, valid);

                    if (word != 63 && word == last_word && !fifo_.empty()) {
                        fifo_word = fifo_.front();
                        fifo_.pop();
                        last_word = fifo_word >> 10;
                        line_number = fifo_word & 0x3ff;
                    }
                }
            } else {
                // Blank out display words, nothing in the FIFO.
                for (int i = 2; i < 66; i++) {
                    scanline_data_[i] = 0;
                }
            }

            scanline_data_[66] = pattern_word;
            scanline_data_[67] = pattern_word;
        } else {
            // Just display the border pattern everywhere.
            int pattern_byte = (effective_scanline & 0x2) == 0
                ? display_border_ & 0xff
                : display_border_ >> 8;
            uint16_t pattern_word = static_cast<uint16_t>(pattern_byte | (pattern_byte << 8));

            for (auto& w : scanline_data_) {
                w = pattern_word;
            }
        }
    }

    if (effective_scanline > 0 && effective_scanline < 860) {
        if (system_.display() != nullptr) {
            system_.display()->draw_scanline(
                effective_scanline, scanline_data_, 64 + 4, invert_);
        }
    }

    // Move to next scanline
    scanline_ += 2;

    // Schedule next retrace as long as the display is still on.
    if (display_on_) {
        system_.scheduler().schedule(
            horizontal_retrace_delay_,
            [this](uint64_t skew, void* ctx) { horizontal_retrace_callback(skew, ctx); });

        // End of scanline: Wake up the display task.
        system_.cp().wake_task(TaskType::Display);
    }
}

void DisplayController::lost_sync_callback(uint64_t /*skew_nsec*/, void* /*context*/) {
    if (sync_present_) {
        // Got sync, keep the display alive and reschedule.
        lost_sync_event_ = system_.scheduler().schedule(
            lost_sync_interval_,
            [this](uint64_t skew, void* ctx) { lost_sync_callback(skew, ctx); });
    } else {
        // No sync since last callback, blank the display.
        if (system_.display() != nullptr) {
            system_.display()->clear();
        }
    }

    sync_present_ = false;
}

} // namespace darkstar
