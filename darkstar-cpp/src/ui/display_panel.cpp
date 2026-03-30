/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    C++ port of Darkstar - Xerox Star emulator
*/
#include "ui/display_panel.h"

#include "core/configuration.h"
#include "core/system.h"
#include "iop/io_processor.h"
#include "iop/keyboard.h"
#include "iop/mouse.h"

#include <wx/dcbuffer.h>
#include <wx/rawbmp.h>
#include <cstring>

namespace darkstar {

DisplayPanel::DisplayPanel(wxWindow* parent, DSystem* system)
    : wxPanel(parent, wxID_ANY)
    , system_(system) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetDoubleBuffered(true);

    std::memset(display_buffer_, 0, sizeof(display_buffer_));
    update_slow_phosphor();
    initialize_keymap();

    Bind(wxEVT_PAINT, &DisplayPanel::on_paint, this);
    Bind(wxEVT_KEY_DOWN, &DisplayPanel::on_key_down, this);
    Bind(wxEVT_KEY_UP, &DisplayPanel::on_key_up, this);
    Bind(wxEVT_MOTION, &DisplayPanel::on_mouse_move, this);
    Bind(wxEVT_LEFT_DOWN, &DisplayPanel::on_mouse_down, this);
    Bind(wxEVT_MIDDLE_DOWN, &DisplayPanel::on_mouse_down, this);
    Bind(wxEVT_RIGHT_DOWN, &DisplayPanel::on_mouse_down, this);
    Bind(wxEVT_LEFT_UP, &DisplayPanel::on_mouse_up, this);
    Bind(wxEVT_MIDDLE_UP, &DisplayPanel::on_mouse_up, this);
    Bind(wxEVT_RIGHT_UP, &DisplayPanel::on_mouse_up, this);
    Bind(wxEVT_LEAVE_WINDOW, &DisplayPanel::on_mouse_leave, this);
    Bind(wxEVT_KILL_FOCUS, &DisplayPanel::on_focus_lost, this);

    SetFocus();
}

DisplayPanel::~DisplayPanel() = default;

void DisplayPanel::draw_scanline(int scanline, const uint16_t* data,
                                  int word_count, bool invert) {
    int rgb_index = scanline * kDisplayWidth;
    uint32_t on_color = invert ? lit_pixel_ : off_pixel_;
    uint32_t off_color = invert ? off_pixel_ : lit_pixel_;

    for (int i = 0; i < word_count; i++) {
        uint16_t w = data[i];
        for (int bit = 15; bit >= 0; bit--) {
            uint32_t color = (w & (1 << bit)) == 0 ? off_color : on_color;
            if (rgb_index < kDisplayWidth * kDisplayHeight) {
                display_buffer_[rgb_index++] = color;
            }
        }
    }
}

void DisplayPanel::render() {
    needs_repaint_ = true;
    frame_count_++;

    // Post refresh to main thread
    CallAfter([this]() {
        Refresh(false);
    });
}

void DisplayPanel::clear() {
    for (int i = 0; i < kDisplayWidth * kDisplayHeight; i++) {
        display_buffer_[i] = 0xFF000000;
    }
    render();
}

void DisplayPanel::set_display_scale(double scale) {
    display_scale_ = scale;
    SetMinSize(wxSize(static_cast<int>(kDisplayWidth * scale),
                      static_cast<int>(kDisplayHeight * scale)));
    GetParent()->Layout();
}

void DisplayPanel::update_slow_phosphor() {
    if (Configuration::slow_phosphor) {
        lit_pixel_ = kLitPixelSlow;
        off_pixel_ = kOffPixelSlow;
    } else {
        lit_pixel_ = kLitPixelNormal;
        off_pixel_ = kOffPixelNormal;
    }
}

void DisplayPanel::on_paint(wxPaintEvent&) {
    wxBufferedPaintDC dc(this);

    // Create wxImage from display buffer
    wxImage image(kDisplayWidth, kDisplayHeight, false);
    unsigned char* rgb = image.GetData();

    {
        std::shared_lock<std::shared_mutex> lock(texture_lock_);
        for (int i = 0; i < kDisplayWidth * kDisplayHeight; i++) {
            uint32_t pixel = display_buffer_[i];
            rgb[i * 3 + 0] = (pixel >> 16) & 0xFF; // R
            rgb[i * 3 + 1] = (pixel >> 8) & 0xFF;  // G
            rgb[i * 3 + 2] = pixel & 0xFF;          // B
        }
    }

    wxSize client_size = GetClientSize();

    if (client_size.GetWidth() != kDisplayWidth ||
        client_size.GetHeight() != kDisplayHeight) {
        image.Rescale(client_size.GetWidth(), client_size.GetHeight(),
                      wxIMAGE_QUALITY_NEAREST);
    }

    wxBitmap bmp(image);
    dc.DrawBitmap(bmp, 0, 0, false);
}

