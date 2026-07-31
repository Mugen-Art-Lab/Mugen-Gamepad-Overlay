#include "skin.hpp"

#include <obs-module.h>
#include <util/bmem.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <string>

namespace mugen {
namespace {

constexpr Color kDefaultColor{116, 126, 143, 255};
constexpr Color kDefaultActive{44, 224, 201, 255};
constexpr Color kDefaultSecondary{20, 23, 28, 255};
constexpr Color kDefaultLabel{232, 236, 242, 255};

Color parse_color(const char *raw, Color fallback)
{
    if (!raw || !*raw)
        return fallback;

    std::string value(raw);
    if (!value.empty() && value.front() == '#')
        value.erase(value.begin());
    if (value.size() != 6 && value.size() != 8)
        return fallback;

    try {
        const unsigned long parsed = std::stoul(value, nullptr, 16);
        if (value.size() == 6) {
            return Color{static_cast<uint8_t>((parsed >> 16U) & 0xFFU),
                         static_cast<uint8_t>((parsed >> 8U) & 0xFFU),
                         static_cast<uint8_t>(parsed & 0xFFU), 255};
        }
        return Color{static_cast<uint8_t>((parsed >> 24U) & 0xFFU),
                     static_cast<uint8_t>((parsed >> 16U) & 0xFFU),
                     static_cast<uint8_t>((parsed >> 8U) & 0xFFU),
                     static_cast<uint8_t>(parsed & 0xFFU)};
    } catch (const std::exception &) {
        return fallback;
    }
}

Action parse_action(const char *raw)
{
    const std::string value = raw ? raw : "";
    if (value == "south") return Action::South;
    if (value == "east") return Action::East;
    if (value == "west") return Action::West;
    if (value == "north") return Action::North;
    if (value == "back") return Action::Back;
    if (value == "guide" || value == "home") return Action::Guide;
    if (value == "start") return Action::Start;
    if (value == "misc1" || value == "capture") return Action::Misc1;
    if (value == "touchpad") return Action::Touchpad;
    if (value == "l1") return Action::LeftShoulder;
    if (value == "r1") return Action::RightShoulder;
    if (value == "l3") return Action::LeftStick;
    if (value == "r3") return Action::RightStick;
    if (value == "up") return Action::DpadUp;
    if (value == "down") return Action::DpadDown;
    if (value == "left") return Action::DpadLeft;
    if (value == "right") return Action::DpadRight;
    return Action::Count;
}

SkinAxis parse_axis(const char *raw)
{
    const std::string value = raw ? raw : "";
    if (value == "left_stick") return SkinAxis::LeftStick;
    if (value == "right_stick") return SkinAxis::RightStick;
    if (value == "left_trigger") return SkinAxis::LeftTrigger;
    if (value == "right_trigger") return SkinAxis::RightTrigger;
    return SkinAxis::None;
}

SkinElementType parse_type(const char *raw, bool &ok)
{
    const std::string value = raw ? raw : "";
    ok = true;
    if (value == "rect") return SkinElementType::Rect;
    if (value == "rounded_rect") return SkinElementType::RoundedRect;
    if (value == "ellipse") return SkinElementType::Ellipse;
    if (value == "circle") return SkinElementType::Circle;
    if (value == "line") return SkinElementType::Line;
    if (value == "dpad") return SkinElementType::Dpad;
    if (value == "stick") return SkinElementType::Stick;
    if (value == "trigger_bar") return SkinElementType::TriggerBar;
    if (value == "arcade_stick") return SkinElementType::ArcadeStick;
    if (value == "text") return SkinElementType::Text;
    ok = false;
    return SkinElementType::Rect;
}

int read_int(obs_data_t *data, const char *key, int fallback = 0)
{
    if (!obs_data_has_user_value(data, key))
        return fallback;
    const long long value = obs_data_get_int(data, key);
    return static_cast<int>(std::clamp<long long>(value, std::numeric_limits<int>::min(),
                                                  std::numeric_limits<int>::max()));
}

bool action_pressed(const GamepadState &state, Action action)
{
    if (action == Action::Count)
        return false;
    return state.pressed[static_cast<size_t>(action)];
}

Color state_color(const SkinElement &element, const GamepadState &state)
{
    return action_pressed(state, element.action) ? element.active : element.color;
}

void draw_label(PixelCanvas &canvas, const SkinElement &element, const GamepadState &state,
                int center_x, int center_y)
{
    if (element.label.empty())
        return;
    const int scale = std::max(1, element.scale);
    const Color label = action_pressed(state, element.action) ? element.active_label_color
                                                              : element.label_color;
    canvas.text(center_x, center_y - (7 * scale) / 2, element.label, scale, label, true);
}

void draw_dpad(PixelCanvas &canvas, const SkinElement &element, const GamepadState &state)
{
    const int size = std::max(12, element.w > 0 ? element.w : element.radius * 2);
    const int arm = std::max(4, size / 3);
    const int cx = element.cx;
    const int cy = element.cy;

    canvas.fill_rounded_rect(cx - arm / 2, cy - size / 2, arm, size, std::max(1, arm / 6), element.secondary);
    canvas.fill_rounded_rect(cx - size / 2, cy - arm / 2, size, arm, std::max(1, arm / 6), element.secondary);

    const auto fill_direction = [&](int x, int y, Action action) {
        canvas.fill_rounded_rect(x, y, arm, arm, std::max(1, arm / 7),
                                 action_pressed(state, action) ? element.active : element.color);
    };
    fill_direction(cx - arm / 2, cy - size / 2, Action::DpadUp);
    fill_direction(cx - arm / 2, cy + size / 2 - arm, Action::DpadDown);
    fill_direction(cx - size / 2, cy - arm / 2, Action::DpadLeft);
    fill_direction(cx + size / 2 - arm, cy - arm / 2, Action::DpadRight);
    canvas.fill_rounded_rect(cx - arm / 2, cy - arm / 2, arm, arm, std::max(1, arm / 7), element.color);
}

void axis_values(const GamepadState &state, SkinAxis axis, float &x, float &y)
{
    x = 0.0F;
    y = 0.0F;
    if (axis == SkinAxis::LeftStick) {
        x = state.left_x;
        y = state.left_y;
    } else if (axis == SkinAxis::RightStick) {
        x = state.right_x;
        y = state.right_y;
    }
}

float trigger_value(const GamepadState &state, SkinAxis axis)
{
    if (axis == SkinAxis::LeftTrigger)
        return state.left_trigger;
    if (axis == SkinAxis::RightTrigger)
        return state.right_trigger;
    return 0.0F;
}

bool parse_skin(obs_data_t *root, const std::string &path, SkinDefinition &out, std::string &error)
{
    if (!root) {
        error = "JSON file could not be opened";
        return false;
    }

    SkinDefinition parsed;
    parsed.id = obs_data_get_string(root, "id");
    parsed.name_key = obs_data_get_string(root, "name_key");
    parsed.width = static_cast<uint32_t>(std::clamp(read_int(root, "width", 480), 64, 2048));
    parsed.height = static_cast<uint32_t>(std::clamp(read_int(root, "height", 270), 64, 2048));
    parsed.source_path = path;

    obs_data_array_t *elements = obs_data_get_array(root, "elements");
    if (!elements) {
        error = "JSON has no elements array";
        return false;
    }

    const size_t count = obs_data_array_count(elements);
    parsed.elements.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        obs_data_t *item = obs_data_array_item(elements, i);
        if (!item)
            continue;

        bool type_ok = false;
        SkinElement element;
        element.type = parse_type(obs_data_get_string(item, "type"), type_ok);
        if (!type_ok) {
            obs_data_release(item);
            continue;
        }

        element.action = parse_action(obs_data_get_string(item, "action"));
        element.axis = parse_axis(obs_data_get_string(item, "axis"));
        element.x = read_int(item, "x");
        element.y = read_int(item, "y");
        element.x2 = read_int(item, "x2");
        element.y2 = read_int(item, "y2");
        element.w = read_int(item, "w");
        element.h = read_int(item, "h");
        element.cx = read_int(item, "cx");
        element.cy = read_int(item, "cy");
        element.rx = read_int(item, "rx");
        element.ry = read_int(item, "ry");
        element.radius = read_int(item, "radius");
        element.radius2 = read_int(item, "radius2");
        element.thickness = std::max(1, read_int(item, "thickness", 1));
        element.travel = read_int(item, "travel");
        element.travel_y = read_int(item, "travel_y", element.travel);
        element.scale = std::max(1, read_int(item, "scale", 1));
        element.centered = obs_data_get_bool(item, "centered");
        element.color = parse_color(obs_data_get_string(item, "color"), kDefaultColor);
        element.active = parse_color(obs_data_get_string(item, "active"), kDefaultActive);
        element.secondary = parse_color(obs_data_get_string(item, "secondary"), kDefaultSecondary);
        element.label_color = parse_color(obs_data_get_string(item, "label_color"), kDefaultLabel);
        element.active_label_color = parse_color(obs_data_get_string(item, "active_label_color"),
                                                 element.label_color);
        element.label = obs_data_get_string(item, "label");

        parsed.elements.push_back(std::move(element));
        obs_data_release(item);
    }
    obs_data_array_release(elements);

