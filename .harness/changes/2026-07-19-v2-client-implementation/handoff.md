# Handoff

## Completed

- 扩展 `SceneTemplate` 为 V2 任务模板模型。
- 用七类科研任务和 0.17 T 参数预设状态重写默认模板。
- 主窗口左侧改为推荐任务模板列表。
- 主窗口中部改为实验链展示。
- 主窗口右侧改为 QC + 参数预设状态。
- 真实 SDK 模式下遵守 `Run HOLD`，不调用真实 `Run()`。
- Mac/Qt 构建通过。

## Verification

- `cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)"` 通过。
- `cmake --build build -j` 通过。

## Remaining Risks

- 真实 SDK 采集和参数写入仍未启用。
- UI 还不是完整多页面导航。
- 参数预设仍是 C++ 静态数据，后续应迁移为 JSON 模板库。

## Follow-ups

- 更新 HTML 原型，使其匹配 V2 客户端信息架构。
- 增加参数预设抽屉/详情页。
- 设计 JSON 模板库并接入客户端。
- 增加结果交接包导出。
