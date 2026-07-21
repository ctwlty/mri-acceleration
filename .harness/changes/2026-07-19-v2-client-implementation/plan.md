# Plan

## Scope

- 扩展 `SceneTemplate` 数据模型。
- 用七类科研任务重写 `SceneCatalog` 默认模板。
- 更新 `MainWindow` 的左侧推荐、中部实验链、右侧 QC/参数预设展示。
- 更新 `DeviceBridge`，在真实 SDK 模式下遵守 `Run HOLD`。
- 运行 Mac/Qt 构建验证。

## Affected Areas

- `client/src/app/SceneTemplate.h`
- `client/src/app/SceneCatalog.cpp`
- `client/src/app/MainWindow.h`
- `client/src/app/MainWindow.cpp`
- `client/src/app/DeviceBridge.cpp`

## Acceptance Criteria

- 首屏列表展示推荐任务模板。
- 模板详情显示一级场景、检测对象、协议预设、适配状态和交接输出。
- 右侧显示 `presetVersion=2`、参数状态、Run 状态、SDK 映射和物理检查。
- 点击 Demo 采集后刷新 QC 指标并记录交接日志。
- `cmake --build build -j` 通过。

## Risks

- 真实设备参数尚未回填，真实 SDK 采集仍应被 HOLD 阻断。
- 当前 UI 仍是 Qt Widgets 单窗口，没有完整多页面导航。

## Rollback

- 回退本次五个客户端文件即可恢复旧模板 UI。
