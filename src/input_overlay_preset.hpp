#pragma once

#include "input_types.hpp"
#include "pixel_canvas.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace mugen {

enum class InputOverlayElementType {
    StaticTexture = 0,
    Button = 2,
    AnalogStick = 5,
    Trigger = 6,
    GamepadId = 7,
    DpadStick = 8,
    Unsupported = -1,
};

struct InputOverlayElement {
    InputOverlayElementType type = InputOverlayElementType::Unsupported;
    int source_x = 0;
    int source_y = 0;
    int width = 0;
    int height = 0;
    int dest_x = 0;
    int dest_y = 0;
    int z_level = 0;
    int code = -1;
    int side = 0;
    int direction = 0;
    int stick_radius = 0;
    bool trigger_mode = false;
};

struct InputOverlayPreset {
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t atlas_width = 0;
    uint32_t atlas_height = 0;
    std::vector<uint8_t> atlas_rgba;
    std::vector<InputOverlayElement> elements;
    std::string config_path;
    std::string image_path;
    size_t supported_elements = 0;
    size_t unsupported_elements = 0;
    size_t clipped_elements = 0;
};

bool load_input_overlay_preset(const std::string &config_path, const std::string &image_path,
                               InputOverlayPreset &out, std::string &error);
void draw_input_overlay_preset(PixelCanvas &canvas, const InputOverlayPreset &preset,
                               const GamepadState &state);

} // namespace mugen
