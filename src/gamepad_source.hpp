// SPDX-FileCopyrightText: 2026 Mugen Art Lab
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "gamepadviewer_skin.hpp"
#include "input_overlay_preset.hpp"
#include "input_types.hpp"
#include "pixel_canvas.hpp"
#include "skin.hpp"
#include "universal_skin.hpp"

#include <SDL3/SDL.h>
#include <graphics/graphics.h>
#include <obs-module.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace mugen {

class GamepadSource {
public:
    GamepadSource(obs_data_t *settings, obs_source_t *source);
    ~GamepadSource();

    void update(obs_data_t *settings);
    void tick(float seconds);
    void render();

    uint32_t width() const noexcept { return output_width_.load(std::memory_order_relaxed); }
    uint32_t height() const noexcept { return output_height_.load(std::memory_order_relaxed); }

    static void defaults(obs_data_t *settings);
    static obs_properties_t *properties(void *data);

private:
    void close_gamepad();
    void try_open_gamepad();
    bool binding_pressed(int binding) const;
    float trigger_binding_value(int binding, SDL_GamepadAxis automatic_axis) const;
    float standard_trigger_value(SDL_GamepadAxis axis) const;
    GamepadState read_state() const;
    void draw(const GamepadState &state);
    float normalize_axis(Sint16 value) const;
    bool load_selected_skin(const std::string &skin_id, const std::string &custom_path,
                            const std::string &input_overlay_config,
                            const std::string &input_overlay_image,
                            const std::string &gamepadviewer_css);
    void replace_skin(SkinDefinition &&skin);
    void replace_input_overlay(InputOverlayPreset &&preset);
    void replace_gamepadviewer(GamepadViewerSkin &&skin);
    void replace_universal();
    void replace_error(std::string error);
    UniversalSkinOptions universal_options() const;
    FaceButtonLabels resolved_face_button_labels() const;
    void destroy_texture();
    void toggle_runtime_enabled();
    static void toggle_hotkey_callback(void *data, obs_hotkey_id id,
                                       obs_hotkey_t *hotkey, bool pressed);

    obs_source_t *source_ = nullptr;
    SDL_Gamepad *gamepad_ = nullptr;
    SDL_Joystick *joystick_ = nullptr; // Owned by gamepad_.
    SDL_JoystickID opened_id_ = 0;

    std::string requested_device_ = "auto";
    std::string skin_id_;
    std::string custom_skin_path_;
    std::string input_overlay_config_path_;
    std::string input_overlay_variant_path_;
    std::string input_overlay_image_path_;
    std::string gamepadviewer_css_path_;
    enum class RenderMode { Universal, Mugen, InputOverlay, GamepadViewer, Error };
    RenderMode render_mode_ = RenderMode::Universal;
    bool hide_disconnected_ = false;
    bool polling_enabled_ = true;
    bool runtime_enabled_ = true;
    float deadzone_ = 0.18F;
    std::string dpad_input_mode_ = "buttons";
    float dpad_axis_threshold_ = 0.55F;
    float gamepadviewer_stick_travel_ = 0.18F;
    float gamepadviewer_trigger_threshold_ = 0.35F;
    float trigger_button_threshold_ = 0.35F;
    std::string universal_service_buttons_ = "auto";
    std::string face_button_labels_mode_ = "auto";
    bool has_back_ = true;
    bool has_start_ = true;
    bool has_guide_ = true;
    bool has_misc1_ = false;
    bool has_touchpad_ = false;
    float reconnect_elapsed_ = 0.0F;
    float device_list_scan_elapsed_ = 0.0F;
    std::string connected_gamepads_signature_;

    std::array<int, static_cast<size_t>(Action::Count)> bindings_{};
    std::array<int, 2> trigger_bindings_{};

    SkinDefinition skin_definition_;
    InputOverlayPreset input_overlay_preset_;
    GamepadViewerSkin gamepadviewer_skin_;
    PixelCanvas canvas_{480, 270};
    std::atomic<uint32_t> output_width_{480};
    std::atomic<uint32_t> output_height_{270};
    gs_texture_t *texture_ = nullptr;
    bool texture_dirty_ = true;
    bool texture_ready_logged_ = false;
    bool texture_failure_logged_ = false;
    bool has_last_state_ = false;
    GamepadState last_state_{};
    std::string last_open_error_;
    std::string last_skin_error_;
    std::string visible_skin_error_;
    std::string skin_status_;
    obs_hotkey_id toggle_hotkey_id_ = OBS_INVALID_HOTKEY_ID;
    mutable std::mutex mutex_;
};

extern obs_source_info gamepad_source_info;

} // namespace mugen
