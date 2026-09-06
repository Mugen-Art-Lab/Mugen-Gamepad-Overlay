// SPDX-FileCopyrightText: 2026 Mugen Art Lab
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gamepad_source.hpp"
#include "preset_variants.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace mugen {
namespace {

constexpr int kRawButtonBase = 1000;
constexpr int kRawHatBase = 2000;
constexpr int kRawAxisBase = 3000;
constexpr int kAxisStride = 2;
constexpr int kTriggerAsButtonBase = 4000;
constexpr int kTriggerAxisSourceBase = 5000;
constexpr int kRawTriggerAxisBase = 6000;
constexpr int kRawTriggerAxisStride = 3;
constexpr int kTriggerSourceAutomatic = 7000;
constexpr int kTriggerSourceDisabled = 7001;
constexpr size_t kLeftTriggerIndex = 0;
constexpr size_t kRightTriggerIndex = 1;
constexpr const char *kInputOverlayAutoVariant = "__auto__";
constexpr const char *kInputOverlayManualVariant = "__manual__";
constexpr float kDeviceListScanInterval = 0.5F;
constexpr const char *kProjectUrl = "https://github.com/Mugen-Art-Lab/Mugen-Gamepad-Overlay";
constexpr const char *kMugenArtLabUrl = "https://github.com/Mugen-Art-Lab";

bool string_equals(const char *value, const char *expected)
{
    return value && expected && std::string_view(value) == expected;
}

bool path_equals(const std::string &a, const std::string &b)
{
#if defined(_WIN32)
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i) {
        const unsigned char ca = static_cast<unsigned char>(a[i]);
        const unsigned char cb = static_cast<unsigned char>(b[i]);
        if (std::tolower(ca) != std::tolower(cb) && !(a[i] == '/' && b[i] == '\\') &&
            !(a[i] == '\\' && b[i] == '/'))
            return false;
    }
    return true;
#else
    return a == b;
#endif
}

void set_property_visible(obs_properties_t *properties, const char *name, bool visible)
{
    if (obs_property_t *property = obs_properties_get(properties, name))
        obs_property_set_visible(property, visible);
}

std::string detected_variants_status(size_t count)
{
    const char *prefix = obs_module_text(count == 1 ? "InputOverlayVariantsFoundOne" :
                                                      "InputOverlayVariantsFoundMany");
    return std::string(prefix ? prefix : "Detected PNG variants: ") + std::to_string(count);
}

void update_input_overlay_variant_status(obs_properties_t *properties, size_t count)
{
    if (obs_property_t *status = obs_properties_get(properties, "input_overlay_variant_status")) {
        const std::string text = detected_variants_status(count);
        obs_property_set_description(status, text.c_str());
    }
}

bool refresh_input_overlay_variants(obs_properties_t *properties, obs_data_t *settings)
{
    obs_property_t *list = obs_properties_get(properties, "input_overlay_variant_path");
    if (!list)
        return false;

    const char *config_raw = obs_data_get_string(settings, "input_overlay_config_path");
    const std::string config = config_raw ? config_raw : "";
    const std::vector<PresetTextureVariant> variants = discover_input_overlay_texture_variants(config);

    obs_property_list_clear(list);
    obs_property_list_add_string(list, obs_module_text("InputOverlayVariant.Auto"), kInputOverlayAutoVariant);
    for (const PresetTextureVariant &variant : variants)
        obs_property_list_add_string(list, variant.label.c_str(), variant.path.c_str());
    obs_property_list_add_string(list, obs_module_text("InputOverlayVariant.Manual"),
                                 kInputOverlayManualVariant);

    const char *selected_raw = obs_data_get_string(settings, "input_overlay_variant_path");
    const char *legacy_raw = obs_data_get_string(settings, "input_overlay_image_path");
    std::string selected = selected_raw ? selected_raw : "";
    const std::string legacy = legacy_raw ? legacy_raw : "";

    const auto find_variant = [&](const std::string &path) {
        return std::find_if(variants.begin(), variants.end(), [&](const PresetTextureVariant &variant) {
            return path_equals(path, variant.path);
        });
    };

    if ((selected.empty() || selected == kInputOverlayAutoVariant) && !legacy.empty()) {
        const auto legacy_match = find_variant(legacy);
        selected = legacy_match != variants.end() ? legacy_match->path : kInputOverlayManualVariant;
    } else if (!selected.empty() && selected != kInputOverlayAutoVariant &&
               selected != kInputOverlayManualVariant && find_variant(selected) == variants.end()) {
        selected = kInputOverlayAutoVariant;
    }

    if (selected.empty())
        selected = kInputOverlayAutoVariant;

    obs_data_set_string(settings, "input_overlay_variant_path", selected.c_str());
    if (selected == kInputOverlayManualVariant) {
        set_property_visible(properties, "input_overlay_image_path", true);
    } else {
        obs_data_set_string(settings, "input_overlay_image_path",
                            selected == kInputOverlayAutoVariant ? "" : selected.c_str());
        set_property_visible(properties, "input_overlay_image_path", false);
    }

    update_input_overlay_variant_status(properties, variants.size());
    return true;
}

void set_skin_status_description(obs_properties_t *properties, const char *translation_key)
{
    if (obs_property_t *status = obs_properties_get(properties, "skin_status"))
        obs_property_set_description(status, obs_module_text(translation_key));
}

bool input_overlay_variant_modified(obs_properties_t *properties, obs_property_t *, obs_data_t *settings)
{
    const char *selected_raw = obs_data_get_string(settings, "input_overlay_variant_path");
    const std::string selected = selected_raw ? selected_raw : "";
    const bool manual = selected == kInputOverlayManualVariant;
    set_property_visible(properties, "input_overlay_image_path", manual);
    if (!manual)
        obs_data_set_string(settings, "input_overlay_image_path",
                            selected == kInputOverlayAutoVariant ? "" : selected.c_str());
    set_skin_status_description(properties, "SkinStatus.InputOverlayPending");
    return true;
}

bool input_overlay_config_modified(obs_properties_t *properties, obs_property_t *, obs_data_t *settings)
{
    const bool refreshed = refresh_input_overlay_variants(properties, settings);
    set_skin_status_description(properties, "SkinStatus.InputOverlayPending");
    return refreshed;
}

bool gamepadviewer_css_modified(obs_properties_t *properties, obs_property_t *, obs_data_t *)
{
    set_skin_status_description(properties, "SkinStatus.GamepadViewerPending");
    return true;
}

bool universal_service_buttons_modified(obs_properties_t *, obs_property_t *, obs_data_t *)
{
    return true;
}

bool dpad_mode_modified(obs_properties_t *properties, obs_property_t *, obs_data_t *settings)
{
    const char *mode = obs_data_get_string(settings, "dpad_input_mode");
    set_property_visible(properties, "dpad_axis_threshold", !string_equals(mode, "buttons"));
    return true;
}

bool skin_modified(obs_properties_t *properties, obs_property_t *, obs_data_t *settings)
{
    const char *skin_raw = obs_data_get_string(settings, "skin");
    const bool universal = !skin_raw || !*skin_raw || string_equals(skin_raw, "universal");
    const bool input_overlay = string_equals(skin_raw, "input_overlay");
    const bool gamepadviewer = string_equals(skin_raw, "gamepadviewer");

    set_property_visible(properties, "universal_help", universal);
    set_property_visible(properties, "universal_service_buttons", universal);
    set_property_visible(properties, "input_overlay_config_path", input_overlay);
    set_property_visible(properties, "input_overlay_variant_path", input_overlay);
    set_property_visible(properties, "input_overlay_variant_status", input_overlay);
    set_property_visible(properties, "input_overlay_help", input_overlay);
    set_property_visible(properties, "gamepadviewer_css_path", gamepadviewer);
    set_property_visible(properties, "gamepadviewer_help", gamepadviewer);
    set_property_visible(properties, "gamepadviewer_stick_travel", gamepadviewer);
    set_property_visible(properties, "gamepadviewer_trigger_threshold", gamepadviewer);
    set_property_visible(properties, "gamepadviewer_analog_help", gamepadviewer);
    set_property_visible(properties, "skin_status", input_overlay || gamepadviewer);

    if (input_overlay) {
        refresh_input_overlay_variants(properties, settings);
        set_skin_status_description(properties, "SkinStatus.InputOverlayPending");
    } else {
        set_property_visible(properties, "input_overlay_image_path", false);
        if (gamepadviewer)
            set_skin_status_description(properties, "SkinStatus.GamepadViewerPending");
    }
    return true;
}

const char *action_setting(Action action)
{
    switch (action) {
    case Action::South: return "bind_south";
    case Action::East: return "bind_east";
    case Action::West: return "bind_west";
    case Action::North: return "bind_north";
    case Action::Back: return "bind_back";
    case Action::Guide: return "bind_guide";
    case Action::Start: return "bind_start";
    case Action::Misc1: return "bind_misc1";
    case Action::Touchpad: return "bind_touchpad";
    case Action::LeftShoulder: return "bind_l1";
    case Action::RightShoulder: return "bind_r1";
    case Action::LeftStick: return "bind_l3";
    case Action::RightStick: return "bind_r3";
    case Action::DpadUp: return "bind_up";
    case Action::DpadDown: return "bind_down";
    case Action::DpadLeft: return "bind_left";
    case Action::DpadRight: return "bind_right";
    case Action::Count: break;
    }
    return "";
}

