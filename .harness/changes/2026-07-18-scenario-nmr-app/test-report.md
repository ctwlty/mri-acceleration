# Test Report

| Gate | Command | Result | Evidence |
| --- | --- | --- | --- |
| structure | `find client -maxdepth 3 -type f` | pass | loader, bridge, main window files present |
| docs | `sed -n '1,220p' client/README.md` | pass | test steps documented |
| sdk scan | `rg -n "ScanStatus|ScanCompleted|Init|ConfigFile|SetParameterFile" eggcontrollerV2 NMRDLL testDLL` | pass | demo flow confirmed |

## Skipped Gates

- Qt build: current environment lacks `cmake` and Qt6 toolchain.
- Runtime hardware test: no actual device session in this environment.
