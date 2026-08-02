# 91 — 静态 As-Is 合同（阶段 2 补证）

> 范围：仅静态读取指定 Git tree；未启动程序、未构建、未加载 DLL/SDK、未执行 Run/Abort。本文件不是 To-Be 设计或放行结论。
>
> 证据标记：`VERIFIED` 为给定 tree 的代码/对象可直接复核；`DOC-ONLY` 为文档或提交说明而非运行证据；`INFERENCE` 为由代码结构推出；`UNKNOWN` 为 tree 未能证明；`CONTRADICTED` 为不同 tree 的行为互斥。`A=80b5d85`，`B0=c236e00`，`B1-R=c77aeb4`，`B1-P=8e4ad0b`，`B2=91343be`；第二阶段最终测试/构建基线为 `70c5607`。

## 1. 阶段 / 发布画像

|阶段|画像、包含范围与排除范围|静态证据|
|---|---|---|
|A|[VERIFIED] A 是旧仪表盘式 Qt 客户端：无 CLI 参数；构造时以空 DLL 调 `loadSdk`，再以相对路径初始化 `demo_output`，随后展示 UI。故“无参”会进入 Demo 回退/演示路径，而非已验证的真实设备会话。|`80b5d85:client/src/main.cpp:15-25`; `80b5d85:client/src/app/MainWindow.cpp:89-105`; `80b5d85:client/src/app/MriSdkLoader.cpp:21-54`|
|B0|[VERIFIED] B0 引入 `--auto-connect`、SDK/配置/参数/输出参数与 GUI 自动建链；B0 是后续两条兄弟线的共同祖先。|`c236e00:client/src/main.cpp:27-136`；Git ancestry：`c236e00` 是 `c77aeb4`、`8e4ad0b` 与 `91343be` 的祖先。|
|B1-R（运行时验证兄弟线）|[VERIFIED] `c77aeb4` 在分支 `codex/migrate-verified-mri-runtime-impl`；它包含 B0、**不**是 B1-P/B2/HEAD 的祖先。因此其“accept MRI connection success code”及其祖先的 runtime 身份/门禁工作不在当前业务 tree。|Git ancestry：`c236e00..c77aeb4`；`git merge-base --is-ancestor c77aeb4 91343be` 返回非零。|
|B1-P（自动化代理线）|[VERIFIED] B1-P 为 B0 的另一子线，加入 eggcontrollerV2 代理；它是 B2 的祖先，故当前业务 tree 包含其自动化边界。|`8e4ad0b:client/tools/eggcontroller_proxy.py:46-115`；Git ancestry：`8e4ad0b` 是 `91343be` 的祖先，`c77aeb4` 不是其祖先。|
|B2 / current 业务树|[VERIFIED] B2 将 Mock 13 步工作流、结果包、历史、代理 UI 接入同一 Qt 程序；第二阶段基线 `70c5607` 在 B2 之上只增加无设备表征测试及测试隔离，生产代码与 `91343be` 相同。|`91343be:client/src/app/MainWindow.cpp:969-1176`; `91343be:client/src/app/MockWorkflow.h:14-198`; `git diff --quiet 91343be..70c5607 -- client ':(exclude)client/tests/**'` 返回 0。|

[CONTRADICTED] 不可把 B1-R 的“已验证运行时发布”描述为 current 的能力：current 包含 B1-P，未合入 B1-R；反之也不可把 B1-P 的代理能力归给 B1-R。以上只是提交图事实，不等同于任何实际设备验收。

## 2. 能力面矩阵

