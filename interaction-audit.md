# Agent-MRI v0.1 交互审计

审计日期：2026-07-29

审计对象：`scenario_nmr_client` 当前 33be 工作树

范围：01–13 Mock 可见流程；真实 SDK、设备、Run/Abort 全部不在本轮执行范围

阶段：`TESTED`

## 证据

- RED：`C:\tmp\agent-mri-v01-tdd\interaction-red.txt`，2 通过、8 失败；复现全局下一步绕过门禁、04/05 无有效反馈、07 无门禁反馈、09–13 假成功和结果动作缺口。
- 补充 RED：`C:\tmp\agent-mri-v01-tdd\interaction-p1-red.txt`，2 通过、5 失败；复现重复添加对照、禁用原因缺失、运行中可切换任务、11 返回目标错误和装饰表格可选择。
- 视觉真实性 RED：`C:\tmp\agent-mri-v01-tdd\visual-truth-red.txt`，2 通过、1 失败；复现 06 使用带假进度的采集资产。
- 视觉真实性 GREEN：`C:\tmp\agent-mri-v01-tdd\visual-truth-green.txt`，3 通过、0 失败；06 改为无进度的规划参考，10–12 空态不再在顶部宣称前置已完成。
- GREEN：`C:\tmp\agent-mri-v01-tdd\interaction-green-final.txt`，29 通过、0 失败、1 个截图测试因未设置目录而跳过。
- Mock 闭环 UI RED：`C:\tmp\agent-mri-v01-tdd\mock-ui-red.txt`，2 通过、2 失败；复现处理按钮永久禁用、取消无可见反馈。
- Mock 闭环 UI GREEN：`C:\tmp\agent-mri-v01-tdd\mock-ui-green-attempt1.txt`，4 通过、0 失败；覆盖唯一 run/snapshot、处理、QC、封存、实际历史和取消无成功工件。
- Part 3 P1 RED：`C:\tmp\agent-mri-v01-tdd\part3-p1-ui-red.txt` 与 `part3-p1-package-red.txt`；复现 08 绕过上游证据、快照遗漏可见选择、manifest 身份篡改未拒绝。
- Part 3 P1 GREEN：`C:\tmp\agent-mri-v01-tdd\part3-p1-ui-green-attempt1.txt`、`part3-p1-package-green-attempt1.txt`、`part3-p1-model-green-attempt1.txt`。
- 最终视觉真实性 RED：`C:\tmp\agent-mri-v01-evidence\final-ac93e8d\mock-closure-visible\09-mock-acquiring.png`、`11-mock-result-qc.png`、`12-result-package.png` 与 `13-history.png`；复现图片资产内嵌 `64% / 72% / RUN-MOCK-001` 与本次动态状态冲突，另有 01“无异常”和 07“预计3分20秒”两项无证据文案。
- 最终视觉真实性 GREEN：`C:\tmp\agent-mri-v01-evidence\final-0d8edf6\mock-closure-visible`；09、11、12、13 的图片不再内嵌伪动态状态，01 改为“设备告警：未核验”，07 改为“预计未计算 / SNR 未评估”。
- 最终聚焦回归：`C:\tmp\agent-mri-v01-evidence\visual-static-green-attempt1.txt`，4 通过、0 失败。
- 全新 Debug：`C:\tmp\agent-mri-v01-final-debug-0d8edf6\ctest.txt`，12/12 CTest 通过。
- 全新 Release：`C:\tmp\agent-mri-v01-final-release-0d8edf6\ctest.txt`，12/12 CTest 通过。
- 可见 Mock 闭环：`C:\tmp\agent-mri-v01-evidence\final-0d8edf6\mock-closure-visible-test.txt`，通过实际 Qt 控件点击完成 01→13→12，3 通过、0 失败。
- 本轮截图：`C:\tmp\agent-mri-v01-evidence\final-0d8edf6\mock-closure-visible\01-entry.png` 至 `13-history.png`，另含 `12-returned-from-history.png`。

## 共享控件

