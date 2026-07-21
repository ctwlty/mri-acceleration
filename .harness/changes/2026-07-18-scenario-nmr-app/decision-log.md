# Decision Log

## Pending Decisions

| Date | Gate | Question | Options | Recommendation | Status |
| --- | --- | --- | --- | --- | --- |

## Confirmed Decisions

| Date | Gate | Decision | Rationale | Confirmed By |
| --- | --- | --- | --- | --- |
| 2026-07-18 | Architecture | Qt/C++ 客户端直接对接 `mridll.dll`，不再绕行 C# 包装层 | `eggcontrollerV2` 已证明底层 demo 可直接用 C/C++/Python 连接 | inferred from request |
| 2026-07-18 | Testing | 保留 Demo 回退模式，保证没有硬件也能测试界面流程 | 便于本地验证与演示 | inferred from request |
| 2026-07-18 | UX | 主窗口增加 SDK 加载与状态显示 | 用户能清楚知道当前是 Demo 还是 Real SDK | inferred from request |

## Assumptions Used

| Date | Assumption | Why Safe | Reversal |
| --- | --- | --- | --- |
| 2026-07-18 | 先按 `eggcontrollerV2` 的 `Init -> ConfigFile -> SetOutputPath -> SetParameterFile -> Run -> ScanStatus/ScanCompleted -> Abort -> CloseSys` 作为最小闭环 | 该流程已在 demo 中跑通 | 若后续真机流程不同，再调整适配层 |
