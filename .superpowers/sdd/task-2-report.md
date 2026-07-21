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
