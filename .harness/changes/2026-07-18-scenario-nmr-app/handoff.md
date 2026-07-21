# Handoff

## Completed

- Added `MriSdkLoader` for direct `mridll.dll` loading on Windows.
- Reworked `DeviceBridge` to use the SDK loader and keep Demo fallback.
- Added SDK status display and load button to the main window.
- Updated `client/README.md` with test steps.
- Synced the updated client into `/Users/martin/Project/nuclear_system/client`.

## Verification

- Verified `eggcontrollerV2` demo flow and raw SDK usage patterns.
- Verified the target project contains the new client files.
- No local Qt build available in this environment.

## Remaining Risks

- Qt6 build and runtime verification still need a Windows machine.
- Real `mridll.dll` behavior may require small parameter-path adjustments.

## Follow-ups

- Add a small raw-data parser if you want `.raw` preview inside the Qt client.
- On Windows, build and run the app with a real `mridll.dll` beside the executable.
