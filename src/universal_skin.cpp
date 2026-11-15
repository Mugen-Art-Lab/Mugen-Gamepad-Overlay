// SPDX-FileCopyrightText: 2026 Mugen Art Lab
// SPDX-License-Identifier: GPL-2.0-or-later

#include "universal_skin.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace mugen {
namespace {

constexpr Color kShadow{4, 7, 12, 82};
constexpr Color kBodyOuter{23, 28, 37, 248};
constexpr Color kBodyInner{35, 42, 53, 255};
constexpr Color kBodyHighlight{91, 105, 123, 205};
constexpr Color kControlDark{15, 19, 26, 255};
constexpr Color kControlMid{64, 74, 89, 255};
constexpr Color kControlLight{112, 124, 143, 255};
constexpr Color kActive{73, 226, 202, 255};
constexpr Color kActiveSoft{73, 226, 202, 110};
constexpr Color kWhite{234, 239, 247, 255};
constexpr Color kMuted{145, 155, 171, 255};
constexpr Color kDisconnected{231, 118, 95, 255};

// The technical skin renders into a 2x texture without reducing it to
// 560x320 again. The host OBS source keeps its original logical dimensions.
// Only this adapter scales primitives: imported skins are unaffected.
class ScaledCanvas {
public:
    explicit ScaledCanvas(PixelCanvas &target) : target_(target) {}
    void clear() { target_.clear(); }
    void fill_rounded_rect(int x, int y, int w, int h, int r, Color c)
    { target_.fill_rounded_rect(4 * x, 4 * y, 4 * w, 4 * h, 4 * r, c); }
    void fill_circle(int x, int y, int r, Color c)
    { target_.fill_circle(4 * x, 4 * y, 4 * r, c); }
    void fill_ellipse(int x, int y, int rx, int ry, Color c)
    { target_.fill_ellipse(4 * x, 4 * y, 4 * rx, 4 * ry, c); }
    void stroke_circle(int x, int y, int r, int thick, Color c)
    { target_.stroke_circle(4 * x, 4 * y, 4 * r, 4 * thick, c); }
    void stroke_ellipse(int x, int y, int rx, int ry, int thick, Color c)
    { target_.stroke_ellipse(4 * x, 4 * y, 4 * rx, 4 * ry, 4 * thick, c); }
    void stroke_rect(int x, int y, int w, int h, int thick, Color c)
    { target_.stroke_rect(4 * x, 4 * y, 4 * w, 4 * h, 4 * thick, c); }
    void line(int x0, int y0, int x1, int y1, int thick, Color c)
    { target_.line(4 * x0, 4 * y0, 4 * x1, 4 * y1, 4 * thick, c); }
    void fill_polygon(const std::vector<CanvasPoint> &points, Color c)
    {
        std::vector<CanvasPoint> scaled;
        scaled.reserve(points.size());
        for (const CanvasPoint &p : points)
            scaled.push_back({4.0F * p.x, 4.0F * p.y});
        target_.fill_polygon(scaled, c);
    }
    void stroke_polygon(const std::vector<CanvasPoint> &points, int thick, Color c)
    {
        std::vector<CanvasPoint> scaled;
        scaled.reserve(points.size());
        for (const CanvasPoint &p : points)
            scaled.push_back({4.0F * p.x, 4.0F * p.y});
        target_.stroke_polygon(scaled, 4 * thick, c);
    }
    // Queue the tiny 5x7 pixel font until after all geometric primitives.
    // Render at native 2x pixels without the previous downsample, preserving
    // character strokes as well as the detailed controller outlines.
    void text(int x, int y, std::string_view label, int scale, Color c, bool center)
    { labels_.push_back({x, y, std::string(label), scale, c, center}); }

