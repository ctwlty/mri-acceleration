# Plan

## Scope

- 清理左侧模板选择区域。
- 增大任务列表高度和条目高度。
- 左侧只显示简短摘要。
- 右侧参数明细标题明确显示不写入 SDK。
- Demo 采集日志补充参数不写入 SDK 的说明。

## Affected Areas

- `client/src/app/MainWindow.cpp`
- `client/src/app/DeviceBridge.cpp`

## Acceptance Criteria

- 左侧不再出现长文本挤压和重叠。
- 用户能清楚知道参数明细只是显示。
- `cmake --build build -j` 通过。
