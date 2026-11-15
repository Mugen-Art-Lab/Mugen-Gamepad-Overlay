// SPDX-FileCopyrightText: 2026 Mugen Art Lab
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "input_types.hpp"
#include "pixel_canvas.hpp"

#include <string_view>

namespace mugen {

enum class FaceButtonGlyph {
    A,
    B,
    X,
    Y,
    Cross,
    Circle,
    Square,
    Triangle,
};

struct FaceButtonLabels {
    FaceButtonGlyph south = FaceButtonGlyph::A;
    FaceButtonGlyph east = FaceButtonGlyph::B;
    FaceButtonGlyph west = FaceButtonGlyph::X;
    FaceButtonGlyph north = FaceButtonGlyph::Y;
};

enum class ControllerVisualProfile {
    Generic,
    XboxLike,
    PlayStationLike,
    NintendoLike,
};

struct UniversalSkinOptions {
    ControllerVisualProfile profile = ControllerVisualProfile::Generic;
    bool show_back = true;
    bool show_start = true;
    bool show_guide = true;
    bool show_misc1 = false;
    bool show_touchpad = false;
    FaceButtonLabels face_buttons{};
};

// Keep OBS source dimensions stable so saved scene transforms do not change.
// The actual texture is rendered at 2x for more detail at normal preview sizes.
constexpr uint32_t kUniversalSkinWidth = 560;
constexpr uint32_t kUniversalSkinHeight = 320;
constexpr uint32_t kUniversalRenderScale = 2;
constexpr uint32_t kUniversalRenderWidth = kUniversalSkinWidth * kUniversalRenderScale;
constexpr uint32_t kUniversalRenderHeight = kUniversalSkinHeight * kUniversalRenderScale;
constexpr uint32_t kErrorSkinWidth = 640;
constexpr uint32_t kErrorSkinHeight = 220;

void draw_universal_skin(PixelCanvas &canvas, const GamepadState &state,
                         const UniversalSkinOptions &options);
void draw_skin_error(PixelCanvas &canvas, std::string_view detail, std::string_view source_id);

} // namespace mugen
