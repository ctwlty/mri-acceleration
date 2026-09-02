> **Historical method reference.** Current safety and acceptance rules in `../../authority/` govern. Dated actions are context only and do not authorize execution.

---
title: 菲特 Slim Pro 离线自动化验收器工作包
date: 2026-08-10
object: firstech-mri-sequence-development
scope: P2F与SeqSimu离线证据采集、身份校验和数值验收自动化
status: implementation-ready
authority: 当前仓库安全/验收文档与同批次原始证据
---

# 03｜自动化验收器工作包

## 1. 本工作包要解决什么

建立一个不依赖人工“看起来正确”的离线验收工具，把每次序列实验固定为：

```text
输入身份
→ 源码唯一diff
→ P2F四工件及中间语义
→ 当次PAR与波表
→ SeqSimu完成性和原始通道数值
→ 非目标变化
→ 安全清场
→ 可追溯报告
```

它服务于：

- Generic Cartesian Full64/63/R2/R4；
- 双轴`.gwave + ADC`诊断序列；
- 以后新的RF、梯度、ADC单变量实验。

本工具只计算和整理证据，不拥有科学结论升级权。

## 2. 当前断点与继承事实

已有多个零错误、带原始输出的PASS实验，可作为只读fixture；无需重跑厂家程序。

现有项目已经明确中间证据顺序：

```text
源码diff
→ .src.cpp/.src.s
→ 当次proj.par/.rfwave/.gwave
→ inf.log/目标通道原始数值
→ manifest/acceptance
```

当前缺口不是“再写一个总结脚本”，而是：

- 没有统一schema；
- 没有统一输入身份门；
- 没有统一P2F四动作记录；
- 没有统一SeqSimu完成性检查；
- 没有统一通道数值解析与非目标比较；
- 没有自动拒绝错批次、旧路径、SHA漂移和不完整结果；
- 没有把机器检查与科学裁决分开。

## 3. 实现边界

建议新建：

```text
<temporary-root>\sequence-offline-acceptance-tool-<timestamp>\
```

优先使用Windows现有Python标准库，不安装第三方依赖，不修改厂家安装目录。

工具建设和fixture测试阶段：

- 不启动P2F；
- 不启动SeqSimu；
- 不启动任何厂家GUI；
- 不操作网络或设备；
- 只读已有证据，负测试必须使用工具目录内的副本。

## 4. 目录与交付结构

```text
sequence-offline-acceptance-tool-<timestamp>\
├─ README.md
├─ sequence_accept.py
├─ run-validator.ps1
├─ profiles\
│  ├─ generic-cartesian.json
│  ├─ dual-axis-gwave-adc.json
│  └─ common-safety.json
├─ schemas\
│  ├─ manifest.schema.json
│  ├─ acceptance.schema.json
│  ├─ compile-report.schema.json
│  └─ safety-final.schema.json
├─ parsers\
│  └─ 文件格式说明与边界约定.md
├─ tests\
│  ├─ positive\
│  ├─ negative\
│  └─ expected\
├─ reports\
└─ docs\
   ├─ 证据审查规则.md
   ├─ 通道文件与指标对照.md
   └─ 已知边界.md
```

如为单文件Python实现，可不创建`parsers`代码包，但上述文档和schema仍须存在。

## 5. 四种工作模式

### 5.1 `fixture-test`

用途：使用已有PASS证据和故意损坏副本验证工具本身。

只读正例入口：

```text
<temporary-root>\sequence-gre-baseline
<temporary-root>\sequence-se-pre-adc
<temporary-root>\sequence-gre-cartesian-compile-gate
```

不得重跑这些实验，不得修改原目录。

### 5.2 `static-audit`

用途：在没有P2F或SeqSimu动作时检查：

- SRC、headers、systemSel和P2F身份；
- 唯一源码diff；
- RF/GWAVE格式、LUT地址、点数、数值、极性、面积和SHA；
- P2F已有四工件的存在性与语义；
- PAR字段、路径和运行资产闭合；
- 是否具备进入下一门的证据。

### 5.3 `preflight`

用途：Start或P2F之前生成机器可读的放行报告。

必须检查：

- 场景ID唯一；
- 输入路径和SHA符合冻结manifest；
- 目标diff与预注册一致；
- 无旧路径、同名历史文件回退；
- Start计数为0、results不存在；
- 必需工具和输入存在；
- 预期事件数、数值公式、容差、非目标文件已预注册；
- 当前厂家进程、网卡、设备可达性、RAW和厂家日志基线已记录；
- 前置场景和清场门已PASS。