const char *trigger_setting(size_t index)
{
    return index == kLeftTriggerIndex ? "bind_left_trigger" : "bind_right_trigger";
}

const char *trigger_binding_label(size_t index)
{
    return obs_module_text(index == kLeftTriggerIndex ? "Binding.LeftTrigger" :
                                                        "Binding.RightTrigger");
}

FaceButtonLabels xbox_face_button_labels()
{
    return {};
}

FaceButtonLabels playstation_face_button_labels()
{
    return {FaceButtonGlyph::Cross, FaceButtonGlyph::Circle,
            FaceButtonGlyph::Square, FaceButtonGlyph::Triangle};
}

FaceButtonLabels nintendo_face_button_labels()
{
    return {FaceButtonGlyph::B, FaceButtonGlyph::A,
            FaceButtonGlyph::Y, FaceButtonGlyph::X};
}

FaceButtonGlyph fallback_face_glyph(SDL_GamepadButton button)
{
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return FaceButtonGlyph::A;
    case SDL_GAMEPAD_BUTTON_EAST: return FaceButtonGlyph::B;
    case SDL_GAMEPAD_BUTTON_WEST: return FaceButtonGlyph::X;
    case SDL_GAMEPAD_BUTTON_NORTH: return FaceButtonGlyph::Y;
    default: return FaceButtonGlyph::A;
    }
}

FaceButtonGlyph face_glyph_from_sdl(SDL_GamepadButtonLabel label,
                                    SDL_GamepadButton button)
{
    switch (label) {
    case SDL_GAMEPAD_BUTTON_LABEL_A: return FaceButtonGlyph::A;
    case SDL_GAMEPAD_BUTTON_LABEL_B: return FaceButtonGlyph::B;
    case SDL_GAMEPAD_BUTTON_LABEL_X: return FaceButtonGlyph::X;
    case SDL_GAMEPAD_BUTTON_LABEL_Y: return FaceButtonGlyph::Y;
    case SDL_GAMEPAD_BUTTON_LABEL_CROSS: return FaceButtonGlyph::Cross;
    case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE: return FaceButtonGlyph::Circle;
    case SDL_GAMEPAD_BUTTON_LABEL_SQUARE: return FaceButtonGlyph::Square;
    case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE: return FaceButtonGlyph::Triangle;
    case SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN: break;
    }
    return fallback_face_glyph(button);
}

FaceButtonLabels face_button_labels_from_type(SDL_GamepadType type)
{
    FaceButtonLabels labels;
    labels.south = face_glyph_from_sdl(
        SDL_GetGamepadButtonLabelForType(type, SDL_GAMEPAD_BUTTON_SOUTH),
        SDL_GAMEPAD_BUTTON_SOUTH);
    labels.east = face_glyph_from_sdl(
        SDL_GetGamepadButtonLabelForType(type, SDL_GAMEPAD_BUTTON_EAST),
        SDL_GAMEPAD_BUTTON_EAST);
    labels.west = face_glyph_from_sdl(
        SDL_GetGamepadButtonLabelForType(type, SDL_GAMEPAD_BUTTON_WEST),
        SDL_GAMEPAD_BUTTON_WEST);
    labels.north = face_glyph_from_sdl(
        SDL_GetGamepadButtonLabelForType(type, SDL_GAMEPAD_BUTTON_NORTH),
        SDL_GAMEPAD_BUTTON_NORTH);
    return labels;
}

FaceButtonLabels face_button_labels_from_gamepad(SDL_Gamepad *gamepad)
{
    if (!gamepad)
        return xbox_face_button_labels();

    FaceButtonLabels labels;
    labels.south = face_glyph_from_sdl(
        SDL_GetGamepadButtonLabel(gamepad, SDL_GAMEPAD_BUTTON_SOUTH),
        SDL_GAMEPAD_BUTTON_SOUTH);
    labels.east = face_glyph_from_sdl(
        SDL_GetGamepadButtonLabel(gamepad, SDL_GAMEPAD_BUTTON_EAST),
        SDL_GAMEPAD_BUTTON_EAST);
    labels.west = face_glyph_from_sdl(
        SDL_GetGamepadButtonLabel(gamepad, SDL_GAMEPAD_BUTTON_WEST),
        SDL_GAMEPAD_BUTTON_WEST);
    labels.north = face_glyph_from_sdl(
        SDL_GetGamepadButtonLabel(gamepad, SDL_GAMEPAD_BUTTON_NORTH),
        SDL_GAMEPAD_BUTTON_NORTH);
    return labels;
}

FaceButtonLabels manual_face_button_labels(std::string_view mode)
{
    if (mode == "playstation")
        return playstation_face_button_labels();
    if (mode == "nintendo")
        return nintendo_face_button_labels();
    return xbox_face_button_labels();
}

struct ConnectedGamepad {
    SDL_JoystickID id = 0;
    SDL_GamepadType type = SDL_GAMEPAD_TYPE_UNKNOWN;
    std::string name;
};

std::vector<ConnectedGamepad> connected_gamepads()
{
    std::vector<ConnectedGamepad> result;
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    if (ids && count > 0)
        result.reserve(static_cast<size_t>(count));

    for (int i = 0; ids && i < count; ++i) {
        const char *name = SDL_GetGamepadNameForID(ids[i]);
        result.push_back({ids[i], SDL_GetGamepadTypeForID(ids[i]),
                          name && *name ? name : "Gamepad"});
    }

    SDL_free(ids);
    return result;
}

std::string gamepad_display_label(const ConnectedGamepad &gamepad)
{
    return gamepad.name + " [" +
           std::to_string(static_cast<uint32_t>(gamepad.id)) + "]";
}

std::string localized_device_label(const char *translation_key,
                                   const std::string &device_label)
{
    const char *translated = obs_module_text(translation_key);
    std::string result = translated ? translated : "";
    constexpr std::string_view placeholder = "{device}";
    const size_t position = result.find(placeholder);
    if (position != std::string::npos)
        result.replace(position, placeholder.size(), device_label);
    else if (!device_label.empty())
        result += " " + device_label;
    return result;
}

std::string connected_gamepads_signature(const std::vector<ConnectedGamepad> &gamepads)
{
    std::string signature;
    for (const ConnectedGamepad &gamepad : gamepads) {
        signature += std::to_string(static_cast<uint32_t>(gamepad.id));
        signature.push_back('\x1f');
        signature += std::to_string(static_cast<int>(gamepad.type));
        signature.push_back('\x1f');
        signature += gamepad.name;
        signature.push_back('\x1e');
    }
    return signature;
}

std::string current_gamepad_signature()
{
    return connected_gamepads_signature(connected_gamepads());
}

SDL_JoystickID selected_gamepad_id(obs_data_t *settings)
{
    const char *device_raw = settings ? obs_data_get_string(settings, "device_id") : nullptr;
    const std::string device = device_raw && *device_raw ? device_raw : "auto";
    if (device != "auto") {
        const unsigned long parsed = std::strtoul(device.c_str(), nullptr, 10);
        return static_cast<SDL_JoystickID>(parsed);
    }

    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    const SDL_JoystickID selected = ids && count > 0 ? ids[0] : 0;
    SDL_free(ids);
    return selected;
}

FaceButtonLabels face_button_labels_from_settings(obs_data_t *settings)
{
    const char *mode_raw = settings ? obs_data_get_string(settings, "face_button_labels") : nullptr;
    const std::string mode = mode_raw && *mode_raw ? mode_raw : "auto";
    if (mode != "auto")
        return manual_face_button_labels(mode);

    const SDL_JoystickID selected = selected_gamepad_id(settings);
    return selected != 0 ? face_button_labels_from_type(SDL_GetGamepadTypeForID(selected))
                         : xbox_face_button_labels();
}

const char *face_button_glyph_text(FaceButtonGlyph glyph)
{
    switch (glyph) {
    case FaceButtonGlyph::A: return "A";
    case FaceButtonGlyph::B: return "B";
    case FaceButtonGlyph::X: return "X";
    case FaceButtonGlyph::Y: return "Y";
    case FaceButtonGlyph::Cross: return "\xE2\x9C\x95";
    case FaceButtonGlyph::Circle: return "\xE2\x97\x8B";
    case FaceButtonGlyph::Square: return "\xE2\x96\xA1";
    case FaceButtonGlyph::Triangle: return "\xE2\x96\xB3";
    }
    return "?";
}

FaceButtonGlyph face_glyph_for_action(Action action, const FaceButtonLabels &labels)
{
    switch (action) {
    case Action::South: return labels.south;
    case Action::East: return labels.east;
    case Action::West: return labels.west;
    case Action::North: return labels.north;
    default: return FaceButtonGlyph::A;
    }
}

