# Requirement

## User Request

根据当前产品设计 V2 和 0.17 T 参数预设文档进行客户端开发。

## Success Criteria

- Qt 客户端从旧的“场景/序列/参数”表达升级为“一级场景 + 检测对象 -> 推荐任务模板”表达。
- 默认模板覆盖七类科研任务。
- UI 显示实验链、QC、结果交接、参数预设版本、适配状态、SDK 映射状态和 `Run HOLD`。
- Demo 模式仍可运行完整 UI 流程。
- 真实 SDK 模式下，HOLD 模板不得调用真实 `Run()`。
- Mac/Qt 构建通过。

## Out Of Scope

- 不把 0.17 T 参数预设写入真实 SDK 参数文件。
- 不解除真实 `Run HOLD`。
- 不实现外部分析软件集成。
- 不自动推导 `noSamples / noViews`。