    if (parsed.elements.empty()) {
        error = "JSON has no supported elements";
        return false;
    }

    out = std::move(parsed);
    error.clear();
    return true;
}

} // namespace

bool load_skin_file(const std::string &path, SkinDefinition &out, std::string &error)
{
    if (path.empty()) {
        error = "Skin path is empty";
        return false;
    }

    obs_data_t *root = obs_data_create_from_json_file(path.c_str());
    const bool ok = parse_skin(root, path, out, error);
    if (root)
        obs_data_release(root);
    return ok;
}

bool load_builtin_skin(const std::string &id, SkinDefinition &out, std::string &error)
{
    const std::string relative = "skins/" + id + ".json";
    char *path = obs_module_file(relative.c_str());
    if (!path) {
        error = "Built-in skin file was not found: " + relative;
        return false;
    }

    const std::string resolved(path);
    bfree(path);
    return load_skin_file(resolved, out, error);
}

void draw_skin(PixelCanvas &canvas, const SkinDefinition &skin, const GamepadState &state)
{
    canvas.clear();

    for (const SkinElement &element : skin.elements) {
        const Color color = state_color(element, state);
        switch (element.type) {
        case SkinElementType::Rect:
            canvas.fill_rect(element.x, element.y, element.w, element.h, color);
            draw_label(canvas, element, state, element.x + element.w / 2, element.y + element.h / 2);
            break;
        case SkinElementType::RoundedRect:
            canvas.fill_rounded_rect(element.x, element.y, element.w, element.h, element.radius, color);
            draw_label(canvas, element, state, element.x + element.w / 2, element.y + element.h / 2);
            break;
        case SkinElementType::Ellipse:
            canvas.fill_ellipse(element.cx, element.cy, element.rx, element.ry, color);
            draw_label(canvas, element, state, element.cx, element.cy);
            break;
        case SkinElementType::Circle:
            canvas.fill_circle(element.cx, element.cy, element.radius, color);
            if (element.thickness > 1)
                canvas.stroke_circle(element.cx, element.cy, element.radius, element.thickness, element.secondary);
            draw_label(canvas, element, state, element.cx, element.cy);
            break;
        case SkinElementType::Line:
            canvas.line(element.x, element.y, element.x2, element.y2, element.thickness, color);
            break;
        case SkinElementType::Dpad:
            draw_dpad(canvas, element, state);
            break;
        case SkinElementType::Stick: {
            float axis_x = 0.0F;
            float axis_y = 0.0F;
            axis_values(state, element.axis, axis_x, axis_y);
            canvas.fill_circle(element.cx, element.cy, element.radius, element.secondary);
            if (element.thickness > 0)
                canvas.stroke_circle(element.cx, element.cy, element.radius, element.thickness, element.color);
            const int knob_x = element.cx + static_cast<int>(axis_x * static_cast<float>(element.travel));
            const int knob_y = element.cy + static_cast<int>(axis_y * static_cast<float>(element.travel_y));
            canvas.fill_circle(knob_x, knob_y, element.radius2, color);
            draw_label(canvas, element, state, knob_x, knob_y);
            break;
        }
        case SkinElementType::TriggerBar: {
            canvas.fill_rounded_rect(element.x, element.y, element.w, element.h, element.radius, element.secondary);
            const int fill = static_cast<int>(std::round(static_cast<float>(element.w) * trigger_value(state, element.axis)));
            if (fill > 0)
                canvas.fill_rounded_rect(element.x, element.y, fill, element.h, element.radius, element.color);
            break;
        }
        case SkinElementType::ArcadeStick: {
            float axis_x = state.left_x;
            float axis_y = state.left_y;
            if (state.pressed[static_cast<size_t>(Action::DpadLeft)]) axis_x = -1.0F;
            if (state.pressed[static_cast<size_t>(Action::DpadRight)]) axis_x = 1.0F;
            if (state.pressed[static_cast<size_t>(Action::DpadUp)]) axis_y = -1.0F;
            if (state.pressed[static_cast<size_t>(Action::DpadDown)]) axis_y = 1.0F;

            const int knob_x = element.x + static_cast<int>(axis_x * static_cast<float>(element.travel));
            const int knob_y = element.y + static_cast<int>(axis_y * static_cast<float>(element.travel_y));
            canvas.fill_circle(element.cx, element.cy, element.radius, element.secondary);
            canvas.line(element.cx, element.cy - 3, knob_x, knob_y + element.radius2 / 3,
                        element.thickness, element.color);
            canvas.fill_circle(knob_x, knob_y, element.radius2, element.active);
            break;
        }
        case SkinElementType::Text:
            canvas.text(element.x, element.y, element.label, element.scale, element.color, element.centered);
            break;
        }
    }
}

} // namespace mugen
