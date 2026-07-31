#include "pixel_canvas.hpp"

#include <cctype>

namespace mugen {

PixelCanvas::PixelCanvas(uint32_t width, uint32_t height)
{
    resize(width, height);
}

void PixelCanvas::resize(uint32_t width, uint32_t height)
{
    width_ = std::max<uint32_t>(1, width);
    height_ = std::max<uint32_t>(1, height);
    pixels_.assign(static_cast<size_t>(width_) * height_ * 4U, 0);
}

void PixelCanvas::clear(Color color)
{
    for (uint32_t y = 0; y < height_; ++y) {
        for (uint32_t x = 0; x < width_; ++x) {
            const size_t i = (static_cast<size_t>(y) * width_ + x) * 4U;
            pixels_[i + 0] = color.r;
            pixels_[i + 1] = color.g;
            pixels_[i + 2] = color.b;
            pixels_[i + 3] = color.a;
        }
    }
}

void PixelCanvas::blend_pixel(int x, int y, Color src)
{
    if (x < 0 || y < 0 || x >= static_cast<int>(width_) || y >= static_cast<int>(height_))
        return;

    const size_t i = (static_cast<size_t>(y) * width_ + static_cast<uint32_t>(x)) * 4U;
    const uint32_t inv = 255U - src.a;

    pixels_[i + 0] = static_cast<uint8_t>((src.r * src.a + pixels_[i + 0] * inv) / 255U);
    pixels_[i + 1] = static_cast<uint8_t>((src.g * src.a + pixels_[i + 1] * inv) / 255U);
    pixels_[i + 2] = static_cast<uint8_t>((src.b * src.a + pixels_[i + 2] * inv) / 255U);
    pixels_[i + 3] = static_cast<uint8_t>(src.a + (pixels_[i + 3] * inv) / 255U);
}

void PixelCanvas::fill_rect(int x, int y, int w, int h, Color color)
{
    if (w <= 0 || h <= 0)
        return;

    const int left = std::max(0, x);
    const int top = std::max(0, y);
    const int right = std::min(static_cast<int>(width_), x + w);
    const int bottom = std::min(static_cast<int>(height_), y + h);

    for (int py = top; py < bottom; ++py)
        for (int px = left; px < right; ++px)
            blend_pixel(px, py, color);
}

void PixelCanvas::stroke_rect(int x, int y, int w, int h, int thickness, Color color)
{
    if (w <= 0 || h <= 0 || thickness <= 0)
        return;
    fill_rect(x, y, w, thickness, color);
    fill_rect(x, y + h - thickness, w, thickness, color);
    fill_rect(x, y, thickness, h, color);
    fill_rect(x + w - thickness, y, thickness, h, color);
}

void PixelCanvas::fill_rounded_rect(int x, int y, int w, int h, int radius, Color color)
{
    if (w <= 0 || h <= 0)
        return;
    radius = std::clamp(radius, 0, std::min(w, h) / 2);
    if (radius == 0) {
        fill_rect(x, y, w, h, color);
        return;
    }

    fill_rect(x + radius, y, w - radius * 2, h, color);
    fill_rect(x, y + radius, w, h - radius * 2, color);
    fill_circle(x + radius, y + radius, radius, color);
    fill_circle(x + w - radius - 1, y + radius, radius, color);
    fill_circle(x + radius, y + h - radius - 1, radius, color);
    fill_circle(x + w - radius - 1, y + h - radius - 1, radius, color);
}

void PixelCanvas::fill_circle(int cx, int cy, int radius, Color color)
{
    fill_ellipse(cx, cy, radius, radius, color);
}

void PixelCanvas::fill_ellipse(int cx, int cy, int rx, int ry, Color color)
{
    if (rx <= 0 || ry <= 0)
        return;

    const float inv_rx2 = 1.0F / static_cast<float>(rx * rx);
    const float inv_ry2 = 1.0F / static_cast<float>(ry * ry);

    for (int y = cy - ry; y <= cy + ry; ++y) {
        for (int x = cx - rx; x <= cx + rx; ++x) {
            const float dx = static_cast<float>(x - cx);
            const float dy = static_cast<float>(y - cy);
            if (dx * dx * inv_rx2 + dy * dy * inv_ry2 <= 1.0F)
                blend_pixel(x, y, color);
        }
    }
}

void PixelCanvas::stroke_circle(int cx, int cy, int radius, int thickness, Color color)
{
    stroke_ellipse(cx, cy, radius, radius, thickness, color);
}

void PixelCanvas::stroke_ellipse(int cx, int cy, int rx, int ry, int thickness, Color color)
{
    if (rx <= 0 || ry <= 0 || thickness <= 0)
        return;

    const int inner_rx = std::max(0, rx - thickness);
    const int inner_ry = std::max(0, ry - thickness);
    const float outer_rx2 = static_cast<float>(rx * rx);
    const float outer_ry2 = static_cast<float>(ry * ry);
    const float inner_rx2 = inner_rx > 0 ? static_cast<float>(inner_rx * inner_rx) : 1.0F;
    const float inner_ry2 = inner_ry > 0 ? static_cast<float>(inner_ry * inner_ry) : 1.0F;

    for (int y = cy - ry; y <= cy + ry; ++y) {
        for (int x = cx - rx; x <= cx + rx; ++x) {
            const float dx = static_cast<float>(x - cx);
            const float dy = static_cast<float>(y - cy);
            const float outer = dx * dx / outer_rx2 + dy * dy / outer_ry2;
            const float inner = inner_rx > 0 && inner_ry > 0
                                    ? dx * dx / inner_rx2 + dy * dy / inner_ry2
                                    : 2.0F;
            if (outer <= 1.0F && inner >= 1.0F)
                blend_pixel(x, y, color);
        }
    }
}

void PixelCanvas::line(int x0, int y0, int x1, int y1, int thickness, Color color)
{
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        fill_circle(x0, y0, std::max(1, thickness / 2), color);
        if (x0 == x1 && y0 == y1)
            break;
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void PixelCanvas::blit_rgba(const uint8_t *source, uint32_t source_width, uint32_t source_height,
                            int source_x, int source_y, int width, int height, int dest_x, int dest_y)
{
    if (!source || source_width == 0 || source_height == 0 || width <= 0 || height <= 0)
        return;

    for (int row = 0; row < height; ++row) {
        const int sy = source_y + row;
        const int dy = dest_y + row;
        if (sy < 0 || sy >= static_cast<int>(source_height) || dy < 0 || dy >= static_cast<int>(height_))
            continue;

        for (int col = 0; col < width; ++col) {
            const int sx = source_x + col;
            const int dx = dest_x + col;
            if (sx < 0 || sx >= static_cast<int>(source_width) || dx < 0 || dx >= static_cast<int>(width_))
                continue;

            const size_t index = (static_cast<size_t>(sy) * source_width + static_cast<uint32_t>(sx)) * 4U;
            blend_pixel(dx, dy, Color{source[index + 0], source[index + 1], source[index + 2], source[index + 3]});
        }
    }
}


void PixelCanvas::blit_rgba_scaled(const uint8_t *source, uint32_t source_width, uint32_t source_height,
                                   int dest_x, int dest_y, int dest_width, int dest_height,
                                   int background_x, int background_y,
                                   int background_width, int background_height,
                                   bool scale_to_box, bool flip_x,
                                   float saturation, float hue_rotation, uint8_t opacity)
{
    if (!source || source_width == 0 || source_height == 0 ||
        dest_width <= 0 || dest_height <= 0 || opacity == 0)
        return;

    const int painted_width = background_width > 0
                                  ? background_width
                                  : (scale_to_box ? dest_width : static_cast<int>(source_width));
    const int painted_height = background_height > 0
                                   ? background_height
                                   : (scale_to_box ? dest_height : static_cast<int>(source_height));
    if (painted_width <= 0 || painted_height <= 0)
        return;

    for (int row = 0; row < dest_height; ++row) {
        const int dy = dest_y + row;
        if (dy < 0 || dy >= static_cast<int>(height_))
            continue;

        for (int col = 0; col < dest_width; ++col) {
            const int dx = dest_x + col;
            if (dx < 0 || dx >= static_cast<int>(width_))
                continue;

            // CSS transforms mirror the already clipped element box. Apply the
            // mirror before resolving background-position/background-size so a
            // large sprite sheet behaves exactly like a browser element.
            const int local_x = (flip_x ? (dest_width - 1 - col) : col) - background_x;
            const int local_y = row - background_y;
            if (local_x < 0 || local_y < 0 ||
                local_x >= painted_width || local_y >= painted_height)
                continue;

            const int sx = static_cast<int>((static_cast<int64_t>(local_x) * source_width) /
                                            static_cast<uint32_t>(painted_width));
            const int sy = static_cast<int>((static_cast<int64_t>(local_y) * source_height) /
                                            static_cast<uint32_t>(painted_height));
            if (sx < 0 || sy < 0 || sx >= static_cast<int>(source_width) ||
                sy >= static_cast<int>(source_height))
                continue;

            const size_t index = (static_cast<size_t>(sy) * source_width +
                                  static_cast<uint32_t>(sx)) * 4U;
            float red = static_cast<float>(source[index + 0]);
            float green = static_cast<float>(source[index + 1]);
            float blue = static_cast<float>(source[index + 2]);

            const float clamped_saturation = std::clamp(saturation, 0.0F, 4.0F);
            if (std::abs(clamped_saturation - 1.0F) > 0.001F) {
                const float gray = 0.2126F * red + 0.7152F * green + 0.0722F * blue;
                red = gray + (red - gray) * clamped_saturation;
                green = gray + (green - gray) * clamped_saturation;
                blue = gray + (blue - gray) * clamped_saturation;
            }

            if (std::abs(hue_rotation) > 0.001F) {
                constexpr float kPi = 3.14159265358979323846F;
                const float radians = hue_rotation * kPi / 180.0F;
                const float cosine = std::cos(radians);
                const float sine = std::sin(radians);
                const float old_red = red;
                const float old_green = green;
                const float old_blue = blue;
                red = (0.213F + 0.787F * cosine - 0.213F * sine) * old_red +
                      (0.715F - 0.715F * cosine - 0.715F * sine) * old_green +
                      (0.072F - 0.072F * cosine + 0.928F * sine) * old_blue;
                green = (0.213F - 0.213F * cosine + 0.143F * sine) * old_red +
                        (0.715F + 0.285F * cosine + 0.140F * sine) * old_green +
                        (0.072F - 0.072F * cosine - 0.283F * sine) * old_blue;
                blue = (0.213F - 0.213F * cosine - 0.787F * sine) * old_red +
                       (0.715F - 0.715F * cosine + 0.715F * sine) * old_green +
                       (0.072F + 0.928F * cosine + 0.072F * sine) * old_blue;
            }

            const uint8_t alpha = static_cast<uint8_t>(
                (static_cast<uint32_t>(source[index + 3]) * opacity) / 255U);
            blend_pixel(dx, dy, Color{
                static_cast<uint8_t>(std::lround(std::clamp(red, 0.0F, 255.0F))),
                static_cast<uint8_t>(std::lround(std::clamp(green, 0.0F, 255.0F))),
                static_cast<uint8_t>(std::lround(std::clamp(blue, 0.0F, 255.0F))), alpha});
        }
    }
}

namespace {

char32_t next_utf8_codepoint(std::string_view value, size_t &offset)
{
    if (offset >= value.size())
        return U'\0';

    const auto first = static_cast<unsigned char>(value[offset++]);
    if (first < 0x80)
        return static_cast<char32_t>(first);

    int extra = 0;
    char32_t codepoint = 0;
    if ((first & 0xE0U) == 0xC0U) {
        extra = 1;
        codepoint = first & 0x1FU;
    } else if ((first & 0xF0U) == 0xE0U) {
        extra = 2;
        codepoint = first & 0x0FU;
    } else if ((first & 0xF8U) == 0xF0U) {
        extra = 3;
        codepoint = first & 0x07U;
    } else {
        return U'?';
    }

    if (offset + static_cast<size_t>(extra) > value.size()) {
        offset = value.size();
        return U'?';
    }

    for (int i = 0; i < extra; ++i) {
        const auto continuation = static_cast<unsigned char>(value[offset++]);
        if ((continuation & 0xC0U) != 0x80U)
            return U'?';
        codepoint = (codepoint << 6U) | (continuation & 0x3FU);
    }
    return codepoint;
}

char32_t uppercase_codepoint(char32_t ch)
{
    if (ch >= U'a' && ch <= U'z')
        return ch - (U'a' - U'A');
    if (ch >= U'а' && ch <= U'я')
        return ch - (U'а' - U'А');
    if (ch == U'ё')
        return U'Ё';
    return ch;
}

} // namespace

std::array<uint8_t, 7> PixelCanvas::glyph(char32_t ch)
{
    ch = uppercase_codepoint(ch);
    switch (ch) {
    case U'A': case U'А': return {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    case U'B': case U'В': return {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
    case U'C': case U'С': return {0x0F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0F};
    case U'D': return {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
    case U'E': case U'Е': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
    case U'F': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
    case U'G': return {0x0F, 0x10, 0x10, 0x17, 0x11, 0x11, 0x0F};
    case U'H': case U'Н': return {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    case U'I': return {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E};
    case U'J': return {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E};
    case U'K': case U'К': return {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
    case U'L': return {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
    case U'M': case U'М': return {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
    case U'N': return {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
    case U'O': case U'О': return {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    case U'P': case U'Р': return {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
    case U'Q': return {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
    case U'R': return {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
    case U'S': return {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
    case U'T': case U'Т': return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    case U'U': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    case U'V': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
    case U'W': return {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A};
    case U'X': case U'Х': return {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
    case U'Y': return {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
    case U'Z': return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};

    case U'Б': return {0x1F, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x1E};
    case U'Г': return {0x1F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10};
    case U'Д': return {0x0E, 0x0A, 0x0A, 0x0A, 0x1F, 0x11, 0x11};
    case U'Ё': return {0x0A, 0x00, 0x1F, 0x10, 0x1E, 0x10, 0x1F};
    case U'Ж': return {0x15, 0x15, 0x0E, 0x04, 0x0E, 0x15, 0x15};
    case U'З': return {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E};
    case U'И': return {0x11, 0x13, 0x15, 0x19, 0x11, 0x11, 0x11};
    case U'Й': return {0x0A, 0x04, 0x11, 0x13, 0x15, 0x19, 0x11};
    case U'Л': return {0x07, 0x05, 0x05, 0x09, 0x09, 0x11, 0x11};
    case U'П': return {0x1F, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11};
    case U'У': return {0x11, 0x11, 0x0A, 0x04, 0x04, 0x08, 0x10};
    case U'Ф': return {0x04, 0x0E, 0x15, 0x15, 0x0E, 0x04, 0x04};
    case U'Ц': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x1F, 0x01};
    case U'Ч': return {0x11, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x01};
    case U'Ш': return {0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x1F};
    case U'Щ': return {0x15, 0x15, 0x15, 0x15, 0x15, 0x1F, 0x01};
    case U'Ъ': return {0x18, 0x08, 0x08, 0x0E, 0x09, 0x09, 0x0E};
    case U'Ы': return {0x11, 0x11, 0x1D, 0x15, 0x15, 0x15, 0x1D};
    case U'Ь': return {0x10, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x1E};
    case U'Э': return {0x0E, 0x11, 0x01, 0x07, 0x01, 0x11, 0x0E};
    case U'Ю': return {0x12, 0x15, 0x15, 0x1D, 0x15, 0x15, 0x12};
    case U'Я': return {0x0F, 0x11, 0x11, 0x0F, 0x05, 0x09, 0x11};

    case U'0': return {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
    case U'1': return {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
    case U'2': return {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
    case U'3': return {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E};
    case U'4': return {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
    case U'5': return {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E};
    case U'6': return {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E};
    case U'7': return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
    case U'8': return {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
    case U'9': return {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E};
    case U'-': return {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
    case U'/': return {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10};
    case U'.': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};
    case U':': return {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00};
    case U'!': return {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04};
    default: return {0, 0, 0, 0, 0, 0, 0};
    }
}

int PixelCanvas::text_width(std::string_view value, int scale) const
{
    if (value.empty() || scale <= 0)
        return 0;

    size_t offset = 0;
    int characters = 0;
    while (offset < value.size()) {
        next_utf8_codepoint(value, offset);
        ++characters;
    }
    return characters * 6 * scale - scale;
}

void PixelCanvas::text(int x, int y, std::string_view value, int scale, Color color, bool centered)
{
    if (scale <= 0)
        return;
    if (centered)
        x -= text_width(value, scale) / 2;

    int cursor = x;
    size_t offset = 0;
    while (offset < value.size()) {
        const char32_t ch = next_utf8_codepoint(value, offset);
        const auto rows = glyph(ch);
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if ((rows[static_cast<size_t>(row)] & (1U << (4 - col))) != 0)
                    fill_rect(cursor + col * scale, y + row * scale, scale, scale, color);
            }
        }
        cursor += 6 * scale;
    }
}

} // namespace mugen
