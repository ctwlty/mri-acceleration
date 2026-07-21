# Handoff

## Completed

- 新增 `parameterDetails` 模板字段。
- 为七类科研任务模板补充协议参数明细。
- 右侧参数预设区新增“协议参数明细”只读文本框。
- 模板切换时参数明细同步更新。
- Mac/Qt 构建通过。

## Verification

- `cmake --build build -j` 通过。

## Remaining Risks

- 参数仍是开发预设，真实 SDK 字段映射待回填。
- 后续建议把参数预设迁移为 JSON 模板库。