FaceButtonGlyph face_glyph_for_button(SDL_GamepadButton button,
                                      const FaceButtonLabels &labels)
{
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return labels.south;
    case SDL_GAMEPAD_BUTTON_EAST: return labels.east;
    case SDL_GAMEPAD_BUTTON_WEST: return labels.west;
    case SDL_GAMEPAD_BUTTON_NORTH: return labels.north;
    default: return FaceButtonGlyph::A;
    }
}

bool is_face_action(Action action)
{
    return action == Action::South || action == Action::East ||
           action == Action::West || action == Action::North;
}

bool is_face_button(SDL_GamepadButton button)
{
    return button == SDL_GAMEPAD_BUTTON_SOUTH || button == SDL_GAMEPAD_BUTTON_EAST ||
           button == SDL_GAMEPAD_BUTTON_WEST || button == SDL_GAMEPAD_BUTTON_NORTH;
}

std::string action_label(Action action, const FaceButtonLabels &labels)
{
    if (is_face_action(action)) {
        const char *key = action == Action::South ? "Binding.SouthBase"
                          : action == Action::East ? "Binding.EastBase"
                          : action == Action::West ? "Binding.WestBase"
                                                   : "Binding.NorthBase";
        return std::string(obs_module_text(key)) + " / " +
               face_button_glyph_text(face_glyph_for_action(action, labels));
    }

    switch (action) {
    case Action::Back: return obs_module_text("Binding.Back");
    case Action::Guide: return obs_module_text("Binding.Guide");
    case Action::Start: return obs_module_text("Binding.Start");
    case Action::Misc1: return obs_module_text("Binding.Misc1");
    case Action::Touchpad: return obs_module_text("Binding.Touchpad");
    case Action::LeftShoulder: return obs_module_text("Binding.LeftShoulder");
    case Action::RightShoulder: return obs_module_text("Binding.RightShoulder");
    case Action::LeftStick: return obs_module_text("Binding.LeftStick");
    case Action::RightStick: return obs_module_text("Binding.RightStick");
    case Action::DpadUp: return obs_module_text("Binding.DpadUp");
    case Action::DpadDown: return obs_module_text("Binding.DpadDown");
    case Action::DpadLeft: return obs_module_text("Binding.DpadLeft");
    case Action::DpadRight: return obs_module_text("Binding.DpadRight");
    case Action::South:
    case Action::East:
    case Action::West:
    case Action::North:
    case Action::Count:
        break;
    }
    return {};
}

SDL_GamepadButton default_button(Action action)
{
    switch (action) {
    case Action::South: return SDL_GAMEPAD_BUTTON_SOUTH;
    case Action::East: return SDL_GAMEPAD_BUTTON_EAST;
    case Action::West: return SDL_GAMEPAD_BUTTON_WEST;
    case Action::North: return SDL_GAMEPAD_BUTTON_NORTH;
    case Action::Back: return SDL_GAMEPAD_BUTTON_BACK;
    case Action::Guide: return SDL_GAMEPAD_BUTTON_GUIDE;
    case Action::Start: return SDL_GAMEPAD_BUTTON_START;
    case Action::Misc1: return SDL_GAMEPAD_BUTTON_MISC1;
    case Action::Touchpad: return SDL_GAMEPAD_BUTTON_TOUCHPAD;
    case Action::LeftShoulder: return SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
    case Action::RightShoulder: return SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
    case Action::LeftStick: return SDL_GAMEPAD_BUTTON_LEFT_STICK;
    case Action::RightStick: return SDL_GAMEPAD_BUTTON_RIGHT_STICK;
    case Action::DpadUp: return SDL_GAMEPAD_BUTTON_DPAD_UP;
    case Action::DpadDown: return SDL_GAMEPAD_BUTTON_DPAD_DOWN;
    case Action::DpadLeft: return SDL_GAMEPAD_BUTTON_DPAD_LEFT;
    case Action::DpadRight: return SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
    case Action::Count: break;
    }
    return SDL_GAMEPAD_BUTTON_INVALID;
}

void add_sdl_button_choices(obs_property_t *property, const FaceButtonLabels &labels)
{
    for (int button = 0; button < SDL_GAMEPAD_BUTTON_COUNT; ++button) {
        const auto sdl_button = static_cast<SDL_GamepadButton>(button);
        const char *name = is_face_button(sdl_button)
                               ? face_button_glyph_text(face_glyph_for_button(sdl_button, labels))
                               : SDL_GetGamepadStringForButton(sdl_button);
        obs_property_list_add_int(property, name ? name : "SDL button", button);
    }
}

void add_raw_button_and_hat_choices(obs_property_t *property)
{
    for (int button = 0; button < 32; ++button) {
        const std::string label = "Raw button " + std::to_string(button);
        obs_property_list_add_int(property, label.c_str(), kRawButtonBase + button);
    }

    static constexpr std::array<const char *, 4> directions{"Up", "Right", "Down", "Left"};
    for (int hat = 0; hat < 4; ++hat) {
        for (int direction = 0; direction < 4; ++direction) {
            const std::string label = "Raw hat " + std::to_string(hat) + " " + directions[direction];
            obs_property_list_add_int(property, label.c_str(), kRawHatBase + hat * 4 + direction);
        }
    }
}

void add_binding_choices(obs_property_t *property, const FaceButtonLabels &labels)
{
    add_sdl_button_choices(property, labels);
    obs_property_list_add_int(property, obs_module_text("TriggerSource.LeftAsButton"),
                              kTriggerAsButtonBase + static_cast<int>(kLeftTriggerIndex));
    obs_property_list_add_int(property, obs_module_text("TriggerSource.RightAsButton"),
                              kTriggerAsButtonBase + static_cast<int>(kRightTriggerIndex));

    add_raw_button_and_hat_choices(property);

    for (int axis = 0; axis < 8; ++axis) {
        const std::string negative = "Raw axis " + std::to_string(axis) + " negative";
        const std::string positive = "Raw axis " + std::to_string(axis) + " positive";
        obs_property_list_add_int(property, negative.c_str(), kRawAxisBase + axis * kAxisStride);
        obs_property_list_add_int(property, positive.c_str(), kRawAxisBase + axis * kAxisStride + 1);
    }
}

void add_trigger_binding_choices(obs_property_t *property, const FaceButtonLabels &labels,
                                 size_t target_index)
{
    obs_property_list_add_int(
        property,
        obs_module_text(target_index == kLeftTriggerIndex ? "TriggerSource.AutoLeft" :
                                                           "TriggerSource.AutoRight"),
        kTriggerSourceAutomatic);
    obs_property_list_add_int(property, obs_module_text("TriggerSource.Disabled"),
                              kTriggerSourceDisabled);
    obs_property_list_add_int(property, obs_module_text("TriggerSource.StandardLeft"),
                              kTriggerAxisSourceBase + static_cast<int>(kLeftTriggerIndex));
    obs_property_list_add_int(property, obs_module_text("TriggerSource.StandardRight"),
                              kTriggerAxisSourceBase + static_cast<int>(kRightTriggerIndex));

    add_sdl_button_choices(property, labels);
    add_raw_button_and_hat_choices(property);

    for (int axis = 0; axis < 8; ++axis) {
        const std::string prefix = "Raw axis " + std::to_string(axis) + " ";
        obs_property_list_add_int(property, (prefix + obs_module_text("RawAxis.FullRange")).c_str(),
                                  kRawTriggerAxisBase + axis * kRawTriggerAxisStride);
        obs_property_list_add_int(property, (prefix + obs_module_text("RawAxis.Positive")).c_str(),
                                  kRawTriggerAxisBase + axis * kRawTriggerAxisStride + 1);
        obs_property_list_add_int(property, (prefix + obs_module_text("RawAxis.Negative")).c_str(),
                                  kRawTriggerAxisBase + axis * kRawTriggerAxisStride + 2);
    }
}

void refresh_face_button_mapping(obs_properties_t *properties, obs_data_t *settings)
{
    const FaceButtonLabels labels = face_button_labels_from_settings(settings);
    for (size_t i = 0; i < static_cast<size_t>(Action::Count); ++i) {
        const auto action = static_cast<Action>(i);
        obs_property_t *property = obs_properties_get(properties, action_setting(action));
        if (!property)
            continue;
        const std::string label = action_label(action, labels);
        obs_property_set_description(property, label.c_str());
        obs_property_list_clear(property);
        add_binding_choices(property, labels);
    }

    for (size_t index = 0; index < 2; ++index) {
        obs_property_t *property = obs_properties_get(properties, trigger_setting(index));
        if (!property)
            continue;
        obs_property_set_description(property, trigger_binding_label(index));
        obs_property_list_clear(property);
        add_trigger_binding_choices(property, labels, index);
    }
}

bool face_button_labels_modified(obs_properties_t *properties, obs_property_t *,
                                 obs_data_t *settings)
{
    refresh_face_button_mapping(properties, settings);
    return true;
}

bool device_modified(obs_properties_t *properties, obs_property_t *, obs_data_t *settings)
{
    const char *mode = obs_data_get_string(settings, "face_button_labels");
    if (!mode || !*mode || string_equals(mode, "auto"))
        refresh_face_button_mapping(properties, settings);
    return true;
}