|入口 / 能力|依赖与调用链|外部副作用|默认阻塞 / 失败恢复|证据阶段|
|---|---|---|---|---|
|无参数启动|[VERIFIED] current 仅创建窗口；不传 `--auto-connect` 即不调用 `loadSdkAndConnect`。控制方式组合框禁用为 `mock`，真实 SDK/连接/预检/Run 按钮禁用。|[VERIFIED] 无 SDK/DLL/设备调用；UI 可进入 Mock 工作流。构造器只选择结果根，不执行结果根写探针。|[VERIFIED] Live 操作默认禁用，提示缺少身份、IDLE、映射、快照、隔离输出、Run owner、人工授权。|`91343be:client/src/main.cpp:76-111`; `91343be:client/src/app/MainWindow.cpp:200-318,1049-1055,1115-1126`|
|`--mock-step 1..13`|[VERIFIED] 主程序校验范围后调用 `setMockWorkflowStep`。|[VERIFIED] 只改变窗口工作流页号；代码未由该选项调用设备入口或结果根写探针。|[VERIFIED] 非整数或越界退出码 2。|`91343be:client/src/main.cpp:42-45,77-85`; `91343be:client/src/app/MainWindow.h:25-31`|
|`--auto-connect --sdk …`|[VERIFIED] `QTimer::singleShot` 调 `MainWindow::loadSdkAndConnect` → `DeviceBridge::loadSdk/connectDevice` → `MriSdkLoader::load/initialize`。|[VERIFIED] Windows 下 `LoadLibraryW`，并调用 Init/ConfigFile/输出目录/参数文件及校准相关 SDK 符号。|[VERIFIED] 缺少 `--sdk` 退出 2；加载、初始化及各 SDK 返回码转为 Fault/可见日志。`--auto-connect` 与 automation 互斥。|`91343be:client/src/main.cpp:30-41,63-135`; `91343be:client/src/app/MriSdkLoader.cpp:22-56,125-218`; `91343be:client/src/app/DeviceBridge.cpp:32-95`|
|automation（`--automation-python --automation-root [--automation-proxy]`）|[VERIFIED] 主程序只校验 Python/root/proxy 并调用 `configureEggController()` 保存配置；current 的 CLI/UI 没有调用 `EggControllerProcess::start()`。`EggControllerProcess::start()` 与代理脚本仍作为休眠组件存在；若被独立调用，代理才会调用原项目 `Ui_MainWindow.samplingBtn_click_sync()` 一次。|[VERIFIED] current automation 入口本身不启动子进程、不切换外部工作目录，也不产生 RAW/PNG；只有休眠组件被另一个 caller 显式启动时才可能产生这些副作用。|[VERIFIED] 与 auto-connect 互斥；缺参/路径无效退出 2；合法配置只显示“已配置；真实入口保持 HOLD”。休眠组件自己的进程/JSON/文件/PNG 失败合同仍可静态读取，但不等于 current 可达行为。|`91343be:client/src/main.cpp:46-110`; `91343be:client/src/app/MainWindow.cpp::configureEggController`; `git grep 'm_eggController->start' 91343be -- client/src` 无命中；`91343be:client/src/app/EggControllerProcess.cpp:23-178`; `91343be:client/tools/eggcontroller_proxy.py:46-115`|
|`mri_sdk_verify` verifier|[VERIFIED] 独立 `QCoreApplication` 可只加载+初始化，`--scan` 才调 `DeviceBridge::startScan`；CMake 单独生成该目标。|[VERIFIED] 同 auto-connect 的真实 SDK 初始化；若 `--scan`，执行 Run、轮询并要求新/更新且非空 RAW。|[VERIFIED] 四个路径必填，否则 2；加载/初始化/Run/会话 Fault/超时分别退出；RAW 未出现由 bridge 失败。|`91343be:client/tools/mri_sdk_verify.cpp:22-145`; `91343be:client/CMakeLists.txt:60-72`; `91343be:client/src/app/DeviceBridge.cpp:133-278`|

## 3. 完整 action registry

### A（80b5d85）