void DisplayPanel::handle_key(wxKeyEvent& event, bool down) {
    int key_code = event.GetKeyCode();

    // Alt releases mouse
    if (key_code == WXK_ALT || key_code == WXK_RAW_CONTROL) {
        if (key_code == WXK_ALT) {
            release_mouse_input();
            return;
        }
    }

    if (!mouse_captured_) {
        event.Skip();
        return;
    }

    // Handle modifier keys (distinguish left/right)
    // wxWidgets doesn't always distinguish L/R modifiers, use raw key code
    #ifdef __WXMSW__
    if (key_code == WXK_SHIFT) {
        // On Windows, use GetRawKeyCode to distinguish L/R
        auto raw = event.GetRawKeyCode();
        KeyCode code = (raw == 0xA1) ? KeyCode::RightShift : KeyCode::LeftShift;
        if (down) system_->iop().keyboard().key_down(code);
        else system_->iop().keyboard().key_up(code);
        return;
    }
    if (key_code == WXK_CONTROL) {
        auto raw = event.GetRawKeyCode();
        KeyCode code = (raw == 0xA3) ? KeyCode::Open : KeyCode::Properties;
        if (down) system_->iop().keyboard().key_down(code);
        else system_->iop().keyboard().key_up(code);
        return;
    }
    #else
    // On macOS/Linux, try to use raw key codes
    if (key_code == WXK_SHIFT) {
        KeyCode code = event.GetRawKeyCode() == 0x3E ?
            KeyCode::RightShift : KeyCode::LeftShift;
        if (down) system_->iop().keyboard().key_down(code);
        else system_->iop().keyboard().key_up(code);
        return;
    }
    if (key_code == WXK_CONTROL) {
        KeyCode code = event.GetRawKeyCode() == 0x69 ?
            KeyCode::Open : KeyCode::Properties;
        if (down) system_->iop().keyboard().key_down(code);
        else system_->iop().keyboard().key_up(code);
        return;
    }
    #endif

    auto it = keymap_.find(key_code);
    if (it == keymap_.end()) {
        event.Skip();
        return;
    }

    KeyCode code = it->second;

    if (code == KeyCode::Lock) {
        if (down) {
            if (!caps_lock_) {
                system_->iop().keyboard().key_down(code);
            } else {
                system_->iop().keyboard().key_up(code);
            }
            caps_lock_ = !caps_lock_;
        }
    } else {
        if (down) {
            system_->iop().keyboard().key_down(code);
        } else {
            system_->iop().keyboard().key_up(code);
        }
    }
}

void DisplayPanel::on_key_down(wxKeyEvent& event) {
    handle_key(event, true);
}

void DisplayPanel::on_key_up(wxKeyEvent& event) {
    handle_key(event, false);
}

void DisplayPanel::on_mouse_move(wxMouseEvent& event) {
    if (!mouse_captured_) return;

    if (skip_next_mouse_move_) {
        skip_next_mouse_move_ = false;
        return;
    }

    wxSize size = GetClientSize();
    int mx = size.GetWidth() / 2;
    int my = size.GetHeight() / 2;

    int dx = event.GetX() - mx;
    int dy = event.GetY() - my;

    if (dx != 0 || dy != 0) {
        system_->iop().mouse().mouse_move(dx, dy);
        skip_next_mouse_move_ = true;
        WarpPointer(mx, my);
    }
}

StarMouseButton DisplayPanel::get_star_button(wxMouseEvent& event) {
    if (event.LeftIsDown() || event.LeftUp() || event.LeftDown())
        return StarMouseButton::Left;
    if (event.MiddleIsDown() || event.MiddleUp() || event.MiddleDown())
        return StarMouseButton::Middle;
    if (event.RightIsDown() || event.RightUp() || event.RightDown())
        return StarMouseButton::Right;
    return StarMouseButton::None;
}

void DisplayPanel::on_mouse_down(wxMouseEvent& event) {
    if (!mouse_captured_) return;

    auto button = get_star_button(event);
    if (button != StarMouseButton::None) {
        system_->iop().mouse().mouse_down(button);
    }
}

void DisplayPanel::on_mouse_up(wxMouseEvent& event) {
    if (!mouse_captured_) {
        capture_mouse_input();
        return;
    }

    auto button = get_star_button(event);
    if (button != StarMouseButton::None) {
        system_->iop().mouse().mouse_up(button);
    }
}

void DisplayPanel::on_mouse_leave(wxMouseEvent&) {
    release_mouse_input();
}

void DisplayPanel::on_focus_lost(wxFocusEvent&) {
    release_mouse_input();
}