| 控件 | enabled 条件 | 可见结果 / 禁用原因 | 结论 | Evidence |
|---|---|---|---|---|
| `PrimarySceneCombo` / `TargetCombo` / `TemplateSearchEdit` / `TemplateList` | 无活动 Mock run | 重建对象/模板或过滤列表；运行中统一禁用并提示先暂停或停止 | 修复并测试 | `templateRestartIsUnavailableDuringMockAcquisition` |
| `UseSelectedTemplateButton` | 已选水模基线且无活动 run | 进入 03；非基线说明“仅供浏览”；运行中说明“Mock 采集中” | 修复并测试 | `selectedTemplateOffersVisibleContinuationFromAnyWorkflowPage` |
| `ControlModeCombo` | 永久禁用 | `v0.1 仅支持 MOCK` | 合同禁用 | `realActionsExplainWhyTheyAreUnavailable` |
| `LoadSdkButton` / `ConnectDeviceButton` / `RealPrecheckButton` / `RealRunButton` | 永久禁用 | 显示 LIVE 七项缺失门禁；无 SDK 副作用 | 合同禁用 | `realActionsExplainWhyTheyAreUnavailable` |
| `DryRunButton` | 当前 Mock 草稿可浏览 | 显示 `DRY_RUN / Mock / 未写入 SDK` | 已测试 | `enabledWorkflowActionsExposeVisibleFeedback` |
| `LeftMockStartButton` | 08 三项确认全部完成 | 代理 `MockAcquireButton`；不绕过确认 | 已测试 | `mockAcquisitionRequiresAllRunConfirmations` |
| `MockPauseButton` / `LeftMockStopButton` | 仅活动 Mock run | 暂停/继续更新文字；停止清活动状态并回 08 | 修复并测试 | `mockPauseStopsAndResumesAutomaticProgression` |
| `WorkflowBackButton` | 02–13 且无活动 run | 13→12、10→08、11→07，其余回前一步；活动 run 时给出停止原因 | 修复并测试 | `persistentWorkflowNavigationIsVisibleAtCommonWindowSize` |
| `WorkflowNextButton` | 当前页 canonical 主动作 enabled | 只代理页内主动作，不再直接 `step + 1` | 修复并测试 | `globalNextDelegatesToCanonicalPageActionAndRespectsItsGate` |

## 逐页控件矩阵

| 页 | 控件 / objectName | 实测反馈 | 修复或禁用判定 | 阶段 |
|---|---|---|---|---|
| 01 | `BeginResearchButton` | 进入 02；全局下一步等价代理 | 正常 | TESTED |
| 02 | 两个模板单选、`SceneSelectionBackButton`、`ShowRecommendedTemplateButton` | 单选卡状态可见；返回 01；水模基线进入 03；不支持模板禁用并说明原因 | 修复“选模板后没有下一步” | TESTED |
| 03 | `AddComparisonButton`、`TemplateBackButton`、`AcceptTemplateButton` | 添加 FSE B 后文字变“已添加”并禁用；返回 02；采用模板进入 04 | 清除重复无反馈按钮 | TESTED |
| 04 | `PreparationBackButton`、`SavePreparationButton` | 返回 03；确认后显示“仅内存、未写文件/SDK”并进入 05 | 修复假保存 | TESTED |
| 05 | `ProtocolL2Current0..4`、`ShowL3Button`、`ProtocolUseOnceButton`、`ProtocolSaveVersionButton`、`ContinueProtocolButton` | 五字段即时校验/计算；编辑会撤销本次确认；持久化明确禁用；通过后进入 06 | 修复无校验和假保存；表格本体不可选择 | TESTED |
| 06 | `OpenLocalizationPlanningButton` | 进入 07；页面和右栏均标注 Mock 规划参考，无假进度 | 修复 68% 假进度 | TESTED |
| 07 | 三方位、Read/Phase、自动/重置/更多、自定义目标、科研参数、`ConfirmLocalizationButton` | 每次操作均有文字/图上反馈；非横断位禁用确认；科研参数回 05 展开 L3；横断确认进入 08 | 修复死按钮和横断位门禁 | TESTED |
| 08 | `RunConfirmationCheck1..3`、`RunConfirmationBackButton`、`MockAcquireButton`、`WorkflowRealRunButton` | 必须同时具备 04 准备、05 协议、07 横断位定位证据及三项运行确认；直接跳页不能开始；返回会清运行确认；真实入口始终 LIVE: BLOCKED | 修复上游证据绕过；真实副作用不可达 | TESTED |
| 09 | 共享暂停/停止；页内无开始按钮 | 只有从 08 合法启动才生成唯一 run/snapshot 并显示 MOCK 进度；暂停/继续更新模型；取消保留身份与审计但清空成功工件；QA 跳页为空态 | 修复“页码即运行”的假状态 | TESTED |
| 10 | `CompleteMockProcessingButton`、`RetryMockProcessingButton` | 仅 `Processing` 状态启用；读取项目合法 Mock PNG、绑定当前 run/snapshot 和 SHA-256；失败才允许同身份重试 | 由确定性模型接管，无 RAW/72% 假进度 | TESTED |
| 11 | `ReturnToLocalizationButton`、`RetryMockQcButton`、`ConfirmResultButton` | 只有重建成功后显示 Mock 图和实际计算 QC；取消/失败/Empty 无图无数值；研究者确认后进入 12 | 修复假 QC；图像与 QC 由同一 SHA-256 绑定 | TESTED |
| 12 | `SaveResultPackageButton`、`OpenResultLocationButton`、`CopyResultPathButton`、`ExternalAnalysisButton`、`OpenHistoryButton` | 结果确认后才可原子封存七项结果包；参数快照冻结可见协议链、L2 与定位几何；成功后可复制/打开实际绝对路径并进入历史；未封存时禁用并解释；外部分析未配置 | 修复假保存/假打开和快照来源遗漏 | TESTED |
| 13 | `BackToResultsButton`、筛选器、只读表、打开/对比/来源按钮 | 两个返回入口均到 12；只读取实际 manifest；空根为 0 行；manifest 必须与目录、快照、来源、QC、审计和图像哈希交叉一致；损坏包标 Warning/Error；不生成样例记录 | 修复硬编码历史、身份篡改和“无返回” | TESTED |