|可操作面|signal / caller → 状态所有者|外部副作用；可见反馈；失败恢复|证据|
|---|---|---|---|
|一级场景、检测对象 `QComboBox`；模板搜索 `QLineEdit`；模板列表选择|[VERIFIED] `currentIndexChanged`/`textChanged`/`currentRowChanged` → `handle*Changed` → `populate*` / `applyScene`；所有者为 `MainWindow::m_catalog` 与视图标签。|[VERIFIED] 无外部 I/O；刷新模板、参数、操作链与指标。无失败分支。|`80b5d85:client/src/app/MainWindow.cpp:162-199,236-239,630-651,769-859`|
|加载 SDK|[VERIFIED] `sdkBtn.clicked` → `handleLoadSdk` → `DeviceBridge::loadSdk/initialize`。|[VERIFIED] 文件选择、可加载 DLL、Init/ConfigFile/设置输出和参数文件；日志/SDK 标签更新。加载失败却回退 Demo 并仍返回成功，因而真实失败不会阻止随后演示操作。|`80b5d85:client/src/app/MainWindow.cpp:205-246,658-673`; `80b5d85:client/src/app/DeviceBridge.cpp:24-47`; `80b5d85:client/src/app/MriSdkLoader.cpp:21-54,107-140`|
|一键建链 / 校准向导|[VERIFIED] `connectBtn`/`precheckBtn` → bridge。|[VERIFIED] real 模式读温度、改连接/扫描徽标；Demo 直接显示“已模拟”。预检仅写日志、无真实检查恢复。|`80b5d85:client/src/app/MainWindow.cpp:240-243,653-678`; `80b5d85:client/src/app/DeviceBridge.cpp:50-74`|
|`DRY_RUN`|[VERIFIED] → `dryRunScene(currentScene)` → `ProtocolMapper::generateDryRun`。|[VERIFIED] 写 application-dir 下 `dry_run_params` 参数预览；诊断文本与日志可见。失败反馈为诊断/日志。|`80b5d85:client/src/app/MainWindow.cpp:243,680-683`; `80b5d85:client/src/app/DeviceBridge.cpp:76-90`|
|Demo 采集|[VERIFIED] `startBtn.clicked` → `startScan`。|[VERIFIED] Demo 只改扫描状态、刷新场景硬编码 QC 指标和日志；real 模式仍可能走 prepare/run（但模板 `HOLD` 时返回）。|`80b5d85:client/src/app/MainWindow.cpp:244,685-688`; `80b5d85:client/src/app/DeviceBridge.cpp:92-125`; `80b5d85:client/src/app/MriSdkLoader.cpp:201-235`|
|暂停 / 急停|[VERIFIED] 分别 → `pauseScan` / `abortScan`。|[VERIFIED] 暂停只改 UI 状态；急停调用 loader Abort，改“已终止”。未见 timeout/确认恢复。|`80b5d85:client/src/app/MainWindow.cpp:245-246,690-703`; `80b5d85:client/src/app/DeviceBridge.cpp:127-144`|
|CLI|[VERIFIED] A 的 `main` 未定义 `QCommandLineParser`。|[VERIFIED] 因而 A 无 `--mock-step`、auto-connect、automation、verifier 入口。|`80b5d85:client/src/main.cpp:15-26`|

### current B（91343be；按稳定对象名/工厂分组）

