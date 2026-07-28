# 33be 场景化控制台交互与真实入口审计

## 结论

- 审计对象：01–13 当前 Qt Release 工作流与左侧全局控制区。
- 交互结论：已确认并修复所有“可见、启用、看起来可操作但没有可见反馈”的控件；Release 行为测试逐页覆盖。
- 当前界面/规划语义：`水模 → 横断位 LOC → 真实预检 → 单次 PTScan → 状态/RAW → eggcontrollerV2 既有结果入口候选`。
- 真实设备结论：**BLOCKED，未集成、未实扫。** 当前仓库的 `eggcontrollerV2/Iface/HCController.py` 实际导入 `console_mock`，且现有代理入口会自行调用一次采集，不是可安全复用的“仅重建已有 RAW”入口。
- 安全状态：可见 GUI 中的真实 SDK 加载、建链、预检、Run、Abort 均保持禁用；基线已有的显式 `--auto-connect --sdk` 运维入口未删除，但普通启动不可触发且本轮未使用。本轮未加载真实 SDK、未连接设备、未调用 Run/Abort、未生成设备 RAW。

## 审计与证据

| 项目 | 结果 |
|---|---|
| 工作树 | `C:\Users\Administrator\.codex\worktrees\33be\核磁共振场景化平台` |
| 分支 | `codex/migrate-verified-mri-runtime` |
| Release 目标 | `C:\tmp\nmr_ui_33be\client\build-release-ascii\dist-full-flow-ui-v2\scenario_nmr_client.exe` |
| 交互截图 | `C:\tmp\interaction-qa-final-20260728-210229\interaction-01.png` 至 `interaction-13.png`（13 张，共 4,406,242 字节） |
| 截图方式 | Release 配置的 Qt 测试目标加载同一 QSS/资源，逐页 `QWidget::grab`；仅 Mock 状态 |
| 外部可见窗口 | exact Release 无参数启动；PID `22160`；启动时间 `2026-07-28 21:04:10 +08:00`；标题“场景化核磁共振控制台”；窗口句柄 `8587734`；响应正常 |
| Release 身份 | `3,645,097` 字节；SHA-256 `D98B25528D0B5EDC127351F6B2426908BEBE528F95B81B6AE60B0271C9C21895` |
| Windows Graphics Capture | 当前主机返回 `SetIsBorderRequired failed: 不支持此接口 (0x80004002)`；未把该失败冒充成外窗截图 |
| 交互自动化 | `enabledWorkflowActionsExposeVisibleFeedback`、`visibleWorkflowControlsHaveNamesAndObservableBehavior`；覆盖按钮、单选、复选、下拉、输入、列表和表格 |
| 安全门禁自动化 | `realActionsExplainWhyTheyAreUnavailable` |
| 水模横断位语义 | `workflowUsesWaterPhantomTransverseSemantics` |

## 全局控制矩阵

“实测 PASS”均指当前 Release 配置 QtTest 对实际控件执行信号/状态验证；不是源码存在性推断。

| 控件 | objectName | Enabled 条件 | 信号/处理 | 预期可见反馈 | 实测 |
|---|---|---|---|---|---|
| 一级场景 | `PrimarySceneCombo` | 是；01 初始未选 | `currentIndexChanged → handlePrimarySceneChanged` | 检测对象与模板列表更新 | PASS |
| 检测对象 | `TargetCombo` | 是；01 初始未选 | `currentIndexChanged → handleTargetChanged` | 模板列表更新；当前默认“标准水模” | PASS |
| 模板搜索 | `TemplateSearchEdit` | 是 | `textChanged → handleTemplateSearchChanged` | 推荐列表过滤 | PASS |
| 推荐模板 | `TemplateList` | 是 | `currentRowChanged → handleSceneChanged` | 选中项和场景摘要更新 | PASS |
| 控制方式 | `ControlModeCombo` | 否 | 禁用 | 固定“自动化基线（Mock）” | PASS，故意禁用 |
| 加载 SDK | `LoadSdkButton` | 否 | 槽存在但 GUI 入口硬 HOLD | 按钮写明“本轮禁用” | PASS，未点击 |
| 一键建链 | `ConnectDeviceButton` | 否 | 槽存在但 GUI 入口硬 HOLD | 按钮写明“本轮禁用” | PASS，未点击 |
| 真实预检 | `RealPrecheckButton` | 否 | 槽存在但 GUI 入口硬 HOLD | 按钮写明“未执行” | PASS，未点击 |
| DRY_RUN | `DryRunButton` | 是 | `clicked → handleDryRun` | 左栏显示“DRY_RUN 完成 · Mock 参数快照 · 未写入 SDK” | PASS；原死按钮已修复 |
| 开始采集（Mock） | `LeftMockStartButton` | 仅 06 | `clicked → 09` | 切页并显示“运行中（Mock）” | PASS |
| 暂停/继续（Mock） | `MockPauseButton` | 06、09 | `clicked → handlePause` | 按钮在暂停/继续间切换；左栏显示状态 | PASS；原隐藏日志问题已修复 |
| 真实 Run | `RealRunButton` | 否 | `handleStart` 仍不调用 SDK | “真实 Run（等待现场确认）” | PASS，未点击 |
| Mock-only 停止 | `LeftMockStopButton` | 06、09 | 06→05、09→08 | 返回上一安全页面 | PASS |
| 真实 Abort | `RealAbortButton` | 否且隐藏 | `handleAbort` 仍不调用 SDK | 不在 Mock 界面暴露 | PASS，未点击 |

