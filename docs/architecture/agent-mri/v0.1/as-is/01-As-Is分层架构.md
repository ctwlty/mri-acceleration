# 01｜As-Is 分层架构

本页按 A–G 七层描述当前代码。每层均列出职责、输入输出、状态所有者、持久化、副作用、
执行模型、错误路径、依赖/越层、重复职责和证据缺口。

## 总体依赖方向

```text
A 入口/构建
  ↓ 直接构造
B MainWindow UI/导航
  ├─→ C MockWorkflow + QTimer（应用编排）
  ├─→ D SceneCatalog/SceneTemplate/MockParameterDraft（领域数据）
  ├─→ F ImageQualityEvaluator/MockResultPackage（QC/工件/历史）
  └─→ E DeviceBridge/MriSdkLoader/EggControllerProcess（外部接口）

G Tests 横向覆盖部分 A–F，但没有当前 HEAD 的新鲜运行绑定。
```

> **VERIFIED FACT**：这不是严格分层依赖。B 层 `MainWindow` 直接实例化并调用 C、D、E、F，
> 同时直接操作文件系统、剪贴板和桌面 URL；它是实际 composition root，也是应用服务和部分领域状态所有者。

## A｜程序入口、构建、打包、部署与运行模式

### 代码证据

- `client/src/main.cpp:20-138`，`main()`：解析 CLI、构造/显示 `MainWindow`、可选跳页/配置自动化/自动建链。
- `client/CMakeLists.txt:15-72`：注入 Git commit，构建 GUI 和独立 `mri_sdk_verify`。
- `client/CMakeLists.txt:74-278`：构建 fake SDK、fake proxy 与 12 个 CTest。
- `client/scripts/build.ps1:1-34`：固定 MSYS2 UCRT64；Debug 才启用测试并执行 CTest。
- `client/scripts/deploy.ps1:17-97`：清理受限 dist、复制 GUI/验证器/代理、运行 `windeployqt6`、隐藏冒烟。

### 分层属性

| 属性 | 当前事实 |
|---|---|
| 职责 | 选择默认 GUI、QA 跳页、真实 auto-connect、automation 配置或独立验证器入口；构建并部署运行依赖。 |
| 输入 | CLI、环境变量、Git HEAD、Qt/MSYS2 路径、build/dist 路径。 |
| 输出 | Qt 事件循环、EXE/DLL、部署目录、控制台退出码/日志。 |
| 状态所有者 | CLI 状态在 `QCommandLineParser`；构建身份由 CMake configure 时的 `NMR_SOFTWARE_COMMIT` 固化。 |
| 持久化 | build/dist 目录；没有安装器、发布 manifest、签名或 CPack。 |
| 副作用 | `--auto-connect` 可加载/初始化真实 SDK；部署会递归删除经过路径前缀保护的 dist；冒烟会启动并终止 GUI。 |
| 同步/异步 | 主入口同步配置；auto-connect 用 `QTimer::singleShot(0)`；GUI 进入事件循环。部署脚本顺序执行。 |
| 超时/取消 | GUI auto-connect 无调用级超时；部署冒烟固定等待 3 秒后结束进程。 |
| 错误路径 | 参数非法直接返回 2；auto-connect 失败只 `qCritical`，GUI 仍运行；样式加载失败静默返回空样式。 |
| 依赖/越层 | `main()` 直接认识 SDK 配置和 eggcontroller 代理参数；发布脚本同时交付 Mock GUI 与真实验证器。 |
| 重复职责 | GUI 和 `mri_sdk_verify` 都装配 `DeviceBridge`；README、脚本和实际 UI 模式描述不同步。 |
| 测试/证据缺口 | 有 CLI idle/fake auto-connect 测试，但没有当前 HEAD 的发行包清单、安全模式隔离或签名证据。 |

## B｜UI、导航、页面和用户事件

### 代码证据

