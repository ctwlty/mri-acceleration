# Test Report

| Gate | Command | Result | Evidence |
| --- | --- | --- | --- |
| product-design-readable | `sed -n '1,260p' docs/scenario-nmr-product-design-v2.md` | Pass | V2 产品设计文档可读 |
| domain-rules-readable | `sed -n '1,160p' .harness/rules/domain.md` | Pass | 新领域规则可读 |
| scope-boundary | manual review | Pass | 文档保留外部分析软件边界、Run HOLD 和待设备适配说明 |

## Skipped Gates

- Qt build: 本轮只做产品设计，不修改客户端代码。
- Prototype visual gate: 用户要求先做产品设计，下一轮可基于本文档更新原型。
