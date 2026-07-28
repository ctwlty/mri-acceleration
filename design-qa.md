# full-flow-v2 Design QA

## 结论

- 验收对象：`scenario_nmr_client.exe` 的 01–12 主流程与 13「历史记录」二级页。
- 范围：Qt UI、Mock 导航、Mock 图像与只读交互；真实 SDK、设备连接和真实 Run 保持 HOLD。
- P0：0
- P1：0
- P2：0
- 允许保留的 P3：见文末。

## 权威设计源与构建

| 项目 | 证据 |
|---|---|
| 用户提供 ZIP | `C:\Users\Administrator\Downloads\full-flow-v2.zip` |
| ZIP 大小 | `17,325,761 bytes` |
| ZIP SHA-256 | `AB82E609FCE4FBE72CDC1F233E1D1206CBE6A0B56379A96EA103CB936522AE2B` |
| 安全解压目录 | `C:\tmp\full-flow-v2-reference-20260728-152144\full-flow-v2` |
| 清单 | `Qt开发图集索引.md`、`00_全流程总览.png`、01–13 共 13 张状态图，均存在 |
| 最终 Release EXE | `C:\tmp\nmr_ui_33be\client\build-release-ascii\dist-full-flow-ui-v2\scenario_nmr_client.exe` |
| EXE 大小 | `3,628,729 bytes` |
| EXE SHA-256 | `5AFE2765D05F9D8523A265B94183ACF058695CED3EAF6551AA3BFE39DCD34C7A` |
| 最终证据目录 | `C:\tmp\full-flow-v2-qa-closed-20260728-190300` |
| 截图方式 | 对最终 EXE 逐页传入 `--mock-step 1..13`，精确匹配窗口标题后以 Windows 原生窗口捕获；每页捕获后关闭精确 PID |
| 实现截图尺寸 | 01–13 均为 `1586×992 px` |
| 参考截图尺寸 | 01、10 为 `1585×992 px`；其余为 `1586×992 px` |
| Viewport / 密度 | 外窗固定 `1586×992`；未覆盖 Windows 系统缩放，Qt DPR 使用系统默认，因此显式 DPR 数值为 unknown |

## 逐页同图比较

每个 `compare-XX.png` 均在一张图片内左置权威参考、右置最终实现；01 和 10 的参考图原始宽度少 1 px，未拉伸参考图。

| 页 | 状态与验收摘要 | 权威参考 | 最终实现 | 同图比较 | 结果 |
|---|---|---|---|---|---|
| 01 | 入口、SDK/设备/存储摘要与真实 Run HOLD；空态推荐卡和主 CTA 可见 | `C:\tmp\full-flow-v2-reference-20260728-152144\full-flow-v2\01_进入系统与设备状态.png` | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\impl-01.png` | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\compare-01.png` | PASS |
| 02 | 场景/对象选择；默认候选和按需对照构成清晰单选结构 | `C:\tmp\full-flow-v2-reference-20260728-152144\full-flow-v2\02_选择场景与检测对象.png` | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\impl-02.png` | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\compare-02.png` | PASS |
| 03 | 模板能力、默认主采集、第二组、重建、QC 和协议链结构完整 | `C:\tmp\full-flow-v2-reference-20260728-152144\full-flow-v2\03_确认推荐任务模板.png` | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\impl-03.png` | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\compare-03.png` | PASS |
| 04 | 样品登记、四项通过/一项待确认的真实状态层级；说明恢复为紧凑单行条 | `C:\tmp\full-flow-v2-reference-20260728-152144\full-flow-v2\04_样品登记与准备预检.png` | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\impl-04.png` | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\compare-04.png` | PASS |
| 05 | LOC→FSE A；L2 可编辑、自动计算、L3 折叠、L4 隐藏和三项动作齐全 | `C:\tmp\full-flow-v2-reference-20260728-152144\full-flow-v2\05_扫描方案与参数确认.png` | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\impl-05.png` | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\compare-05.png` | PASS |
| 06 | LOC Mock 进度、三方位缩略图和采集状态；真实 Run 未执行 | `C:\tmp\full-flow-v2-reference-20260728-152144\full-flow-v2\06_LOC定位采集.png` | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\impl-06.png` | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\compare-06.png` | PASS |
| 07 | 三标准方位、Read/Phase、中心/覆盖/切片线拖动、自动/恢复和更多方位 | `C:\tmp\full-flow-v2-reference-20260728-152144\full-flow-v2\07_定位与切片规划.png` | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\impl-07.png` | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\compare-07.png` | PASS |
| 08 | 四行参数快照、FSE A、冻结说明、独立三项确认卡；真实 Run 禁用 | `C:\tmp\full-flow-v2-reference-20260728-152144\full-flow-v2\08_运行前确认与参数快照.png` | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\impl-08.png` | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\compare-08.png` | PASS |
| 09 | FSE A Mock 采集中；左侧主控明确为蓝色「运行中」，不可二次启动 | `C:\tmp\full-flow-v2-reference-20260728-152144\full-flow-v2\09_FSE结构成像采集中.png` | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\impl-09.png` | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\compare-09.png` | PASS |
| 10 | 原生输出、来源绑定、RAW 解析、标准重建、QC 五步；未声称已验证 k-space | `C:\tmp\full-flow-v2-reference-20260728-152144\full-flow-v2\10_RAW保存与重建处理中.png` | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\impl-10.png` | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\compare-10.png` | PASS |
| 11 | 最终 Mock 图、QC、科研用户确认边界、返回定位/重新采集和确认结果 | `C:\tmp\full-flow-v2-reference-20260728-152144\full-flow-v2\11_标准结果与QC确认.png` | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\impl-11.png` | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\compare-11.png` | PASS |
| 12 | 六项结果包、四项元数据、保存/打开/移交/历史动作完整 | `C:\tmp\full-flow-v2-reference-20260728-152144\full-flow-v2\12_结果包保存与任务结束.png` | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\impl-12.png` | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\compare-12.png` | PASS |
| 13 | 只读历史筛选、表格、选中记录摘要、打开结果、对比参考、来源记录和返回 | `C:\tmp\full-flow-v2-reference-20260728-152144\full-flow-v2\13_历史记录_按需.png` | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\impl-13.png` | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\compare-13.png` | PASS |

