# Requirement

## User Request

补充 `ProtocolMapper + 字段白名单 + DRY_RUN 生成参数文件 + SDK 诊断页/日志`。

## Success Criteria

- 新增协议到 SDK 字段的白名单映射层。
- 可以基于当前任务模板生成 DRY_RUN 参数文件预览。
- UI 提供 DRY_RUN 入口。
- 右侧提供 SDK 诊断和字段白名单详情。
- 日志输出 DRY_RUN 状态和文件路径。
- 真实 `Run()` 继续保持 `HOLD`，不把显示参数直接写入 SDK。

## Out Of Scope

- 不解除真实设备采集。
- 不宣称所有字段已完成实机验证。
- 不把右侧自然语言参数文本解析为 SDK 字段。
- 不修改 SDK DLL 本身。
