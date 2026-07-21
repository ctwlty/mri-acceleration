# Task 1 — Verified Runtime Staging Report

## Scope

Implemented the verified MRI runtime staging workflow in the isolated `mri-runtime-impl` worktree, based on commit `c236e00`.

Commit: independent `feat: stage verified MRI runtime` commit (SHA reported to the parent task after creation).

Changed files:

- `client/runtime/mri-runtime-manifest.json` — production constraints for `mridll.dll`, complete `hw_cfg`, and `PTScan.par`.
- `client/scripts/stage-mri-runtime.ps1` — source and post-copy SHA/inventory verification, narrowly scoped copy, and x64 MSVC runtime resolution check without redistribution.
- `client/tests/test_stage_mri_runtime.ps1` — temporary fake asset-tree test with a test-specific manifest.
- `client/scripts/deploy.ps1` — requires both MRI source arguments unless `-QtOnly` is explicit; invokes staging for verified deployments and excludes MSVC runtime DLLs from dependency redistribution.
- `client/CMakeLists.txt` — registers `stage_mri_runtime` as a CTest test.

## TDD evidence

1. RED: after adding `client/tests/test_stage_mri_runtime.ps1` and before creating the staging script, ran:

   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File client\tests\test_stage_mri_runtime.ps1
   ```

   It exited `1` because `client/scripts/stage-mri-runtime.ps1` did not exist (`-File parameter does not exist`).

2. GREEN: after the smallest implementation, reran the same focused test. It exited `0` and printed `STAGE_MRI_RUNTIME_TEST_OK`. The test proves:

   - verified fake sources create `mridll.dll`, complete nested `hw_cfg/`, and `profiles/PTScan.par`;
   - missing `mridll.dll` is rejected;
   - a changed DLL SHA-256 is rejected;
   - `mridll.dll.backup_20250303` and an unselected parameter file are not copied.

3. Review regression RED/GREEN: a review found that staging into its own source could delete verified inputs. The focused test was extended to require a source/destination overlap error; it initially failed with the prior hash-validation error. After adding an overlap guard before validation/copying, it passes and the source remains untouched. The same review led to hidden `hw_cfg` files being included in inventory/hash enumeration and deployment staging being placed in `dist/mri-runtime/`.

## Verification commands and results

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
& 'C:\msys64\ucrt64\bin\cmake.exe' -S client -B client\build-task1 -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
& 'C:\msys64\ucrt64\bin\cmake.exe' --build client\build-task1 --parallel 4
& 'C:\msys64\ucrt64\bin\ctest.exe' --test-dir client\build-task1 -R '^stage_mri_runtime$' --output-on-failure
& 'C:\msys64\ucrt64\bin\ctest.exe' --test-dir client\build-task1 --output-on-failure
```

Results:

- CMake configure: exit `0`.
- Build: exit `0` (54 Ninja steps).
- Focused CTest: `1/1` passed (`stage_mri_runtime`).
- Full CTest: `7/7` passed (`mri_sdk_loader`, `device_bridge`, `device_actions`, `main_window`, `sdk_verify_fake`, `gui_auto_connect_fake`, `stage_mri_runtime`).
- `powershell.exe ... client/scripts/deploy.ps1` without runtime parameters exits `1` before any deployment action with: `Provide both -MriSdkRoot and -ParameterFile, or explicitly use -QtOnly.`
- Deployment calls the stage script with `dist/mri-runtime` as destination, producing the bundled runtime layout beneath the app package rather than beside application binaries.

## Review notes and limitations

- No real MRI binary, configuration tree, parameter file, device, or device operation was used. The test uses generated temporary fake assets and an override manifest, as required.
- The production manifest records the exact supplied global constraint values. Validation of an actual vendor asset tree remains intentionally outside this task because those binaries are ignored and were not copied into Git.
- MSVC runtime DLLs are checked for resolution and PE x64 machine type; they are deliberately excluded from deployment copying.
- Independent review found no Critical issues; both Important findings were fixed and re-verified.
