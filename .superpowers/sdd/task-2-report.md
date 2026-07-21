# Task 2 Report: App-Relative Runtime Resolution

## RED / GREEN

- RED: added `test_mri_runtime_resolver` before production resolver code. The focused CMake build failed because `MriRuntimeResolver.cpp` did not yet exist.
- GREEN: implemented `MriRuntimeResolver` and the focused resolver test passed.
- RED: amended the process-level GUI test to launch from a bundled `mri-runtime` fixture without `--sdk`; the pre-change GUI returned exit code 2 because it still required `--sdk`.
- GREEN: integrated the resolver into startup; the process fixture passed with exactly one `Init`, zero `Run`, and zero `Abort` calls.
- RED: extended the staging test to require `mri-runtime-manifest.json`; the pre-change staging script failed with `Staged runtime manifest is missing.`
- GREEN: staged the manifest with the verified assets and reran the staging test successfully.

## Changes

- Added `MriRuntimePaths`, per-field `MriRuntimeOverrides`, and app-relative defaults:
  - `<app>/mri-runtime/mridll.dll`
  - `<app>/mri-runtime/hw_cfg/init.ini`
  - `<app>/mri-runtime/profiles/PTScan.par`
  - `<app>/mri-output` (created and probed for write access)
- Added runtime-manifest validation for every bundled field that has not been explicitly overridden.
- Preserved explicit CLI compatibility: `--sdk`, `--init`, `--par`, and `--output` replace only their matching field. An all-explicit legacy invocation does not require a bundled package.
- Startup now leaves the GUI open on resolution failure, writes a clear runtime error to the GUI log and diagnostic log, and does not schedule an SDK call.
- Manual DLL selection starts in the bundled runtime directory when that directory exists.
- Runtime staging now copies `mri-runtime-manifest.json` beside the staged assets.

## Verification

`cmake --build client/build-task1` followed by `ctest --test-dir client/build-task1 --output-on-failure`: **8/8 passed**.

Focused checks passed for `mri_runtime_resolver`, `gui_auto_connect_fake`, and `stage_mri_runtime`.

## Manifest / Gate Boundary

Resolver validation proves that bundled, non-overridden fields match the staged manifest, including DLL and PTScan hashes plus the complete `hw_cfg` directory manifest. Per-field CLI overrides remain intentionally usable for existing explicit command lines; they are not an authorization to execute an acquisition. The subsequent baseline/precheck gate is responsible for requiring the approved production identity before `Run` can become available.

## Safety

Only the fake SDK was exercised. Startup tests assert `Init == 1`, `Run == 0`, and `Abort == 0`; no real SDK or device operation was called.

## Follow-up Security Fix

Independent review found that the original resolver treated the package-local JSON manifest as its only trust source. An attacker could change both an asset and the JSON value that described it. The follow-up fix moves the production baseline into compiled constants:

- DLL SHA-256: `D32AF2B676A4956A3D9AB8707B49F47083328A5CE9236FBB5324E44C28054CE8`
- `init.ini` SHA-256: `644D2F4DAD06E5FD5AC6DF7161C63A4164F5B56F926C66DC77D3892CAD411956`
- PTScan SHA-256: `6FD62B50A56B802D070AE52737A57516FECE927FCE28BDA17979D4C046C36783`
- `hw_cfg`: 455 files, 206656 bytes, SHA-256 `A8BFF731985A8886EEB53191A6AFD9F5F037931A841A50A4960738595FC45F6F`

The packaged JSON is now required only as matching structure/diagnostic metadata; it cannot replace those compiled expectations. Fake fixtures use an API compiled only into test targets, and the test GUI reads injected fake expectations only under `MRI_RUNTIME_RESOLVER_TESTING`.

The resolver now includes `QDir::Hidden | QDir::System` while counting and hashing `hw_cfg`, matching the PowerShell staging script's `-Force` behavior. Regressions cover malformed/missing JSON, a rewritten manifest paired with a tampered DLL, tampered PTScan/init assets, missing and hidden `hw_cfg` entries, all-explicit legacy paths, and invalid bundled auto-connect. The latter proves the GUI remains in its event loop, records a resolution trace, and calls fake `Init`, `Run`, and `Abort` zero times.

Follow-up verification: focused resolver/process tests and the full CTest suite passed **8/8**. CLI overrides remain parse-compatible, but they do not bypass the later baseline/precheck `Run` authorization gate.

## Follow-up 2: Per-field Override Isolation

Manifest validation now follows the same field-by-field precedence as CLI resolution: an override skips only the metadata and asset identity check for that same bundled field. Every bundled field that remains selected is still checked against the compiled production baseline. Regression tests cover invalid bundled PT metadata with `--par`, invalid DLL metadata with `--sdk`, invalid init metadata with `--init`, and failure when an invalid entry remains bundled.

The invalid bundled auto-connect process fixture now contains the ordinary loadable fake DLL, reachable `init.ini`, PTScan file, and writable default output directory. It fails only because the packaged DLL identity metadata is wrong before any SDK call. The test pre-creates and reads the fake-call log, asserting exactly zero `Init`, `Run`, and `Abort` calls while the GUI event loop remains active and writes a clear failure trace.