输出只能是：

```text
READY
NO_START
UNKNOWN
```

### 5.4 `postrun-accept`

用途：运行后解析原始证据并形成机器验收结果。

必须检查：

- P2F/tcc/mri_c数字退出码；
- `SeqSimuEnd`与ERROR数；
- 必需原始文件完整性；
- 事件数、时刻、点数、复波形、幅值、相位、积分、相对ky或ADC窗；
- 目标变化符合公式；
- 非目标文件无未解释漂移；
- 输入/输出同次身份；
- 安全清场结果。

## 6. 建议命令接口

命令名可调整，但能力必须等价：

```text
python sequence_accept.py fixture-test --config <config.json>
python sequence_accept.py static-audit --manifest <manifest.json>
python sequence_accept.py preflight --manifest <manifest.json>
python sequence_accept.py postrun-accept --manifest <manifest.json> --results <results-dir>
```

PowerShell包装器负责：

- 传入绝对路径；
- 捕获Python数字退出码；
- 不吞stderr；
- 记录开始/结束时间；
- 把报告原子写入本轮`evidence`目录；
- 不自动启动厂家GUI或点击Start。

若以后集成GUI操作，必须由总控在唯一窗口、唯一PAR、Start=0均可视觉确认后单独执行；验收器本身不得盲点坐标。

## 7. 统一审查顺序

任何profile均按以下顺序，不允许跳层：

1. `manifest.json`：实验身份与预注册目标；
2. 源码和源码diff：实际改动；
3. headers、`systemSel.hw`、P2F身份；
4. `.rfwave/.gwave`：波表设计；
5. P2F动作记录、四工件路径和SHA；
6. P2F PAR：参数导出；
7. `.src.cpp`：高层展开；
8. `.src.s`：参数读取、循环、地址和控制器语义；
9. `.fcode`：仅作工件身份；
10. 当次`proj.par`与运行资产：实际加载输入；
11. `inf.log`和进程退出记录：是否完整结束；
12. 目标通道原始数值：科学量；
13. `GradMatSel.txt`与`shapenum.txt`：方向选择线索和拓扑；
14. 非目标文件差分；
15. `safety-final.json`：安全清场；
16. `acceptance.json`和Markdown报告。

## 8. 文件解析与主要判断

| 文件 | 工具必须输出的判断 |
|---|---|
| `.src` | 控制块、循环、事件顺序、参数声明、波表API和地址表达 |
| 源码diff | 改动文件、行、字段和值；是否只有预注册目标 |
| headers/systemSel/P2F | 路径、大小、mtime、SHA和版本身份 |
| `.src.cpp` | 循环条件、事件调用、参数实参、LUT地址和隐藏消费者 |
| `.src.s` | 参数读取还是立即数、循环比较、地址递增、控制器调用 |
| P2F PAR | 字段名、类型、默认值、唯一性和路径 |
| 运行`proj.par` | 实际SRC/S/波表/PhaseGain路径和当次参数值 |
| `.rfwave/.gwave` | 点数、地址、间隔、数值、极性、面积和SHA |
| `.fcode` | 路径、大小、SHA、是否与对照冻结一致 |
| `inf.log` | `SeqSimuEnd`、ERROR、Trigger和事件时序 |
| `rfdatarealNo1.txt`/`rfdataimagNo1.txt` | RF复幅、相位、相位斜率、点数和起止 |
| `gradsData.txt` | 逻辑GradS幅度、极性、时长、面积和RF相对时间 |
| `gradrData.txt` | 逻辑GradR逐点波形、面积、读出叶和RX对齐 |
| `gradpData.txt` | 逻辑GradP逐点波形、面积、pair和相对ky |
| `rxdata1.txt` | RX事件数、点数、dwell、窗口和可用采样时刻证据 |
| `GradMatSel.txt` | 逻辑矩阵选择；不得自动翻译成物理轴 |
| `shapenum.txt` | 各通道事件块数、数量和边界 |

解析器必须保存：

- 原始文件SHA；
- 行数和非空字节数；
- 使用的分隔符、数值精度和边界点规则；
- 无法解释字段时标为`UNKNOWN`，不得猜测。

## 9. `manifest.json`最低字段

