# Reference document index

## Authority order

1. `../authority/`
2. This evidence set's implementation/tool/simbridge contract
3. External vendor manuals as versioned evidence
4. Historical project documents as background only

If a copied project document conflicts with current authority, current authority wins. In particular, old single-experiment instructions, old GradP point-count conclusions and old handoff status must not become new execution constraints.

## External vendor references — not distributed

| Reference | Use | Critical caveat |
|---|---|---|
| Sequence programming manual, version 1.0.9 | Sequence language, waveNo, gwave/LUT and stated limits. | Manual facts still need target-version confirmation. |
| SpectrometerIDE user manual, version 1.12 | PAR/SRC/S/wave dependency model and compile/simulation descriptions. | Offline evidence does not authorize a hardware run. |
| DLL API manual, historical version 2.26 | Historical `Simulate/Pause/Resume/Abort` signature and Socket-server requirement. | Historical evidence does **not** prove current DLL export, ABI, support or safety. |
| DLL API manual, version 3.3 | Newer DLL interface context and revision history. | Omitted internal interfaces are not proof of presence or absence. |

The referenced PDF files are intentionally excluded from this evidence set. Obtain authorized copies separately when version-specific verification is required.

Not included: sequence manual V1.0.7, hardware manual, Converter manual, system upgrade note, historical DLL/EXE/SDK archives. They are unnecessary for this narrow offline MCP module and increase confusion or disclosure risk.

## Project background — non-authoritative copies

| File | Useful content | Caveat |
|---|---|---|
| `project-docs/Custom_SRC_Workflow_2026-08-10.md` | SRC minimal-diff and staged validation workflow. | Background method, not a command to run hardware. |
| `project-docs/Intermediate_File_Review_Guide_2026-08-10.md` | Same-batch evidence chain and conclusion boundaries. | Older experiment examples may be superseded. |
| `project-docs/Offline_Acceptance_Work_Package_2026-08-10.md` | Manifest/fixture/negative-test design. | `../authority/ACCEPTANCE.md` is the active acceptance authority. |

## Deliberately excluded assets

- All `mridll.dll`, `P2F_x32.exe`, `SeqSimu.exe`, `tcc`, `mri_c`, LIB/PDB and vendor installation copies.
- All `NMRDLL.zip`, `testDLL.zip`, `eggcontrollerV2.zip` and other historical SDK archives.
- Old Windows task ZIPs, GradP V1/V2/V3 task instructions, old handoffs and RAW evidence.

Windows Codex must inspect the current installed files in place and report their hashes. It must never replace them with an archive from another machine or version.

The older `Sequence Development and Control Reference V2.0` is deliberately omitted: it contains expired GUI/Start instructions and old authorization language. `CURRENT_EVIDENCE.md` replaces its status claims for this handoff.
