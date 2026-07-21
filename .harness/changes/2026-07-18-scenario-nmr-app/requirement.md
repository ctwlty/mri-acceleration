# Requirement

## User Request

基于真实 SDK 继续集成核磁共振客户端，并提供可测试路径。

## Success Criteria

- Qt/C++ 客户端能够加载 `mridll.dll` 并按真实 demo 流程工作。
- 没有硬件或 DLL 时，客户端自动回退到 Demo 模式，仍可测试界面流程。
- 运行方式、SDK 选择方式和测试步骤写入客户端文档。

## Out Of Scope

- 不做新 UI 范围扩张。
- 不改动设备固件或真实生产配置。
- 不触碰临床范围。
