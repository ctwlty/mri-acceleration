# Review

## Findings

- 客户端已从旧的场景/序列表达切换为任务模板推荐表达。
- 七类科研任务已进入默认模板。
- 参数预设状态、`presetVersion=2`、`Run HOLD`、SDK 映射状态和物理检查状态已进入 UI。
- `DeviceBridge` 在真实 SDK 模式下会阻断 HOLD 模板的真实 `Run()`。
- Demo 模式仍能执行 UI 流程并刷新 QC 指标。

## Safety

- 未把开发预设写入真实 SDK。
- 未解除真实 `Run HOLD`。
- 未把 `FSE A / FSE B` 宣称为 T1/T2。
- 未自动推导 SDK 底层采样字段。

## Residual Risk

- 当前仍是单窗口三栏壳，完整多页面体验下一轮再做。
- 参数预设仍为 C++ 静态数据，后续建议迁移到 JSON 模板库。
