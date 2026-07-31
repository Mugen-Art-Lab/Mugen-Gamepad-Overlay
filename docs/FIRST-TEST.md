# First manual test

[Русская версия](FIRST-TEST-RU.md)

1. Close OBS Studio completely and run `BUILD_WINDOWS.cmd`.
2. Install the generated Setup executable or standard ZIP.
3. Verify automatic labels: DualSense should use `✕ / ○ / □ / △`; an Xbox-compatible controller should use `A / B / X / Y`.
4. Manually switch Xbox, PlayStation, and Nintendo labels and check the built-in skin and advanced mapping labels.
5. Map a face button as the left-trigger source and verify digital `0% / 100%` behavior.
6. Map `Left trigger as button` to a logical button and test the adjustable threshold, then restore defaults.
7. Load `qa-tests/input-overlay-variants/demo-pad.json` and switch among its three PNG variants.
8. Load several real CSS skins from `frolovlife/gamepadviewer-skins`.
9. Load `qa-tests/expected-error/missing-texture.json`; the `SKIN NOT LOADED` frame should appear.
10. Restart OBS Studio and verify that source settings persist.

The full project QA plan is currently maintained in Russian in [TEST-CHECKLIST-RU.md](TEST-CHECKLIST-RU.md).
