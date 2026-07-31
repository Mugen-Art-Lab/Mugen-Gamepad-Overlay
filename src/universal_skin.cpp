#include "universal_skin.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace mugen {
namespace {

constexpr Color kShadow{4, 7, 12, 115};
constexpr Color kBodyOuter{23, 28, 37, 248};
constexpr Color kBodyInner{35, 42, 53, 255};
constexpr Color kBodyHighlight{67, 78, 94, 205};
constexpr Color kControlDark{15, 19, 26, 255};
constexpr Color kControlMid{64, 74, 89, 255};
constexpr Color kControlLight{112, 124, 143, 255};
constexpr Color kActive{73, 226, 202, 255};
constexpr Color kActiveSoft{73, 226, 202, 110};
constexpr Color kWhite{234, 239, 247, 255};
constexpr Color kMuted{145, 155, 171, 255};
constexpr Color kDisconnected{231, 118, 95, 255};

bool pressed(const GamepadState &state, Action action)
{
    return state.pressed[static_cast<size_t>(action)];
}

void pill(PixelCanvas &canvas, int x, int y, int w, int h, Action action,
          const GamepadState &state, std::string_view label)
{
    const bool active = pressed(state, action);
    canvas.fill_rounded_rect(x, y + 2, w, h, h / 2, kShadow);
    canvas.fill_rounded_rect(x, y, w, h, h / 2, active ? kActive : kControlMid);
    canvas.text(x + w / 2, y + h / 2 - 4, label, 1,
                active ? Color{18, 29, 32, 255} : kWhite, true);
}

Color face_button_accent(FaceButtonGlyph glyph)
{
    switch (glyph) {
    case FaceButtonGlyph::A: return Color{91, 199, 135, 255};
    case FaceButtonGlyph::B: return Color{223, 96, 100, 255};
    case FaceButtonGlyph::X: return Color{92, 155, 224, 255};
    case FaceButtonGlyph::Y: return Color{228, 191, 86, 255};
    case FaceButtonGlyph::Cross: return Color{92, 155, 224, 255};
    case FaceButtonGlyph::Circle: return Color{223, 96, 100, 255};
    case FaceButtonGlyph::Square: return Color{210, 112, 184, 255};
    case FaceButtonGlyph::Triangle: return Color{91, 199, 135, 255};
    }
    return kControlLight;
}

void draw_face_button_glyph(PixelCanvas &canvas, int cx, int cy,
                            FaceButtonGlyph glyph, Color color)
{
    switch (glyph) {
    case FaceButtonGlyph::A:
        canvas.text(cx, cy - 7, "A", 2, color, true);
        break;
    case FaceButtonGlyph::B:
        canvas.text(cx, cy - 7, "B", 2, color, true);
        break;
    case FaceButtonGlyph::X:
        canvas.text(cx, cy - 7, "X", 2, color, true);
        break;
    case FaceButtonGlyph::Y:
        canvas.text(cx, cy - 7, "Y", 2, color, true);
        break;
    case FaceButtonGlyph::Cross:
        canvas.line(cx - 7, cy - 7, cx + 7, cy + 7, 3, color);
        canvas.line(cx + 7, cy - 7, cx - 7, cy + 7, 3, color);
        break;
    case FaceButtonGlyph::Circle:
        canvas.stroke_circle(cx, cy, 9, 3, color);
        break;
    case FaceButtonGlyph::Square:
        canvas.stroke_rect(cx - 8, cy - 8, 17, 17, 3, color);
        break;
    case FaceButtonGlyph::Triangle:
        canvas.line(cx, cy - 9, cx - 9, cy + 8, 3, color);
        canvas.line(cx - 9, cy + 8, cx + 9, cy + 8, 3, color);
        canvas.line(cx + 9, cy + 8, cx, cy - 9, 3, color);
        break;
    }
}

void face_button(PixelCanvas &canvas, int cx, int cy, Action action,
                 const GamepadState &state, FaceButtonGlyph glyph)
{
    const bool active = pressed(state, action);
    const Color accent = face_button_accent(glyph);
    const Color glyph_color = active ? Color{19, 25, 32, 255} : Color{21, 25, 31, 255};
    canvas.fill_circle(cx + 2, cy + 3, 23, kShadow);
    canvas.fill_circle(cx, cy, 22, active ? kWhite : accent);
    canvas.stroke_circle(cx, cy, 22, 3, active ? kActive : kControlDark);
    draw_face_button_glyph(canvas, cx, cy, glyph, glyph_color);
}

void dpad(PixelCanvas &canvas, int cx, int cy, const GamepadState &state)
{
    constexpr int size = 82;
    constexpr int arm = 28;
    const int left = cx - size / 2;
    const int top = cy - size / 2;

    canvas.fill_rounded_rect(cx - arm / 2 + 2, top + 3, arm, size, 7, kShadow);
    canvas.fill_rounded_rect(left + 2, cy - arm / 2 + 3, size, arm, 7, kShadow);
    canvas.fill_rounded_rect(cx - arm / 2, top, arm, size, 7, kControlDark);
    canvas.fill_rounded_rect(left, cy - arm / 2, size, arm, 7, kControlDark);

    const auto dir = [&](int x, int y, Action action) {
        canvas.fill_rounded_rect(x, y, arm, arm, 6,
                                 pressed(state, action) ? kActive : kControlMid);
    };
    dir(cx - arm / 2, top, Action::DpadUp);
    dir(cx - arm / 2, top + size - arm, Action::DpadDown);
    dir(left, cy - arm / 2, Action::DpadLeft);
    dir(left + size - arm, cy - arm / 2, Action::DpadRight);
    canvas.fill_rounded_rect(cx - arm / 2, cy - arm / 2, arm, arm, 6, kControlMid);
}

void stick(PixelCanvas &canvas, int cx, int cy, Action click_action,
           float axis_x, float axis_y, const GamepadState &state)
{
    const bool active = pressed(state, click_action);
    const float magnitude = std::sqrt(axis_x * axis_x + axis_y * axis_y);
    if (magnitude > 1.0F) {
        axis_x /= magnitude;
        axis_y /= magnitude;
    }
    const int knob_x = cx + static_cast<int>(std::lround(axis_x * 13.0F));
    const int knob_y = cy + static_cast<int>(std::lround(axis_y * 13.0F));

    canvas.fill_circle(cx + 2, cy + 3, 34, kShadow);
    canvas.fill_circle(cx, cy, 33, kControlDark);
    canvas.stroke_circle(cx, cy, 33, 3, kBodyHighlight);
    if (std::abs(axis_x) > 0.01F || std::abs(axis_y) > 0.01F)
        canvas.fill_circle(knob_x, knob_y, 25, kActiveSoft);
    canvas.fill_circle(knob_x, knob_y, 22, active ? kActive : kControlMid);
    canvas.stroke_circle(knob_x, knob_y, 22, 3, active ? kWhite : kControlLight);
    canvas.fill_circle(knob_x, knob_y, 5, active ? kWhite : kControlDark);
}

void trigger_meter(PixelCanvas &canvas, int x, int y, float value, bool right)
{
    value = std::clamp(value, 0.0F, 1.0F);
    constexpr int w = 92;
    constexpr int h = 9;
    canvas.fill_rounded_rect(x, y, w, h, 4, kControlDark);
    const int fill = static_cast<int>(std::lround(value * static_cast<float>(w)));
    if (fill <= 0)
        return;
    if (right)
        canvas.fill_rounded_rect(x + w - fill, y, fill, h, 4, kActive);
    else
        canvas.fill_rounded_rect(x, y, fill, h, 4, kActive);
}

void center_controls(PixelCanvas &canvas, const GamepadState &state,
                     const UniversalSkinOptions &options)
{
    const bool touchpad = options.show_touchpad;
    if (touchpad) {
        const bool active = pressed(state, Action::Touchpad);
        canvas.fill_rounded_rect(222, 93, 116, 55, 12, kShadow);
        canvas.fill_rounded_rect(222, 90, 116, 55, 12, active ? kActive : kControlDark);
        canvas.stroke_rect(232, 102, 96, 1, 1, active ? kWhite : kBodyHighlight);
        canvas.text(280, 112, "TOUCH", 1, active ? Color{18, 29, 32, 255} : kMuted, true);
    }

    const int side_y = touchpad ? 112 : 121;
    if (options.show_back)
        pill(canvas, touchpad ? 187 : 228, side_y, 29, 14, Action::Back, state, "-");
    if (options.show_start)
        pill(canvas, touchpad ? 344 : 303, side_y, 29, 14, Action::Start, state, "+");

    if (options.show_guide) {
        const bool active = pressed(state, Action::Guide);
        const int cy = touchpad ? 166 : 156;
        canvas.fill_circle(282, cy + 2, 14, kShadow);
        canvas.fill_circle(280, cy, 13, active ? kActive : kControlMid);
        canvas.stroke_circle(280, cy, 13, 2, active ? kWhite : kControlDark);
        canvas.text(280, cy - 4, "H", 1,
                    active ? Color{18, 29, 32, 255} : kWhite, true);
    }

    if (options.show_misc1) {
        const bool active = pressed(state, Action::Misc1);
        const int cy = touchpad ? 165 : 157;
        canvas.fill_circle(312, cy + 2, 9, kShadow);
        canvas.fill_circle(310, cy, 8, active ? kActive : kControlMid);
        canvas.text(310, cy - 3, "C", 1,
                    active ? Color{18, 29, 32, 255} : kWhite, true);
    }
}

} // namespace

