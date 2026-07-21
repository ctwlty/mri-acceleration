# Handoff

## Completed

- 左侧任务列表改为主选择器。
- 左侧去掉协议、交接、风险等长文本详情。
- 左侧只保留一级场景、检测对象和 Run 状态。
- 右侧参数明细标题改为“协议参数明细（显示，不直接写入 SDK）”。
- Demo 采集日志增加“协议参数当前仅显示，不写入 SDK 控制字段”。

## Verification

- `cmake --build build -j` 通过。

## Answer To SDK Question

当前协议参数明细不会直接应用到 SDK 控制上。它只是开发预设展示。真实 SDK 写入仍需要完成序列字段映射、实机验证，并解除 `Run HOLD` 后才可接入。

## Follow-ups

- 增加一级场景筛选。
- 增加检测对象搜索。
- 将参数预设迁移到 JSON 模板库，再设计受控写入 SDK 的映射层。