    void draw_labels(PixelCanvas &output) const
    {
        for (const Label &label : labels_)
            output.text(2 * label.x, 2 * label.y, label.value,
                        2 * label.scale, label.color, label.center);
    }

private:
    struct Label {
        int x;
        int y;
        std::string value;
        int scale;
        Color color;
        bool center;
    };
    PixelCanvas &target_;
    std::vector<Label> labels_;
};

// This is an *abstract input monitor*, not a replica of a controller.
// A single clean panel works with every device family. SDL still selects
// real button labels, positions and system controls for each family.
void draw_technical_panel(ScaledCanvas &canvas, ControllerVisualProfile profile)
{
    // Three concentric rounded surfaces form one deliberately simple
    // silhouette. No tracing or imported vendor artwork is involved.
    canvas.fill_rounded_rect(69, 72, 426, 207, 35, kShadow);
    canvas.fill_rounded_rect(66, 68, 428, 206, 34, kBodyHighlight);
    canvas.fill_rounded_rect(68, 70, 424, 202, 32, kBodyOuter);
    canvas.fill_rounded_rect(80, 82, 400, 176, 25, kBodyInner);

    // The three families intentionally share the same visual language.
    // The accent is understated; different positions and glyphs carry
    // the actual identity, not a pretend manufacturer's plastic shell.
    const Color accent = kActive;
    canvas.fill_rounded_rect(99, 81, 76, 3, 1, accent);
    canvas.fill_rounded_rect(387, 81, 76, 3, 1, accent);
    canvas.fill_rounded_rect(102, 249, 92, 2, 1, kBodyHighlight);
    canvas.fill_rounded_rect(366, 249, 92, 2, 1, kBodyHighlight);

    // These are only quiet decoration, independent of live controller input.
    // The family readout is queued later so the pixel font remains sharp.
    (void)profile;
}

bool pressed(const GamepadState &state, Action action)
{
    return state.pressed[static_cast<size_t>(action)];
}

void pill(ScaledCanvas &canvas, int x, int y, int w, int h, Action action,
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

void draw_face_button_glyph(ScaledCanvas &canvas, int cx, int cy,
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

void face_button(ScaledCanvas &canvas, int cx, int cy, Action action,
                 const GamepadState &state, FaceButtonGlyph glyph,
                 ControllerVisualProfile profile)
{
    const bool active = pressed(state, action);
    const bool nintendo = profile == ControllerVisualProfile::NintendoLike;
    const bool playstation = profile == ControllerVisualProfile::PlayStationLike;
    const Color glyph_color = active
        ? Color{17, 24, 30, 255}
        : (nintendo ? Color{208, 215, 224, 255}
           : playstation ? Color{219, 228, 242, 255}
                         : face_button_accent(glyph));
    // Modern controllers have dark button caps and color *on the glyph*,
    // not four giant solid-colored circles floating over the shell.
    const int radius = playstation ? 14 : 15;
    canvas.fill_circle(cx + 1, cy + 2, radius + 2, kShadow);
    canvas.fill_circle(cx, cy, radius, active ? kActive : Color{28, 33, 42, 255});
    canvas.stroke_circle(cx, cy, radius, 1, Color{100, 110, 124, 225});
    draw_face_button_glyph(canvas, cx, cy, glyph, glyph_color);
}

void dpad(ScaledCanvas &canvas, int cx, int cy, const GamepadState &state,
          ControllerVisualProfile profile)
{
    if (profile == ControllerVisualProfile::XboxLike) {
        canvas.fill_circle(cx, cy, 37, Color{20, 23, 28, 255});
        canvas.stroke_circle(cx, cy, 37, 2, Color{80, 87, 97, 255});
    }
    const int size = profile == ControllerVisualProfile::Generic ? 74 : 67;
    const int arm = profile == ControllerVisualProfile::PlayStationLike ? 20 : 24;
    const int left = cx - size / 2;
    const int top = cy - size / 2;
    const bool separated = profile == ControllerVisualProfile::PlayStationLike;

    if (!separated) {
        canvas.fill_rounded_rect(cx - arm / 2 + 1, top + 2, arm, size, 4, kShadow);
        canvas.fill_rounded_rect(left + 1, cy - arm / 2 + 2, size, arm, 4, kShadow);
        canvas.fill_rounded_rect(cx - arm / 2, top, arm, size, 4, kControlDark);
        canvas.fill_rounded_rect(left, cy - arm / 2, size, arm, 4, kControlDark);
    }
    const auto dir = [&](int x, int y, Action action) {
        canvas.fill_rounded_rect(x, y, arm, arm, separated ? 7 : 3,
                                 pressed(state, action) ? kActive : Color{63, 73, 86, 255});
    };
    dir(cx - arm / 2, top, Action::DpadUp);
    dir(cx - arm / 2, top + size - arm, Action::DpadDown);
    dir(left, cy - arm / 2, Action::DpadLeft);
    dir(left + size - arm, cy - arm / 2, Action::DpadRight);
    if (!separated) {
        canvas.fill_rounded_rect(cx - arm / 2, cy - arm / 2, arm, arm, 2,
                                 Color{63, 73, 86, 255});
    } else {
        const Color icon_color{194, 207, 222, 240};
        canvas.line(cx - 4, top + 12, cx, top + 7, 1, icon_color);
        canvas.line(cx, top + 7, cx + 4, top + 12, 1, icon_color);
        canvas.line(cx - 4, top + size - 13, cx, top + size - 8, 1, icon_color);
        canvas.line(cx, top + size - 8, cx + 4, top + size - 13, 1, icon_color);
        canvas.line(left + 11, cy - 4, left + 6, cy, 1, icon_color);
        canvas.line(left + 6, cy, left + 11, cy + 4, 1, icon_color);
        canvas.line(left + size - 12, cy - 4, left + size - 7, cy, 1, icon_color);
        canvas.line(left + size - 7, cy, left + size - 12, cy + 4, 1, icon_color);
    }
}


void stick(ScaledCanvas &canvas, int cx, int cy, Action click_action,
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

    canvas.fill_circle(cx + 2, cy + 3, 29, kShadow);
    canvas.fill_circle(cx, cy, 28, kControlDark);
    canvas.stroke_circle(cx, cy, 28, 2, kBodyHighlight);
    if (std::abs(axis_x) > 0.01F || std::abs(axis_y) > 0.01F)
        canvas.fill_circle(knob_x, knob_y, 21, kActiveSoft);
    canvas.fill_circle(knob_x, knob_y, 19, active ? kActive : kControlMid);
    canvas.stroke_circle(knob_x, knob_y, 19, 2, active ? kWhite : kControlLight);
    canvas.fill_circle(knob_x, knob_y, 4, active ? kWhite : kControlDark);
}

void trigger_meter(ScaledCanvas &canvas, int x, int y, float value, bool right)
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

// Small system controls use SDL actions for glow, not generic decorative
// letters. The layouts differ across families; any button without SDL input
// capability stays visual-only, never falsely lights up.
void small_system_button(ScaledCanvas &canvas, int cx, int cy, int radius,
                         bool pressed_now, std::string_view label)
{
    canvas.fill_circle(cx+1,cy+2,radius+1,kShadow);
    canvas.fill_circle(cx,cy,radius,pressed_now ? kActive : kControlDark);
    canvas.stroke_circle(cx,cy,radius,1,pressed_now ? kWhite : kControlLight);
    if (!label.empty())
        canvas.text(cx,cy-3,label,1,pressed_now ? kControlDark : kWhite,true);
}

void short_side_button(ScaledCanvas &canvas, int cx, int cy, bool active,
                       bool menu)
{
    // Both DualSense Create/Options buttons are narrow, horizontally oriented.
    canvas.fill_rounded_rect(cx-12,cy-5,24,10,4,active ? kActive : kControlDark);
    canvas.stroke_rect(cx-11,cy-5,22,10,1,active ? kWhite : kControlLight);
    if (menu) {
        canvas.line(cx-5,cy-2,cx+5,cy-2,1,kMuted);
        canvas.line(cx-5,cy+1,cx+5,cy+1,1,kMuted);
    } else {
        canvas.line(cx-5,cy-2,cx+4,cy-2,1,kMuted);
        canvas.line(cx-4,cy+1,cx+5,cy+1,1,kMuted);
    }
}

void home_symbol(ScaledCanvas &canvas, int x, int y, Color c)
{
    canvas.line(x-5,y,x,y-5,2,c);
    canvas.line(x,y-5,x+5,y,2,c);
    canvas.line(x-4,y,x-4,y+5,2,c);
    canvas.line(x+4,y,x+4,y+5,2,c);
    canvas.line(x-4,y+5,x+4,y+5,2,c);
}

void microphone_symbol(ScaledCanvas &canvas, int x, int y, Color c)
{
    canvas.fill_rounded_rect(x-2,y-5,4,8,2,c);
    canvas.line(x-5,y,x-4,y+5,1,c);
    canvas.line(x-4,y+5,x+4,y+5,1,c);
    canvas.line(x+4,y+5,x+5,y,1,c);
    canvas.line(x,y+5,x,y+8,1,c);
    canvas.line(x-3,y+8,x+3,y+8,1,c);
}

void center_controls(ScaledCanvas &canvas, const GamepadState &state,
                     const UniversalSkinOptions &options)
{
    const bool visible = options.show_back || options.show_start ||
                         options.show_guide || options.show_misc1 ||
                         options.show_touchpad;
    if (!visible)
        return;

    const bool back = options.show_back && pressed(state, Action::Back);
    const bool start = options.show_start && pressed(state, Action::Start);
    const bool guide = options.show_guide && pressed(state, Action::Guide);
    const bool misc = options.show_misc1 && pressed(state, Action::Misc1);
    const bool touch = options.show_touchpad && pressed(state, Action::Touchpad);

    switch (options.profile) {
    case ControllerVisualProfile::PlayStationLike: {
        // Large touchpad, slim horizontal Create/Options, PS and microphone.
        canvas.fill_rounded_rect(221, 90, 118, 56, 12,
                                 touch ? kActive : Color{21, 27, 35, 255});
        canvas.fill_rounded_rect(225, 94, 110, 48, 10,
                                 touch ? kActive : Color{28, 34, 43, 255});
        if (options.show_back) short_side_button(canvas, 201,111,back,false);
        if (options.show_start) short_side_button(canvas, 359,111,start,true);
        // Mic is a visual system key; Misc1 animates it only when exposed.
        small_system_button(canvas,280,169,12,guide,"PS");
        small_system_button(canvas,280,197,9,misc,"");
        microphone_symbol(canvas,280,195,misc ? kControlDark : kWhite);
        break;
    }
    case ControllerVisualProfile::NintendoLike: {
        // Nintendo family: - / + / Home / C. No invented extra Guide/Capture.
        if (options.show_back)
            small_system_button(canvas,244,112,10,back,"-");
        if (options.show_start)
            small_system_button(canvas,319,112,10,start,"+");
        small_system_button(canvas,290,151,12,guide,"");
        home_symbol(canvas,290,150,guide ? kControlDark : kWhite);
        small_system_button(canvas,289,202,10,misc,"C");
        break;
    }
    case ControllerVisualProfile::XboxLike: {
        // Xbox family: central Guide, View (two panes), Menu (three lines),
        // and a separate Share key below. Do not borrow PS/Nintendo glyphs.
        small_system_button(canvas,280,101,15,guide,"");
        const Color logo_color = guide ? kControlDark : kWhite;
        canvas.line(273,94,287,108,2,logo_color);
        canvas.line(287,94,273,108,2,logo_color);

        small_system_button(canvas,250,151,10,back,"");
        canvas.stroke_rect(246,148,8,6,1,back ? kControlDark : kWhite);
        canvas.stroke_rect(249,151,8,6,1,back ? kControlDark : kWhite);

        small_system_button(canvas,310,151,10,start,"");
        for (int i=0;i<3;++i)
            canvas.line(305,148+i*3,315,148+i*3,1,start ? kControlDark : kWhite);

        small_system_button(canvas,280,177,9,misc,"");
        canvas.stroke_rect(275,172,8,6,1,misc ? kControlDark : kWhite);
        canvas.line(280,171,286,168,1,misc ? kControlDark : kWhite);
        break;
    }
    case ControllerVisualProfile::Generic:
        if (options.show_back)
            short_side_button(canvas,247,122,back,false);
        if (options.show_start)
            short_side_button(canvas,313,122,start,true);
        if (options.show_guide)
            small_system_button(canvas,280,157,12,guide,"H");
        if (options.show_misc1)
            small_system_button(canvas,309,157,9,misc,"C");
        break;
    }
}

} // namespace

void draw_universal_skin(PixelCanvas &output, const GamepadState &state,
                         const UniversalSkinOptions &options)
{
    if (output.width() != kUniversalRenderWidth || output.height() != kUniversalRenderHeight)
        return;

    // 4x geometry resolves into the 2x OBS texture, retaining crisp edges.
    // The tiny technical font is drawn separately after the AA resolve.
    static thread_local PixelCanvas geometry(kUniversalSkinWidth * 4U,
                                             kUniversalSkinHeight * 4U);
    static thread_local PixelCanvas shell_background(kUniversalSkinWidth * 4U,
                                                     kUniversalSkinHeight * 4U);
    static thread_local ControllerVisualProfile cached_profile =
        ControllerVisualProfile::Generic;
    static thread_local bool background_valid = false;
    if (!background_valid || cached_profile != options.profile) {
        shell_background.clear();
        ScaledCanvas background(shell_background);
        draw_technical_panel(background, options.profile);
        cached_profile = options.profile;
        background_valid = true;
    }
    // Only buttons/sticks need repainting for live input; the complex outline
    // is cached and copied as pixels, lowering CPU usage during movement.
    geometry.copy_from(shell_background);
    ScaledCanvas canvas(geometry);

    // Shoulders and analog trigger meters.
    const bool xbox = options.profile == ControllerVisualProfile::XboxLike;
    const bool nintendo = options.profile == ControllerVisualProfile::NintendoLike;
    const std::string_view shoulder_left = nintendo ? "L" : (xbox ? "LB" : "L1");
    const std::string_view shoulder_right = nintendo ? "R" : (xbox ? "RB" : "R1");
    const std::string_view trigger_left = nintendo ? "ZL" : (xbox ? "LT" : "L2");
    const std::string_view trigger_right = nintendo ? "ZR" : (xbox ? "RT" : "R2");
    pill(canvas, 116, 44, 108, 24, Action::LeftShoulder, state, shoulder_left);
    pill(canvas, 336, 44, 108, 24, Action::RightShoulder, state, shoulder_right);
    trigger_meter(canvas, 125, 29, state.left_trigger, false);
    trigger_meter(canvas, 343, 29, state.right_trigger, true);
    canvas.text(113, 29, trigger_left, 1, kMuted, false);
    canvas.text(435, 29, trigger_right, 1, kMuted, false);

    // Layouts also match the real relative heights of sticks and d-pad.
    // The original 232px PlayStation stick row fell below its physical shell.
    int right_stick_x = 338;
    int right_stick_y = 200;
    if (xbox) {
        stick(canvas, 158, 145, Action::LeftStick, state.left_x, state.left_y, state);
        dpad(canvas, 220, 201, state, options.profile);
    } else if (nintendo) {
        stick(canvas, 161, 145, Action::LeftStick, state.left_x, state.left_y, state);
        dpad(canvas, 223, 202, state, options.profile);
        right_stick_y = 198;
    } else {
        dpad(canvas, 155, 147, state, options.profile);
        stick(canvas, 220, 183, Action::LeftStick, state.left_x, state.left_y, state);
        right_stick_y = 183;
    }
    stick(canvas, right_stick_x, right_stick_y, Action::RightStick,
          state.right_x, state.right_y, state);

    const int face_cx = xbox ? 391 : (nintendo ? 404 : 406);
    const int face_top_y = xbox ? 111 : 109;
    const int face_mid_y = xbox ? 145 : 142;
    const int face_bottom_y = xbox ? 179 : 176;
    const int face_spacing = xbox ? 35 : 39;
    face_button(canvas, face_cx, face_bottom_y, Action::South, state,
                options.face_buttons.south, options.profile);
    face_button(canvas, face_cx + face_spacing, face_mid_y, Action::East, state,
                options.face_buttons.east, options.profile);
    face_button(canvas, face_cx - face_spacing, face_mid_y, Action::West, state,
                options.face_buttons.west, options.profile);
    face_button(canvas, face_cx, face_top_y, Action::North, state,
                options.face_buttons.north, options.profile);

    center_controls(canvas, state, options);

    // The family is identified explicitly; all profiles use one consistent
    // abstract technical panel instead of imitating real plastic shells.
    const std::string_view family = xbox ? "XBOX LAYOUT" :
        nintendo ? "NINTENDO LAYOUT" :
        options.profile == ControllerVisualProfile::PlayStationLike
            ? "PLAYSTATION LAYOUT" : "GENERIC LAYOUT";
    canvas.text(280, 260, family, 1, kMuted, true);

    const Color status = state.connected ? kActive : kDisconnected;
    canvas.fill_circle(280, 292, 5, status);
    canvas.text(280, 300, state.connected ? "INPUT READY" : "NO GAMEPAD", 1,
                state.connected ? kMuted : kDisconnected, true);
    output.downsample_from_2x(geometry);
    canvas.draw_labels(output);
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
