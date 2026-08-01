# 06｜待 Grill 决策树

本页只列源码无法回答、会改变 To-Be 架构或发布边界的物质性选择。顺序体现依赖；上游未定时，
不应先讨论下游页面细节。

## 决策顺序

```text
G1 发布物真实副作用边界（最高风险，先 Grill；与 G2 的产品语义可独立决定）
  ├─决定构建/部署是否分包
  ├─决定 MainWindow 是否还装配 DeviceBridge/EggControllerProcess
  └─决定测试必须证明“默认禁用”还是“物理不可达”

G2 返回、放弃、结束、新建任务与历史入口语义
       ↓
G3 Task / Template / Plan / Run / Snapshot 身份模型
       ↓
G4 生产状态唯一真值与路由派生方式
       ↓
G5 结果包事务、审计与可信程度
       ↓
G6 长任务异步/取消/失败/恢复保证

G1 与 G4 的结果共同约束最终 application state 和生产 target；G1 不替代 G2–G4。
```

## G1｜Agent MRI 的 Mock-only 是 UI 默认值，还是交付物硬边界？

### 为什么必须先拍板

**VERIFIED FACT**：

- 无参数 GUI 的真实控制 HOLD，Mock 链不调用设备（`MainWindow.cpp:1069-1126,3666-3804,5418-5530`）。
- `--auto-connect` 仍可加载/初始化真实 SDK（`main.cpp:113-135`）。
- `MainWindow::loadSdkAndConnect()` 是 public（`MainWindow.h:27`、`MainWindow.cpp:5400-5410`）。
- `mri_sdk_verify --scan` 可 Run（`client/tools/mri_sdk_verify.cpp:81-145`）。
- 部署脚本会把 verifier 与会调用旧程序非 demo 采集入口的 Python proxy 一起复制
  （`deploy.ps1:24-44`、`eggcontroller_proxy.py:46-64`）。

因此当前只有“默认 UI Mock-only”，没有“交付物能力 Mock-only”。该选择会直接决定 A/B/E/G 层边界，
不能靠后续按钮文案解决。

### 选项

| 选项 | 方案 | 收益 | 代价/风险 |
|---|---|---|---|
| A | 保留单一发行包；真实入口继续休眠/默认禁用 | 改动最小，工程调试方便 | 真实副作用面仍随产品交付；CLI/伴随工具绕过 UI HOLD；产品声明必须写成“默认禁用” |
| B | Agent MRI 生产包物理 Mock-only；生产 target 编译期移除 `--auto-connect`、公开连接入口及真实适配器装配，并从 dist 排除 verifier/proxy；工程能力留在源码/测试或独立内部构建 | 与“厂家 MRIScanner 负责真实扫描”边界最清晰；可做强不可达验收 | 需要构建 profile、依赖边界和发布测试；工程人员切换工具多一步 |
| C | 正式拆为两个明确命名/签名的 target/交付物：Agent MRI Mock target 在编译期不含真实入口/装配；MRI 集成工程工具包独立保留这些能力；不得混装/互相冒充 | 保留未来工程验证能力，同时让产品发行可物理隔离；职责和证据最清楚 | 发布、版本、权限和运维成本最高；必须定义谁能获得工程包 |

### 推荐

推荐 **C（两个明确交付物）**；若当前只允许维护一个公开发行包，则退而选 **B**。

理由：厂家 MRIScanner 已被明确指定为真实扫描与正常成像的权威工具。删除工程源码并无必要，但把真实验证器、
auto-connect 和非 demo proxy 与 Mock 产品混在同一发行包，会让“Agent MRI 只做演示”无法成为可测试的硬事实。
分包既保留已验证工程资产，也能让生产 Mock target 在编译期不包含真实入口/装配，并用二进制行为和文件清单共同证明物理不可达；仅从 dist 删除 DLL 或伴随工具不够。

### 需要毛远洋只回答这一项

> **你希望 Agent MRI 的“只做 Mock 演示”约束落在哪一级：A 默认 UI 禁用、B 生产包物理 Mock-only，
> 还是 C 拆成 Mock 产品包与内部工程工具包？**

在该问题确定前，不建议先重画页面、修补真实门禁或重新接入 eggcontroller。

## G2｜返回、放弃、结束、新建任务、历史的产品语义

依赖：无；可与 G1 并行拍板，但具体状态/路由设计必须等 G1、G2、G3 都明确。

需要决定：

- Processing→08、QcReady→07 是取消、失败、保存草稿，还是并存多个 run？
- Packaged 后是否有显式“结束任务”和“新建空任务/复制当前方案”？
- 历史入口由“当前 run 已封存”还是“结果根存在可报告 manifest”授权？
- 下一任务是否清空 FSE B/L2/定位几何，还是明确复制？

这些生命周期语义不能继续由 Back 路由和左栏模板按钮隐式决定；它们是后续状态集合与路由的输入。

## G3｜需要哪些稳定业务身份？

依赖：G2；哪些对象需要跨任务保留身份，取决于生命周期选择。

当前没有 Task/Plan 类型，Template 无 ID，snapshot ID 被显示为方案版本。需决定：

- 最小身份：`taskId/templateId/planId+version/runId/snapshotId/resultPackageId`；或
- 继续只保留 run/snapshot，并明确 UI 不承诺方案版本/任务历史。

推荐前者的最小受控子集，但字段语义必须由产品使用场景确定，不能从现有 13 页反推。

## G4｜生产状态的唯一真值与路由派生方式

依赖：G2、G3。G1 只决定 application state 是否还包含真实设备能力，不影响“生产状态必须有唯一真值”原则。

- 选项 A：页面号继续独立，保留 QA 任意跳页；要求显式区分 QA route 与 production route，并建立 page/state 合法组合表。
- 选项 B：生产页由 G2 定义的生命周期状态和 G3 身份模型派生；QA 使用独立 harness，不修改生产 aggregate。
- 推荐倾向 B，因为当前 page/state/UI bool 三真值直接导致跳页和按钮同步问题；但不能在 G2 之前先把现有 `MockWorkflowState` 当成最终状态集合。

## G5｜结果包要防偶发损坏，还是提供可信 provenance？

依赖：G3，并受 G2 的结束/保留语义影响。

- 轻量：保留 size/hash 和身份交叉检查，明确不防恶意篡改。
- 可信：增加 build/dirty/resource 身份、外部可信摘要或签名、严格工件集合和事务所有者。
- 同时要决定 `PACKAGE_SAVED` 唯一由 workflow 还是 package transaction 拥有，以及 Cancelled/Failed 是否持久化。

## G6｜长任务与恢复保证

依赖：G2、G4、G5。

需要决定：

- QC、写包、verify/history 是否必须后台执行并支持进度/取消/超时；
- Running 关闭窗口的确认与审计；
- 崩溃/重启是否恢复草稿、run、失败/取消证据；
- 多进程并发是否在支持环境内。

若本产品仍是单机短时 Mock 演示，可批准较小保证，但必须在产品合同中明确，而非由当前同步实现默认为真。