void draw_universal_skin(PixelCanvas &canvas, const GamepadState &state,
                         const UniversalSkinOptions &options)
{
    canvas.clear();

    // Soft silhouette and grips. This is deliberately neutral rather than a
    // replica of a particular manufacturer's controller.
    canvas.fill_ellipse(280, 161, 209, 94, kShadow);
    canvas.fill_ellipse(146, 224, 78, 92, kShadow);
    canvas.fill_ellipse(414, 224, 78, 92, kShadow);

    canvas.fill_ellipse(280, 151, 204, 90, kBodyOuter);
    canvas.fill_ellipse(146, 215, 74, 88, kBodyOuter);
    canvas.fill_ellipse(414, 215, 74, 88, kBodyOuter);
    canvas.fill_rounded_rect(95, 70, 370, 145, 58, kBodyOuter);

    canvas.fill_ellipse(280, 151, 184, 72, kBodyInner);
    canvas.fill_rounded_rect(114, 84, 332, 111, 45, kBodyInner);
    canvas.stroke_ellipse(280, 151, 203, 89, 3, kBodyHighlight);

    // Shoulders and analog trigger meters.
    pill(canvas, 116, 44, 108, 24, Action::LeftShoulder, state, "L1");
    pill(canvas, 336, 44, 108, 24, Action::RightShoulder, state, "R1");
    trigger_meter(canvas, 125, 29, state.left_trigger, false);
    trigger_meter(canvas, 343, 29, state.right_trigger, true);
    canvas.text(113, 29, "L2", 1, kMuted, false);
    canvas.text(435, 29, "R2", 1, kMuted, false);

    dpad(canvas, 157, 151, state);

    face_button(canvas, 407, 180, Action::South, state, options.face_buttons.south);
    face_button(canvas, 447, 143, Action::East, state, options.face_buttons.east);
    face_button(canvas, 367, 143, Action::West, state, options.face_buttons.west);
    face_button(canvas, 407, 106, Action::North, state, options.face_buttons.north);

    center_controls(canvas, state, options);

    stick(canvas, 221, 232, Action::LeftStick, state.left_x, state.left_y, state);
    stick(canvas, 339, 232, Action::RightStick, state.right_x, state.right_y, state);

    const Color status = state.connected ? kActive : kDisconnected;
    canvas.fill_circle(280, 292, 5, status);
    canvas.text(280, 300, state.connected ? "INPUT READY" : "NO GAMEPAD", 1,
                state.connected ? kMuted : kDisconnected, true);
}