- `MainWindow::MainWindow`（`client/src/app/MainWindow.cpp:200-318`）：构造三栏、适配器和计时器；两个 13 页栈分别在 `buildCenterPane` 与 `buildRightPane` 创建（`:1212-1217,4786-4790`）。
- `buildLeftPane`（`:969-1180`）：任务选择、Mock 控制、永久 HOLD 的真实控制。
- `buildCenterPane`（`:1182-1270`）：全局 Back/Next 及 canonical action 代理。
- `makeWorkflowPage`（`:1344-3016`）：01–13 页和页内事件。
- `setWorkflowStep/refreshWorkflow`（`:4273-4587`）：路由、按钮状态和文案刷新。
- `makeLegacyWorkflowPage`（`:3018-3224`）、`buildLegacyRightPane`（`:4795-4838`）：保留但无活动调用。

### 分层属性

| 属性 | 当前事实 |
|---|---|
| 职责 | 构造全部视图；将控件事件转换成页面跳转、确认布尔、Mock 动作或可见反馈。 |
| 输入 | 用户点击/编辑、`SceneCatalog`、`MockWorkflow` 状态、定时器、结果包/历史投影。 |
| 输出 | 两个 `QStackedWidget` 的当前页、标签/按钮状态、Mock 进度/图像/QC/历史表格。 |
| 状态所有者 | 页面号由 `m_workflowStep` 持有；大量草稿直接保存在控件值和动态属性；三个上游确认是 `MainWindow` 布尔。 |
| 持久化 | UI 本身不持久化；只有第 12 页显式封存后才写盘。 |
| 副作用 | 可写性 gate 会创建目录/临时文件；结果页可写剪贴板并请求打开本地目录。 |
| 同步/异步 | 多数事件同步；第 09 页由 GUI 单次计时器异步推进。 |
| 超时/取消 | Mock 计时器可暂停/继续/取消；页面构建、QC、封存、历史读取没有异步取消。 |
| 错误路径 | 多数失败写 `AutomationStatusLabel`；没有统一页面级 Error 模型。部分 handler 用提前 `return` 保持 HOLD。 |
| 依赖/越层 | 通过 `findChild(objectName)` 跨页面取值/触发按钮；直接调用领域、QC、文件系统、桌面服务、设备和进程类。 |
| 重复职责 | 页内 CTA、底部 Next、左栏动作代理同一行为；canonical 映射复制三处；整套 legacy 页面仍在源码。 |
| 测试/证据缺口 | 有大量 offscreen 控件断言，但一个“所有可见交互”测试提前返回；无当前 HEAD 可见运行和辅助功能全链证据。 |

## C｜应用编排、Mock 工作流、定时/异步/取消

### 代码证据

- `MainWindow::startMockRun`（`MainWindow.cpp:3666-3715`）：从 UI 汇总准备证据，创建新 candidate，prepare/start。
- 构造器中的 `m_mockAcquisitionTimer`（`:216-241`）：3.2 秒后把进度直接设为 100 并跳 10。
- `completeMockProcessing`（`:3718-3804`）：读 QRC PNG、绑定来源、同步 QC、跳 11。
- `MockWorkflow` 状态转换（`client/src/app/MockWorkflow.cpp:291-529`）。
- UI 暂停/继续/取消（`MainWindow.cpp:1156-1175,5479-5516`）。

### 分层属性

| 属性 | 当前事实 |
|---|---|
| 职责 | 协调准备、身份冻结、Mock 执行、处理、QC、确认和封存状态。 |
| 输入 | `MockParameterDraft`、准备证据、三项开始确认、Mock PNG、QC 结果、结果包路径。 |
| 输出 | 状态、run/snapshot ID、进度、审计事件、重建/QC 元数据、packagePath。 |
| 状态所有者 | 领域状态由 `MockWorkflow::m_state`；UI 另有 `m_mockRunActive/m_mockExecutionCompleted/mockPaused` 镜像。 |
| 持久化 | 运行中全为内存；只有后来成功封存的包才持久化审计/快照/结果。Cancelled，以及未重试成功并封存的 Failed run，没有独立持久化记录。 |
| 副作用 | 开始前可写探针；处理读 QRC；封存写结果根。Mock 路径本身不调用 SDK/Run/Abort。 |
| 同步/异步 | 执行等待由 QTimer 异步；状态转换、PNG 读取、QC 和写包均在 GUI 线程同步。 |
| 超时/取消 | 3.2 秒是演示完成时刻，不是失败超时。Running/Paused 可取消；Processing/QC/写包/历史不能取消。 |
| 错误路径 | `Failed` 清重建/QC/package；任意有 run/snapshot 的 Failed 可 `retryProcessing()`，不区分失败阶段。 |
| 依赖/越层 | `MainWindow` 而非独立应用服务编排状态模型、计时器、QC 和包事务。 |
| 重复职责 | 页面状态、UI 镜像和 `MockWorkflowState` 三套状态；`PACKAGE_SAVED` 由 writer 和 workflow 分别生成。 |
| 测试/证据缺口 | 未覆盖窗口在 Running/Paused 关闭、跨阶段返回后的旧 run 归宿、长任务取消/超时、失败阶段合法重试。 |

