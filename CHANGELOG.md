# Changelog

## 0.8.2 — redesigned skin, consistent sizing, OBS 32/33 compatibility

### Changes

- Rebuilt the built-in technical skin with a cleaner, higher-resolution controller design, distinct Xbox-like, PlayStation-like, Nintendo-like and neutral layouts, and automatic or manual family selection.
- Added an **optional common 1120×640 source canvas** across built-in, Input Overlay and GamepadViewer skins. When enabled, imported skin art is fitted with preserved aspect ratio, allowing users to switch skins while keeping OBS source dimensions stable. It is **disabled by default** to preserve existing scene layouts.
- Fixed an OBS Studio 33 plugin-load failure caused by obsolete `gs_image_file_init/free` imports. Decode local raster assets using Windows Imaging Component (WIC) without the removed OBS image API.
- Ship both OBS 33+ and OBS 32 plugin folder layouts in installer, standard ZIP and portable ZIP; the Windows x64 binaries are checked for compatibility with both layouts.
- Add a correct plugin name/version (`0.8.2`) to metadata used by the OBS plugin manager.
- Add the official [Mugen Art Lab website](https://mugenartlab.com/) to the plugin's About section and installer publisher/support information. Keep the project GitHub button and GitHub Releases update URL.
- Keep controller input local; no telemetry, browser integration or background network requests were added.

### Compatibility and limitations

- Windows x64 only. Input Overlay 5.x JSON+PNG and locally supported GamepadViewer CSS remain separate optional skin formats; third-party skin packs are not bundled.
- Existing skin transforms are preserved by default. Enabling the common canvas may require **one manual resize/reposition in OBS** because the source reports 1120×640 logical dimensions.
- OBS properties previews on HDR monitors may appear brighter or softer than SDR previews; HDR/SDR captures of the native source were identical in a controlled test. This does not indicate a change to the saved source pixels.

## 0.8.1 — project attribution and release metadata

- Added an About section to source properties with the current plugin version and Mugen Art Lab attribution.
- Added buttons for the Mugen Gamepad Overlay repository and the Mugen Art Lab organization page.
- Installer post-install information now receives the release version automatically from `VERSION`.
- No changes to controller input, skin rendering, or existing source settings.

## 0.8.0 — first public release

- Native local SDL3 controller input source for OBS Studio on Windows x64.
- Built-in universal technical skin.
- Local Input Overlay 5.x JSON + PNG support.
- Local GamepadViewer CSS + SVG/PNG/JPEG support without a browser or JavaScript.
- Analog sticks, D-pad, shoulders, L3/R3, analog triggers, and supported service buttons.
- Automatic and manual Xbox, PlayStation, and Nintendo labels.
- Live hot-plug refresh in source properties.
- Advanced button, raw input, and trigger-source mapping.
- Bilingual missing/invalid-skin error frames.
- Per-source toggle hotkey.

