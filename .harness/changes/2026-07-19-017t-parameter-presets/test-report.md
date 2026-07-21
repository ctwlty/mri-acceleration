# Test Report

| Gate | Command | Result | Evidence |
| --- | --- | --- | --- |
| preset-doc-readable | `sed -n '1,220p' docs/scenario-nmr-017t-parameter-presets.md` | Pass | 参数预设文档可读 |
| design-doc-updated | `rg -n "参数预设|presetVersion|序列标称 TE|noSamples" docs/scenario-nmr-product-design-v2.md` | Pass | V2 设计已引用参数预设规则 |
| domain-rules-updated | `rg -n "presetVersion|目标重建矩阵|序列标称 TE|noSamples|Run" .harness/rules/domain.md` | Pass | Harness 领域规则已更新 |

## Skipped Gates

- Qt build: 本轮只更新文档和 Harness 规则。
- Runtime test: 参数预设仍为开发预设，真实 `Run()` 保持 HOLD。
