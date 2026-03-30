/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <wx/wx.h>
#include <cstdint>
#include <shared_mutex>
#include <unordered_map>

#include "display/display_device.h"
#include "iop/keyboard.h"
#include "iop/mouse.h"

namespace darkstar {

class DSystem;

class DisplayPanel : public wxPanel, public IDisplayDevice {
public:
    DisplayPanel(wxWindow* parent, DSystem* system);
    ~DisplayPanel() override;

    // IDisplayDevice
    void draw_scanline(int scanline, const uint16_t* data,
                       int word_count, bool invert) override;
    void render() override;
    void clear() override;

    void set_display_scale(double scale);
    void update_slow_phosphor();
    bool is_mouse_captured() const { return mouse_captured_; }
    void capture_mouse_input();
    void release_mouse_input();

    static constexpr int kDisplayWidth = 1088;
    static constexpr int kDisplayHeight = 860;

private:
    void on_paint(wxPaintEvent& event);
    void on_key_down(wxKeyEvent& event);
    void on_key_up(wxKeyEvent& event);
    void on_mouse_move(wxMouseEvent& event);
    void on_mouse_down(wxMouseEvent& event);
    void on_mouse_up(wxMouseEvent& event);
    void on_mouse_leave(wxMouseEvent& event);
    void on_focus_lost(wxFocusEvent& event);

    void handle_key(wxKeyEvent& event, bool down);
    void initialize_keymap();

    StarMouseButton get_star_button(wxMouseEvent& event);

    DSystem* system_;

    // Display buffer (ARGB8888)
    uint32_t display_buffer_[kDisplayWidth * kDisplayHeight];
    std::shared_mutex texture_lock_;
    wxBitmap display_bitmap_;
    bool needs_repaint_ = false;

    // Phosphor colors
    uint32_t lit_pixel_;
    uint32_t off_pixel_;
    static constexpr uint32_t kLitPixelSlow = 0xFFEFFEFF;
    static constexpr uint32_t kOffPixelSlow = 0x20000000;
    static constexpr uint32_t kLitPixelNormal = 0xFFEFFEFF;
    static constexpr uint32_t kOffPixelNormal = 0xFF000000;

    double display_scale_ = 1.0;
    int frame_count_ = 0;

    // Keyboard
    std::unordered_map<int, KeyCode> keymap_;
    bool caps_lock_ = false;

    // Mouse
    bool mouse_captured_ = false;
    bool skip_next_mouse_move_ = false;
};

} // namespace darkstar
