# Plan

## Scope

- 扩展 `SceneTemplate`，增加 `parameterDetails`。
- 在 `SceneCatalog` 中为七类任务补齐参数摘要。
- 在右侧“参数预设”下增加“协议参数明细”只读区域。
- 模板切换时同步刷新参数明细。

## Affected Areas

- `client/src/app/SceneTemplate.h`
- `client/src/app/SceneCatalog.cpp`
- `client/src/app/MainWindow.h`
- `client/src/app/MainWindow.cpp`

## Acceptance Criteria

- 选择任务模板后能看到具体参数。
- 参数区可滚动/可阅读。
- `cmake --build build -j` 通过。

## Risks

- 当前参数仍为开发预设，不代表设备实机验证。
