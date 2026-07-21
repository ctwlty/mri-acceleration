# Affected Files

| File | Reason |
| --- | --- |
| `client/src/app/SceneTemplate.h` | 扩展任务模板字段，加入一级场景、输出、参数预设和 Run HOLD 状态 |
| `client/src/app/SceneCatalog.cpp` | 七类科研任务默认模板和 0.17 T 预设状态 |
| `client/src/app/MainWindow.h` | 新增实验链和参数预设展示标签 |
| `client/src/app/MainWindow.cpp` | 重构任务推荐、实验链、QC/参数预设展示 |
| `client/src/app/DeviceBridge.cpp` | 真实 SDK 模式下增加 `Run HOLD` 门禁，Demo 模式保留 UI 流程 |
