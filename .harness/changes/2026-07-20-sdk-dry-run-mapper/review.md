# Review

## Findings

- ProtocolMapper separates display parameters from SDK field writes.
- DRY_RUN output explicitly records `realRun=HOLD` and `writeToSdk=false`.
- UI now exposes diagnostic status without changing scan behavior.

## Residual Risk

- DRY_RUN button was verified by build/startup, not by live click in the Qt window.
- Actual SDK fields such as `noSamples/noViews`, sequence-specific TE, FOV, ETL, and NEX still require device/vendor mapping validation.
- Future real-run enablement should require a separate decision gate.