## 01–13 截图清单

| 状态 | 字节 | SHA-256 |
|---|---:|---|
| `01-entry.png` | 89,829 | `4AF257886BC1156CCB73F4D5D17D1C976DF124C54B864B31EA42F6C49073502C` |
| `02-task-selection.png` | 115,750 | `1CF88F95906E292F5313B551E251C4ADCC0D90E59ABA5C3DE9A6D1B97C6D5AAF` |
| `03-template-confirmation.png` | 117,996 | `B4B8E3F968FDD1C47E51EF22E47BF49202E362E81B8E319B4D2BC81782E9EA3C` |
| `04-preparation.png` | 123,169 | `E39D846FBEAFC263394597768A2691DFEB5F50C9B23CF499CF7C43FF652824A2` |
| `05-protocol.png` | 130,836 | `6CB6B06E5328C7447383EE03685EF233E3756CC1A819FC4CF363F0BD68AF453C` |
| `06-loc.png` | 469,241 | `94BD4941C0707B537E26F8EF2AA71621155983D95AA0E72F106D004233D85582` |
| `07-localization.png` | 473,123 | `A20266F67B7D321AA0573D9F429BB5B7D79121CCE60F7B2CB66CE189716D232B` |
| `08-run-confirmation-ready.png` | 128,779 | `FC59A561C68D09CC9412E6B0B0144290A18099B5C6DE20CD0665DBB8319C021A` |
| `09-mock-acquiring.png` | 605,148 | `AD854897D6009F72841D4B51A97A6289615962997FD16656FE0D214AB23D365D` |
| `10-mock-processing.png` | 119,856 | `F26830062F86478E78DC6EBA6D9D0C9196258F82CFE858A55E17F1D910BEA3DF` |
| `11-mock-result-qc.png` | 454,568 | `62FFA3A97C34614D9485214C628AB5382ED3C3020A51DFE41BD1DAC95D0D552F` |
| `12-result-package.png` | 290,100 | `14426155DB37EE9A09E47DCB6D97AA78C6260636228E07A1D9204DF7C313172B` |
| `12-returned-from-history.png` | 290,100 | `14426155DB37EE9A09E47DCB6D97AA78C6260636228E07A1D9204DF7C313172B` |
| `13-history.png` | 135,053 | `3E76842A9357FCAAB6C10DE392465088E1C8AE2B3C50C0FC58F0B55E83A815A9` |

## 剩余边界

- 本审计未加载 SDK、未连接设备、未调用 Run/Abort、未生成真实 RAW。
- 08–13 已由确定性 Mock 状态模型接管：唯一 run/snapshot、结果根可写、暂停/继续/取消、合法 Mock 图像绑定、图像级 QC、七项结果包与实际历史均有自动化测试。
- 历史对比在 v0.1 明确不支持，按钮保持禁用而不是伪造反馈。
- 本轮已识别的 P0/P1/P2 全部关闭；03“采用模板并继续”、12“返回标准结果与 QC”、13 顶部和底部返回均有本次可见证据。