总览证据：`C:\tmp\full-flow-v2-qa-closed-20260728-190300\compare-contact-sheet.png`

## 关键区聚焦比较

| 页 | 聚焦内容 | 证据 |
|---|---|---|
| 05 | L2 编辑表、自动结果、L3 与三项动作 | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\focus-05.png` |
| 07 | 三方位定位图、Read/Phase、覆盖框、切片线和拖动控制 | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\focus-07.png` |
| 10 | 三阶段预览和 1–5 处理链 | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\focus-10.png` |
| 12 | 结果主图、六项结果包与元数据 | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\focus-12.png` |
| 13 | 只读筛选、历史表格与记录动作 | `C:\tmp\full-flow-v2-qa-closed-20260728-190300\focus-13.png` |

## 设计面核验

- 字体层级：窗口标题、单行流程状态、页面标题、卡片标题、正文和证据/警示文案层级稳定；无标题栏下的第二个大号节点标题。
- 布局：01–13 维持左任务/对象/控制、中核心工作、右当前状态/QC/输出摘要三栏；中栏顶部仅一条紧凑状态条。
- 色彩与状态：蓝色主操作、绿色通过、橙色待确认/安全提示、灰色不可用和真实 Run HOLD 语义一致。
- 图像：使用项目内 Mock LOC、FSE 采集、重建和样品资产；所有可见图像均明确 Mock/设计示例，不绑定真实 RAW。
- 文案：真实设备入口为 HOLD；第 10 页明确 RAW 合同尚待设备实测验证，未出现「已验证 k-space」表述。
- 控件：02 单选、05 L2 编辑/L3 折叠、07 三方位与拖动规划、08 Mock-only 确认、09 Mock-only 停止、12 保存/历史、13 筛选/选中/来源记录均有自动化覆盖。

## P0/P1/P2 修复与复测历史

| 严重度 | 原问题 | 最小修复 | 复测证据 |
|---|---|---|---|
| P1 | 09 已显示 64% 采集中，但左侧仍显示可再次点击的「开始采集」 | 采集中切为蓝色「运行中（Mock）」且禁用二次启动；Mock-only 停止仍可返回 08 | `compare-09.png`；`main_window` 回归测试 |
| P2 | 04 单行说明被纵向拉伸成大空卡 | 固定为 42 px 单行证据条 | `compare-04.png`；紧凑说明回归测试 |
| P2 | 06–09 右侧状态卡过度压缩，信息层级不足 | 运作态改为 84 px 分隔行，保留成功/待确认/警示语义 | `compare-06.png`–`compare-09.png`；状态行高度回归测试 |
| P2 | 08 参数快照和三项确认缺乏独立分组，标题吸收剩余高度 | 快照固定为紧凑条、确认标题固定高度、三项确认置于独立卡，页面标题限定 90 px | `compare-08.png`；快照/确认卡回归测试 |
| P2 | 02–05 早期版本缺少候选单选、模板能力结构、预检行和可编辑 L2 层级 | 保留并复测上轮已完成的 Mock UI 结构 | `compare-02.png`–`compare-05.png`；全量 `main_window` 测试 |
| P2 | 10–13 早期版本缺少五步处理、科研确认边界、六项结果包和只读历史联动 | 保留并复测上轮已完成的结果/历史结构 | `compare-10.png`–`compare-13.png`；全量 `main_window` 测试 |

## 构建与测试

最终代码上的命令：

```powershell
C:\msys64\ucrt64\bin\cmake.exe --build C:\tmp\nmr_ui_33be\client\build-release-ascii --config Release -j 4
C:\msys64\ucrt64\bin\ctest.exe --test-dir C:\tmp\nmr_ui_33be\client\build-release-ascii -C Release --output-on-failure
```

结果：`10/10 passed`，总耗时 `6.15 s`。

通过项：

1. `mri_sdk_loader`
2. `device_bridge`
3. `device_actions`
4. `image_quality_evaluator`
5. `main_window`
6. `eggcontroller_process`
7. `eggcontroller_proxy_py`
8. `sdk_verify_fake`
9. `gui_auto_connect_fake`
10. `gui_automation_mode_fake`

说明：与 SDK/设备相关的自动化测试只使用 fake DLL/代理；视觉截图只使用 `--mock-step`，没有给 GUI 传入 `--sdk`、`--init`、`--par`、`--output` 或 `--auto-connect`。

## 允许保留的 P3

- Windows 原生标题栏图标、按钮字形与参考渲染环境存在轻微差异。
- 参考图部分右侧状态使用定制线性图标；实现使用清晰的文本层级和分隔行，语义与顺序一致。
- 01、10 的参考图原始宽度为 1585 px，而最终实现统一为 1586 px；同图证据保留原始 1 px 差异。
- 少量字体光学尺寸、边框线宽和局部留白与参考存在细微差异，不影响层级、状态或任务完成。

## 安全确认

- 未加载真实 `mridll.dll`。
- 未连接真实设备。
- 未点击或调用真实控制。
- 未调用 MRI `Run` 或 `Abort`。
- 未生成 RAW。
- 最终截图进程均按精确 PID 关闭；截图完成后无临时 `scenario_nmr_client.exe` 遗留。

`final result: passed`
