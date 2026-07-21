# Plan

## Scope

- 新增产品设计 V2 文档。
- 把推荐逻辑从“场景 -> 序列/参数”升级为“一级场景 + 检测对象 -> 推荐任务模板”。
- 定义任务模板的固定四段结构。
- 定义 QC 与外部分析交接边界。
- 记录真实 `Run()` HOLD 和参数待适配约束。

## Affected Areas

- `docs/scenario-nmr-product-design-v2.md`
- `.harness/changes/2026-07-19-research-task-product-design/*`
- `.harness/rules/domain.md`

## Acceptance Criteria

- 文档包含用户流、页面结构、模板表、数据对象、MVP 范围和验收标准。
- 文档明确首屏不展示序列/参数作为推荐条件。
- 文档明确外部分析软件边界。
- 文档明确未验证参数和协议必须标记“待设备适配”。

## Risks

- 具体协议参数仍在梳理，模板只能先以产品结构和候选协议描述。
- 真实设备采集能力未验收，不能在产品设计中暗示真实 `Run()` 已可用。

## Rollback

- 本次仅新增 V2 文档，不覆盖旧产品方案和技术设计。
