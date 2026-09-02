# Implementation guide

This document records the implementation contract and operating workflow. Source code, runtime dependencies and vendor components are not included in this evidence-only directory.

## Architecture

```text
Codex
  └─ STDIO MCP server (this package; never loads vendor DLL)
       ├─ static inspection
       ├─ P2F adapter → unique compile run + manifest
       ├─ simulation-input freezer → immutable staged input
       ├─ reserved simulation tool (V1 hard-blocked; no dynamic process)
       └─ pure parsers → summaries / bounded slices / logical moments
```

Codex edits sequence files directly inside configured `source_roots`. The MCP has no arbitrary editor, shell, DLL-loader, device, or RAW tool.

## Install on Windows

Recommended with `uv`:

```powershell
Set-Location .\01-implementation
uv sync --dev --frozen --no-editable
$env:FIRSTECH_MCP_CONFIG = "<private-config-path>"
.\.venv\Scripts\pytest.exe
.\.venv\Scripts\ruff.exe check .
.\.venv\Scripts\python.exe -m compileall -q src scripts
```

Fallback with standard Python 3.11+:

```powershell
py -3.11 -m venv .venv
.\.venv\Scripts\python.exe -m pip install .
.\.venv\Scripts\python.exe -m pip install "pytest>=8.4,<9" "ruff>=0.12,<1"
.\.venv\Scripts\python.exe -m pytest
.\.venv\Scripts\ruff.exe check .
.\.venv\Scripts\python.exe -m compileall -q src scripts
```

Do not install into the vendor Python/runtime and do not copy files into `<SpectrometerIDE-install-root>`.

## Verify the handoff package

From the package root, run this before dependency installation:

```powershell
py -3.11 .\01-implementation\scripts\verify_package.py .\package-manifest.json .
```

The manifest intentionally excludes itself so it can describe the rest of the package without a self-hash paradox. Its fixed `package_id` does not depend on the extracted directory name, so the top-level directory may safely be renamed to the recommended short `handoff_v1` path.

## Read-only target probe

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\01-implementation\scripts\probe_toolchain.ps1 `
  -ConfigPath "<private-config-path>" `
  -OutputPath ".\evidence\toolchain-probe.json"
```

The probe reads PE bytes through `pe_probe.py`; it never calls `LoadLibrary`. Review the JSON before enabling P2F calls. The probe does not enable the simulator.

## Start MCP manually

```powershell
$env:FIRSTECH_MCP_CONFIG = "<private-config-path>"
.\.venv\Scripts\python.exe -m firstech_sequence_mcp.server
```

STDOUT is reserved for MCP protocol frames. Vendor child output is captured into run files; diagnostics use STDERR.

## Intended tool sequence

1. `inspect_toolchain()`
2. Codex edits a sequence under an allowed source root.
3. `compile_sequence(source_path)`
4. Retain the compile run ID **and** compile manifest SHA. Prepare the runtime PAR beside the compiled SRC with relative in-workspace dependencies, then call `stage_simulation_input(compile_run_id, expected_compile_manifest_sha256, runtime_par_path)` and retain the returned stage ID and stage manifest SHA.
5. `probe_simulation_backend()`
6. Keep `simulate_sequence(simulation_input_id, expected_manifest_sha256)` blocked until Windows layer E is separately accepted.
7. After a valid simulation run: `analyze_simulation`, `read_channel_slice`, `compare_simulations`, `compute_logical_gradient_moment`.

## Run identity

Every mutating tool creates a fresh `run_root/<run_id>` with `exist_ok=False`. The manifest records inputs, hashes, commands, exit codes, logs, output hashes, status and evidence limitations. A failed run is preserved; the tool never silently retries or overwrites it.

## Simulation bridge status

No vendor bridge binary is shipped. `simulation.backend = "blocked"` is the safe and expected initial state. `02-reference/SIMBRIDGE_CONTRACT.md` defines the narrow JSON process contract Windows Codex may implement only after current DLL/ABI/Socket/safety verification. The bridge may not expose any real-device function.

On an Internet-connected Codex host, a normal default route may make the strict `host-no-route` preflight impossible to satisfy. Treat this as an architecture blocker, not a reason to remove the check. A separately isolated worker/VM or pre-approved target-IP egress isolation must first be designed and verified outside this package.

After A–C plus direct P2F preflight pass, copy `.codex.config.toml.example` into a trusted sequence project's `.codex/config.toml`, restart Codex Desktop or open a new task, and verify with `/mcp` or `codex mcp list` before completing D.