## D｜领域与产品概念

### 代码证据

- `client/src/app/SceneTemplate.h:5-34`：全字符串 DTO，无模板稳定 ID。
- `SceneCatalog::defaults`（`client/src/app/SceneCatalog.cpp:75-233`）：编译时静态目录。
- `MockParameterDraft`/`MockSnapshot`/`MockAuditEvent`（`MockWorkflow.h:44-114`）。
- `MainWindow::currentMockDraft`（`MainWindow.cpp:3549-3623`）：启动时从 UI 汇总领域输入。

### 分层属性

| 属性 | 当前事实 |
|---|---|
| 职责 | 表达场景模板展示字段、Mock 草稿、冻结快照、run、重建、QC 和审计。 |
| 输入 | 编译时 catalog；UI 控件值；固定 sample/template 文案；定位控件属性。 |
| 输出 | 可校验草稿、JSON 快照、run/snapshot 身份和审计事件。 |
| 状态所有者 | catalog 在 `MainWindow::m_catalog`；草稿在启动前没有独立对象所有者；run/snapshot 在 `MockWorkflow`。 |
| 持久化 | 模板不可保存；snapshot 仅成功包持久化；没有 Task 或 Plan 仓库。 |
| 副作用 | 领域模型自身基本纯内存；UUID/时间由可注入依赖生成。 |
| 同步/异步 | 全部同步。 |
| 超时/取消 | 领域状态支持 Running/Paused 的 cancel；没有恢复、过期或超时概念。 |
| 错误路径 | `validationErrors()` 检查方向、FOV、矩阵、TR/TE、层厚/间距、覆盖和输出根。 |
| 依赖/越层 | `currentMockDraft()` 直接用 `findChild` 读取 UI；只有 catalog 索引 0 被当作可运行基线，隐式依赖列表顺序。 |
| 重复职责 | 同一模板名/版本/ID分散在 catalog、页面文案和硬编码 snapshot；snapshot ID 被历史表显示为“方案版本”。 |
| 测试/证据缺口 | 无稳定 Task/Template/Plan ID 不变量；未测试第二次任务应继承还是清空对照/L2/定位草稿。 |

## E｜外部接口与副作用边界

详细矩阵见 [04-接口与副作用边界](./04-接口与副作用边界.md)。

| 属性 | 当前事实 |
|---|---|
| 职责 | DLL 动态加载/初始化/Run/Abort/轮询；外部 Python/eggcontroller 进程；DRY_RUN 文件映射。 |
| 输入 | DLL、init/par/output 路径；CLI；外部程序及 JSONL；SceneTemplate。 |
| 输出 | SDK 状态/RAW 路径/错误；代理 RAW/两图路径；`.dryrun.par`。 |
| 状态所有者 | `DeviceBridge` 持会话/轮询/RAW 前快照；`MriSdkLoader` 持 DLL/函数指针/SDK 状态；`EggControllerProcess` 持 QProcess/缓冲/待交付工件。 |
| 持久化 | SDK 输出 RAW；DRY_RUN 文件；外部代理产物。默认 Mock 主链不产生这些。 |
| 副作用 | `LoadLibraryW`、SDK Init/Run/Abort/CloseSys、QProcess、目录/文件写入。 |
| 同步/异步 | DLL 调用同步；扫描状态 QTimer 轮询；QProcess 异步。 |
| 超时/取消 | 扫描/停止/RAW settle 有外层超时；单次 DLL 调用无超时。QProcess 无总超时/取消 API。 |
| 错误路径 | SDK 错误转 Fault；异常/扫描超时会 Abort。代理非法 JSON 记日志，工件不完整则 failed。 |
| 依赖/越层 | `MainWindow` 直接拥有具体适配器；默认 UI HOLD 与 CLI/验证器可达真实路径形成双轨。 |
| 重复职责 | `DeviceActionAvailability` 声明 Ready 可 Run，但生产 UI 忽略它并永久禁用；README 描述第三种行为。 |
| 测试/证据缺口 | 无真机七门禁、防并发 owner、调用卡死、当前 HEAD 零真实调用的发布级证明。 |