|可操作面（所有类别覆盖）|signal / caller → 状态所有者|外部副作用；可见反馈；失败恢复|证据|
|---|---|---|---|
|场景三件套、模板列表与“使用所选模板”|[VERIFIED] combo/edit/list → `handle*`；确认按钮先要求当前条目 UserRole 为 0，再 `applyScene`、转第 3 页。所有者 `m_catalog`、`m_workflowStep`。|[VERIFIED] 无外部 I/O；列表/模板标签/页号刷新；不满足选择时静默返回。|`91343be:client/src/app/MainWindow.cpp:997-1042,1130-1141,5262-5388`|
|控制方式 `ControlModeCombo`|[VERIFIED] `currentIndexChanged` → `updateControlMode`；控件创建后固定禁用且仅含 `mock`。|[VERIFIED] 无外部 I/O；明确可访问性文字：HistoricalRaw/Live BLOCKED。|`91343be:client/src/app/MainWindow.cpp:1044-1063,1142`; `91343be:client/src/app/MainWindow.cpp:5631-5645`|
|左栏真实动作：Load SDK、Connect、RealPrecheck、RealRun、RealAbort|[VERIFIED] signal 仍连至 `handle*`，但 Load/Connect/Precheck/RealRun 均禁用；Abort 隐藏且禁用。直接调用这些 `handle*` 也只记录 HOLD 并 return，不进入 bridge。|[VERIFIED] UI 与 handler caller 均不发 DLL/设备命令；真正可达 bridge 的旁路是 public `loadSdkAndConnect()` 被 `--auto-connect` 调用，以及独立 verifier，而不是这些 handlers。|`91343be:client/src/app/MainWindow.cpp:1069-1126,1143-1155,5418-5477,5527-5530`; `91343be:client/src/main.cpp:113-135`; `91343be:client/tools/mri_sdk_verify.cpp:22-145`|
|左栏 `DRY_RUN`|[VERIFIED] click → `handleDryRun`，只追加“Mock 参数快照已保留；未写入 SDK”并更新状态标签；current 不调用 `DeviceBridge::dryRunScene`。|[VERIFIED] 无文件或 SDK 副作用；可见反馈是日志与 `AutomationStatusLabel`。`DeviceBridge::dryRunScene` 仍存在为休眠旧能力，但 current UI 无 caller。|`91343be:client/src/app/MainWindow.cpp:1080-1082,1146,5465-5472`; `git grep 'dryRunScene' 91343be -- client/src/app/MainWindow.cpp` 无命中；`91343be:client/src/app/DeviceBridge.cpp:121-131`|
|左栏 Mock 开始、暂停、Mock-only 急停|[VERIFIED] 开始按钮只在第 8 页且 `m_mockAcquireButton` enabled 时代理 click；暂停默认禁用；急停调用 `MockWorkflow::cancel`、停 timer、回第 8 页。状态所有者 `m_mockWorkflow`/timer/`m_mockRunActive`。|[VERIFIED] Mock start/stop 不写设备；取消失败显示状态，成功显示 run/snapshot 且“不生成成功结果”。|`91343be:client/src/app/MainWindow.cpp:1083-1103,1147-1176`; `91343be:client/src/app/MockWorkflow.h:141-198`|
|中心全局 上一步 / 下一步|[VERIFIED] 两按钮依据当前页寻找 canonical action 并 `click` 或 `setWorkflowStep`；所有者 `m_workflowStep`。|[VERIFIED] 仅工作流页号/控件状态；下一步复用各页门禁，缺失 canonical 时禁用。|`91343be:client/src/app/MainWindow.cpp:1220-1269,4273-4355,4365-4588`|
|第 1–4 页：开始、两张模板卡的 `QRadioButton`、返回/查看推荐/添加 FSE B/采用、确认 Mock 预设|[VERIFIED] buttons/radio 均为 `setWorkflowStep` 或内存 bool（`m_comparisonEnabled`、`m_preparationConfirmed`）；卡片本身无 click handler，仅 radio 可操作。|[VERIFIED] 无文件/设备写；确认预设明确“仅内存、未写文件/SDK”。|`91343be:client/src/app/MainWindow.cpp:1370-1389,1428-1523,1573-1602,1684-1702`|
|第 5 页：5 个 L2 `QLineEdit`、L3 展开、仅本次使用、另存为新版本、确认方案|[VERIFIED] 五编辑框 `textChanged` 更新协议草稿/校验/定位规划；L3 仅切换显示；“仅本次使用”置确认并刷新；“另存为新版本”创建但未连接 signal（可见但无作用）；继续转第 6 页。|[VERIFIED] 都是内存 Mock 候选协议；校验失败会禁用确认并显示字段状态。`另存为新版本` 为静态死按钮，不可宣称保存。|`91343be:client/src/app/MainWindow.cpp:1742-1869`; `91343be:client/src/app/MainWindow.cpp:3243-3341`|
|第 6 页：进入切片规划/返回|[VERIFIED] 仅 `setWorkflowStep(7/5)`。|[VERIFIED] 无外部 I/O；页号可见。|`91343be:client/src/app/MainWindow.cpp:1894-1914`|
|第 7 页：三缩略图、三方位按钮、Read/Phase、自动调整、恢复推荐、斜切说明、中心复位、厚度/间距 `QLineEdit`、定位 `QSlider`、目标 `QComboBox`、应用目标、返回科研参数、确认定位|[VERIFIED] 稳定工厂创建并以 index connect；均更新 `LocalizationPlanner` 动态属性/草稿/`m_localizationConfirmed` 或页号。自定义 `LocalizationPlanner` 还支持鼠标拖动（中心、覆盖、尺寸、层组）与键盘方向键。|[VERIFIED] 内存 Mock 规划；确认才允许后续准备。无 SDK/RAW 写入；重置提供恢复推荐。|`91343be:client/src/app/MainWindow.cpp:1947-2356`; `91343be:client/src/app/MainWindow.cpp:523-655,767-823,3343-3547`|
|第 8 页：三项 `QCheckBox`、真实 Run、进入 Mock 采集、返回|[VERIFIED] checkbox toggled 重算可用性；真实 Run 按钮为 HOLD/禁用；Mock acquire → `startMockRun`。|[VERIFIED] 运行确认门禁重算及 `startMockRun` 会调用 `mockOutputRootWritable`，在结果根创建并自动删除写探针；可能更新目录 mtime。仅三项确认、准备证据、输出目录可写都满足才启动；失败显示 block reason，不进入运行；后退清除确认。|`91343be:client/src/app/MainWindow.cpp:2434-2522,3626-3653,3666-3717`; `91343be:client/src/app/MockWorkflow.cpp:291-410`|
|第 9–11 页：Mock 计时/处理完成、重试处理、返回定位、确认结果、重试 QC|[VERIFIED] timer 驱动 Mock progress；处理读取既有 Mock PNG、算 QC 并绑定 reconstruction；失败经 `fail/retryProcessing/recordQcFailure` 回可重试状态。|[VERIFIED] 正常 Mock 路径的标准图/QC 为内存直到打包；失败状态显式控制重试按钮和结果可见性。|`91343be:client/src/app/MainWindow.cpp:2594-2708,3718-3806,3985-4102`; `91343be:client/src/app/ImageQualityEvaluator.cpp:50`; `91343be:client/src/app/MockWorkflow.cpp:429-520`|
|第 12 页：保存结果包、打开位置、外部分析、历史、复制路径、返回当前结果|[VERIFIED] save → `saveMockResultPackage`；copy/open/history 有 handlers；“外部数据分析软件”创建但未连接 signal（静态死按钮）。|[VERIFIED] 保存会创建 staging、写 JSON/PNG/审计、原子 rename；未确认/QC/包状态不满足则禁用。打开/复制依赖已封存路径；保存失败留在可恢复状态并显示原因。|`91343be:client/src/app/MainWindow.cpp:2754-2835,3807-3867,4104-4153`; `91343be:client/src/app/MockResultPackage.cpp:197-335`|
|第 13 页：样品/模板/日期 `QComboBox`、关键词 `QLineEdit`、只读表格、打开/对比/来源|[VERIFIED] filters/text/currentCell 改内存筛选与选中摘要；open/source handlers；compare 被创建后置禁用且未连接。|[VERIFIED] `loadHistory` 只读取/校验实际 manifest；无记录时禁用筛选/表格并给提示；无设备/写 I/O。|`91343be:client/src/app/MainWindow.cpp:2840-2988,3868-3983`; `91343be:client/src/app/MockResultPackage.cpp:338-669`|
|图像交互|[VERIFIED] `ReferenceImageView` 只绘制/可换 source，未覆写鼠标事件；不能宣称其可点击。唯一图像面交互是第 7 页 `LocalizationPlanner`。|[VERIFIED] 失败时参考图提示资产不可用；定位图加载失败提示且无设备回退。|`91343be:client/src/app/MainWindow.cpp:875-912`; `91343be:client/src/app/MainWindow.cpp:508-513,523-655`|
|`QToolButton`|[VERIFIED] A 与 current 的 `client/src` 无 `QToolButton` 创建/信号。|[VERIFIED] 不存在该类 action。|`git grep QToolButton 80b5d85/91343be -- client/src`|
|CLI 入口|[VERIFIED] current 的 `--mock-step`、auto-connect、automation 与 verifier 已列于第 2 节；它们是 UI 之外的全部静态 CLI 入口。|[VERIFIED] 各自失败退出/信号见第 2 节。|`91343be:client/src/main.cpp:27-138`; `91343be:client/tools/mri_sdk_verify.cpp:22-145`|

