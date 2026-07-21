# Task 3 Report: Typed Execution Gate and Baseline Precheck

## Delivered

- Added `ExecutionGate { Hold, VerifiedBaseline, VerifiedScene }` and an explicit execution context.
- Scientific catalog templates are typed `Hold`; they remain usable for browsing and `DRY_RUN` only.
- Added a separate UI selector for `设备基线（已实机验证 PTScan）`; it is not a scientific-scene taxonomy entry.
- Replaced the scene-discarding scan overload with `startScan(const ExecutionContext&)`. `MainWindow` passes the active context.
- A Run requires `Ready`, `VerifiedBaseline`, a fresh passing precheck, and a matching active context. Precheck verifies fixed runtime/PTScan identity, connection, idle `ScanStatus`, and writable output while reporting connection and temperature.
- Precheck is invalidated on SDK load, configuration, mode selection, scene change, scan start/completion, abort, and fault.
- Resolver now independently proves the fixed identity against the actual resolved DLL/init/hw_cfg/PTScan paths. CLI overrides can therefore become eligible when their resolved assets match; nonmatching explicit legacy paths can initialize but cannot Run.

## TDD / tests

Initial RED: the focused action test asserted that Ready defaults to Run HOLD. It failed against the prior `Ready => canRun` implementation.

Focused tests and the fresh full suite passed after implementation:

```powershell
cmake --build client/build-task1 --parallel 4
ctest --test-dir client/build-task1 --output-on-failure
```

Result: `8/8` passed. The suite uses only the fake SDK. GUI auto-connect still verifies `Init=1`, `Run=0`, and `Abort=0`.

## Safety

No real SDK, hardware, or device operation was invoked. `DRY_RUN` is covered with a fake-SDK call-log assertion and performs no SDK writes.