## F｜数据、工件、manifest、QC、历史与来源追溯

| 属性 | 当前事实 |
|---|---|
| 职责 | 绑定 Mock PNG 与 SHA-256；计算图像级 QC；原子式写包；校验并投影历史。 |
| 输入 | `:/mock-reconstruction.png`、snapshot、run ID、commit、QC、审计、任务说明。 |
| 输出 | 6 个工件 + `manifest.json`；完整性结果；历史记录和有效预览路径。 |
| 状态所有者 | PNG bytes 在 `MainWindow`；来源/QC 元数据在 `MockWorkflow`；磁盘目录/manifest 是封存历史真值。 |
| 持久化 | `parameter-snapshot.json`、`mock-source.json`、PNG、QC、audit、task note、manifest。 |
| 副作用 | 创建 staging、逐文件 `QSaveFile`、回读哈希、目录 rename；失败尝试递归清理。 |
| 同步/异步 | QC、写包、verify、历史枚举和哈希全部同步。 |
| 超时/取消 | 无。大文件/大历史根可能阻塞 UI。 |
| 错误路径 | 拒绝覆盖、安全 run ID、根不一致、图/QC hash 不一致；Warning/Error 历史无预览。 |
| 依赖/越层 | writer 自行合成 `PACKAGE_SAVED`；UI 业务卡“原始数据”实际对应来源记录而非真实 RAW。 |
| 重复职责 | package writer 和 workflow 都拥有保存成功事件；内存审计与磁盘审计不是同一事件序列。 |
| 测试/证据缺口 | 无签名/可信锚、dirty/resource 身份、磁盘满/并发/cleanup 失败、大历史性能、取消/失败审计持久化。 |

## G｜测试、验收证据和未覆盖风险

| 属性 | 当前事实 |
|---|---|
| 职责 | 验证 loader/bridge、Mock 状态机、包、QC、UI、代理和 CLI 冒烟。 |
| 输入 | fake SDK、fake proxy、QTemporaryDir、offscreen Qt、环境变量。 |
| 输出 | CTest 结果、临时工件、UI 断言、fake 调用日志。 |
| 状态所有者 | 测试进程和临时目录；没有统一证据 manifest 绑定源码树/构建/测试/截图。 |
| 持久化 | 历史外部日志和旧验收文档；当前 HEAD 未生成新证据。 |
| 副作用 | 仅测试 fake 库/临时目录；本轮未执行。 |
| 同步/异步 | Qt Test 含 QTRY 等待；CMake GUI 测试以 3 秒 timeout 证明事件循环存活。 |
| 超时/取消 | 测试层有有限等待；没有生产长任务的系统性 timeout/cancel 验收。 |
| 错误路径 | 单元测试覆盖多个错误；生产构建/部署/身份错配未形成统一阻断。 |
| 依赖/越层 | `test_main_window` 编译几乎全部生产源；UI 测试可直接调用 public `loadSdkAndConnect` 和 QA 跳页。 |
| 重复职责 | 旧合同、interaction 文档、build log 各自宣称通过，但提交绑定不一致。 |
| 测试/证据缺口 | 当前 HEAD 无新鲜全量测试；有禁用/提前返回测试；无生产发布物副作用面、重启历史、性能/并发/安全证据。 |

## 分层结论

- **VERIFIED FACT**：`MockWorkflow` 和 `MockResultPackage` 已形成可独立测试的核心，但它们没有成为唯一应用真值。
- **VERIFIED FACT**：`MainWindow` 是 UI、应用编排、部分领域状态、文件/桌面副作用和外部适配器的共同所有者。
- **INFERENCE（高置信）**：若不先决定发布物是否必须物理 Mock-only，任何页面/领域重构都会继续携带真实旁路的安全歧义。
- **OPEN DECISION**：发布边界、状态唯一真值、结果事务/历史入口等选择见
  [06-待 Grill 决策树](./06-待Grill决策树.md)。
