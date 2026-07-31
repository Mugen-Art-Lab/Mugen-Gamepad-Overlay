# How the plugin sees a controller

[Русская версия](CONTROLLER-DETECTION-RU.md)

Mugen Gamepad Overlay does not inspect the labels printed on a physical controller. It receives the system representation exposed through SDL: device name, controller type, normalized button layout, and available controls.

## Why the name can differ

The same physical controller can use different names at different Windows layers and in different connection modes. Bluetooth Settings may show the retail model name, `joy.cpl` may show the game-interface name, and SDL may expose a normalized compatible-controller name.

During testing, 8BitDo M30 appeared as:

- DInput: `8BitDo M30 Gamepad`;
- XInput: `Xbox One S Controller`.

Both modes worked. In XInput mode the controller deliberately presents itself as a standard Xbox-compatible device for game compatibility. Steam Input, DS4Windows, vendor adapters, and similar layers can also create a virtual controller with a different name and type.

## Why automatic labels can differ from the shell

**Automatic from device data** uses the type and layout reported by Windows and SDL. Xbox `A/B/X/Y`, PlayStation `✕/○/□/△`, or Nintendo order therefore follows the software representation rather than a photograph or the markings on the controller shell.

When automatic labels do not match the physical controller, select Xbox, PlayStation, or Nintendo manually. This changes the built-in technical skin and mapping-interface labels; it does not redraw third-party PNG or CSS artwork.

## Why joy.cpl may show less

The legacy `joy.cpl` panel is useful for basic buttons, axes, and POV checks, but it does not expose every control available through modern input paths. Guide/Home can be visible to SDL and Mugen Gamepad Overlay even when `joy.cpl` has no separate indicator for it.

## Universal and specialized skins

The built-in skin shows a normalized modern layout. A specialized GamepadViewer skin can represent the physical layout of a retro or six-button controller. For example, 8BitDo M30 can be exposed as a standard gamepad while a Mega Drive CSS skin correctly displays `A/B/C/X/Y/Z`.

## Unsupported legacy joysticks

Windows may list a device in `joy.cpl` while SDL does not classify it as a normalized Gamepad. Such `Generic USB Joystick` devices do not appear in Mugen Gamepad Overlay 0.8.0. A general raw-Joystick mode is a separate future design problem.
