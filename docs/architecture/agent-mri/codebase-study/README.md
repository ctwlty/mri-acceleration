# Agent MRI 双版本代码解读

本目录是对同一阿里云仓库中两个主要版本阶段的 As-Is 解读入口。它回答“代码现在是什么、如何运行、哪些行为有源码证据”，不提出 To-Be 方案，也不把历史设计文档当作已经实现的事实。

> 当前理解验收门禁：**CONDITIONAL**。源码、静态二进制与无设备测试可解决的缺口已在第二阶段收口；厂家接口合同与真实数据链仍未验证。**Grill 与 To-Be 设计继续暂停**。

## 对象身份

| 对象 | 权威边界 | 结论 |
|---|---|---|
| Project A：阿里云原始 Qt 界面 | `origin/master` 本地跟踪快照 `80b5d853761ead14a8728c0be9c807fda3f75004` | 单窗口 Qt 控制台；默认 Demo；含可达的原生 DLL 加载、初始化和 Abort 代码，但 Qt client 没有 CTest/自动断言与完整扫描闭环 |
| Project B：后续维护版本 | 以 `c236e00436f9d9f151bc4ec099790e92c0f53524` 为共同基线，随后形成 `c77aeb4239d040af678e03bd25f4907cc3a3b39d` 运行时资产分支与 `8e4ad0b3874ade743235554759fbbc48a20bda6f` 代理分支；当前代码快照为 `91343be1992deaccdb99ac7a0b8b1052db7a22c6` | 不是一个提交点，而是“原生 Qt SDK 接入 → 两条并行演进 → 当前 Mock 产品流”的里程碑链 |
| 当前 `33be` | 分支 `codex/migrate-verified-mri-runtime`；第二阶段最终测试构建身份 `70c5607454d661a9b325798b627c1412ed2869ba` | `91343be` 之后只增加审计文档与无设备表征测试；生产代码仍与 `91343be` 相同，位于 B 的代理/Mock 分支，不包含 `c77aeb4` 的运行时解析器与清单 |

**VERIFIED FACT**：A 与 B 都来自 remote `git@codeup.aliyun.com:644fb9e097d94d909e43536f/nuclear_system.git` 的同一 Git 对象库及 worktree 家族。现有证据不支持把它们称为两个独立仓库。

## 阅读顺序

1. [代码理解验收报告](./90-代码理解验收报告.md)：第一、第二阶段门禁、material unknown 与最终 CONDITIONAL 结论。
2. 第二阶段补证：
   - [静态 As-Is 合同](./91-静态As-Is合同.md)
   - [离线身份与接口证据](./92-离线身份与接口证据.md)
   - [离线构建与表征测试证据](./93-离线构建与表征测试证据.md)
   - [机器证据目录](./evidence-stage2/)
3. [代码血缘与版本边界](./00-代码血缘与版本边界.md)：确认 A、B、两条 B 分支和当前 `33be` 的位置。
4. Project A：
   - [入口、构建与运行](./project-a-aliyun-original/01-入口构建与运行.md)
   - [界面、页面与交互](./project-a-aliyun-original/02-界面页面与交互.md)
   - [业务流程与状态](./project-a-aliyun-original/03-业务流程与状态.md)
   - [接口、数据与副作用](./project-a-aliyun-original/04-接口数据与副作用.md)
   - [测试证据与已知缺口](./project-a-aliyun-original/05-测试证据与已知缺口.md)
5. Project B：
   - [入口、构建与运行](./project-b-maintained/01-入口构建与运行.md)
   - [界面、页面与交互](./project-b-maintained/02-界面页面与交互.md)
   - [业务流程与状态](./project-b-maintained/03-业务流程与状态.md)
   - [DLL、自动化接口、数据与副作用](./project-b-maintained/04-DLL自动化接口数据与副作用.md)
   - [测试证据与已知缺口](./project-b-maintained/05-测试证据与已知缺口.md)
6. A → B 对照：
   - [变更矩阵](./comparison/01-A到B变更矩阵.md)
   - [行为保留、修正、废弃、未知](./comparison/02-行为保留修正废弃未知.md)
   - [补丁耦合与技术债地图](./comparison/03-补丁耦合与技术债地图.md)
   - [后续 Grill 决策依赖树](./comparison/04-后续Grill问题依赖树.md)

## 证据约定

- **VERIFIED FACT**：由固定 Git 提交树、Git 图、当前只读文件或本机文件哈希直接证明。
- **INFERENCE**：从多个已验证事实推导，明确给出置信度，不冒充运行事实。
- **UNKNOWN**：现有静态证据不足，或需要执行、目标环境/设备证据才能确认。
- 引用格式为 ``提交短 SHA:path:line``；当行号会随维护变化时，使用 ``提交短 SHA:path::稳定符号``。
- A 的结论只读取 `80b5d85` 提交树；B 每个结论只读取对应里程碑提交树。当前 `33be` 的既有审计仅作为当前快照参考，见 [v0.1 As-Is](../v0.1/as-is/README.md)。

## 第二阶段证据边界

- 进行了 Git/源码/配置读取、D32 字节级 PE 解析、外部目录哈希和仓库外精确提交构建；没有刷新远端。
- A `80b5d85` 显式 UCRT64 Debug 构建成功、注册 0 tests；current `70c5607` Debug/Release 构建成功，审查后允许的安全测试各 9/9 通过。9/9 是 CTest 进程数；`main_window` 内两个截图子用例因未设置证据目录而按设计 `QSKIP`。
- 没有加载真实 DLL、连接设备、执行 `--auto-connect`、verifier `--scan` 或任何真实 Run/Abort；没有启动 MRIScanner、真实 eggcontrollerV2 或面向用户 Qt 窗口。
- 三项测试按权限跳过：`device_bridge`（显式 fake Run/Abort 路径）、`sdk_verify_fake`（`--scan`）、`gui_auto_connect_fake`（`--auto-connect`）。
- 离线成功只证明代码可构建及允许范围的当前行为；不证明厂家 ABI、设备安全、真实扫描或 RAW→PNG 同次关系。