float clamp01(float value)
{
    return std::clamp(value, 0.0F, 1.0F);
}

bool nearly_equal(float a, float b)
{
    return std::abs(a - b) < 0.004F;
}

bool state_equal(const GamepadState &a, const GamepadState &b)
{
    return a.pressed == b.pressed && a.connected == b.connected && nearly_equal(a.left_x, b.left_x) &&
           nearly_equal(a.left_y, b.left_y) && nearly_equal(a.right_x, b.right_x) &&
           nearly_equal(a.right_y, b.right_y) && nearly_equal(a.left_trigger, b.left_trigger) &&
           nearly_equal(a.right_trigger, b.right_trigger);
}

bool open_external_url(const char *url)
{
#if defined(_WIN32)
    const HINSTANCE result = ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32)
        blog(LOG_WARNING, "[Mugen Gamepad Overlay] Could not open URL: %s", url);
#else
    blog(LOG_WARNING, "[Mugen Gamepad Overlay] Opening external URLs is not supported on this platform: %s", url);
#endif
    return false;
}

bool open_project_page(obs_properties_t *, obs_property_t *, void *)
{
    return open_external_url(kProjectUrl);
}

bool open_mugen_art_lab(obs_properties_t *, obs_property_t *, void *)
{
    return open_external_url(kMugenArtLabUrl);
}

} // namespace

GamepadSource::GamepadSource(obs_data_t *settings, obs_source_t *source) : source_(source)
{
    toggle_hotkey_id_ = obs_hotkey_register_source(
        source_, "MugenGamepadOverlay.Toggle", obs_module_text("Hotkey.ToggleOverlay"),
        toggle_hotkey_callback, this);
    update(settings);
    draw(GamepadState{});
    connected_gamepads_signature_ = current_gamepad_signature();
}

