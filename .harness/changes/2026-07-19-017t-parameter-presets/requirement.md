# Requirement

## User Request

补充 Agent MRI 0.17 T 七类科研任务协议与参数预设内容，并把这些开发预设纳入实验模板产品设计。

## Success Criteria

- 新增 0.17 T 参数预设文档。
- 明确 `presetVersion=2`。
- 明确参数是开发预设，不代表实机验证。
- 明确 `FSE A / FSE B` 的矩阵是目标重建矩阵，不直接等同 SDK 底层采样字段。
- 明确 `12.9 ms` 是历史序列标称 TE，未核验前不得称为有效 TE。
- 产品设计中补充参数预设的展示、导出和模板集成方式。

## Out Of Scope

- 不把参数写入真实 SDK 调用。
- 不解除真实 `Run()` HOLD。
- 不声明任何协议已完成设备验证。
- 不自动推导 `noSamples / noViews` 等底层字段。
