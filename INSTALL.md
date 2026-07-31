# Installation and removal

## Recommended installer

1. Close OBS Studio completely.
2. Run `Mugen-Gamepad-Overlay-0.8.0-Windows-x64-Setup.exe` as an administrator.
3. Start OBS Studio.
4. Add a source and choose **Mugen Gamepad Overlay**.

The installer places the plugin at:

`C:\ProgramData\obs-studio\plugins\mugen-gamepad-overlay`

To update, close OBS Studio and run the newer installer. To remove the plugin, use **Installed apps** in Windows or delete the folder above while OBS Studio is closed.

## Standard manual ZIP

Extract the contents of `Mugen-Gamepad-Overlay-0.8.0-Windows-x64.zip` into:

`C:\ProgramData\obs-studio\plugins`

The final DLL path must be:

`C:\ProgramData\obs-studio\plugins\mugen-gamepad-overlay\bin\64bit\mugen-gamepad-overlay.dll`

## Portable/custom OBS ZIP

Close OBS Studio, then extract `Mugen-Gamepad-Overlay-0.8.0-Windows-x64-portable.zip` into the root folder of the portable/custom OBS installation. The archive contains `obs-plugins` and `data` folders.

## Troubleshooting

- Make sure OBS Studio was fully closed during installation or update.
- Confirm that Windows and OBS Studio are x64.
- Open **Help → Log Files → View Current Log** and search for `Mugen Gamepad Overlay` or `mugen-gamepad-overlay.dll`.
- If the source is missing, include the OBS version, Windows version, installation method, controller name, connection mode, and current log in a bug report.
