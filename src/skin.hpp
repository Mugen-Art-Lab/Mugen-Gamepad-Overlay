// SPDX-FileCopyrightText: 2026 Mugen Art Lab
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "input_types.hpp"
#include "pixel_canvas.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace mugen {

enum class SkinElementType {
    Rect,
    RoundedRect,
    Ellipse,
    Circle,
    Line,
    Dpad,
    Stick,
    TriggerBar,
    ArcadeStick,
    Text,
};

enum class SkinAxis {
    None,
    LeftStick,
    RightStick,
    LeftTrigger,
    RightTrigger,
};

struct SkinElement {
    SkinElementType type = SkinElementType::Rect;
    Action action = Action::Count;
    SkinAxis axis = SkinAxis::None;

    int x = 0;
    int y = 0;
    int x2 = 0;
    int y2 = 0;
    int w = 0;
    int h = 0;
    int cx = 0;
    int cy = 0;
    int rx = 0;
    int ry = 0;
    int radius = 0;
    int radius2 = 0;
    int thickness = 1;
    int travel = 0;
    int travel_y = 0;
    int scale = 1;
    bool centered = false;

    Color color{116, 126, 143, 255};
    Color active{44, 224, 201, 255};
    Color secondary{20, 23, 28, 255};
    Color label_color{232, 236, 242, 255};
    Color active_label_color{24, 28, 34, 255};
    std::string label;
};

struct SkinDefinition {
    std::string id;
    std::string name_key;
    uint32_t width = 480;
    uint32_t height = 270;
    std::vector<SkinElement> elements;
    std::string source_path;
};

bool load_builtin_skin(const std::string &id, SkinDefinition &out, std::string &error);
bool load_skin_file(const std::string &path, SkinDefinition &out, std::string &error);
void draw_skin(PixelCanvas &canvas, const SkinDefinition &skin, const GamepadState &state);

} // namespace mugen
