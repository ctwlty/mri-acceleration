# Windows return template

## 1. Object

- Module: `tools/firstech-sequence-mcp`
- Target machine / OS:
- Code location:
- Local config location (do not paste secret values):
- Report date:

## 2. Package identity

- ZIP SHA-256:
- Manifest verification: PASS / FAIL
- Missing / extra / mismatched files:

## 3. Static target evidence

| Item | Actual path | Version | PE | SHA-256 | Evidence status |
|---|---|---|---|---|---|
| P2F | | | | | |
| current mridll | | | | | |
| SeqSimu | | | | | |
| tcc | | | | | |
| mri_c | | | | | |
| include/systemSel | | | n/a | | |

- Current DLL exports (static only):
- `LoadLibrary/ctypes/PInvoke` used during inspection: MUST BE NO
- Vendor processes started during inspection: MUST BE 0
- Existing route/TCP/process risks:

## 4. Tests

- pytest:
- ruff:
- compileall:
- Negative tests:
- MCP Inspector/Codex connectivity:

## 5. P2F validation

- Source copy identity/SHA:
- Compile run ID:
- Compile manifest SHA-256:
- Four action exit codes:
- FCODE / PAR / ASM / CPP artifact SHA:
- Original/install directory change count:
- SpectrometerIDE starts: MUST BE 0
- Evidence stage: TARGET-ENV-VERIFIED / BLOCKED

## 6. Simulation gate

- Backend remains blocked: MUST BE YES IN V1
- Current DLL ABI verified:
- Socket protocol verified:
- Offline/no-route condition verified:
- Dynamic invocation count: MUST BE 0 IN V1
- Simulation input ID:
- Simulation run ID:
- SeqSimuEnd / ERROR / expected channel checks:
- New spectrometer connections:
- RAW changes:
- Evidence stage: TARGET-ENV-VERIFIED / BLOCKED

## 7. Analysis boundary

- Logical channels parsed:
- Coordinate space label:
- Physical axis mapping:
- Gradient units:
- ADC pointwise timestamps:
- Claims intentionally not made:

## 8. Facts / inferences / unknowns / blockers

### Facts


### Inferences


### Unknowns


### Blockers and smallest next action


## 9. Final conclusion

- Windows MCP compile path:
- Windows MCP offline simulation path:
- Safe to integrate into project A/B now: NO / COMPILE-ONLY / YES-WITH-SCOPE
- Files returned:
