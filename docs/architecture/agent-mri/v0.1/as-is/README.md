# Agent MRI v0.1 As-Is 架构审计

本目录是 Agent MRI 当前实现的权威 As-Is 入口。它描述的是提交
`91343be1992deaccdb99ac7a0b8b1052db7a22c6` 中实际存在的代码、调用链、状态和证据，
不把 13 页图集、旧产品合同或历史验收结论预设为正确的 To-Be。

## 当前产品边界

- 厂家 MRIScanner 负责真实扫描与正常成像。
- Agent MRI 当前只承担 Mock 演示。
- 本次审计没有启动 GUI、SDK、设备、eggcontrollerV2 或厂家程序，没有调用 Run/Abort。
- 本次审计没有修改业务代码、测试、资源、构建配置或设备适配。

> **VERIFIED FACT**：无参数启动时，可见 GUI 的真实 SDK/连接/Run/Abort 控件保持 HOLD；Mock 主链不调用设备。
>
> **VERIFIED FACT**：同一个源码交付面仍包含显式 `--auto-connect`、公开
> `MainWindow::loadSdkAndConnect()` 和可用 `--scan` 的 `mri_sdk_verify`。因此“默认 UI
> 不触发设备”成立，但“交付二进制完全没有真实副作用入口”不成立。

## 文档导航

1. [审计范围与证据](./00-审计范围与证据.md)：基线、方法、证据等级和限制。
2. [As-Is 分层架构](./01-As-Is分层架构.md)：A–G 七层职责、输入输出、状态、副作用与耦合。
3. [运行时调用链](./02-运行时调用链.md)：入口到 Mock 结束、结果包和历史，以及页面跳跃/重复按钮/结束态断点。
4. [状态与数据所有权](./03-状态与数据所有权.md)：真实所有者、镜像状态、持久化边界和一致性风险。
5. [接口与副作用边界](./04-接口与副作用边界.md)：默认 Mock、安全旁路、设备/进程/文件系统接口。
6. [测试覆盖与证据缺口](./05-测试覆盖与证据缺口.md)：测试源码证明什么、没有证明什么、现有证据为何不能绑定当前 HEAD。
7. [待 Grill 决策树](./06-待Grill决策树.md)：仅保留需要毛远洋拍板的物质性 To-Be 选择。

## 证据标签

- **VERIFIED FACT**：可由当前 HEAD 的源码、构建定义或已有文件直接定位。
- **INFERENCE**：从多个已验证事实推导出的高/中置信判断，不冒充运行事实。
- **OPEN DECISION**：源码无法回答、会改变 To-Be 边界或架构的产品选择。

所有源码引用使用 `仓库相对路径:行号` 或稳定符号。行号对应上述 HEAD；后续源码变化时应以稳定符号复核。

## 一眼看懂当前实现

```text
main.cpp
  └─ MainWindow（UI、路由、Mock 编排、外部适配器装配）
       ├─ MockWorkflow（run/snapshot/状态/审计）
       ├─ ImageQualityEvaluator（Mock PNG 图像级 QC）
       ├─ MockResultPackage（写包/校验/历史读取）
       ├─ DeviceBridge → MriSdkLoader → mridll.dll（默认 UI 不可达；CLI 旁路可达）
       └─ EggControllerProcess → QProcess（已配置但生产 UI 无 start 调用）
```

当前最大结构性事实不是“缺少某个页面”，而是 `MainWindow` 同时拥有路由、草稿、计时、
Mock 状态镜像、QC、文件写入、历史投影和真实适配器；页面状态与领域状态不是同一个真值源。
