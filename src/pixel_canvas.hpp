// SPDX-FileCopyrightText: 2026 Mugen Art Lab
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <vector>

namespace mugen {

struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

class PixelCanvas {
public:
    PixelCanvas(uint32_t width, uint32_t height);

    void resize(uint32_t width, uint32_t height);

    uint32_t width() const noexcept { return width_; }
    uint32_t height() const noexcept { return height_; }
    uint32_t pitch() const noexcept { return width_ * 4U; }
    const uint8_t *data() const noexcept { return pixels_.data(); }

    void clear(Color color = {0, 0, 0, 0});
    void fill_rect(int x, int y, int w, int h, Color color);
    void stroke_rect(int x, int y, int w, int h, int thickness, Color color);
    void fill_rounded_rect(int x, int y, int w, int h, int radius, Color color);
    void fill_circle(int cx, int cy, int radius, Color color);
    void fill_ellipse(int cx, int cy, int rx, int ry, Color color);
    void stroke_circle(int cx, int cy, int radius, int thickness, Color color);
    void stroke_ellipse(int cx, int cy, int rx, int ry, int thickness, Color color);
    void line(int x0, int y0, int x1, int y1, int thickness, Color color);
    void text(int x, int y, std::string_view value, int scale, Color color, bool centered = false);
    void blit_rgba(const uint8_t *source, uint32_t source_width, uint32_t source_height,
                   int source_x, int source_y, int width, int height, int dest_x, int dest_y);
    void blit_rgba_scaled(const uint8_t *source, uint32_t source_width, uint32_t source_height,
                          int dest_x, int dest_y, int dest_width, int dest_height,
                          int background_x = 0, int background_y = 0,
                          int background_width = 0, int background_height = 0,
                          bool scale_to_box = true, bool flip_x = false,
                          float saturation = 1.0F, float hue_rotation = 0.0F,
                          uint8_t opacity = 255);
    int text_width(std::string_view value, int scale) const;

private:
    void blend_pixel(int x, int y, Color color);
    static std::array<uint8_t, 7> glyph(char32_t ch);

    uint32_t width_;
    uint32_t height_;
    std::vector<uint8_t> pixels_;
};

} // namespace mugen
