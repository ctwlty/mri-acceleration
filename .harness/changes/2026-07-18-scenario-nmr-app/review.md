# Review

## Findings

- Real SDK path should target `mridll.dll` directly.
- `eggcontrollerV2` confirms a runnable demo flow and a clear raw data parsing pattern.
- UI can stay testable without hardware by falling back to Demo mode.

## Security

- No credentials or secrets added.
- DLL path is user-selected and not hardcoded into the client.

## Domain Rules

- Keep workflow ordered as connect/configure/precheck/scan/save/analyze.
- Keep template-driven UI above raw SDK detail.
- Preserve research/teaching boundary.
