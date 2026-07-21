# Plan

## Architecture

Add a safety layer:

```text
SceneTemplate
  -> ProtocolMapper
  -> field whitelist / status
  -> DRY_RUN .par preview
  -> DeviceBridge diagnostic signal
  -> MainWindow SDK diagnostic panel
```

## Field Status

- `mapped`: safe structural call or baseline file reference.
- `pending`: SDK field name/unit/sequence scope requires verification.
- `display-only`: product parameter must not be written to SDK.

## Implementation

- Add `ProtocolMapper.{h,cpp}`.
- Register it in `CMakeLists.txt`.
- Add `DeviceBridge::dryRunScene()`.
- Add `sdkDiagnosticChanged` signal.
- Add left-side `DRY_RUN` button.
- Add right-side `SDK 诊断 / 字段白名单` text view.

## Acceptance

- Build succeeds.
- Offscreen startup succeeds.
- Generated DRY_RUN files keep `realRun=HOLD` and `writeToSdk=false`.