## 4. 状态与数据所有权

|对象 / 数据|唯一或主要所有者|读写边界与持久化|证据|
|---|---|---|---|
|UI 页号（1–13）|[VERIFIED] `MainWindow::m_workflowStep` 与 stacked widgets。|[VERIFIED] `setWorkflowStep` 夹到 1..13，切页时重置相关确认/计时；`--mock-step` 仅设置此 UI 状态。|`91343be:client/src/app/MainWindow.h:168-201`; `91343be:client/src/app/MainWindow.cpp:4273-4355`; `91343be:client/src/main.cpp:77-85`|
|MockWorkflow 状态、runId、snapshot、审计|[VERIFIED] `MockWorkflow` 持有 data source、状态机、草稿/快照、进度、audit、重建、QC、确认、package path。|[VERIFIED] 状态限定为 Empty→Prepared/Running/Paused/Cancelled/Processing/Reconstructed/QcReady/Packaged/Failed；输出仅由结果包操作落盘。|`91343be:client/src/app/MockWorkflow.h:14-198`; `91343be:client/src/app/MockWorkflow.cpp:272-637`|
|DeviceBridge 会话与设备状态|[VERIFIED] `DeviceBridge` 持有 loader、config、timer、RAW 前后快照、session state、徽标/扫描/错误字段。|[VERIFIED] `loadSdk/connectDevice/startScan/abortScan` 唯一更新路径；通过 Qt signals 将结果投射到 UI。|`91343be:client/src/app/DeviceBridge.h:12-94`; `91343be:client/src/app/DeviceBridge.cpp:32-385`|
|EggControllerProcess 与 automation 工件|[VERIFIED] `EggControllerProcess` 独占 `QProcess`、stdout buffer、pending artifacts、terminal signal；`MainWindow` 保存 launch config 并连接 completed/failed 等信号，但 current 没有调用 `start()`，因此这些工件在当前生产入口不可达。|[VERIFIED] 若休眠组件将来被显式启动，代理可产生/返回外部 RAW 与两 PNG，C++ 端会校验路径、非空、任务目录/文件名及 PNG 解码；静态合同不能冒充 current 已执行的数据链。|`91343be:client/src/app/MainWindow.cpp:278-291,914-921`; `git grep 'm_eggController->start' 91343be -- client/src` 无命中；`91343be:client/src/app/EggControllerProcess.cpp:23-187`; `91343be:client/tools/eggcontroller_proxy.py:58-92`|
|snapshot（参数快照）|[VERIFIED] `MockWorkflow::m_snapshot`，由 start 时冻结；`MockParameterDraft` 含方案、几何、输出根等。|[VERIFIED] 保存结果包时写 `parameter-snapshot.json`，并在 manifest 身份一致性验证中重读。|`91343be:client/src/app/MockWorkflow.h:44-103,185-198`; `91343be:client/src/app/MockResultPackage.cpp:274-335,498-518`|
|RAW|[VERIFIED] 真实 RAW 由 SDK 输出目录所有；bridge 仅记录扫描前签名并发现新增非空 `.raw`。automation RAW 由 eggcontroller 返回并由过程边界校验。|[VERIFIED] Mock 正常路径不生成真实 RAW；无法发现新 RAW 时 Fault。|`91343be:client/src/app/DeviceBridge.cpp:144-278,358-385`; `91343be:client/src/app/EggControllerProcess.cpp:149-178`; `91343be:client/src/app/MainWindow.cpp:4203-4213`|
|PNG / 重建图|[VERIFIED] automation 的 K-space/final PNG 属原 eggcontroller 输出；Mock standard PNG bytes 暂由 `MainWindow::m_mockStandardResultPng` 持有并绑定到 `MockWorkflow` reconstruction。|[VERIFIED] 封存时写 `standard-mock-result.png`，并以 SHA-256 与 source/QC 交叉校验。|`91343be:client/src/app/MainWindow.h:186-202`; `91343be:client/src/app/EggControllerProcess.cpp:125-178`; `91343be:client/src/app/MockResultPackage.cpp:274-335,546-564`|
|QC|[VERIFIED] `MockWorkflow::m_qc` 保存图像级 SNR、均匀性、尺寸、图 hash；`ImageQualityEvaluator` 计算。|[VERIFIED] 结果 UI 只在 reconstruction+QC 存在时展示；写 `mock-qc.json` 并验证身份/hash。|`91343be:client/src/app/MockWorkflow.h:125-133,194-197`; `91343be:client/src/app/ImageQualityEvaluator.cpp:50`; `91343be:client/src/app/MainWindow.cpp:4031-4102`; `91343be:client/src/app/MockResultPackage.cpp:252-280,533-564`|
|package|[VERIFIED] `MockResultPackage` 拥有默认根、write/verify；workflow 仅存最终 package path。|[VERIFIED] staging 写六类工件+manifest 后原子重命名；同名不覆盖（由输入/写入校验）；错误清理 staging 并返回错误。|`91343be:client/src/app/MockResultPackage.h:11-78`; `91343be:client/src/app/MockResultPackage.cpp:197-335`|
|history|[VERIFIED] `MockResultPackage::loadHistory` 从结果根读取目录/manifest 并调用 verify；UI 表只是投影/筛选。|[VERIFIED] 忽略 staging；仅完整包提供 preview；历史页无写入或设备调用。|`91343be:client/src/app/MockResultPackage.cpp:605-669`; `91343be:client/src/app/MainWindow.cpp:3868-3983`|

