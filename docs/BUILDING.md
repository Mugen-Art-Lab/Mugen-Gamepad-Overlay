# Building on Windows

[Русская версия](BUILDING-RU.md)

## Requirements

- Windows 10 or Windows 11 x64;
- Visual Studio 2022 or Build Tools 2022 with **Desktop development with C++**;
- MSVC x64/x86 build tools, a Windows SDK, and CMake tools for Windows;
- internet access for the first workspace configuration;
- optional: Inno Setup 6 for the Setup executable.

The repository contains the plugin-specific source rather than a copy of the whole OBS build template. The build script creates `build-workspace/` from pinned revision `7540ffa` of the official [`obsproject/obs-plugintemplate`](https://github.com/obsproject/obs-plugintemplate). That generated folder is ignored by Git, is not a second repository, and can be deleted for a clean rebuild. Template-only GitHub workflows, sample files, and public QA presets are not copied into it. `GENERATED-WORKSPACE.txt` explains its purpose.

## Local one-command build

Close OBS Studio, then run:

```text
BUILD_WINDOWS.cmd
```

On the first run the script:

1. downloads the pinned OBS plugin template;
2. creates `build-workspace/`;
3. overlays the Mugen Gamepad Overlay source and documentation;
4. downloads the pinned SDL3 and NanoSVG source dependencies through CMake;
5. builds the Windows x64 `Release` configuration;
6. packages the release files.

Later local builds reuse the same workspace and downloaded dependencies.

## GitHub Actions build

A manual GitHub Actions workflow is also available under **Actions → Windows Release Build → Run workflow**.

It builds the selected branch on a clean GitHub-hosted Windows runner, validates the expected release files and their SHA-256 checksums, and uploads the result as a temporary Actions artifact. Running the workflow does **not** create or publish a GitHub Release; release publication remains a separate manual decision.

This is useful for clean reproducible builds without keeping the generated OBS workspace and downloaded build dependencies on the local machine.

## Output

Local public release files are created in:

```text
build-workspace\release\<version>
```

Expected files:

```text
Mugen-Gamepad-Overlay-<version>-Windows-x64-Setup.exe
Mugen-Gamepad-Overlay-<version>-Windows-x64.zip
Mugen-Gamepad-Overlay-<version>-Windows-x64-portable.zip
SHA256SUMS.txt
```

The local installer is omitted when Inno Setup 6 is not installed. Install it with:

```powershell
winget install JRSoftware.InnoSetup
```

Then run `BUILD_WINDOWS.cmd` again.

The GitHub Actions workflow installs Inno Setup automatically and therefore expects all four release files.

## Clean workspace

To regenerate all OBS template files and build caches locally, delete `build-workspace/` and run `BUILD_WINDOWS.cmd` again. Do not delete it merely for a normal source change; incremental builds are much faster.

## Important

- Build outputs and the generated workspace are not committed.
- The release DLL is built from the source in this repository plus the pinned dependencies declared in `CMakeLists.txt`.
- Close OBS Studio before installing, replacing, or removing the DLL.
