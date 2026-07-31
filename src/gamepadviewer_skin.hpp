// SPDX-FileCopyrightText: 2026 Mugen Art Lab
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "input_types.hpp"
#include "pixel_canvas.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace mugen {

struct GpvRasterImage {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> rgba;
};

struct GpvLayer {
    std::shared_ptr<const GpvRasterImage> image;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int background_x = 0;
    int background_y = 0;
    int background_width = 0;
    int background_height = 0;
    bool scale_to_box = true;
    bool flip_x = false;
    float saturation = 1.0F;
    float hue_rotation = 0.0F;
    uint8_t opacity = 255;
};

struct GpvLayerPair {
    std::optional<GpvLayer> normal;
    std::optional<GpvLayer> pressed;
};

struct GpvTriggerVisual {
    // GamepadViewer normally drives `.trigger` opacity continuously from the
    // analog axis and uses `.trigger-button` for a separate click/pressed layer.
    std::optional<GpvLayer> analog;
    GpvLayerPair button;
};

struct GamepadViewerSkin {
    uint32_t width = 1;
    uint32_t height = 1;
    std::optional<GpvLayer> base;
    std::optional<GpvLayer> disconnected;
    std::optional<GpvLayer> disconnected_overlay;
    std::vector<GpvLayer> static_layers;
    std::array<GpvLayerPair, static_cast<size_t>(Action::Count)> actions{};
    GpvTriggerVisual left_trigger;
    GpvTriggerVisual right_trigger;
    std::array<GpvLayerPair, 4> dpad{}; // up, right, down, left
    std::array<std::optional<GpvLayer>, 9> fight_stick{}; // neutral, L, R, U, D, UL, UR, DL, DR
    std::string css_path;
    size_t loaded_layers = 0;
    size_t skipped_rules = 0;
    size_t rejected_assets = 0;
    size_t analog_sticks = 0;
    size_t analog_triggers = 0;
    std::string warning;
};

bool load_gamepadviewer_skin(const std::string &css_path, GamepadViewerSkin &out, std::string &error);
void draw_gamepadviewer_skin(PixelCanvas &canvas, const GamepadViewerSkin &skin,
                             const GamepadState &state, float stick_travel,
                             float trigger_threshold);

} // namespace mugen
