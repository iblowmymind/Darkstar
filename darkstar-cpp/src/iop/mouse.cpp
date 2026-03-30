/*
    BSD 2-Clause License
    Copyright Vulcan Inc. 2017-2018 and Living Computer Museum + Labs 2018
    All rights reserved.
    C++ port of Darkstar - Xerox Star emulator
*/

#include "iop/mouse.h"

#include <algorithm>

namespace darkstar {

Mouse::Mouse()
    : mouse_x_(0)
    , mouse_y_(0)
    , buttons_(StarMouseButton::None)
{
}

void Mouse::clear()
{
    mouse_x_ = 0;
    mouse_y_ = 0;
}

void Mouse::mouse_down(StarMouseButton button)
{
    buttons_ |= button;
}

void Mouse::mouse_up(StarMouseButton button)
{
    buttons_ &= (~button);
}

void Mouse::mouse_move(int dx, int dy)
{
    mouse_x_ += dx;
    mouse_y_ += dy;

    // Clip into range (-128 to 127)
    mouse_x_ = std::max(-128, std::min(127, mouse_x_));
    mouse_y_ = std::max(-128, std::min(127, mouse_y_));
}

} // namespace darkstar
