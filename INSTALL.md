# Installation, update, and removal

[Русская версия](INSTALL-RU.md)

## Recommended installer

1. Close OBS Studio completely.
2. Run `Mugen-Gamepad-Overlay-0.8.1-Windows-x64-Setup.exe`.
3. Accept the Windows administrator prompt.
4. Start OBS Studio.
5. Add a source and choose **Mugen Gamepad Overlay**.

The installer places the plugin at:

`C:\ProgramData\obs-studio\plugins\mugen-gamepad-overlay`

The installer is currently unsigned, so Windows SmartScreen may show an unknown-publisher warning. Download release files only from the Mugen Art Lab GitHub repository and compare their SHA-256 values with `SHA256SUMS.txt` when desired.

To update, close OBS Studio and run the newer installer. Existing source settings remain in the OBS scene collection. To remove the plugin, use **Installed apps** in Windows.

## Standard manual ZIP

Extract the contents of `Mugen-Gamepad-Overlay-0.8.1-Windows-x64.zip` into:

`C:\ProgramData\obs-studio\plugins`

The final DLL path must be:

`C:\ProgramData\obs-studio\plugins\mugen-gamepad-overlay\bin\64bit\mugen-gamepad-overlay.dll`

To remove a manual installation, close OBS Studio and delete the `mugen-gamepad-overlay` folder.

## Portable/custom OBS ZIP

Close OBS Studio, then extract `Mugen-Gamepad-Overlay-0.8.1-Windows-x64-portable.zip` into the root folder of the portable/custom OBS installation. The archive contains `obs-plugins` and `data` folders.

To remove it manually, delete:

```text
obs-plugins\64bit\mugen-gamepad-overlay.dll
data\obs-plugins\mugen-gamepad-overlay
```

## Troubleshooting

- Make sure OBS Studio was fully closed during installation, update, or removal.
- Confirm that Windows and OBS Studio are x64.
- Open **Help → Log Files → View Current Log** and search for `Mugen Gamepad Overlay` or `mugen-gamepad-overlay.dll`.
- If the source is missing, include the OBS version, Windows version, installation method, controller name, connection mode, and current log in a bug report.
