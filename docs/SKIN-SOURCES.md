# Skin sources

[Русская версия](SKIN-SOURCES-RU.md)

Third-party skins are not included with Mugen Gamepad Overlay. Their authors and licenses remain independent from this project.

## Manually tested

### GamepadViewer skins

[`frolovlife/gamepadviewer-skins`](https://github.com/frolovlife/gamepadviewer-skins) contains a large retro-controller collection. Download the complete repository so CSS files keep access to their relative asset folders, then select the required `.css` file in the source properties.

### Input Overlay 5.0.5 presets

Gamepad `JSON + PNG` presets from the Input Overlay 5.0.5 package were tested. Keyboard and mouse presets are ignored by this gamepad-only source. After a JSON file is selected, the plugin finds compatible PNG files beside it and lists available visual variants.

## Compatibility note

Compatibility with arbitrary CSS found online is not guaranteed. GamepadViewer skins normally run in a full browser environment, while Mugen Gamepad Overlay implements a deliberately limited local renderer without JavaScript or network requests.
