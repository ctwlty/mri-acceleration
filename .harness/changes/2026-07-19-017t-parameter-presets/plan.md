# Plan

## Scope

- 新增 `docs/scenario-nmr-017t-parameter-presets.md`。
- 更新 `docs/scenario-nmr-product-design-v2.md`，补充参数预设设计。
- 更新 `.harness/rules/domain.md`，固化参数预设边界。
- 记录本次参数预设变更。

## Affected Areas

- `docs/scenario-nmr-017t-parameter-presets.md`
- `docs/scenario-nmr-product-design-v2.md`
- `.harness/rules/domain.md`
- `.harness/changes/2026-07-19-017t-parameter-presets/*`

## Acceptance Criteria

- 七类科研任务参数预设完整写入文档。
- 参数预设文档明确 B0、样品包络、梯度限制和开发预设状态。
- V2 产品设计能说明参数预设如何进入模板详情、导出报告和交接包。
- Harness 规则明确不得把未验证参数描述为实机验证参数。

## Risks

- 当前参数仍需设备适配和 SDK 字段映射。
- 部分术语如有效 TE、目标矩阵、底层采样字段必须严格区分。

## Rollback

- 本次只新增和更新文档，不影响客户端运行。