```text
schema_version
experiment_id
scenario_type
created_at
authority_refs
safety_scope
source.path/size/sha256
source_diff.path/sha256/expected_changes
headers[]
system_sel
p2f.identity
p2f.actions[]
build_artifacts[]
runtime_par.path/sha256/expected_fields
waves[]
phasegain.path/sha256/pairs/order
expected.event_counts
expected.numeric_rules[]
expected.non_target_files[]
expected.quantization_model
expected.boundary_point_rule
prerequisite_gates[]
process_baseline
network_baseline
raw_baseline
vendor_log_baseline
```

所有路径必须保存绝对路径；SHA使用完整64位十六进制，报告可另显示缩写。

## 10. `acceptance.json`最低结构

顶层至少包含：

```text
schema_version
experiment_id
generated_at
automated_verdict
scientific_verdict
checks[]
warnings[]
unknowns[]
evidence_files[]
```

每个`checks[]`元素：

```text
check_id
category
status: PASS|FAIL|UNKNOWN|SKIP
expected
observed
tolerance
formula
source_files
source_sha256
reason
```

强制分离：

```text
automated_verdict = PASS|FAIL|UNKNOWN
scientific_verdict = PENDING_REVIEW|CONFIRMED_BY_SOL|REJECTED_BY_SOL
```

验收器永远不能自行写`CONFIRMED_BY_SOL`。

## 11. profile专项规则

### 11.1 Generic Cartesian profile

必须检查：

- 四场景`.fcode/.src.s/.src.cpp/P2F PAR`SHA一致；
- `noViews`为1…64整数；
- 活动PhaseGain恰为`2×noViews`项完整pair；
- Trigger/RF/GradS/GradR/GradP触发/RX均等于`noViews`；
- GradP叶数等于`2×noViews`；
- 实际`M_RX`逐项对应manifest中的source view；
- 完整pair闭合残差不超过预注册量化上界；
- 未选source view没有RX；
- P2F执行数为0。

### 11.2 双轴GWAVE与ADC profile

必须检查：

- 冻结SRC/GWAVE SHA；
- 两个LUT地址不重叠，点数各160；
- `.src.cpp/.src.s`保持地址、duration、gain和ADC实参；
- GradR/GradP逐点输出与各自LUT一致；
- 两轴首尾时间一致；
- 带符号面积比为4:3，容差来自量化误差传播；
- RX一窗、8点、20 µs dwell；
- RX窗位于两轴共同有效区；
- 只有逐点时间证据存在时才判ADC逐点同步PASS。

## 12. fixture测试

### 12.1 正例

至少从已有PASS证据中选择：

1. G14零错误基线：完成性、事件数和基础通道解析；
2. G14相对逻辑ky证据：GradP面积、`M_RX`和闭合残差；
3. CSJ_SE_PRE ADC证据：RX点数和窗口；
4. Generic编译门：P2F四工件与`noViews`静态语义。

工具对正例的独立重算必须与已有权威结论一致；不允许直接读取旧报告中的PASS字段作为自己的结果。

### 12.2 负例

所有负例只在工具自己的测试目录制作副本。至少包括：

- 删除必需原始文件；
- 修改一个输入SHA；
- 把`ERROR=0`改成非0；
- 删除`SeqSimuEnd`；
- 把运行PAR路径改到旧目录；
- 把PhaseGain完整pair拆开；
- 修改事件数；
- 交换一个非目标通道文件；
- 制造`.fcode`漂移；
- 让GradR/GradP LUT地址重叠；
- 只提供RX窗、不提供逐点时刻，然后要求逐点同步。

预期：

- 硬错误判`FAIL`；
- 证据不足判`UNKNOWN`；
- 不适用项判`SKIP`；
- 不能把UNKNOWN吞成PASS。

### 12.3 可重复性

同一fixture连续运行至少3次：

- `checks`顺序固定；
- 数值结果一致；
- 除`generated_at`和输出路径外，规范化JSON内容一致；
- 原fixture所有文件SHA运行前后不变；
- 工具数字退出码符合约定。

## 13. 工具退出码建议

```text
0 = automated_verdict PASS
2 = automated_verdict FAIL
3 = automated_verdict UNKNOWN
4 = schema/usage error
5 = safety preflight NO_START
```

报告必须落盘后再退出。即使FAIL/UNKNOWN也应保留可读报告，除非连manifest都无法解析。

## 14. 量化验收

自动化验收器自身PASS需要同时满足：

