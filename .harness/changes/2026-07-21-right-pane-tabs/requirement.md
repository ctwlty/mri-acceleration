# Requirement

## User Request

右侧页面显示逻辑混乱，需要重新整理。

## Success Criteria

- 右侧不再把 QC、参数预设、协议明细、SDK 诊断和日志堆在同一垂直页面。
- 长文本不再挤压或覆盖其他内容。
- QC 指标保持首屏可见。
- 参数、SDK 诊断、日志按功能分组显示。
- 不改变 SDK、DRY_RUN 或 `Run HOLD` 行为。

## Out Of Scope

- 不新增真实 SDK 写入。
- 不修改 ProtocolMapper 字段映射。
- 不调整左侧任务选择和中间操作链。
