# Decision Log

## Pending Decisions

| Date | Gate | Question | Options | Recommendation | Status |
| --- | --- | --- | --- | --- | --- |
| 2026-07-19 | Data | 交接包最终采用什么文件夹结构和元数据格式 | JSON 清单 / CSV 清单 / 外部软件专用格式 | 先用 JSON 清单 + 文件夹结构，等外部软件确定后扩展 | Pending |
| 2026-07-19 | QC | 科研样品 QC 阈值由系统固定还是用户配置 | 固定阈值 / 模板阈值 / 用户可配置 | 首版用模板阈值 + 用户确认 | Pending |

## Confirmed Decisions

| Date | Gate | Decision | Rationale | Confirmed By |
| --- | --- | --- | --- | --- |
| 2026-07-19 | Product | 首屏只使用“一级场景 + 检测对象”推荐任务模板 | 用户明确不要求先理解序列和参数 | user |
| 2026-07-19 | Product | 一级场景按主要科研结果分类，不按对象行业分类 | 避免把小鼠、根茎、生蚝、种子、水果误当一级场景 | user |
| 2026-07-19 | Product | 序列和参数属于任务模板内部，不作为首屏推荐条件 | 降低操作复杂度 | user |
| 2026-07-19 | Scope | 外部分析软件负责血管分割、成分定量、糖度预测和 fMRI 统计 | 当前控制台负责图像获得、QC 和交接 | user |
| 2026-07-19 | Safety | 设备连接和采集验收完成前，真实 `Run()` 保持 HOLD | 避免未验证采集误导用户 | user |
| 2026-07-19 | Naming | 基础序列继续称为 `FSE A` 和 `FSE B`，未核验前不命名为 T1/T2 | 防止科研语义过度承诺 | user |

## Assumptions Used

| Date | Assumption | Why Safe | Reversal |
| --- | --- | --- | --- |
| 2026-07-19 | 产品设计先落 Markdown 文档，不改客户端代码 | 用户明确“先做产品设计” | 用户确认后进入 UI 开发 |
| 2026-07-19 | 任务模板可先用候选协议描述，具体参数后续补齐 | 用户说明部分具体参数仍在梳理 | 参数确定后更新模板库和 UI |
