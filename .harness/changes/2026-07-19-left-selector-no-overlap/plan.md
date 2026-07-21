# Plan

## Scope

- 移除左侧模板列表下方的摘要表格和说明文字。
- 保留顶部任务标题、模板列表和操作按钮。
- 降低列表最小高度，避免窗口高度不足时挤压。

## Affected Areas

- `client/src/app/MainWindow.cpp`

## Verification

- `cmake --build build -j`