1. 四种模式均可调用；
2. 所有JSON均通过本地schema校验；
3. 正例fixture独立重算与权威结论一致；
4. 每个负例被正确拒绝，FAIL和UNKNOWN不混淆；
5. 连续3次结果确定；
6. 原始fixture运行前后SHA变化数为0；
7. 所有检查都能回指原始文件和SHA；
8. `automated_verdict`与`scientific_verdict`分离；
9. 任何证据缺失都不能产生自动PASS；
10. 工具建设与fixture阶段厂家程序启动数为0、网络/RAW/厂家日志变化为0。

## 15. 立即STOP条件

出现任一项立即停止工具建设或当前验收：

- 工具准备修改原始证据目录；
- 需要安装不明第三方包或修改厂家环境；
- schema不能表达UNKNOWN或证据来源；
- 解析器靠文件名猜实验身份而不核SHA和manifest；
- 直接继承旧报告PASS而没有独立重算；
- 只检查进程`exit 0`而不检查数值；
- 把`.fcode`肉眼解释为轨迹；
- 自动把逻辑GradR/GradP命名为物理Gx/Gy；
- 自动把窗口同步升级成逐点同步；
- 自动把机器PASS升级成科学能力PASS；
- 工具意外启动P2F、SeqSimu、厂家GUI或设备程序；
- 输出不确定或连续运行结果不一致。

## 16. 安全与清场

工具建设和fixture测试结束后：

1. 确认厂家/P2F/SeqSimu/tcc/mri_c启动数为0；
2. 原始fixture目录SHA变化数为0；
3. 只删除工具自己生成的临时负例副本，不删除报告；
4. 网络配置不变；
5. RAW目录数量、总字节和mtime不变；
6. 厂家日志无新增运行记录；
7. 输出工具级`safety-final.json`。

在以后由总控调用`preflight/postrun-accept`时，验收器只读取清场记录，不自行结束无法唯一归属的进程。

## 17. 输出文档

### 17.1 工具总文档

`README.md`固定包含：

1. 工具用途；
2. 不做什么；
3. 四种模式；
4. 命令示例；
5. 输入目录约定；
6. 输出目录约定；
7. 退出码；
8. FAIL/UNKNOWN解释；
9. Generic和双轴profile；
10. 安全边界。

### 17.2 每次运行报告

```text
evidence\
├─ manifest.json
├─ compile-report.json
├─ artifact-analysis.json
├─ run-input-report.json
├─ acceptance.json
├─ safety-final.json
└─ result-report.md
```

没有P2F的PAR-only场景，`compile-report.json`仍须存在，并明确：

```text
p2f_execution_count=0
frozen_artifacts_verified=true|false
```

没有SeqSimu的静态审计，`acceptance.json`中动态项必须为`SKIP`或`UNKNOWN`，不得缺省成PASS。

### 17.3 `result-report.md`

固定结构：

1. 一句话机器结论；
2. 输入身份；
3. 执行动作；
4. P2F与中间文件；
5. 当次运行输入；
6. SeqSimu完成性；
7. 目标通道数值；
8. 非目标差分；
9. 安全清场；
10. `automated_verdict`；
11. `scientific_verdict=PENDING_REVIEW`；
12. PASS能证明什么；
13. 不能证明什么；
14. UNKNOWN和缺口；
15. 原始证据路径和SHA。

## 18. PASS能证明什么

工具自身PASS后，可证明：

> 已建立一套可重复的离线证据检查方法，能够从manifest、源码diff、P2F中间工件、运行PAR、波表、SeqSimu原始数值和安全记录中，自动识别完整证据、输入漂移、缺失文件、数值不符和结论越级。

它还能显著降低：

- 打开错误同名PAR；
- 混用不同批次工件；
- 只看GUI或exit0；
- 忘记比较非目标通道；
- 把UNKNOWN误写成PASS；
- 后台进程和证据清场遗漏。

## 19. PASS不能证明什么

自动化验收器PASS不等于：

- 任一新序列科学正确；
- P2F语义或SeqSimu数值一定物理正确；
- 逻辑梯度等于物理梯度；
- 真实设备安全；
- RAW、重建或图像正确；
- Generic、双轴或Spiral能力已经PASS。

每个能力仍须由自己的预注册指标、原始数值和Sol复核裁决。

## 20. 最终裁决

工具总裁决：

```text
OFFLINE_ACCEPTANCE_TOOL = PASS|FAIL|UNKNOWN
```

并且必须同时保留：

```text
automated_verdict = PASS|FAIL|UNKNOWN
scientific_verdict = PENDING_REVIEW
```

只有Sol依据原始证据复核后，才可在外部总控报告中把特定能力升级为`CONFIRMED`。
