/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/
#pragma once

#include <cstdint>

namespace darkstar {

enum class StarMouseButton : uint8_t {
    None   = 0,
    Left   = 0x4,
    Right  = 0x2,
    Middle = 0x1,
};

inline StarMouseButton operator|(StarMouseButton a, StarMouseButton b) {
    return static_cast<StarMouseButton>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline StarMouseButton operator&(StarMouseButton a, StarMouseButton b) {
    return static_cast<StarMouseButton>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}
inline StarMouseButton operator~(StarMouseButton a) {
    return static_cast<StarMouseButton>(~static_cast<uint8_t>(a));
}
inline StarMouseButton& operator|=(StarMouseButton& a, StarMouseButton b) {
    a = a | b; return a;
}
inline StarMouseButton& operator&=(StarMouseButton& a, StarMouseButton b) {
    a = a & b; return a;
}

class Mouse {
public:
    Mouse();

    int mouse_x() const { return mouse_x_; }
    int mouse_y() const { return mouse_y_; }
    StarMouseButton buttons() const { return buttons_; }

    void clear();
    void mouse_down(StarMouseButton button);
    void mouse_up(StarMouseButton button);
    void mouse_move(int dx, int dy);

private:
    int mouse_x_;
    int mouse_y_;
    StarMouseButton buttons_;
};

} // namespace darkstar
