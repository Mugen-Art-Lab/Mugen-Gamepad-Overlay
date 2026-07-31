/*
 * Mugen Gamepad Overlay
 * Copyright (C) 2026 Mugen Art Lab
 *
 * GPL-2.0-or-later
 */

#define SDL_MAIN_HANDLED 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <obs-module.h>

#include "gamepad_source.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("mugen-gamepad-overlay", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
    return "Local SDL3 gamepad overlay source for OBS Studio";
}

bool obs_module_load(void)
{
    SDL_SetMainReady();
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        blog(LOG_ERROR, "[Mugen Gamepad Overlay] SDL3 initialization failed: %s", SDL_GetError());
        return false;
    }

    // We poll explicitly from the OBS source tick callback. No input thread,
    // no keyboard hook, no browser, no WebSocket server.
    SDL_SetGamepadEventsEnabled(false);

    obs_register_source(&mugen::gamepad_source_info);
    blog(LOG_INFO, "[Mugen Gamepad Overlay] Loaded (0.8.0)");
    return true;
}

void obs_module_unload(void)
{
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    blog(LOG_INFO, "[Mugen Gamepad Overlay] Unloaded");
}
