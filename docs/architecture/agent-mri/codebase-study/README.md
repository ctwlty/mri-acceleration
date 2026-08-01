# Agent MRI 双版本代码解读

本目录是对同一阿里云仓库中两个主要版本阶段的 As-Is 解读入口。它回答“代码现在是什么、如何运行、哪些行为有源码证据”，不提出 To-Be 方案，也不把历史设计文档当作已经实现的事实。

## 对象身份

| 对象 | 权威边界 | 结论 |
|---|---|---|
| Project A：阿里云原始 Qt 界面 | `origin/master` 本地跟踪快照 `80b5d853761ead14a8728c0be9c807fda3f75004` | 单窗口 Qt 控制台；默认 Demo；含可达的原生 DLL 加载、初始化和 Abort 代码，但 Qt client 没有 CTest/自动断言与完整扫描闭环 |
| Project B：后续维护版本 | 以 `c236e00436f9d9f151bc4ec099790e92c0f53524` 为共同基线，随后形成 `c77aeb4239d040af678e03bd25f4907cc3a3b39d` 运行时资产分支与 `8e4ad0b3874ade743235554759fbbc48a20bda6f` 代理分支；当前代码快照为 `91343be1992deaccdb99ac7a0b8b1052db7a22c6` | 不是一个提交点，而是“原生 Qt SDK 接入 → 两条并行演进 → 当前 Mock 产品流”的里程碑链 |
| 当前 `33be` | 分支 `codex/migrate-verified-mri-runtime`，HEAD `adcab9799ce8430361e8e7c8ad523f68a6a22cc1` | `adcab979` 只增加当前 As-Is 文档；业务代码与 `91343be` 相同，位于 B 的代理/Mock 分支，不包含 `c77aeb4` 的运行时解析器与清单 |

**VERIFIED FACT**：A 与 B 都来自 remote `git@codeup.aliyun.com:644fb9e097d94d909e43536f/nuclear_system.git` 的同一 Git 对象库及 worktree 家族。现有证据不支持把它们称为两个独立仓库。

## 阅读顺序

1. [代码血缘与版本边界](./00-代码血缘与版本边界.md)：先确认 A、B、两条 B 分支和当前 `33be` 的位置。
2. Project A：
   - [入口、构建与运行](./project-a-aliyun-original/01-入口构建与运行.md)
   - [界面、页面与交互](./project-a-aliyun-original/02-界面页面与交互.md)
   - [业务流程与状态](./project-a-aliyun-original/03-业务流程与状态.md)
   - [接口、数据与副作用](./project-a-aliyun-original/04-接口数据与副作用.md)
   - [测试证据与已知缺口](./project-a-aliyun-original/05-测试证据与已知缺口.md)
3. Project B：
   - [入口、构建与运行](./project-b-maintained/01-入口构建与运行.md)
   - [界面、页面与交互](./project-b-maintained/02-界面页面与交互.md)
   - [业务流程与状态](./project-b-maintained/03-业务流程与状态.md)
   - [DLL、自动化接口、数据与副作用](./project-b-maintained/04-DLL自动化接口数据与副作用.md)
   - [测试证据与已知缺口](./project-b-maintained/05-测试证据与已知缺口.md)
4. A → B 对照：
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

## 本轮证据边界

- 本轮只进行了 Git、源码、配置和已有文件的只读核验，并新增本目录文档。
- 没有启动 Qt、MRIScanner、eggcontrollerV2 或任何厂家程序。
- 没有加载 SDK、连接设备、执行 Run/Abort、修改参数或生成 RAW/图像。
- 没有刷新远端，也没有运行构建或测试；因此文档中的测试结论只表示“测试源码存在/历史报告声称”，不表示当前主机新鲜通过。
