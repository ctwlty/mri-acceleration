# Decision Log

## Pending Decisions

| Date | Gate | Question | Options | Recommendation | Status |
| --- | --- | --- | --- | --- | --- |
| 2026-07-19 | SDK Mapping | `noSamples / noViews` 等底层字段如何从目标矩阵和序列参数映射 | 手工配置 / 序列映射表 / SDK 读取 | 等当前序列映射明确后做序列映射表 | Pending |
| 2026-07-19 | Validation | 哪些模板先解除 `Run HOLD` | FSE A/B 先验收 / LOC 先验收 / 全部保持 HOLD | LOC + FSE A/B 先做实机验收 | Pending |

## Confirmed Decisions

| Date | Gate | Decision | Rationale | Confirmed By |
| --- | --- | --- | --- | --- |
| 2026-07-19 | Preset | 参数预设版本为 `presetVersion=2` | 用户给定版本 | user |
| 2026-07-19 | Safety | 0.17 T 参数均为开发预设，不代表设备已实机验证 | 用户明确说明 | user |
| 2026-07-19 | Naming | `FSE A / FSE B` 矩阵是目标重建矩阵，不直接沿用历史高采样规模 | 防止 SDK 字段误填 | user |
| 2026-07-19 | Naming | `12.9 ms` 保留为历史序列标称 TE，未核验前不写成有效 TE | 防止科研语义过度承诺 | user |
| 2026-07-19 | Safety | 真实最短 TE、梯度爬升、RF 功率、均匀成像区和 SDK 字段仍需实机回填 | 用户明确说明 | user |

## Assumptions Used

| Date | Assumption | Why Safe | Reversal |
| --- | --- | --- | --- |
| 2026-07-19 | 先以 Markdown 文档承载参数预设，不写入客户端代码 | 本轮用户要求补充内容，不要求开发 | 进入 UI/模板库开发时转换为结构化数据 |
