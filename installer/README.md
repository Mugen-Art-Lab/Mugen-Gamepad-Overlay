# Windows installer

[Русская версия](README-RU.md)

The installer is built with Inno Setup 6 after the plugin DLL has been compiled. The normal path is to run `BUILD_WINDOWS.cmd` from the repository root. The release-packaging script creates the standard and portable ZIP files and builds the Setup executable when Inno Setup is available.

Install Inno Setup with:

```powershell
winget install JRSoftware.InnoSetup
```

Then run `BUILD_WINDOWS.cmd` again.

The installer targets a normal all-users OBS installation and places the plugin in:

```text
C:\ProgramData\obs-studio\plugins\mugen-gamepad-overlay
```

A separate portable ZIP is provided for portable or custom OBS installations.

The installer and uninstaller refuse to replace or remove the plugin while `obs64.exe` is running. The language-selection window is bilingual; later pages and the OBS retry dialog follow the selected language.
