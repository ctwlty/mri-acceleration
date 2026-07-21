# Decision Log

## Pending Decisions

| Date | Gate | Question | Options | Recommendation | Status |
| --- | --- | --- | --- | --- | --- |
| 2026-07-19 | UI | 是否把任务推荐、实验执行、QC、交接拆成多页面 | 单窗口三栏 / 多页面导航 / Tab 导航 | 下一轮用 Tab 或左侧导航扩展 | Pending |
| 2026-07-19 | Data | 参数预设是否转换为 JSON 模板库 | 继续 C++ 静态数据 / JSON 模板库 / SQLite | 下一轮先做 JSON 模板库 | Pending |

## Confirmed Decisions

| Date | Gate | Decision | Rationale | Confirmed By |
| --- | --- | --- | --- | --- |
| 2026-07-19 | Product | 客户端首版按 V2 设计实现任务推荐表达 | 用户要求根据当前文档开发 | user |
| 2026-07-19 | Safety | 真实 SDK 模式下，`Run HOLD` 模板不得调用真实 `Run()` | 保持未验证参数安全边界 | inferred from approved docs |
| 2026-07-19 | Data | 0.17 T 参数预设只进入 UI 状态展示，不写入 SDK 参数文件 | 当前序列映射未完成 | inferred from approved docs |

## Assumptions Used

| Date | Assumption | Why Safe | Reversal |
| --- | --- | --- | --- |
| 2026-07-19 | 先复用当前 Qt 三栏结构，不做多页面路由 | 变更范围可控且能快速验证 | 下一轮再做完整 IA 重构 |
