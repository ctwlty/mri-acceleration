# Requirement

## User Request

左侧采集模板显示混乱，需要优化；同时确认协议参数明细是否会直接应用到 SDK 控制。

## Success Criteria

- 左侧只承担模板选择，不再塞入长详情。
- 左侧摘要只保留一级场景、检测对象和 Run 状态。
- 协议参数明细区域明确提示“显示，不直接写入 SDK”。
- Demo 日志明确参数不会写入 SDK 控制字段。
- 构建通过。

## Out Of Scope

- 不改变任务模板数据。
- 不将参数预设写入 SDK。
- 不解除 `Run HOLD`。
