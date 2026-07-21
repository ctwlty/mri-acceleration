# Task 4 Pre-Hardware Release-Prep Report

Date: 2026-07-21
Branch: `codex/migrate-verified-mri-runtime-impl`
Base / unchanged parent branch: `c236e00`
Scope: release preparation only; no real SDK load, device access, Run, or Abort.

## Delivered

- Fixed the Task 3 Minor: a successful bridge reconnect resets the authoritative execution selection to HOLD, emits the authorization change, and `MainWindow` synchronizes the baseline combo and action buttons from that gate.
- Restored one canonical `hw_cfg` inventory algorithm in PowerShell staging, Qt runtime verification, and all fixtures: include Hidden/System files, use `\` relative-path separators, stable relative-path sorting, uppercase file hashes, UTF-8 records joined by `\n`, and no trailing newline.
- Fixed the Windows PowerShell 5 default-manifest path: `$PSScriptRoot` is now resolved after parameter binding, so production staging/deploy does not require an explicit `-ManifestPath`.
- Completed `client/README.md` and `docs/mri-runtime-migration-report.md` with asset boundaries, exact identity, prerequisites, staged layout, deploy/start commands, CLI precedence, HOLD/baseline semantics, scene unlock criteria, call order, safety and rollback.

## TDD / regression evidence

1. UI RED: `reconnectResetsBaselineVisualsToHold` observed combo `VerifiedBaseline (1)` after reconnect, expected `Hold (0)` at `test_main_window.cpp:92`. GREEN: focused MainWindow run passed 3/3; fresh full suite also passed.
2. Default manifest RED: omitting `-ManifestPath` failed during parameter binding with `Join-Path ... Path is an empty string`, instead of reaching the intended source validation. GREEN: focused staging test printed `STAGE_MRI_RUNTIME_TEST_OK`.
3. Canonical manifest RED:
   - PowerShell fixed fixture expected `1B611088...DA47`, old staging calculated `07A66A5C...B7AD3`.
   - Qt fixed fixture was rejected as `MRI runtime hw_cfg does not match`.
   GREEN: both focused regressions passed, production baseline remained `A8BFF731...F6F`, and the real source/staged copy both verified to that value.

## Fresh Debug verification

Commands:

```powershell
Remove-Item -LiteralPath '<worktree>\client\build' -Recurse -Force
powershell.exe -NoProfile -ExecutionPolicy Bypass -File client\scripts\build.ps1 `
  -Configuration Debug -BuildRoot '<worktree>\client\build'
```

Result: configure/generate succeeded, 76/76 build steps succeeded, CTest 8/8 passed in 7.37 seconds:

- `mri_sdk_loader`
- `device_bridge`
- `device_actions`
- `mri_runtime_resolver`
- `main_window`
- `sdk_verify_fake`
- `gui_auto_connect_fake`
- `stage_mri_runtime`

All SDK-loading tests used the fake test DLL. The process-level auto-connect fixture proves Init only and zero Run/Abort.

## Fresh Release and safe smoke

Commands:

```powershell
Remove-Item -LiteralPath '<worktree>\client\build-release' -Recurse -Force
powershell.exe -NoProfile -ExecutionPolicy Bypass -File client\scripts\build.ps1 `
  -Configuration Release -BuildRoot '<worktree>\client\build-release'

powershell.exe -NoProfile -ExecutionPolicy Bypass -File client\scripts\deploy.ps1 `
  -BuildRoot '<worktree>\client\build-release' -QtOnly
```

Results:

- Release configure/generate succeeded; 21/21 build steps succeeded.
- Release build path: `C:\Users\Administrator\.codex\visualizations\2026\07\21\019f8476-a80e-7eb1-b7c0-d915064ef076\mri-runtime-impl\client\build-release`.
- QtOnly package path: `C:\Users\Administrator\.codex\visualizations\2026\07\21\019f8476-a80e-7eb1-b7c0-d915064ef076\mri-runtime-impl\client\dist`.
- QtOnly native hidden-window smoke passed without elevation: 44 files / 85,104,022 bytes. The package contained no MRI runtime at smoke time.
- `windeployqt6` skipped Windows `D3Dcompiler_47.dll`; optional translation catalog and Direct3D 12 shader-compiler warnings were non-fatal. Required `Qt6Core.dll`, `Qt6Widgets.dll`, and `platforms/qwindows.dll` were present.

Release executables:

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `scenario_nmr_client.exe` | 594,446 | `50CEF941B9290432201F07277B329CEC24D31659B729FB04DAEF82DA4AD59868` |
| `mri_sdk_verify.exe` | 396,383 | `F14376544CF1384D565CE846B4F4CB287DCCFAC1C4D6CDA4522C7DA84143163E` |

## Real-source staging and audit (no DLL load)

After the QtOnly smoke exited, the real assets were staged without launching any executable:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File client\scripts\stage-mri-runtime.ps1 `
  -MriSdkRoot 'C:\Users\Administrator\Desktop\eggcontrol\eggcontrollerV2\Iface\mriRely' `
  -ParameterFile 'C:\MRIScanner\Scan\PTScan.par' `
  -Destination '<worktree>\client\dist\mri-runtime'
```

Audit:

| Item | Source / expected | Staged |
| --- | --- | --- |
| `hw_cfg` | 455 files / 206,656 bytes / `A8BFF731985A8886EEB53191A6AFD9F5F037931A841A50A4960738595FC45F6F` | exact match |
| `mridll.dll` | 34,896,384 bytes / `D32AF2B676A4956A3D9AB8707B49F47083328A5CE9236FBB5324E44C28054CE8` | exact match |
| `hw_cfg/init.ini` | 301 bytes / `644D2F4DAD06E5FD5AC6DF7161C63A4164F5B56F926C66DC77D3892CAD411956` | exact match |
| `profiles/PTScan.par` | 21,684 bytes / `6FD62B50A56B802D070AE52737A57516FECE927FCE28BDA17979D4C046C36783` | exact match |

Complete staged runtime: 458 files / 35,125,269 bytes. Complete Release package after staging: 502 files / 120,229,291 bytes. Copied `vcruntime140.dll`, `vcruntime140_1.dll`, or `msvcp140.dll`: 0. Real assets remain ignored. The staged real package was not launched and the real DLL was not loaded.

## Remaining authorized hardware closure (main reviewer only)

1. Complete independent whole-branch review and close every Critical/Important finding.
2. Confirm no other SDK host process is running; stop and ask rather than closing one automatically.
3. Confirm the isolated `client/dist` package still has the identities above and the output path exists/is writable.
4. Snapshot existing RAW files.
5. Launch this isolated Release GUI, auto-connect, explicitly select VerifiedBaseline, and pass a fresh precheck.
6. Execute exactly one PTScan Run. Never retry. Record every observed `ScanStatus`.
7. On error/timeout only, allow at most one Abort; otherwise do not Abort.
8. Verify the new/updated non-empty RAW path, bytes, UTC timestamp, and SHA-256; after normal completion leave the GUI Ready.

No step in this report performs or consumes that one authorized acquisition.
