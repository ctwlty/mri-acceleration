# Plan

## Scope

- 增加 Windows 下的动态 DLL 加载层。
- 将 `DeviceBridge` 改成真实 SDK 适配层。
- 在主窗口增加 SDK 加载与状态提示。
- 保留 Demo 模式，保证无硬件时可测试。

## Affected Areas

- `client/src/app/MriSdkLoader.*`
- `client/src/app/DeviceBridge.*`
- `client/src/app/MainWindow.*`
- `client/CMakeLists.txt`
- `client/README.md`

## Acceptance Criteria

- 真实 `mridll.dll` 可按 demo 流程接入。
- Demo 模式可直接启动并操作界面。
- SDK 状态、路径、错误信息可见。

## Required Gates

- Qt 工程结构检查
- 文档可读性检查
- 手工启动检查（Windows/Qt 环境）

## Risks

- 当前环境没有 Qt6 编译链，无法本地构建验证。
- 真机参数路径需在 Windows 上最终确认。

## Rollback

- 保留 Demo 模式与原有 UI 结构，可随时回退 SDK 选择。
