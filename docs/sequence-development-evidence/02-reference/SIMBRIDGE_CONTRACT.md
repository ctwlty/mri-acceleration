# Narrow simbridge contract (not yet target-verified)

> **V1 status: specification only.** The shipped MCP code cannot select a command backend and cannot start this bridge. Windows Codex may implement it only after Acceptance E prerequisites are proven; until then `simulate_sequence remains blocked`.

## Why a separate process

The MCP server must stay alive if a vendor DLL crashes, hangs or has a mismatched ABI. It therefore never imports or loads `mridll.dll`. A target-built, bitness-matched child process is the only permitted dynamic boundary.

## Invocation

The configured executable is invoked exactly once with an argument array:

```text
simbridge.exe --request <absolute-request-json> --response <absolute-response-json>
```

No shell, arbitrary flags, arbitrary DLL path or arbitrary export name is accepted from MCP input. The bridge path and SHA are fixed by private config plus an accepted gate file.

The bridge must create a Windows Job Object with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`, assign itself and every child to that job before vendor execution, and prove all recorded children have exited before success. Process-group flags or name-based `taskkill` are not an equivalent containment boundary.

## Request JSON

```json
{
  "schema": "firstech-simbridge-request/v1",
  "simulation_run_id": "sim-...",
  "simulation_input_id": "stage-...",
  "simulation_input_manifest_sha256": "...",
  "workspace": "<run-root>\\stage-...\\workspace",
  "runtime_par": "<run-root>\\stage-...\\workspace\\proj.par",
  "output_dir": "<run-root>\\sim-...\\output",
  "bind_host": "127.0.0.1",
  "port": 0,
  "tr": 1,
  "loop": 1
}
```

The bridge chooses an unused loopback port when `port=0`, creates the Socket server required by the historical v2.26 contract, records all received messages, and then invokes only the verified historical simulation exports. It must reject any non-loopback bind.

## Response JSON

```json
{
  "schema": "firstech-simbridge-response/v1",
  "status": "ok",
  "loaded_dll_path": "<SpectrometerIDE-install-root>\\bin\\mridll.dll",
  "loaded_dll_sha256": "...",
  "vendor_return_code": 0,
  "compiler_exit_codes": {"tcc": 0, "mri_c": 0},
  "seq_simu_end_count": 1,
  "error_count": 0,
  "child_pids": [1234],
  "artifacts": ["inf.log", "gradrData.txt", "gradpData.txt"],
  "limitations": ["logical channels only"]
}
```

The MCP independently verifies the canonical loaded DLL path and SHA, requires `vendor_return_code=0`, requires every compiler exit code to be zero, confirms every recorded child has exited, re-hashes artifacts and re-parses `inf.log`; it does not trust bridge success text alone.

## Required gate file

In a future post-V1 implementation, before any dynamic backend is accepted, the private gate JSON must contain:

```json
{
  "schema": "firstech-simulation-gate/v1",
  "authorized": true,
  "target_env_verified": true,
  "loopback_only": true,
  "no_route_to_spectrometer_verified": true,
  "current_dll_sha256": "...",
  "bridge_sha256": "...",
  "safety_config_sha256": "...",
  "spectrometer_targets": ["<spectrometer-host>:<port>"],
  "protected_roots": ["<SpectrometerIDE-install-root>", "<raw-data-root>"],
  "approved_on": "YYYY-MM-DD",
  "approved_by": "Mao Yuanyang"
}
```

The package does not create or approve this gate automatically. The gate must bind the full safety configuration, non-empty immutable target list, protected roots, current DLL and bridge identities. Current DLL exports/ABI/Socket behavior, fixed dependency loading, Windows Job Object containment and offline route conditions must be demonstrated first.

## Stop conditions

- DLL/bridge bitness mismatch, missing dependency or unknown calling convention.
- Actual loaded DLL canonical path/SHA differs from the accepted current installation.
- Socket tries to bind any non-loopback address.
- Any route or active connection to a configured spectrometer target, including a loopback route that could be forwarded.
- Multiple `SeqSimuEnd`, nonzero/real ERROR, missing required result, crash or timeout.
- Any nonzero vendor/compiler return, uncontained child or child still alive at completion.
- Any change to vendor installation, device parameters or RAW areas.

On stop, preserve the run; do not retry and do not launch SpectrometerIDE.