## 01–13 页面控件矩阵

| 页 | 可见控件（文字 / objectName） | Enabled / 信号 | 预期与实测反馈 |
|---|---|---|---|
| 01 | 开始选择任务 / `BeginResearchButton` | 是；`clicked → 02` | 切至 02，PASS |
| 02 | 两个模板单选 / `PrimaryTemplateRecommendationRadio`、`RepeatTemplateRecommendationRadio` | 是；互斥组，主项 `toggled` 更新卡片选中态 | 单选圆点与卡片边框更新，PASS |
| 02 | 返回 / `SceneSelectionBackButton`；查看推荐模板 / `ShowRecommendedTemplateButton` | 是；分别 →01、→03 | 页面切换，PASS |
| 03 | 添加 FSE B 对照 / `AddComparisonButton` | 是；设置对照标志并刷新 | 协议链加入 FSE B，PASS |
| 03 | 返回推荐列表 / `TemplateBackButton`；采用模板并继续 / `AcceptTemplateButton` | 是；→02、→04 | 页面切换，PASS |
| 04 | 返回模板 / `PreparationBackButton`；保存并继续 / `SavePreparationButton` | 是；→03、→05 | 页面切换，PASS |
| 05 | L2 五个编辑框 / `ProtocolL2Current0..4` | 是；`textChanged` 更新计算摘要 | 输入值自身可见，FOV/矩阵联动摘要，PASS |
| 05 | 专家参数 / `ShowL3Button` | 是；切换 `L3DetailsLabel` | 展开/收起与按钮文字同步，PASS |
| 05 | 仅本次使用 / `ProtocolUseOnceButton` | 是；更新左栏状态 | 明示“Mock 参数快照、未写入 SDK”，PASS；原死按钮已修复 |
| 05 | 另存为新版本 / `ProtocolSaveVersionButton` | 是；更新左栏状态 | 明示“版本保存待真实接入、未写文件/SDK”，PASS；原死按钮已修复 |
| 05 | 确认方案并继续 / `ContinueProtocolButton` | 是；→06 | 页面切换，PASS |
| 06 | 进入切片规划 / `OpenLocalizationPlanningButton` | 是；→07 | 页面切换，PASS |
| 07 | 定位画布 / `LocalizationPlannerView` | 是；鼠标拖动 | `planningCoverageModified=true` 并重绘，PASS |
| 07 | 横断/冠状/矢状 / `OrientationAxialButton`、`OrientationCoronalButton`、`OrientationSagittalButton` | 是；更新方位与按钮样式 | 当前方位、样式和画布更新，PASS；默认横断 |
| 07 | 交换 Read/Phase / `ReadPhaseSwapButton` | 是；调用 `swapReadPhase` | 画布与属性更新，PASS |
| 07 | 自动调整 / `AutoPlanningButton`；恢复推荐 / `ResetPlanningButton` | 是 | 覆盖修改标志设定/清除并重绘，PASS |
| 07 | 更多方位 / `MoreOrientationButton` | 是 | 按钮改为“自定义斜切（Mock）”，PASS |
| 07 | 成像目标 / `ImagingTargetCombo` | 是 | 当前选择直接可见，PASS |
| 07 | 修改 / `ModifyImagingTargetButton` | 是；读取当前目标并更新状态 | 左栏显示已应用的 Mock 目标，PASS；原死按钮已修复 |
| 07 | 科研参数 / `ResearchParametersButton` | 是；更新状态 | 明示转到第 05 页 L3、未写 SDK，PASS；原死按钮已修复 |
| 07 | 确认定位 / `ConfirmLocalizationButton` | 是；→08 | 页面切换，PASS |
| 08 | 三项确认 / `RunConfirmationCheck1..3` | 是；Qt 勾选态 | 勾选状态可见；不用于放行真实 Run，PASS |
| 08 | 真实 Run / `WorkflowRealRunButton` | 否 | 无设备调用 | 文案“等待现场确认”；紧凑门禁条写明未通过真实预检，PASS |
| 08 | 返回调整定位 / `RunConfirmationBackButton`；PTScan Mock 采集 / `MockAcquireButton` | 是；→07、→09 | 页面切换；Mock 自动推进 10，PASS |
| 09 | 无中栏动作；使用全局暂停/Mock-only 停止 | 全局规则 | 状态或安全返回 08，PASS |
| 10 | Mock 处理完成并查看结果 / `CompleteMockProcessingButton` | 是；→11 | 页面切换，PASS |
| 11 | 返回定位/重新采集 / `ReturnToLocalizationButton`；确认结果 / `ConfirmResultButton` | 是；→07、→12 | 页面切换，PASS |
| 12 | 保存结果包 / `SaveResultPackageButton` | 是；本地 Mock 状态更新后禁用自身 | 显示“Mock 结果包已保存；未调用 SDK”，PASS |
| 12 | 打开结果位置 / `OpenResultLocationButton` | 是；仅更新状态 | 明示“Mock 未生成磁盘结果目录”，不打开外部程序，PASS；原死按钮已修复 |
| 12 | 外部分析 / `ExternalAnalysisButton` | 是；仅更新状态 | 明示“真实结果生成后方可移交”，不启动外部程序，PASS；原死按钮已修复 |
| 12 | 打开历史记录 / `OpenHistoryButton` | 是；→13 | 页面切换，PASS |
| 13 | 返回结果 / `BackToResultsButton` | 是；→12 | 页面切换，PASS |
| 13 | 样品/模板/日期/关键词 / `HistorySampleFilter`、`HistoryTemplateFilter`、`HistoryDateFilter`、`HistoryFilter` | 是；调用只读过滤 | 表格行显隐更新，PASS |
| 13 | 历史表 / `HistoryReadOnlyTable` | 是、不可编辑；选行 | 右侧所选记录摘要更新，PASS |
| 13 | 打开/对比/来源 / `HistoryOpenButton`、`HistoryCompareButton`、`HistorySourceButton` | 是；更新 `HistoryActionState` | 三种只读 Mock 反馈均可见，PASS |

