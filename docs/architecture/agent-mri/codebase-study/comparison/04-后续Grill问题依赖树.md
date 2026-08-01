# Comparison｜04 后续 Grill 决策依赖树

本页只整理未来讨论的依赖顺序与所需证据，不提出问题、不推荐选项、不形成 To-Be 方案。只有来源会话明确进入 Grill 阶段后，才逐项展开。

## 依赖树

```text
D1 产品/发行边界
├─ D2 运行模式边界（Mock / native SDK / eggcontroller / vendor app）
│  ├─ D3 真实副作用所有权与唯一 Run owner
│  │  ├─ D4 SDK runtime 身份、参数身份与部署来源
│  │  └─ D5 错误、Abort、timeout、掉电与恢复语义
│  └─ D6 Mock 与真实证据如何隔离、如何标识
├─ D7 主任务与导航合同
│  ├─ D8 页面/领域状态的唯一真值源
│  ├─ D9 模板、参数、定位、确认与 snapshot 的冻结点
│  └─ D10 结束态、保存、历史、新任务与返回路径
├─ D11 数据和结果合同
│  ├─ D12 Task / Run / Snapshot / Source 的稳定身份
│  ├─ D13 RAW / 图像 / QC / manifest 的来源绑定
│  └─ D14 取消、失败、重启恢复与不覆盖语义
└─ D15 验收与发布门
   ├─ D16 Mock 交互、视觉、结果包证据
   ├─ D17 runtime/设备目标环境证据
   └─ D18 安装、依赖、版本、回滚与支持边界
```

## 每个节点的 As-Is 依据与进入条件

| 节点 | 为什么需要未来拍板 | 已有 VERIFIED FACT | 展开前还需的证据 |
|---|---|---|---|
| D1 | 当前同一源码面同时含 Mock GUI、native SDK CLI/verifier、proxy | 默认 Mock 不触发真实链，但旁路可达 | 目标交付对象、现场运维责任、允许的二进制能力 |
| D2 | B1-R 与 B1-P 是兄弟分支，vendor app 又是外部责任 | 三条路径不是同一接口 | 每条路径的权威入口与支持环境 |
| D3 | A/B0/代理可能分别拥有 Run | 当前 UI HOLD 不等于系统级唯一 owner | 进程占用、人工授权、并发/互斥要求 |
| D4 | c77 固定历史 runtime；当前 PTScan 已漂移 | manifest hash 与当前外部文件不一致 | 厂家/项目批准的版本清单与变更控制 |
| D5 | SDK 有外层 timeout/Abort；proxy 无总 timeout；Mock cancel 只内存 | 状态机语义跨路径不同 | 厂家状态码/Abort 安全证据、恢复演练边界 |
| D6 | 当前 Mock 包清晰标 MOCK，但真实旁路与同一应用共存 | `DataSourceKind` 已区分，发布物未物理隔离 | 用户/审计对混合模式的容忍度 |
| D7 | 当前 13 页与重复按钮出现断路/状态分裂机制 | 第 2、12 页及三层 CTA 映射有源码证据 | 目标用户任务的不可省略步骤与完成定义 |
| D8 | page、控件、布尔、MockWorkflow 多真值 | QA 可把 Empty 放任意页 | 页面与领域状态冲突时的权威规则 |
| D9 | snapshot 只在 Mock start 时冻结，真实链未绑定 | 当前 Mock identity 完整、SDK/proxy 未统一 | 模板/参数/定位的审批与可编辑时点 |
| D10 | Packaged 后只在 12/13 循环；无显式新任务 | 历史入口依赖内存 Packaged | 任务生命周期与保存失败后的用户动作 |
| D11 | Mock 有包；native SDK 只有 RAW；proxy 协议定义三路径但仓内真实入口有断点 | 三条产物语义不同，proxy 三路径只是协议期待而非真实运行事实 | 业务必须交付的最小工件集合 |
| D12 | Mock 有 run/snapshot；A/B0无；proxy task 来源外部 | 当前没有跨路径统一 ID | 跨程序/重启/设备的 ID 命名与作用域 |
| D13 | Mock 图/QC强绑定；真实 RAW→图像未进入同一包 | 不能把 Mock QC外推为实机 | 真实重建入口和一次运行的来源证据 |
| D14 | Cancel/Failed 多数不落盘；同名包拒绝覆盖 | 成功包较完整，失败证据弱 | 审计保留期限、恢复/重试与清理政策 |
| D15 | 现有测试按分支分散，关键 UI 测试有断点 | 注册数不等于当前通过 | 目标环境、放行角色和证据保留要求 |
| D16 | Mock 核心有单测但全控件覆盖断裂 | 包/状态源码证据较强，UI全链较弱 | 新鲜 build/CTest/点击/截图/manifest 绑定 |
| D17 | 本轮未接设备；c77 历史门禁不在 current | 没有当前目标环境事实 | 明确授权后的只读/有副作用验收协议 |
| D18 | A 无发布契约；B 多模式部署且外部依赖漂移 | 脚本/manifest 分属不同分支 | 安装器、依赖、签名、版本与回滚责任 |

## 源码已能回答、无需未来再问的事实

- A 与 B 是同仓库主要阶段，不是两个独立仓库。
- B 不是单点；runtime 与 proxy/Mock 从 `c236e00` 分叉。
- 当前 `33be` 不含 c77 runtime resolver/manifest/gate。
- 当前默认 GUI 是 Mock；automation 仅配置不 start；显式 auto-connect/verifier 仍可达真实代码。
- 当前 Mock 成功包有 6 个业务工件和 1 个 manifest；它不包含真实 RAW。
- Project A 没有分页任务流、run/snapshot、结果包或自动化测试。

## 暂停点

到此只完成事实底座和依赖排序。本文不选 D1 的答案，也不提前展开任何 Grill 选项；等待来源会话明确进入 Grill 阶段。