GamepadSource::~GamepadSource()
{
    if (toggle_hotkey_id_ != OBS_INVALID_HOTKEY_ID) {
        obs_hotkey_unregister(toggle_hotkey_id_);
        toggle_hotkey_id_ = OBS_INVALID_HOTKEY_ID;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    close_gamepad();
    destroy_texture();
}

void GamepadSource::toggle_hotkey_callback(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
    if (pressed && data)
        static_cast<GamepadSource *>(data)->toggle_runtime_enabled();
}

void GamepadSource::toggle_runtime_enabled()
{
    std::lock_guard<std::mutex> lock(mutex_);
    runtime_enabled_ = !runtime_enabled_;
    if (!runtime_enabled_)
        close_gamepad();
    else
        reconnect_elapsed_ = 1.0F;

    has_last_state_ = false;
    draw(GamepadState{});
    texture_dirty_ = true;
    blog(LOG_INFO, "[Mugen Gamepad Overlay] Runtime overlay %s by hotkey",
         runtime_enabled_ ? "enabled" : "disabled");
}

void GamepadSource::defaults(obs_data_t *settings)
{
    obs_data_set_default_string(settings, "device_id", "auto");
    obs_data_set_default_string(settings, "skin", "universal");
    obs_data_set_default_string(settings, "custom_skin_path", "");
    obs_data_set_default_string(settings, "input_overlay_config_path", "");
    obs_data_set_default_string(settings, "input_overlay_variant_path", kInputOverlayAutoVariant);
    obs_data_set_default_string(settings, "input_overlay_image_path", "");
    obs_data_set_default_string(settings, "gamepadviewer_css_path", "");
    obs_data_set_default_string(settings, "universal_service_buttons", "auto");
    obs_data_set_default_string(settings, "face_button_labels", "auto");
    obs_data_set_default_bool(settings, "hide_disconnected", false);
    obs_data_set_default_bool(settings, "polling_enabled", true);
    obs_data_set_default_int(settings, "deadzone", 18);
    obs_data_set_default_string(settings, "dpad_input_mode", "buttons");
    obs_data_set_default_int(settings, "dpad_axis_threshold", 55);
    obs_data_set_default_int(settings, "gamepadviewer_stick_travel", 18);
    obs_data_set_default_int(settings, "gamepadviewer_trigger_threshold", 35);
    obs_data_set_default_int(settings, "trigger_button_threshold", 35);
    obs_data_set_default_int(settings, trigger_setting(kLeftTriggerIndex),
                             kTriggerSourceAutomatic);
    obs_data_set_default_int(settings, trigger_setting(kRightTriggerIndex),
                             kTriggerSourceAutomatic);

    for (size_t i = 0; i < static_cast<size_t>(Action::Count); ++i) {
        const auto action = static_cast<Action>(i);
        obs_data_set_default_int(settings, action_setting(action), static_cast<int>(default_button(action)));
    }
}

obs_properties_t *GamepadSource::properties(void *data)
{
    obs_properties_t *properties = obs_properties_create();

    obs_property_t *devices = obs_properties_add_list(properties, "device_id", obs_module_text("Device"),
                                                       OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    const std::vector<ConnectedGamepad> gamepads = connected_gamepads();
    const std::string automatic_label = gamepads.empty()
                                            ? obs_module_text("Device.AutoDisconnected")
                                            : localized_device_label(
                                                  "Device.AutoCurrent",
                                                  gamepad_display_label(gamepads.front()));
    obs_property_list_add_string(devices, automatic_label.c_str(), "auto");
    obs_property_set_modified_callback(devices, device_modified);

    for (const ConnectedGamepad &gamepad : gamepads) {
        const std::string id = std::to_string(static_cast<uint32_t>(gamepad.id));
        const std::string label = gamepad_display_label(gamepad);
        obs_property_list_add_string(devices, label.c_str(), id.c_str());
    }

    obs_property_t *skins = obs_properties_add_list(properties, "skin", obs_module_text("Skin"),
                                                     OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(skins, obs_module_text("Skin.Universal"), "universal");
    obs_property_list_add_string(skins, obs_module_text("Skin.InputOverlay"), "input_overlay");
    obs_property_list_add_string(skins, obs_module_text("Skin.GamepadViewer"), "gamepadviewer");
    obs_property_set_modified_callback(skins, skin_modified);

    obs_properties_add_text(properties, "universal_help", obs_module_text("UniversalHelp"), OBS_TEXT_INFO);
    obs_property_t *service_buttons = obs_properties_add_list(
        properties, "universal_service_buttons", obs_module_text("UniversalServiceButtons"),
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(service_buttons, obs_module_text("UniversalServiceButtons.Auto"), "auto");
    obs_property_list_add_string(service_buttons, obs_module_text("UniversalServiceButtons.All"), "all");
    obs_property_list_add_string(service_buttons, obs_module_text("UniversalServiceButtons.Basic"), "basic");
    obs_property_list_add_string(service_buttons, obs_module_text("UniversalServiceButtons.Hidden"), "hidden");
    obs_property_set_modified_callback(service_buttons, universal_service_buttons_modified);

    obs_property_t *face_button_labels = obs_properties_add_list(
        properties, "face_button_labels", obs_module_text("FaceButtonLabels"),
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(face_button_labels, obs_module_text("FaceButtonLabels.Auto"), "auto");
    obs_property_list_add_string(face_button_labels, obs_module_text("FaceButtonLabels.Xbox"), "xbox");
    obs_property_list_add_string(face_button_labels, obs_module_text("FaceButtonLabels.PlayStation"), "playstation");
    obs_property_list_add_string(face_button_labels, obs_module_text("FaceButtonLabels.Nintendo"), "nintendo");
    obs_property_set_modified_callback(face_button_labels, face_button_labels_modified);
    obs_properties_add_text(properties, "face_button_labels_help",
                            obs_module_text("FaceButtonLabels.Help"), OBS_TEXT_INFO);

    obs_property_t *input_overlay_config =
        obs_properties_add_path(properties, "input_overlay_config_path", obs_module_text("InputOverlayConfigPath"),
                                OBS_PATH_FILE, "Input Overlay preset (*.json);;JSON (*.json)", nullptr);
    obs_property_set_modified_callback(input_overlay_config, input_overlay_config_modified);

    obs_property_t *input_overlay_variants =
        obs_properties_add_list(properties, "input_overlay_variant_path", obs_module_text("InputOverlayVariant"),
                                OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_set_modified_callback(input_overlay_variants, input_overlay_variant_modified);

    obs_properties_add_path(properties, "input_overlay_image_path", obs_module_text("InputOverlayImagePath"),
                            OBS_PATH_FILE, "PNG texture (*.png);;PNG (*.png)", nullptr);
    obs_properties_add_text(properties, "input_overlay_variant_status",
                            obs_module_text("InputOverlayVariantsNotScanned"), OBS_TEXT_INFO);
    obs_properties_add_text(properties, "input_overlay_help", obs_module_text("InputOverlayHelp"), OBS_TEXT_INFO);

    obs_property_t *gamepadviewer_css =
        obs_properties_add_path(properties, "gamepadviewer_css_path", obs_module_text("GamepadViewerCssPath"),
                                OBS_PATH_FILE, "GamepadViewer CSS (*.css);;CSS (*.css)", nullptr);
    obs_property_set_modified_callback(gamepadviewer_css, gamepadviewer_css_modified);
    obs_properties_add_text(properties, "gamepadviewer_help", obs_module_text("GamepadViewerHelp"), OBS_TEXT_INFO);
    obs_property_t *stick_travel = obs_properties_add_int_slider(
        properties, "gamepadviewer_stick_travel", obs_module_text("GamepadViewerStickTravel"), 0, 40, 1);
    obs_property_int_set_suffix(stick_travel, "%");
    obs_property_t *trigger_threshold = obs_properties_add_int_slider(
        properties, "gamepadviewer_trigger_threshold", obs_module_text("GamepadViewerTriggerThreshold"), 0, 100, 1);
    obs_property_int_set_suffix(trigger_threshold, "%");
    obs_properties_add_text(properties, "gamepadviewer_analog_help",
                            obs_module_text("GamepadViewerAnalogHelp"), OBS_TEXT_INFO);
    obs_properties_add_text(properties, "skin_status", obs_module_text("SkinStatus.NotLoaded"), OBS_TEXT_INFO);

    obs_properties_add_bool(properties, "polling_enabled", obs_module_text("PollingEnabled"));
    obs_properties_add_bool(properties, "hide_disconnected", obs_module_text("HideDisconnected"));
    obs_property_t *deadzone = obs_properties_add_int_slider(properties, "deadzone", obs_module_text("Deadzone"), 0, 50, 1);
    obs_property_int_set_suffix(deadzone, "%");

    obs_property_t *dpad_mode = obs_properties_add_list(
        properties, "dpad_input_mode", obs_module_text("DpadInputMode"),
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(dpad_mode, obs_module_text("DpadInputMode.Buttons"), "buttons");
    obs_property_list_add_string(dpad_mode, obs_module_text("DpadInputMode.LeftStick"), "left_stick");
    obs_property_list_add_string(dpad_mode, obs_module_text("DpadInputMode.Both"), "both");
    obs_property_set_modified_callback(dpad_mode, dpad_mode_modified);
    obs_property_t *dpad_threshold = obs_properties_add_int_slider(
        properties, "dpad_axis_threshold", obs_module_text("DpadAxisThreshold"), 20, 95, 1);
    obs_property_int_set_suffix(dpad_threshold, "%");
    obs_properties_add_text(properties, "hotkey_help", obs_module_text("HotkeyHelp"), OBS_TEXT_INFO);

    obs_properties_t *mapping = obs_properties_create();
    obs_properties_add_text(mapping, "mapping_help", obs_module_text("MappingHelp"), OBS_TEXT_INFO);
    const FaceButtonLabels default_labels = xbox_face_button_labels();
    for (size_t index = 0; index < 2; ++index) {
        obs_property_t *trigger_binding = obs_properties_add_list(
            mapping, trigger_setting(index), trigger_binding_label(index),
            OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
        add_trigger_binding_choices(trigger_binding, default_labels, index);
    }
    obs_property_t *trigger_button_threshold = obs_properties_add_int_slider(
        mapping, "trigger_button_threshold", obs_module_text("TriggerButtonThreshold"),
        1, 100, 1);
    obs_property_int_set_suffix(trigger_button_threshold, "%");
    obs_properties_add_text(mapping, "trigger_mapping_help",
                            obs_module_text("TriggerMappingHelp"), OBS_TEXT_INFO);
    for (size_t i = 0; i < static_cast<size_t>(Action::Count); ++i) {
        const auto action = static_cast<Action>(i);
        const std::string label = action_label(action, default_labels);
        obs_property_t *binding = obs_properties_add_list(mapping, action_setting(action), label.c_str(),
                                                           OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
        add_binding_choices(binding, default_labels);
    }
    obs_properties_add_group(properties, "advanced_mapping", obs_module_text("AdvancedMapping"),
                             OBS_GROUP_NORMAL, mapping);

    obs_properties_t *about = obs_properties_create();
    const std::string about_info =
        std::string("Mugen Gamepad Overlay ") + MUGEN_GAMEPAD_OVERLAY_VERSION + "\n" +
        obs_module_text("About.ProjectBy") + "\n" + obs_module_text("About.FreeSoftware");
    obs_properties_add_text(about, "about_info", about_info.c_str(), OBS_TEXT_INFO);
    obs_properties_add_button2(about, "about_project_page", obs_module_text("About.ProjectPage"),
                               open_project_page, nullptr);
    obs_properties_add_button2(about, "about_mugen_art_lab", obs_module_text("About.MugenArtLab"),
                               open_mugen_art_lab, nullptr);
    obs_properties_add_group(properties, "about", obs_module_text("About"), OBS_GROUP_NORMAL, about);

    GamepadSource *source = static_cast<GamepadSource *>(data);
    if (source && source->source_) {
        obs_data_t *current = obs_source_get_settings(source->source_);
        if (current) {
            skin_modified(properties, nullptr, current);
            dpad_mode_modified(properties, nullptr, current);
            refresh_face_button_mapping(properties, current);
            if (obs_property_t *status = obs_properties_get(properties, "skin_status")) {
                const char *description = source->skin_status_.empty()
                                              ? obs_module_text("SkinStatus.NotLoaded")
                                              : source->skin_status_.c_str();
                obs_property_set_description(status, description);
            }
            obs_data_release(current);
        }
    } else {
        set_property_visible(properties, "universal_help", true);
        set_property_visible(properties, "universal_service_buttons", true);
        set_property_visible(properties, "input_overlay_config_path", false);
        set_property_visible(properties, "input_overlay_variant_path", false);
        set_property_visible(properties, "input_overlay_variant_status", false);
        set_property_visible(properties, "input_overlay_image_path", false);
        set_property_visible(properties, "input_overlay_help", false);
        set_property_visible(properties, "gamepadviewer_css_path", false);
        set_property_visible(properties, "gamepadviewer_help", false);
        set_property_visible(properties, "gamepadviewer_stick_travel", false);
        set_property_visible(properties, "gamepadviewer_trigger_threshold", false);
        set_property_visible(properties, "gamepadviewer_analog_help", false);
        set_property_visible(properties, "skin_status", false);
        set_property_visible(properties, "dpad_axis_threshold", false);
    }

    return properties;
}

void GamepadSource::destroy_texture()
{
    if (!texture_)
        return;
    obs_enter_graphics();
    gs_texture_destroy(texture_);
    obs_leave_graphics();
    texture_ = nullptr;
}

void GamepadSource::replace_skin(SkinDefinition &&skin)
{
    const bool dimensions_changed = canvas_.width() != skin.width || canvas_.height() != skin.height;
    skin_definition_ = std::move(skin);
    input_overlay_preset_ = {};
    gamepadviewer_skin_ = {};
    visible_skin_error_.clear();
    render_mode_ = RenderMode::Mugen;

    if (dimensions_changed) {
        destroy_texture();
        canvas_.resize(skin_definition_.width, skin_definition_.height);
        output_width_.store(skin_definition_.width, std::memory_order_relaxed);
        output_height_.store(skin_definition_.height, std::memory_order_relaxed);
        texture_ready_logged_ = false;
        texture_failure_logged_ = false;
    }

    texture_dirty_ = true;
    has_last_state_ = false;
}

void GamepadSource::replace_input_overlay(InputOverlayPreset &&preset)
{
    const bool dimensions_changed = canvas_.width() != preset.width || canvas_.height() != preset.height;
    input_overlay_preset_ = std::move(preset);
    gamepadviewer_skin_ = {};
    visible_skin_error_.clear();
    render_mode_ = RenderMode::InputOverlay;

    if (dimensions_changed) {
        destroy_texture();
        canvas_.resize(input_overlay_preset_.width, input_overlay_preset_.height);
        output_width_.store(input_overlay_preset_.width, std::memory_order_relaxed);
        output_height_.store(input_overlay_preset_.height, std::memory_order_relaxed);
        texture_ready_logged_ = false;
        texture_failure_logged_ = false;
    }

    texture_dirty_ = true;
    has_last_state_ = false;
}

void GamepadSource::replace_gamepadviewer(GamepadViewerSkin &&skin)
{
    const bool dimensions_changed = canvas_.width() != skin.width || canvas_.height() != skin.height;
    gamepadviewer_skin_ = std::move(skin);
    input_overlay_preset_ = {};
    visible_skin_error_.clear();
    render_mode_ = RenderMode::GamepadViewer;

    if (dimensions_changed) {
        destroy_texture();
        canvas_.resize(gamepadviewer_skin_.width, gamepadviewer_skin_.height);
        output_width_.store(gamepadviewer_skin_.width, std::memory_order_relaxed);
        output_height_.store(gamepadviewer_skin_.height, std::memory_order_relaxed);
        texture_ready_logged_ = false;
        texture_failure_logged_ = false;
    }

    texture_dirty_ = true;
    has_last_state_ = false;
}

void GamepadSource::replace_universal()
{
    const bool dimensions_changed = canvas_.width() != kUniversalSkinWidth ||
                                    canvas_.height() != kUniversalSkinHeight;
    skin_definition_ = {};
    input_overlay_preset_ = {};
    gamepadviewer_skin_ = {};
    visible_skin_error_.clear();
    render_mode_ = RenderMode::Universal;

    if (dimensions_changed) {
        destroy_texture();
        canvas_.resize(kUniversalSkinWidth, kUniversalSkinHeight);
        output_width_.store(kUniversalSkinWidth, std::memory_order_relaxed);
        output_height_.store(kUniversalSkinHeight, std::memory_order_relaxed);
        texture_ready_logged_ = false;
        texture_failure_logged_ = false;
    }

    texture_dirty_ = true;
    has_last_state_ = false;
}

void GamepadSource::replace_error(std::string error)
{
    const bool dimensions_changed = canvas_.width() != kErrorSkinWidth ||
                                    canvas_.height() != kErrorSkinHeight;
    skin_definition_ = {};
    input_overlay_preset_ = {};
    gamepadviewer_skin_ = {};
    visible_skin_error_ = std::move(error);
    render_mode_ = RenderMode::Error;

    if (dimensions_changed) {
        destroy_texture();
        canvas_.resize(kErrorSkinWidth, kErrorSkinHeight);
        output_width_.store(kErrorSkinWidth, std::memory_order_relaxed);
        output_height_.store(kErrorSkinHeight, std::memory_order_relaxed);
        texture_ready_logged_ = false;
        texture_failure_logged_ = false;
    }

    texture_dirty_ = true;
    has_last_state_ = false;
}

UniversalSkinOptions GamepadSource::universal_options() const
{
    UniversalSkinOptions options;
    if (universal_service_buttons_ == "hidden") {
        options.show_back = false;
        options.show_start = false;
        options.show_guide = false;
        options.show_misc1 = false;
        options.show_touchpad = false;
    } else if (universal_service_buttons_ == "basic") {
        options.show_back = true;
        options.show_start = true;
        options.show_guide = false;
        options.show_misc1 = false;
        options.show_touchpad = false;
    } else if (universal_service_buttons_ == "all") {
        options.show_back = true;
        options.show_start = true;
        options.show_guide = true;
        options.show_misc1 = true;
        options.show_touchpad = true;
    } else {
        options.show_back = has_back_;
        options.show_start = has_start_;
        options.show_guide = has_guide_;
        options.show_misc1 = has_misc1_;
        options.show_touchpad = has_touchpad_;
    }
    options.face_buttons = resolved_face_button_labels();
    return options;
}

FaceButtonLabels GamepadSource::resolved_face_button_labels() const
{
    if (face_button_labels_mode_ != "auto")
        return manual_face_button_labels(face_button_labels_mode_);
    if (gamepad_)
        return face_button_labels_from_gamepad(gamepad_);

    SDL_JoystickID selected = 0;
    if (requested_device_ != "auto") {
        const unsigned long parsed = std::strtoul(requested_device_.c_str(), nullptr, 10);
        selected = static_cast<SDL_JoystickID>(parsed);
    } else {
        int count = 0;
        SDL_JoystickID *ids = SDL_GetGamepads(&count);
        if (ids && count > 0)
            selected = ids[0];
        SDL_free(ids);
    }
    return selected != 0 ? face_button_labels_from_type(SDL_GetGamepadTypeForID(selected))
                         : xbox_face_button_labels();
}

bool GamepadSource::load_selected_skin(const std::string &skin_id, const std::string &custom_path,
                                       const std::string &input_overlay_config,
                                       const std::string &input_overlay_image,
                                       const std::string &gamepadviewer_css)
{
    if (skin_id == "universal") {
        last_skin_error_.clear();
        skin_status_.clear();
        replace_universal();
        blog(LOG_INFO, "[Mugen Gamepad Overlay] Loaded built-in universal technical skin");
        return true;
    }

    std::string error;
    if (skin_id == "input_overlay") {
        InputOverlayPreset preset;
        if (load_input_overlay_preset(input_overlay_config, input_overlay_image, preset, error)) {
            last_skin_error_.clear();
            visible_skin_error_.clear();
            const size_t supported = preset.supported_elements;
            const size_t unsupported = preset.unsupported_elements;
            const size_t clipped = preset.clipped_elements;
            const std::string config = preset.config_path;
            const std::string image = preset.image_path;
            const uint32_t width = preset.width;
            const uint32_t height = preset.height;
            replace_input_overlay(std::move(preset));
            skin_status_ = std::string(obs_module_text("SkinStatus.InputOverlayLoaded")) +
                           std::to_string(width) + "x" + std::to_string(height) + ", " +
                           std::to_string(supported) + " / " + std::to_string(unsupported) +
                           " / " + std::to_string(clipped);
            blog(LOG_INFO,
                 "[Mugen Gamepad Overlay] Loaded Input Overlay preset: %s + %s (%ux%u, %zu supported, %zu skipped, %zu clipped)",
                 config.c_str(), image.c_str(), width, height, supported, unsupported, clipped);
            return true;
        }
    } else if (skin_id == "gamepadviewer") {
        GamepadViewerSkin loaded;
        if (load_gamepadviewer_skin(gamepadviewer_css, loaded, error)) {
            last_skin_error_.clear();
            visible_skin_error_.clear();
            const uint32_t width = loaded.width;
            const uint32_t height = loaded.height;
            const size_t layers = loaded.loaded_layers;
            const size_t skipped = loaded.skipped_rules;
            const size_t rejected = loaded.rejected_assets;
            const size_t sticks = loaded.analog_sticks;
            const size_t triggers = loaded.analog_triggers;
            const std::string warning = loaded.warning;
            replace_gamepadviewer(std::move(loaded));
            skin_status_ = std::string(obs_module_text("SkinStatus.GamepadViewerLoaded")) +
                           std::to_string(width) + "x" + std::to_string(height) + ", " +
                           std::to_string(layers) + " / " + std::to_string(skipped) +
                           " / " + std::to_string(rejected) + " — " +
                           obs_module_text("SkinStatus.Analog") + std::to_string(sticks) +
                           " / " + std::to_string(triggers);
            if (!warning.empty())
                skin_status_ += " — " + warning;
            blog(LOG_INFO,
                 "[Mugen Gamepad Overlay] Loaded local GamepadViewer CSS: %s (%ux%u, %zu layers, %zu skipped rules, %zu rejected assets, %zu analog sticks, %zu analog triggers)",
                 gamepadviewer_css.c_str(), width, height, layers, skipped, rejected,
                 sticks, triggers);
            return true;
        }
    } else if (skin_id == "custom") {
        // Kept only for migration and development builds. It is intentionally
        // absent from the normal properties list in the release candidate.
        SkinDefinition loaded;
        if (load_skin_file(custom_path, loaded, error)) {
            last_skin_error_.clear();
            visible_skin_error_.clear();
            skin_status_.clear();
            replace_skin(std::move(loaded));
            blog(LOG_INFO, "[Mugen Gamepad Overlay] Loaded experimental Mugen JSON skin: %s (%ux%u)",
                 skin_definition_.source_path.c_str(), skin_definition_.width, skin_definition_.height);
            return true;
        }
    } else {
        error = "Unknown skin source";
    }

    const std::string selected_path = skin_id == "custom" ? custom_path
                                      : skin_id == "input_overlay" ? input_overlay_config
                                      : skin_id == "gamepadviewer" ? gamepadviewer_css
                                                                    : skin_id;
    if (error.empty())
        error = "Skin could not be loaded";
    const std::string key = selected_path + ": " + error;
    skin_status_ = std::string(obs_module_text("SkinStatus.Error")) + error;
    if (key != last_skin_error_) {
        last_skin_error_ = key;
        blog(LOG_WARNING, "[Mugen Gamepad Overlay] Could not load skin '%s': %s",
             skin_id.c_str(), error.c_str());
    }

    // Do not silently pretend that an imported skin loaded successfully by
    // falling back to the built-in pad. A clear error frame is much easier to
    // diagnose and prevents misleading screenshots or streams.
    replace_error(error);
    return false;
}

void GamepadSource::update(obs_data_t *settings)
{
    std::lock_guard<std::mutex> lock(mutex_);

    const char *device = obs_data_get_string(settings, "device_id");
    const char *skin = obs_data_get_string(settings, "skin");
    const char *custom_path = obs_data_get_string(settings, "custom_skin_path");
    const char *input_overlay_config = obs_data_get_string(settings, "input_overlay_config_path");
    const char *input_overlay_variant = obs_data_get_string(settings, "input_overlay_variant_path");
    const char *input_overlay_image = obs_data_get_string(settings, "input_overlay_image_path");
    const char *gamepadviewer_css = obs_data_get_string(settings, "gamepadviewer_css_path");
    const std::string new_device = device && *device ? device : "auto";
    std::string new_skin = skin && *skin ? skin : "universal";
    if (new_skin == "modern" || new_skin == "snes" || new_skin == "neogeo" ||
        new_skin == "custom") {
        new_skin = "universal";
        obs_data_set_string(settings, "skin", "universal");
    }
    const std::string new_custom_path = custom_path ? custom_path : "";
    const std::string new_input_overlay_config = input_overlay_config ? input_overlay_config : "";
    const std::string new_input_overlay_variant = input_overlay_variant ? input_overlay_variant : "";
    const std::string new_input_overlay_manual_image = input_overlay_image ? input_overlay_image : "";
    const std::string new_gamepadviewer_css = gamepadviewer_css ? gamepadviewer_css : "";
    const std::string new_input_overlay_image =
        new_input_overlay_variant == kInputOverlayManualVariant ? new_input_overlay_manual_image
        : new_input_overlay_variant == kInputOverlayAutoVariant ? new_input_overlay_manual_image
        : !new_input_overlay_variant.empty() ? new_input_overlay_variant
                                              : new_input_overlay_manual_image;

    const bool device_changed = new_device != requested_device_;
    const bool current_skin_missing =
        render_mode_ == RenderMode::InputOverlay ? input_overlay_preset_.elements.empty()
        : render_mode_ == RenderMode::GamepadViewer ? gamepadviewer_skin_.loaded_layers == 0
        : render_mode_ == RenderMode::Mugen ? skin_definition_.elements.empty()
        : render_mode_ == RenderMode::Error;
    const bool skin_changed = new_skin != skin_id_ || new_custom_path != custom_skin_path_ ||
                              new_input_overlay_config != input_overlay_config_path_ ||
                              new_input_overlay_variant != input_overlay_variant_path_ ||
                              new_input_overlay_image != input_overlay_image_path_ ||
                              new_gamepadviewer_css != gamepadviewer_css_path_ || current_skin_missing;
    requested_device_ = new_device;
    skin_id_ = new_skin;
    custom_skin_path_ = new_custom_path;
    input_overlay_config_path_ = new_input_overlay_config;
    input_overlay_variant_path_ = new_input_overlay_variant;
    input_overlay_image_path_ = new_input_overlay_image;
    gamepadviewer_css_path_ = new_gamepadviewer_css;
    hide_disconnected_ = obs_data_get_bool(settings, "hide_disconnected");
    const bool new_polling = obs_data_get_bool(settings, "polling_enabled");
    deadzone_ = std::clamp(static_cast<float>(obs_data_get_int(settings, "deadzone")) / 100.0F,
                           0.0F, 0.5F);
    const char *dpad_mode = obs_data_get_string(settings, "dpad_input_mode");
    dpad_input_mode_ = dpad_mode && *dpad_mode ? dpad_mode : "buttons";
    if (dpad_input_mode_ != "buttons" && dpad_input_mode_ != "left_stick" &&
        dpad_input_mode_ != "both")
        dpad_input_mode_ = "buttons";
    dpad_axis_threshold_ = std::clamp(
        static_cast<float>(obs_data_get_int(settings, "dpad_axis_threshold")) / 100.0F,
        0.2F, 0.95F);
    gamepadviewer_stick_travel_ = std::clamp(
        static_cast<float>(obs_data_get_int(settings, "gamepadviewer_stick_travel")) / 100.0F,
        0.0F, 0.4F);
    gamepadviewer_trigger_threshold_ = std::clamp(
        static_cast<float>(obs_data_get_int(settings, "gamepadviewer_trigger_threshold")) / 100.0F,
        0.0F, 1.0F);
    trigger_button_threshold_ = std::clamp(
        static_cast<float>(obs_data_get_int(settings, "trigger_button_threshold")) / 100.0F,
        0.01F, 1.0F);
    const char *service_buttons = obs_data_get_string(settings, "universal_service_buttons");
    const std::string new_service_buttons = service_buttons && *service_buttons
                                                ? service_buttons
                                                : "auto";
    if (new_service_buttons == "auto" || new_service_buttons == "all" ||
        new_service_buttons == "basic" || new_service_buttons == "hidden")
        universal_service_buttons_ = new_service_buttons;
    else
        universal_service_buttons_ = "auto";

    const char *face_labels = obs_data_get_string(settings, "face_button_labels");
    const std::string new_face_labels = face_labels && *face_labels ? face_labels : "auto";
    if (new_face_labels == "auto" || new_face_labels == "xbox" ||
        new_face_labels == "playstation" || new_face_labels == "nintendo")
        face_button_labels_mode_ = new_face_labels;
    else
        face_button_labels_mode_ = "auto";

    for (size_t i = 0; i < static_cast<size_t>(Action::Count); ++i) {
        const auto action = static_cast<Action>(i);
        bindings_[i] = static_cast<int>(obs_data_get_int(settings, action_setting(action)));
    }
    for (size_t index = 0; index < trigger_bindings_.size(); ++index)
        trigger_bindings_[index] = static_cast<int>(
            obs_data_get_int(settings, trigger_setting(index)));

    if (device_changed || (!new_polling && polling_enabled_))
        close_gamepad();
    polling_enabled_ = new_polling;

    if (skin_changed)
        load_selected_skin(skin_id_, custom_skin_path_, input_overlay_config_path_,
                           input_overlay_image_path_, gamepadviewer_css_path_);

    texture_dirty_ = true;
    has_last_state_ = false;
}

void GamepadSource::close_gamepad()
{
    if (gamepad_) {
        SDL_CloseGamepad(gamepad_);
        gamepad_ = nullptr;
        joystick_ = nullptr;
        opened_id_ = 0;
    }
}

void GamepadSource::try_open_gamepad()
{
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    if (!ids || count <= 0) {
        SDL_free(ids);
        return;
    }

    SDL_JoystickID selected = ids[0];
    if (requested_device_ != "auto") {
        const unsigned long parsed = std::strtoul(requested_device_.c_str(), nullptr, 10);
        selected = 0;
        for (int i = 0; i < count; ++i) {
            if (static_cast<unsigned long>(ids[i]) == parsed) {
                selected = ids[i];
                break;
            }
        }
    }

    if (selected != 0) {
        gamepad_ = SDL_OpenGamepad(selected);
        if (gamepad_) {
            joystick_ = SDL_GetGamepadJoystick(gamepad_);
            opened_id_ = selected;
            has_back_ = SDL_GamepadHasButton(gamepad_, SDL_GAMEPAD_BUTTON_BACK);
            has_start_ = SDL_GamepadHasButton(gamepad_, SDL_GAMEPAD_BUTTON_START);
            has_guide_ = SDL_GamepadHasButton(gamepad_, SDL_GAMEPAD_BUTTON_GUIDE);
            has_misc1_ = SDL_GamepadHasButton(gamepad_, SDL_GAMEPAD_BUTTON_MISC1);
            has_touchpad_ = SDL_GamepadHasButton(gamepad_, SDL_GAMEPAD_BUTTON_TOUCHPAD);
            const char *name = SDL_GetGamepadName(gamepad_);
            last_open_error_.clear();
            blog(LOG_INFO,
                 "[Mugen Gamepad Overlay] Opened gamepad: %s (Back=%d Start=%d Guide=%d Misc1=%d Touchpad=%d)",
                 name ? name : "unknown", has_back_, has_start_, has_guide_, has_misc1_, has_touchpad_);
        } else {
            const char *raw_error = SDL_GetError();
            const std::string error = raw_error && *raw_error ? raw_error : "unknown SDL error";
            if (error != last_open_error_) {
                last_open_error_ = error;
                blog(LOG_WARNING, "[Mugen Gamepad Overlay] Could not open gamepad: %s", error.c_str());
            }
        }
    }

    SDL_free(ids);
}

float GamepadSource::normalize_axis(Sint16 value) const
{
    const float raw = value < 0 ? static_cast<float>(value) / 32768.0F : static_cast<float>(value) / 32767.0F;
    const float magnitude = std::abs(raw);
    if (magnitude <= deadzone_)
        return 0.0F;
    const float normalized = (magnitude - deadzone_) / std::max(0.001F, 1.0F - deadzone_);
    return std::copysign(clamp01(normalized), raw);
}

float GamepadSource::standard_trigger_value(SDL_GamepadAxis axis) const
{
    if (!gamepad_)
        return 0.0F;
    return clamp01(static_cast<float>(SDL_GetGamepadAxis(gamepad_, axis)) / 32767.0F);
}

bool GamepadSource::binding_pressed(int binding) const
{
    if (!gamepad_ || !joystick_)
        return false;

    if (binding >= 0 && binding < SDL_GAMEPAD_BUTTON_COUNT)
        return SDL_GetGamepadButton(gamepad_, static_cast<SDL_GamepadButton>(binding));

    if (binding >= kTriggerAsButtonBase && binding < kTriggerAsButtonBase + 2) {
        const SDL_GamepadAxis axis = binding == kTriggerAsButtonBase
                                         ? SDL_GAMEPAD_AXIS_LEFT_TRIGGER
                                         : SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
        return standard_trigger_value(axis) >= trigger_button_threshold_;
    }

    if (binding >= kRawButtonBase && binding < kRawButtonBase + 32) {
        const int button = binding - kRawButtonBase;
        return button < SDL_GetNumJoystickButtons(joystick_) && SDL_GetJoystickButton(joystick_, button);
    }

    if (binding >= kRawHatBase && binding < kRawHatBase + 16) {
        const int encoded = binding - kRawHatBase;
        const int hat = encoded / 4;
        const int direction = encoded % 4;
        if (hat >= SDL_GetNumJoystickHats(joystick_))
            return false;
        const Uint8 value = SDL_GetJoystickHat(joystick_, hat);
        static constexpr std::array<Uint8, 4> masks{SDL_HAT_UP, SDL_HAT_RIGHT, SDL_HAT_DOWN, SDL_HAT_LEFT};
        return (value & masks[direction]) != 0;
    }

    if (binding >= kRawAxisBase && binding < kRawAxisBase + 8 * kAxisStride) {
        const int encoded = binding - kRawAxisBase;
        const int axis = encoded / kAxisStride;
        const bool positive = (encoded % kAxisStride) == 1;
        if (axis >= SDL_GetNumJoystickAxes(joystick_))
            return false;
        const float value = normalize_axis(SDL_GetJoystickAxis(joystick_, axis));
        return positive ? value > 0.55F : value < -0.55F;
    }

    return false;
}

float GamepadSource::trigger_binding_value(int binding,
                                           SDL_GamepadAxis automatic_axis) const
{
    if (!gamepad_ || !joystick_ || binding == kTriggerSourceDisabled)
        return 0.0F;

    if (binding == kTriggerSourceAutomatic)
        return standard_trigger_value(automatic_axis);

    if (binding >= kTriggerAxisSourceBase && binding < kTriggerAxisSourceBase + 2) {
        const SDL_GamepadAxis axis = binding == kTriggerAxisSourceBase
                                         ? SDL_GAMEPAD_AXIS_LEFT_TRIGGER
                                         : SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
        return standard_trigger_value(axis);
    }

    if (binding >= kRawTriggerAxisBase &&
        binding < kRawTriggerAxisBase + 8 * kRawTriggerAxisStride) {
        const int encoded = binding - kRawTriggerAxisBase;
        const int axis = encoded / kRawTriggerAxisStride;
        const int mode = encoded % kRawTriggerAxisStride;
        if (axis >= SDL_GetNumJoystickAxes(joystick_))
            return 0.0F;

        const Sint16 raw = SDL_GetJoystickAxis(joystick_, axis);
        if (mode == 0)
            return clamp01((static_cast<float>(raw) + 32768.0F) / 65535.0F);
        if (mode == 1)
            return clamp01(static_cast<float>(raw) / 32767.0F);
        return clamp01(-static_cast<float>(raw) / 32768.0F);
    }

    return binding_pressed(binding) ? 1.0F : 0.0F;
}

GamepadState GamepadSource::read_state() const
{
    GamepadState state;
    if (!runtime_enabled_ || !polling_enabled_ || !gamepad_ || !SDL_GamepadConnected(gamepad_))
        return state;

    state.connected = true;
    for (size_t i = 0; i < static_cast<size_t>(Action::Count); ++i)
        state.pressed[i] = binding_pressed(bindings_[i]);

    state.left_x = normalize_axis(SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_LEFTX));
    state.left_y = normalize_axis(SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_LEFTY));
    state.right_x = normalize_axis(SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_RIGHTX));
    state.right_y = normalize_axis(SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_RIGHTY));
    state.left_trigger = trigger_binding_value(trigger_bindings_[kLeftTriggerIndex],
                                               SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    state.right_trigger = trigger_binding_value(trigger_bindings_[kRightTriggerIndex],
                                                SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);

    if (dpad_input_mode_ != "buttons") {
        const bool axis_up = state.left_y <= -dpad_axis_threshold_;
        const bool axis_down = state.left_y >= dpad_axis_threshold_;
        const bool axis_left = state.left_x <= -dpad_axis_threshold_;
        const bool axis_right = state.left_x >= dpad_axis_threshold_;
        const bool replace = dpad_input_mode_ == "left_stick";
        const auto merge = [&](Action action, bool axis_pressed) {
            bool &value = state.pressed[static_cast<size_t>(action)];
            value = replace ? axis_pressed : (value || axis_pressed);
        };
        merge(Action::DpadUp, axis_up);
        merge(Action::DpadDown, axis_down);
        merge(Action::DpadLeft, axis_left);
        merge(Action::DpadRight, axis_right);
    }
    return state;
}

void GamepadSource::tick(float seconds)
{
    bool refresh_properties = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!runtime_enabled_)
            return;

        if (polling_enabled_)
            SDL_UpdateGamepads();

        if (gamepad_ && !SDL_GamepadConnected(gamepad_)) {
            blog(LOG_INFO, "[Mugen Gamepad Overlay] Gamepad disconnected");
            close_gamepad();
        }

        reconnect_elapsed_ += seconds;
        if (!gamepad_ && polling_enabled_ && reconnect_elapsed_ >= 1.0F) {
            reconnect_elapsed_ = 0.0F;
            try_open_gamepad();
        }

        device_list_scan_elapsed_ += seconds;
        if (polling_enabled_ && device_list_scan_elapsed_ >= kDeviceListScanInterval) {
            device_list_scan_elapsed_ = 0.0F;
            const std::string signature = current_gamepad_signature();
            if (signature != connected_gamepads_signature_) {
                connected_gamepads_signature_ = signature;
                refresh_properties = true;
            }
        }

        const GamepadState state = read_state();
        if (!has_last_state_ || !state_equal(state, last_state_)) {
            draw(state);
            last_state_ = state;
            has_last_state_ = true;
            texture_dirty_ = true;
        }
    }

    if (refresh_properties && source_)
        obs_source_update_properties(source_);
}

void GamepadSource::draw(const GamepadState &state)
{
    canvas_.clear();
    if (!runtime_enabled_)
        return;
    if (render_mode_ != RenderMode::Error && !state.connected && hide_disconnected_)
        return;
    switch (render_mode_) {
    case RenderMode::Universal:
        draw_universal_skin(canvas_, state, universal_options());
        break;
    case RenderMode::InputOverlay:
        draw_input_overlay_preset(canvas_, input_overlay_preset_, state);
        break;
    case RenderMode::GamepadViewer:
        draw_gamepadviewer_skin(canvas_, gamepadviewer_skin_, state,
                                gamepadviewer_stick_travel_,
                                gamepadviewer_trigger_threshold_);
        break;
    case RenderMode::Mugen:
        draw_skin(canvas_, skin_definition_, state);
        break;
    case RenderMode::Error:
        draw_skin_error(canvas_, visible_skin_error_, skin_id_);
        break;
    }
}

void GamepadSource::render()
{
    std::lock_guard<std::mutex> lock(mutex_);

    const uint32_t width = canvas_.width();
    const uint32_t height = canvas_.height();
    if (!texture_) {
        const uint8_t *initial = canvas_.data();
        texture_ = gs_texture_create(width, height, GS_RGBA, 1, &initial, GS_DYNAMIC);
        texture_dirty_ = false;

        if (texture_) {
            if (!texture_ready_logged_) {
                blog(LOG_INFO, "[Mugen Gamepad Overlay] Render texture created: %ux%u RGBA", width, height);
                texture_ready_logged_ = true;
            }
        } else if (!texture_failure_logged_) {
            blog(LOG_ERROR, "[Mugen Gamepad Overlay] Could not create the render texture");
            texture_failure_logged_ = true;
        }
    } else if (texture_dirty_) {
        gs_texture_set_image(texture_, canvas_.data(), canvas_.pitch(), false);
        texture_dirty_ = false;
    }

    if (!texture_)
        return;

    obs_source_draw(texture_, 0, 0, width, height, false);
}

static const char *get_name(void *)
{
    return obs_module_text("MugenGamepadOverlay");
}

static void *create_source(obs_data_t *settings, obs_source_t *source)
{
    return new GamepadSource(settings, source);
}

static void destroy_source(void *data)
{
    delete static_cast<GamepadSource *>(data);
}

static void update_source(void *data, obs_data_t *settings)
{
    static_cast<GamepadSource *>(data)->update(settings);
}

static void tick_source(void *data, float seconds)
{
    static_cast<GamepadSource *>(data)->tick(seconds);
}

static void render_source(void *data, gs_effect_t *)
{
    static_cast<GamepadSource *>(data)->render();
}

static uint32_t get_width(void *data)
{
    return static_cast<GamepadSource *>(data)->width();
}

static uint32_t get_height(void *data)
{
    return static_cast<GamepadSource *>(data)->height();
}

static obs_source_info make_source_info()
{
    obs_source_info info{};
    info.id = "mugen_gamepad_overlay";
    info.type = OBS_SOURCE_TYPE_INPUT;
    info.output_flags = OBS_SOURCE_VIDEO;
    info.get_name = get_name;
    info.create = create_source;
    info.destroy = destroy_source;
    info.update = update_source;
    info.get_defaults = GamepadSource::defaults;
    info.get_properties = GamepadSource::properties;
    info.get_width = get_width;
    info.get_height = get_height;
    info.video_tick = tick_source;
    info.video_render = render_source;
    return info;
}

obs_source_info gamepad_source_info = make_source_info();

} // namespace mugen
