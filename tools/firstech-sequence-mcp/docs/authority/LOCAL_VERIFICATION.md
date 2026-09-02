# Local verification record

Date: 2026-08-11

Environment: macOS, CPython 3.11.9. This record validates only the portable MCP/scaffold logic; it is **not** Windows target evidence and did not invoke any Firstech program.

| Check | Result |
|---|---|
| Dependency resolution from `uv.lock` | PASS |
| MCP Python SDK | pinned and tested at `mcp==2.0.0` |
| `ruff check .` | PASS |
| `pytest` | PASS, 32 tests |
| `python -m compileall -q src scripts` | PASS |
| Real STDIO client initialize + `list_tools` | PASS, exactly 9 tools |
| Package-policy negative check with generated caches present | PASS, manifest build refused |

Registered tools observed through an actual MCP client session:

```text
analyze_simulation
compare_simulations
compile_sequence
compute_logical_gradient_moment
inspect_toolchain
probe_simulation_backend
read_channel_slice
simulate_sequence
stage_simulation_input
```

The tests cover offline config rejection, vendor/tool/root overlap rejection, raw-root symlink/reparse rejection, path escape, strict unique run IDs, case-folded cache skipping, RAW/platform-binary rejection, P2F success/failure manifests, retained compile-manifest SHA enforcement, same-source input freezing, post-compile source-change rejection, drive/UNC/single-root PAR-reference rejection, completion/error parsing, NaN/Inf and duplicate-channel rejection, forensic-only failed-run analysis, bounded slices, comparison, logical moments, the unconditional V1 simulation block, static PE parsing, exact MCP tool/annotation registration, rename-safe package identity and build/verify self-pollution prevention.

All final checks used a clean virtual environment outside the handoff tree. No virtual environment, cache, compiled Python file or platform binary is part of the package.

Not tested here: current Windows P2F execution, current DLL exports/ABI, Socket protocol, simbridge, Windows routes/processes, SeqSimu dynamic execution or physical trajectory calibration. These remain under `ACCEPTANCE.md` layers C–E.
