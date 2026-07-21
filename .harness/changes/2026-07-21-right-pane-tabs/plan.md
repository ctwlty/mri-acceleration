# Plan

## UX Change

右侧改为：

```text
质控与诊断
  QC 指标概览
  Tabs:
    参数预设
    SDK 诊断
    日志
```

## Implementation

- Use `QTabWidget` for right pane detail sections.
- Keep metric cards fixed at top.
- Move parameter form and parameter details into `参数预设` tab.
- Move DRY_RUN output into `SDK 诊断` tab.
- Move log view into `日志` tab.
- Reduce metric value font size and enable wrapping.
- Give right pane more splitter width.

## Acceptance

- Build passes.
- Offscreen startup passes.
- Right pane no longer has overlapping labels in static layout.