namespace {

bool detail_contains(std::string_view detail, std::string_view needle)
{
    return !needle.empty() && detail.find(needle) != std::string_view::npos;
}

void draw_error_copy(PixelCanvas &canvas, std::string_view title_ru,
                     std::string_view title_en, std::string_view instruction_ru,
                     std::string_view instruction_en)
{
    const Color primary{239, 242, 248, 255};
    const Color secondary{174, 183, 197, 255};
    const Color accent{221, 103, 94, 255};

    canvas.text(112, 47, title_ru, 2, primary, false);
    canvas.text(112, 76, title_en, 1, secondary, false);
    canvas.text(112, 101, instruction_ru, 1, secondary, false);
    canvas.text(112, 119, instruction_en, 1, secondary, false);
    canvas.text(320, 158, "ПОДРОБНОСТИ В СВОЙСТВАХ", 1, accent, true);
    canvas.text(320, 176, "DETAILS IN SOURCE PROPERTIES", 1, accent, true);
}

} // namespace

void draw_skin_error(PixelCanvas &canvas, std::string_view detail, std::string_view source_id)
{
    canvas.clear();
    canvas.fill_rounded_rect(18, 18, static_cast<int>(canvas.width()) - 36,
                             static_cast<int>(canvas.height()) - 36, 18,
                             Color{28, 32, 41, 245});
    canvas.stroke_rect(30, 30, static_cast<int>(canvas.width()) - 60,
                       static_cast<int>(canvas.height()) - 60, 2,
                       Color{221, 103, 94, 255});
    canvas.fill_circle(72, 75, 23, Color{221, 103, 94, 255});
    canvas.text(72, 64, "!", 3, Color{28, 32, 41, 255}, true);

    const bool input_overlay_preset_missing =
        source_id == "input_overlay" && detail_contains(detail, "config path is empty");
    const bool input_overlay_png_missing =
        source_id == "input_overlay" &&
        detail_contains(detail, "Select the PNG texture atlas used by this Input Overlay preset");
    const bool gamepadviewer_missing =
        source_id == "gamepadviewer" &&
        detail_contains(detail, "Select a local GamepadViewer CSS file");

    if (input_overlay_preset_missing) {
        draw_error_copy(canvas, "ПРЕСЕТ НЕ ВЫБРАН", "PRESET NOT SELECTED",
                        "ВЫБЕРИТЕ JSON В СВОЙСТВАХ",
                        "SELECT JSON IN SOURCE PROPERTIES");
    } else if (input_overlay_png_missing) {
        draw_error_copy(canvas, "PNG НЕ ВЫБРАН", "PNG NOT SELECTED",
                        "ВЫБЕРИТЕ PNG В СВОЙСТВАХ",
                        "SELECT PNG IN SOURCE PROPERTIES");
    } else if (gamepadviewer_missing) {
        draw_error_copy(canvas, "CSS-СКИН НЕ ВЫБРАН", "CSS SKIN NOT SELECTED",
                        "ВЫБЕРИТЕ CSS В СВОЙСТВАХ",
                        "SELECT CSS IN SOURCE PROPERTIES");
    } else {
        draw_error_copy(canvas, "СКИН НЕ ЗАГРУЖЕН", "SKIN NOT LOADED",
                        "ПРОВЕРЬТЕ ФАЙЛ И ЖУРНАЛ OBS",
                        "CHECK THE FILE AND OBS LOG");
    }
}

} // namespace mugen
