# Plan

## Scope

- 加宽左侧 Splitter 区域。
- 增加 `QListWidget` 最小高度和伸展权重。
- 将列表项改成两行显示：一级场景 + 任务模板。
- 增大列表项高度和点击面积。
- 调整列表样式。

## Affected Areas

- `client/src/app/MainWindow.cpp`
- `client/resources/app.qss`

## Acceptance Criteria

- 左侧列表明显更大。
- 可见任务模板数量更多。
- Mac/Qt 构建通过。

## Verification

- `cmake --build build -j`
