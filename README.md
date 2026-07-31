# Mugen Gamepad Overlay 0.8.0

A local, native gamepad overlay source for OBS Studio on Windows.

Mugen Gamepad Overlay reads controller input through SDL3 and renders it directly inside OBS Studio. It does not require a browser source, website, WebSocket server, or network input relay.

![Input Overlay DualSense skin](docs/images/input-overlay-dualsense.png)

## Highlights

- Built-in universal technical skin for immediate testing.
- Local Input Overlay 5.x `JSON + PNG` presets.
- Local GamepadViewer `CSS + SVG/PNG/JPEG` skins rendered without a browser or JavaScript.
- Analog sticks, L3/R3, D-pad, shoulders, analog triggers, Guide/Home, Capture/Misc, and Touchpad click when exposed by the controller.
- Automatic or manual Xbox, PlayStation, and Nintendo face-button labels.
- Live controller hot-plug updates in an open properties window.
- Advanced button and trigger remapping with SDL and raw button/hat/axis sources.
- Clear bilingual error frames for missing or invalid skin files.
- Per-source enable/disable hotkey.

![Built-in Nintendo layout](docs/images/universal-nintendo.png)

## Installation

### Installer — recommended

1. Close OBS Studio.
2. Run `Mugen-Gamepad-Overlay-0.8.0-Windows-x64-Setup.exe`.
3. Start OBS Studio and add a new **Mugen Gamepad Overlay** source.

The installer uses the recommended all-users plugin directory:

`C:\ProgramData\obs-studio\plugins\mugen-gamepad-overlay`

### Manual ZIP

Use the standard ZIP for a normal OBS Studio installation and the portable ZIP for a portable or custom OBS directory. See [INSTALL.md](INSTALL.md) for exact paths and removal instructions.

## Skin sources

Third-party skins are not bundled.

- GamepadViewer: the `frolovlife/gamepadviewer-skins` collection was manually tested.
- Input Overlay: gamepad presets from Input Overlay 5.0.5 were tested.

Imported artwork keeps its own labels and licensing. The built-in label selector only changes the universal skin and mapping interface.

![GamepadViewer DualShock 2 skin](docs/images/gamepadviewer-dualshock2.png)

## Controller detection

Automatic labels follow the controller type and layout reported by Windows and SDL. They may differ from the markings on the physical controller because of Bluetooth/XInput/DInput modes, drivers, Steam Input, DS4Windows, adapters, or virtual controllers.

Verified real-device scenarios include DualSense, Xbox-compatible controllers, Flydigi Vader 2 Pro through USB and its receiver, 8BitDo M30 in multiple modes, SVEN X-PAD, and Flipper Zero in USB Game Controller mode. Specialized raw Joystick/HOTAS devices are outside the 0.8.0 scope.

More detail: [docs/CONTROLLER-DETECTION-RU.md](docs/CONTROLLER-DETECTION-RU.md).

## Known limits

- Windows x64 only in 0.8.0.
- Raw SDL Joystick/HOTAS devices are not supported unless SDL exposes them as a Gamepad.
- Arbitrary browser CSS, JavaScript, HTML, remote assets, and CSS animation are not guaranteed.
- Old Input Overlay INI presets and RetroArch CFG overlays are not supported.
- Touchpad click may be available; touch coordinates and gestures are not.

## Privacy

The plugin reads controller input locally. It does not include telemetry, advertisements, a browser engine, or network requests.

## AI development disclosure

Mugen Gamepad Overlay was developed by Mugen Art Lab with substantial assistance from OpenAI ChatGPT for code generation, debugging, documentation, and release tooling. Project direction, feature decisions, real-device testing, visual evaluation, validation, and release responsibility remain with Mugen Art Lab.

See [AI-DISCLOSURE.md](AI-DISCLOSURE.md).

## Support and issues

Report reproducible bugs through the repository issue tracker:

https://github.com/Mugen-Art-Lab/Mugen-Gamepad-Overlay/issues

Mugen Gamepad Overlay is free and open source. Optional support links are available on the Mugen Art Lab GitHub profile:

https://github.com/Mugen-Art-Lab

## License

GPL-2.0-or-later. See [LICENSE](LICENSE) and [data/THIRD-PARTY-NOTICES.txt](data/THIRD-PARTY-NOTICES.txt).

Mugen Gamepad Overlay is an independent third-party plugin and is not affiliated with or endorsed by the OBS Project.