## 5. Preserve / Correct / Retire / Unknown（仅证据候选）

|候选|静态依据（不是实施指令）|
|---|---|
|Preserve|[VERIFIED] `MockWorkflow` 的显式数据源、快照、状态转换和 audit；结果包的 staging→原子 rename、manifest/hash/历史复核，均为 current 已实现的可追溯边界。`91343be:client/src/app/MockWorkflow.h:14-198`; `91343be:client/src/app/MockResultPackage.cpp:197-669`|
|Correct|[VERIFIED] A 的空 DLL→Demo 回退和 UI “Demo 采集”与 current 的默认 Live BLOCKED 互相冲突；若保留 A 路径，会削弱当前“无默认真实/演示混同”的边界。`80b5d85:client/src/app/MriSdkLoader.cpp:21-54`; `91343be:client/src/app/MainWindow.cpp:1115-1126`|
|Retire|[VERIFIED] current 中“另存为新版本”“交给外部数据分析软件”“设为对比参考”均创建但没有 signal handler（后两者另有 disabled）；它们是可见但未实现的动作候选。`91343be:client/src/app/MainWindow.cpp:1835-1869,2754-2812,2960-2988`|
|Unknown|[UNKNOWN] current 静态 tree 不能证明真实 DLL、init/par、设备、输出目录、eggcontroller 原入口或 verifier 在现场能成功；也不能把 B1-R 的 runtime 验证成果移植为 current 事实。需隔离环境的运行证据。`91343be:client/src/main.cpp:86-135`; Git topology in §1|
