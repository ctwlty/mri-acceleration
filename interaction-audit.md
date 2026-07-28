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
- 截图测试：`C:\tmp\agent-mri-v01-evidence\interaction-20260729\capture-test.txt`，3 通过、0 失败。
- 本轮截图：`C:\tmp\agent-mri-v01-evidence\interaction-20260729\interaction-01.png` 至 `interaction-13.png`。

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
| 08 | `RunConfirmationCheck1..3`、`RunConfirmationBackButton`、`MockAcquireButton`、`WorkflowRealRunButton` | 三项确认同步控制 Mock 开始；返回会清确认；真实入口始终 LIVE: BLOCKED | 正常；真实副作用不可达 | TESTED |
| 09 | 共享暂停/停止；页内无开始按钮 | 只有从 08 合法启动才显示 MOCK 运行；QA 跳页为空态；运行中任务选择和返回被锁定 | 修复“页码即运行”的假状态 | TESTED |
| 10 | `CompleteMockProcessingButton`、`RetryMockProcessingButton` | 未有模型证据时均禁用并说明原因；无图、无 RAW/72% 假进度 | 诚实 Empty；由 Mock 模型接管后再启用 | TESTED |
| 11 | `ReturnToLocalizationButton`、`RetryMockQcButton`、`ConfirmResultButton` | Empty 时无图/QC；返回 07；重试/确认均禁用并说明原因；持久返回同样到 07 | 修复假 QC 和错误返回 | TESTED |
| 12 | `SaveResultPackageButton`、`OpenResultLocationButton`、`CopyResultPathButton`、`ExternalAnalysisButton`、`OpenHistoryButton` | 无封存包时全部禁用，逐项解释；页面有固定可见返回 | 修复假保存/假打开和“无返回” | TESTED |
| 13 | `BackToResultsButton`、筛选器、空表、打开/对比/来源按钮 | 两个返回入口均到 12；无实际 manifest 时 0 行、无预览，筛选和记录动作禁用并解释 | 修复硬编码历史和“无返回” | TESTED |

## 01–13 截图清单

| 页 | 字节 | SHA-256 |
|---|---:|---|
| 01 | 89,277 | `F5A561DD67486C2ED7AB6935A8F2EF096503B14E6386EE9AFF08EE7F3313F4AF` |
| 02 | 114,395 | `A31063E236EEC334E7C72187681C0D4F8008A8ACA43096CF99B4BECC72CFC55C` |
| 03 | 118,319 | `32E4F107F6F2392A06017DFA27C31FF576A8C1A59FE635F150712FEFC843ECE7` |
| 04 | 122,977 | `51B6973A4A563FB3D10E943BDE9441A4705DF009B000BB9FE2A90E98C48A6D80` |
| 05 | 128,944 | `DADA8A78DD2B0ED3C73C996A846DCF9826A64D45E1823D00F4A11E74A8DED87E` |
| 06 | 468,000 | `FDAE85D0685335E83E3A3836750C25DF43ABA92242BB8D9937345EAB80D9B6A6` |
| 07 | 471,803 | `891EC64011921BCB93233376663F366D0217E0E5ED51D40954E0E50FA7FED72B` |
| 08 | 125,960 | `E724F1699918278A0482A108787BC3C6C6BFBCCB51CC4DCE00D9346881D415A8` |
| 09 | 79,758 | `2A99070E9F2F8116EC672A51D688EFE7BA3D1553543AE0836D909721A7826EEF` |
| 10 | 112,358 | `19C4841DB08E9494E6203A3363728F2AD8E3BB0BCE0DAC9CD2A33CB5103BDDF8` |
| 11 | 86,420 | `5D541DAC05B3BA76B35BEF0F1D35AE54D8E41086BDF3F2565A8239D00E5C0C80` |
| 12 | 108,469 | `EF423719FE6F4B5D766844E386D2B591CA7C9FF6BECE7D363C90356199F696EB` |
| 13 | 85,969 | `A2E415DF36CA705DDAAD6759DAFB8276033475258A7E50E1E581D9CD76E046AD` |

## 剩余边界

- 本审计未加载 SDK、未连接设备、未调用 Run/Abort、未生成真实 RAW。
- 08 的唯一 run/snapshot、结果根可写门禁、Mock 重建/QC、封存包和历史加载由下一独立 Mock 状态模型提交完成；未完成前 10–13 保持 Empty/BLOCKED。
- 历史对比在 v0.1 明确不支持，按钮保持禁用而不是伪造反馈。
