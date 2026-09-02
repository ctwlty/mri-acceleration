# MCP tool contract

Tool annotations help clients display approvals; server-side path, run, backend and safety checks remain the real boundary.

| Tool | Writes | Contract |
|---|---:|---|
| `inspect_toolchain()` | No | Static file identity/PE exports and read-only system snapshot; never loads DLL. |
| `compile_sequence(source_path)` | New run only | Source must be inside a white-listed root. Stages workspace/include/systemSel, runs fixed P2F commands with `shell=False`, records four expected outputs. |
| `stage_simulation_input(compile_run_id, expected_compile_manifest_sha256, runtime_par_path)` | New run only | Compile run and its retained manifest SHA must match. PAR must be beside its compiled SRC. Copies that workspace and compile artifacts, rejects absolute/rooted/UNC/parent references, hashes all inputs, returns ID + manifest SHA. |
| `probe_simulation_backend()` | No | Reports the V1 hard block and target-verification reasons; never starts a bridge. |
| `simulate_sequence(simulation_input_id, expected_manifest_sha256)` | No in V1 | Revalidates the exact frozen file set and compile manifest, then always raises the unreleased-backend safety block. It cannot start a dynamic process in this package. |
| `analyze_simulation(run_id)` | No | Returns `inf.log` completion/errors and compact channel statistics. |
| `read_channel_slice(run_id, channel, start, count)` | No | Bounded to configured maximum; never returns an entire large waveform. |
| `compare_simulations(base_run_id, candidate_run_id)` | No | Count, max-absolute difference and RMSE for matching logical channels. |
| `compute_logical_gradient_moment(run_id)` | No | Trapezoidal relative integral for logical GradR/GradP only; explicitly not physical k-space. |

## Structured result principles

- Every result includes a schema/version, status and stable run ID where applicable.
- Large arrays remain in files. MCP returns summaries, hashes and bounded slices.
- Unknown values remain `unknown`; they are not converted to zero or inferred units.
- A process exit code, GUI appearance or `Simulate()` return alone is not success.
- `destructive_hint=false` means “does not overwrite/delete”; it does not mean “read-only”. Compile/stage retain approval prompts. The reserved `simulate_sequence` surface also remains prompt-gated even though V1 hard-blocks it.