## 装饰项边界

- 02 的模板卡片、06/07/11 的缩略图、12 的六项结果包卡均为 `QFrame/QLabel`，不是 `QAbstractButton`，无按钮焦点、无按压信号。
- 处理表、参数快照和历史表按各自合同设置为只读或仅选择，不伪装为可编辑控件。
- 隐藏的 `WorkflowBackButton/WorkflowNextButton` 不是用户可见控件，不计入可见按钮矩阵。

## 已确认死按钮、根因与修复

| 原问题 | 根因 | 最小修复 |
|---|---|---|
| DRY_RUN、Mock 暂停点后无反馈 | 只调用 `appendLog`，但新三栏界面从未创建/挂载 `m_logView` | 复用现有 `AutomationStatusLabel` 显示结果；暂停按钮同步改字 |
| 05“仅本次使用/另存为新版本” | 构造后没有 `connect` | 接入只读 Mock 状态反馈；不写文件、不写 SDK |
| 07“修改/科研参数” | 构造后没有 `connect` | 显示目标应用结果或 L3 入口说明 |
| 12“打开结果位置/外部分析” | 构造后没有 `connect`，Mock 又没有真实路径/结果 | 明示不可执行原因；不打开文件管理器或外部程序 |
| 08 门禁提示形成大空卡 | `QLabel` 默认垂直扩张 | 固定为 72 px 紧凑状态条，并加高度回归测试 |
| 默认场景仍是根茎/通用结构 | 目录中无水模横断位默认模板 | 新增首选“水模横断位 PTScan 基线”；其余科研模板保留 |

