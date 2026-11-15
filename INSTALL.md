# Installation, update, and removal

[Русская версия](INSTALL-RU.md)

## Recommended installer

1. Close OBS Studio completely.
2. Run `Mugen-Gamepad-Overlay-0.8.2-Windows-x64-Setup.exe`.
3. Accept the Windows administrator prompt.
4. Start OBS Studio.
5. Add a source and choose **Mugen Gamepad Overlay**.

The installer places the plugin at:

`C:\ProgramData\obs-studio\plugins\mugen-gamepad-overlay`

The same installed package includes two DLL copies for OBS 32/33 compatibility:

```text
C:\ProgramData\obs-studio\plugins\mugen-gamepad-overlay\
  mugen-gamepad-overlay.dll          (OBS 33+ layout)
  bin\64bit\mugen-gamepad-overlay.dll (OBS 32 layout)
  data\locale\...
```

OBS Studio 33 tries the new DLL first and skips the legacy copy when the new one loads successfully. This prevents the "Legacy" badge while preserving OBS Studio 32 compatibility. Do not manually delete the legacy copy if you still use OBS 32.

The installer is currently unsigned, so Windows SmartScreen may show an unknown-publisher warning. Download release files only from the Mugen Art Lab GitHub repository and compare their SHA-256 values with `SHA256SUMS.txt` when desired.

To update, close OBS Studio and run the newer installer. Existing source settings remain in the OBS scene collection. To remove the plugin, use **Installed apps** in Windows.

For help or ideas, use the official [Mugen Art Lab website](https://mugenartlab.com/) (**Write**); for detailed reproducible bugs, use [GitHub Issues](https://github.com/Mugen-Art-Lab/Mugen-Gamepad-Overlay/issues).

### Switching skins without changing OBS source dimensions

In Mugen Gamepad Overlay source properties, turn on **Use one source size for all skins (1120×640)** to keep the source bounding box consistent when switching between built-in, Input Overlay and GamepadViewer skins. Resize or reposition the source **once** in the OBS scene after enabling. This mode is disabled by default for backward compatibility; with it off, imported skins retain their native dimensions.

## Standard manual ZIP

Extract the contents of `Mugen-Gamepad-Overlay-0.8.2-Windows-x64.zip` into:

`C:\ProgramData\obs-studio\plugins`

Both DLL locations shown above must be present to support OBS 32 and 33. To remove a manual installation, close OBS Studio and delete the `mugen-gamepad-overlay` folder.

## Portable/custom OBS ZIP

Close OBS Studio, then extract `Mugen-Gamepad-Overlay-0.8.2-Windows-x64-portable.zip` into the root folder of the **portable** OBS installation.

The ZIP includes the OBS 33+ `plugins` structure **and** the OBS 32 `obs-plugins` / `data` structure:

```text
plugins\mugen-gamepad-overlay\mugen-gamepad-overlay.dll
plugins\mugen-gamepad-overlay\data\locale\...
obs-plugins\64bit\mugen-gamepad-overlay.dll
data\obs-plugins\mugen-gamepad-overlay\locale\...
```

When removing a manual portable installation, close OBS and remove all four plugin-specific paths, not any shared `plugins`, `obs-plugins`, or `data` directory. The portable layout is not intended as a replacement for the standard ProgramData installation.

## Troubleshooting

- Make sure OBS Studio was fully closed during installation, update, or removal.
- Confirm that Windows and OBS Studio are x64.
- In OBS Studio 33 the plugin should not be marked **Legacy** when the new DLL is being loaded. OBS 32 still uses the legacy DLL layout.
- Open **Help → Log Files → View Current Log** and search for `Mugen Gamepad Overlay` or `mugen-gamepad-overlay.dll`.
- If the source is missing, include the OBS version, Windows version, installation method, controller name, connection mode, and current log in a bug report.