void DisplayPanel::capture_mouse_input() {
    if (system_->is_executing()) {
        mouse_captured_ = true;
        SetCursor(wxCursor(wxCURSOR_BLANK));
        if (!HasCapture()) {
            CaptureMouse();
        }
    }
}

void DisplayPanel::release_mouse_input() {
    mouse_captured_ = false;
    SetCursor(wxNullCursor);
    if (HasCapture()) {
        ReleaseMouse();
    }
}

void DisplayPanel::initialize_keymap() {
    // Letter keys
    keymap_['A'] = KeyCode::A; keymap_['B'] = KeyCode::B;
    keymap_['C'] = KeyCode::C; keymap_['D'] = KeyCode::D;
    keymap_['E'] = KeyCode::E; keymap_['F'] = KeyCode::F;
    keymap_['G'] = KeyCode::G; keymap_['H'] = KeyCode::H;
    keymap_['I'] = KeyCode::I; keymap_['J'] = KeyCode::J;
    keymap_['K'] = KeyCode::K; keymap_['L'] = KeyCode::L;
    keymap_['M'] = KeyCode::M; keymap_['N'] = KeyCode::N;
    keymap_['O'] = KeyCode::O; keymap_['P'] = KeyCode::P;
    keymap_['Q'] = KeyCode::Q; keymap_['R'] = KeyCode::R;
    keymap_['S'] = KeyCode::S; keymap_['T'] = KeyCode::T;
    keymap_['U'] = KeyCode::U; keymap_['V'] = KeyCode::V;
    keymap_['W'] = KeyCode::W; keymap_['X'] = KeyCode::X;
    keymap_['Y'] = KeyCode::Y; keymap_['Z'] = KeyCode::Z;

    // Number keys
    keymap_['0'] = KeyCode::N0; keymap_['1'] = KeyCode::N1;
    keymap_['2'] = KeyCode::N2; keymap_['3'] = KeyCode::N3;
    keymap_['4'] = KeyCode::N4; keymap_['5'] = KeyCode::N5;
    keymap_['6'] = KeyCode::N6; keymap_['7'] = KeyCode::N7;
    keymap_['8'] = KeyCode::N8; keymap_['9'] = KeyCode::N9;

    // Punctuation/special
    keymap_['-'] = KeyCode::Minus;
    keymap_['='] = KeyCode::Equals;
    keymap_[WXK_RETURN] = KeyCode::Return;
    keymap_[WXK_SPACE] = KeyCode::Space;
    keymap_[WXK_BACK] = KeyCode::Backspace;
    keymap_['['] = KeyCode::LBracket;
    keymap_[']'] = KeyCode::RBracket;
    keymap_['.'] = KeyCode::Period;
    keymap_[','] = KeyCode::Comma;
    keymap_['/'] = KeyCode::FSlash;
    keymap_[';'] = KeyCode::Colon;
    keymap_['\''] = KeyCode::Quote;
    keymap_['`'] = KeyCode::BackQuote;
    keymap_[WXK_RIGHT] = KeyCode::FArrow;
    keymap_[WXK_TAB] = KeyCode::Tab;
    keymap_[WXK_CAPITAL] = KeyCode::Lock;
    keymap_['\\'] = KeyCode::Font;

    // Function keys -> Star left key block
    keymap_[WXK_F1] = KeyCode::Again;
    keymap_[WXK_F2] = KeyCode::Delete;
    keymap_[WXK_F3] = KeyCode::Find;
    keymap_[WXK_F4] = KeyCode::Copy;
    keymap_[WXK_F5] = KeyCode::Same;
    keymap_[WXK_F6] = KeyCode::Move;
    keymap_[WXK_F7] = KeyCode::Open;
    keymap_[WXK_F8] = KeyCode::Properties;

    // Function keys -> Star top key block
    keymap_[WXK_F9] = KeyCode::Center;
    keymap_[WXK_F10] = KeyCode::Bold;
    keymap_[WXK_F11] = KeyCode::Italics;
    keymap_[WXK_F12] = KeyCode::Underline;
    keymap_[WXK_PRINT] = KeyCode::Superscript;
    keymap_[WXK_SCROLL] = KeyCode::Subscript;
    keymap_[WXK_PAUSE] = KeyCode::LargerSmaller;
    keymap_[WXK_NUMLOCK] = KeyCode::Defaults;

    // Navigation keys -> Star right key block
    keymap_[WXK_HOME] = KeyCode::SkipNext;
    keymap_[WXK_PAGEUP] = KeyCode::Undo;
    keymap_[WXK_END] = KeyCode::DefnExpand;
    keymap_[WXK_PAGEDOWN] = KeyCode::Stop;
    keymap_[WXK_UP] = KeyCode::Help;
    keymap_[WXK_LEFT] = KeyCode::Margins;
    keymap_[WXK_DOWN] = KeyCode::Keyboard;
}

} // namespace darkstar