## 真实入口盘点

### Qt 直接 SDK

1. 普通 GUI 启动：不加载 DLL。
2. 只有显式 `--auto-connect --sdk <mridll.dll>` 才会进入
   `main.cpp → MainWindow::loadSdkAndConnect → DeviceBridge::loadSdk/connectDevice → MriSdkLoader`。
3. `DeviceBridge::startScan` 已有
   `prepareScan → Run → ScanStatus 轮询 → 新/更新 RAW 检测 → Ready`；异常/超时才 Abort。
4. 当前 GUI 的加载、建链、预检、Run、Abort 槽均硬 HOLD，用户界面不可达。
   基线已有的 `--auto-connect --sdk` 仅可由显式命令行参数触发，普通无参数启动不会进入该路径；fake-SDK 回归依赖此 opt-in 入口。
5. Qt 默认参数路径为 `C:\MRIScanner\Scan\PTScan.par`，默认输出为 `D:\mri_data\par0423-3`；本轮没有读取或写入设备参数。
6. “横断”当前是 Qt 定位规划属性；尚无证据它被映射写入 PTScan/SDK，不能据界面选择宣称设备必然执行横断位。

### eggcontrollerV2 结果入口

1. Qt 进程边界为 `client/tools/eggcontroller_proxy.py`，其 `run_once` 会调用原程序 `samplingBtn_click_sync()` **一次**，并要求返回 RAW、K-space PNG、最终 PNG。
2. 当前 Qt 生产代码没有调用 `m_eggController->start(...)`，因此可见 GUI 到代理仍不可达。
3. 更关键的是当前仓库 `eggcontrollerV2/Iface/HCController.py` 实际为：
   `from Iface.console_mock import console`；`consolev3` 仅被注释。
4. 该代理不是“读取已有 RAW 后仅重建”的入口；直接启用会自行进入采集流程。因此在恢复并验证真实 import/入口前，不得接到真实按钮。

**真实闭环剩余阻塞：**

- 需要一个已经实机验证、当前代码实际导入 `consolev3` 的 eggcontrollerV2 根目录，或原程序明确提供的“仅处理本次 RAW”既有入口。
- 需要确认 PTScan.par 的横断位语义和哈希；当前 UI 横断属性未映射到 SDK 参数。
- 以上两项未满足前，真实 Run 保持禁用，不能声称“已集成”或“已实扫”。

## 首次水模横断位 Run 前唯一人工确认清单

1. 线圈与水模：确认接收线圈型号、水模居中、固定和现场安全。
2. 协议与方向：确认本次唯一 PTScan.par 的绝对路径/哈希，且设备实际协议与覆盖为横断位。
3. 设备状态：确认没有其他控制程序占用；连接码、温度、ScanStatus 空闲均由真实 SDK 返回。
4. 输出：确认目标目录可写，并记录扫描前 RAW 基线。
5. 单次 Run：现场明确授权“只运行一次”；完成后停止，不自动第二次；仅异常/超时允许既有逻辑 Abort 一次。

## 构建、测试与安全记录

- TDD：新增真实性与完整交互断言后先出现 `2 failed`；修正文案及测试目标后聚焦回归 `4 passed, 0 failed`。
- Debug：提交后最终全量 CTest `10/10 passed`，总计 `6.80 s`。
- Release：提交后最终全量 CTest `10/10 passed`，总计 `6.77 s`。
- 独立代码复审：最终无剩余 Critical / Important；显式 `--auto-connect` 被确认是未扩大的基线 opt-in 路径，不属于可见 GUI HOLD 的死按钮修复范围。
- 测试中的 SDK/代理均为 fake/临时目录；没有加载真实 `mridll.dll`。
- 最终可见 Release 仅有一个实例，命令行只有 EXE 本身；进程模块中未发现 MRI DLL。
- 本轮未点击任何真实控制按钮，未调用 MRI Run/Abort，未生成设备 RAW。
