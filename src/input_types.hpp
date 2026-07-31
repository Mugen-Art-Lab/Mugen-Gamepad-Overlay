// SPDX-FileCopyrightText: 2026 Mugen Art Lab
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>

namespace mugen {

enum class Action : size_t {
    South,
    East,
    West,
    North,
    Back,
    Guide,
    Start,
    Misc1,
    Touchpad,
    LeftShoulder,
    RightShoulder,
    LeftStick,
    RightStick,
    DpadUp,
    DpadDown,
    DpadLeft,
    DpadRight,
    Count,
};

struct GamepadState {
    std::array<bool, static_cast<size_t>(Action::Count)> pressed{};
    float left_x = 0.0F;
    float left_y = 0.0F;
    float right_x = 0.0F;
    float right_y = 0.0F;
    float left_trigger = 0.0F;
    float right_trigger = 0.0F;
    bool connected = false;
};

} // namespace mugen
