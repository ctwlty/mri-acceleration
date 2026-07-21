# Requirement

## User Request

协议预设区域当前只显示协议别名，需要显示具体参数值。

## Success Criteria

- 当前任务模板能显示 TR、TE、FOV、矩阵、层厚、NEX 等协议参数。
- 七类科研任务模板都有参数明细。
- 参数明细不改变真实 SDK 写入逻辑。
- 构建通过。

## Out Of Scope

- 不把开发预设写入真实 SDK。
- 不解除 `Run HOLD`。
- 不推导 SDK 底层 `noSamples / noViews`。
