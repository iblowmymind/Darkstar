/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>
#include <mutex>
#include <queue>

namespace darkstar {

enum class KeyCode : uint8_t {
    Invalid = 0x00,

    D1 = 0x10,
    T10 = 0x11,
    Defaults = 0x12,
    LargerSmaller = 0x13,
    Subscript = 0x14,
    Undo = 0x15,
    Superscript = 0x16,
    Properties = 0x17,
    Move = 0x18,
    Copy = 0x19,
    Underline = 0x1a,
    Italics = 0x1b,
    Bold = 0x1c,
    Center = 0x1d,
    T1 = 0x1e,

    R4 = 0x20,
    SkipNext = 0x22,
    Help = 0x23,
    Margins = 0x24,
    R3 = 0x25,
    L10 = 0x27,
    Same = 0x28,
    L4 = 0x29,
    L1 = 0x2a,
    A9 = 0x2d,

    DefnExpand = 0x30,
    R10 = 0x31,
    Keyboard = 0x32,
    Font = 0x33,
    R9 = 0x34,
    Stop = 0x35,
    Space = 0x36,
    Open = 0x37,
    L8 = 0x38,
    Find = 0x39,
    Again = 0x3a,
    Delete = 0x3b,
    A8 = 0x3c,
    A11 = 0x3d,

    A12 = 0x41,
    RightShift = 0x42,
    FSlash = 0x43,
    Period = 0x44,
    Comma = 0x45,
    M = 0x46,
    N = 0x47,
    B = 0x48,
    V = 0x49,
    C = 0x4a,
    X = 0x4b,
    Z = 0x4c,
    K47 = 0x4e,

    Return = 0x50,
    BackQuote = 0x51,
    Quote = 0x52,
    Colon = 0x53,
    L = 0x54,
    K = 0x55,
    J = 0x56,
    H = 0x57,
    G = 0x58,
    F = 0x59,
    D = 0x5a,
    S = 0x5b,
    A = 0x5c,
    Lock = 0x5e,
    LeftShift = 0x5f,

    A10 = 0x60,
    RBracket = 0x61,
    LBracket = 0x62,
    P = 0x63,
    O = 0x64,
    I = 0x65,
    U = 0x66,
    Y = 0x67,
    T = 0x68,
    R = 0x69,
    E = 0x6a,
    W = 0x6b,
    Q = 0x6c,
    Tab = 0x6d,
    D2 = 0x6f,

    Backspace = 0x70,
    Equals = 0x71,
    Minus = 0x72,
    N0 = 0x73,
    N9 = 0x74,
    N8 = 0x75,
    N7 = 0x76,
    N6 = 0x77,
    N5 = 0x78,
    N4 = 0x79,
    N3 = 0x7a,
    N2 = 0x7b,
    N1 = 0x7c,
    FArrow = 0x7d,
};

class Keyboard {
public:
    Keyboard();

    uint8_t read_data();
    void next_data();
    bool data_ready();

    void enable_diagnostic_mode();
    void disable_diagnostic_mode();

    void key_down(KeyCode keycode);
    void key_up(KeyCode keycode);

private:
    KeyCode key_data_;
    std::queue<KeyCode> keyboard_queue_;
    mutable std::mutex mutex_;
};

} // namespace darkstar
