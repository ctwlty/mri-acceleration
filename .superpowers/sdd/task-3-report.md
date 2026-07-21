# Task 3 Report: Typed Execution Gate and Baseline Precheck

## Delivered

- Added `ExecutionGate { Hold, VerifiedBaseline, VerifiedScene }` and explicit baseline selection.
- Scientific catalog templates are typed `Hold`; they remain usable for browsing and `DRY_RUN` only.
- Added a separate UI selector for `设备基线（已实机验证 PTScan）`; it is not a scientific-scene taxonomy entry.
- Replaced the scene-discarding scan overload with `startScan(const PrecheckTicket&)`. `MainWindow` passes only the ticket minted by the latest successful precheck.
- A Run requires `Ready`, `VerifiedBaseline`, a fresh passing precheck ticket, and matching issuer/generation identity. Precheck verifies fixed runtime/PTScan identity, connection, idle `ScanStatus`, and writable output while reporting connection and temperature.
- Precheck is invalidated on SDK load, configuration, mode selection, scene change, scan start/completion, abort, and fault.
- Resolver now independently proves the fixed identity against the actual resolved DLL/init/hw_cfg/PTScan paths. CLI overrides can therefore become eligible when their resolved assets match; nonmatching explicit legacy paths can initialize but cannot Run.
- Replaced the copyable caller-supplied execution context with an opaque `PrecheckTicket`. Tickets are privately minted, single-use, generation-bound, and bound to a unique `DeviceBridge` issuer, so they cannot be forged, replayed, or transferred between bridge instances.
- `startScan` rechecks connection, idle scan status, and output writability immediately before any SDK write. A changed condition invalidates the ticket, so restoring the condition still requires a new precheck.
- A successful reconfiguration closes the prior Ready session without Abort before reinitializing, and invalidates every prior ticket.
- Removed the verifier's unsafe/broken `--scan` path. `mri_sdk_verify` is initialization-only and cannot mint a production baseline proof or call Run/Abort.

## TDD / tests

RED cases observed during the Critical follow-up included cross-bridge ticket acceptance, replay after a failed busy-status recheck, missing CloseSys on successful reconfiguration, and the verifier still accepting `--scan`. Each failed for the intended reason before the corresponding minimal fix.

Focused tests and the fresh full suite passed after implementation:

```powershell
cmake --build client/build-task1 --parallel 4
ctest --test-dir client/build-task1 --output-on-failure
```

Result: `8/8` passed. Focused `device_bridge` also passed independently. Coverage includes Ready+HOLD, missing/fresh precheck, unforgeable production proof/ticket types, replay/mismatch/stale tickets, SDK/config/mode/scene/completion/abort/fault invalidation, disconnect/busy/unwritable-output rechecks, DRY_RUN zero writes, successful completion without Abort, and fault/timeout Abort behavior. GUI auto-connect verifies `Init=1`, `Run=0`, and `Abort=0`.

## Safety

No real SDK, hardware, or device operation was invoked. `DRY_RUN` is covered with a fake-SDK call-log assertion and performs no SDK writes.

## Windows fake DLL crash

The prior `0xc0000005` was a test-fixture lifecycle/build-dependency failure, not a removed safety assertion. `FakeSdkControl` used `QVERIFY` inside its constructor; when a directly rebuilt test executable saw an older fake DLL without the new control export, construction returned early and the test immediately called a null `FakeReset` pointer before Qt could print a failure. Test targets now depend explicitly on their fake DLL targets, and the control wrapper turns a missing export into a readable Qt test failure instead of an indirect call through null. The stale-ticket scenarios are registered and pass in the complete test process.

## Second Critical follow-up

- `BaselineIdentityProof` now privately carries the canonical SDK, init, `hw_cfg`, and PTScan paths plus the fixed hashes and complete hardware-directory inventory digest verified when it was minted. Resolver verification is private and available only to `DeviceBridge`; test-only expectations remain behind `MRI_RUNTIME_RESOLVER_TESTING`.
- A copied proof cannot authorize a config with different SDK/init/PTScan paths. The complete bound identity is re-read and verified before `LoadLibrary`, before SDK initialization, during precheck, and immediately before prepare/Run.
- Regression tests replace the DLL after resolve, replace init before precheck, replace PTScan before Run, and add a hidden `hw_cfg` file before Run. Every case invalidates authorization and records zero Run calls.
- Active execution selection is now explicit `{ source kind, source id, gate }`. A scientific `SceneTemplate` selects its own id and `Hold` gate, clears any baseline ticket, and forces the UI selector back to scientific HOLD. A verified-baseline ticket is bound to the mutually exclusive baseline source and cannot survive a scene selection.
- The new identity-detach and selection APIs were observed RED before implementation. The init/PTScan/hidden-inventory and UI regressions were also verified by temporarily removing their corresponding guards, observing the intended failures, then restoring the guards and rerunning the suite.

Fresh verification after this follow-up:

```powershell
cmake --build client/build-task1 --parallel 4
ctest --test-dir client/build-task1 --output-on-failure
```

Result: `8/8` passed, including the complete DeviceBridge matrix, MainWindow scientific-selection HOLD behavior, initialization-only verifier, and GUI auto-connect zero-Run/zero-Abort checks.
