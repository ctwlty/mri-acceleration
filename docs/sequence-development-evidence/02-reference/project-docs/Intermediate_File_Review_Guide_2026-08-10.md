> **Historical method reference.** Current safety and acceptance rules in `../../00-authority/` govern. Dated actions are context only and do not authorize execution.

---
title: 菲特 Slim Pro 离线序列开发中间文件审查与判读指南
date: 2026-08-10
updated: 2026-08-10
object: firstech-mri-sequence-offline-evidence-review
scope: Slim Pro（systemSel:3）.src、P2F四工件、运行输入、SeqSimu原始数值与证据身份
status: working-guide
authority: 当前仓库安全/验收文档与同批次原始证据
---

# 菲特 Slim Pro 离线序列开发中间文件审查与判读指南

## 1. 本指南解决什么问题

本指南用于回答五个必须分开的事实问题：

1. `.src`想让序列做什么；
2. P2F实际把源码理解成了什么；
3. 当次SeqSimu真正加载了什么参数和波表；
4. SeqSimu实际输出了什么RF、梯度和RX数值；
5. 上述文件是否确实属于同一次实验。

只有五个问题全部有可复核证据，才能形成完整因果链：

```text
源码diff + .src
        ↓
P2F_x32 + headers + systemSel.hw
        ↓
.src.cpp + .src.s + 编译基础.par + .fcode
        ↓
当次proj.par + .rfwave/.gwave + PhaseGain等运行资产
        ↓
SeqSimu：inf.log + RF/GradS/GradR/GradP/RX原始数值
        ↓
manifest.json + acceptance.json + safety-final.json
```

不能从任何中间一层直接跳到“序列能力已经掌握”。

## 2. 证据等级与允许结论

| 已有证据 | 最多允许写出的结论 | 不能写什么 |
|---|---|---|
| 只有`.src`、波表或静态检查 | `DESIGNED` / `STATIC READY` | 不能写P2F、SeqSimu或硬件PASS |
| P2F四项exit 0且四工件完整 | `COMPILE GATE PASS` | 不能写波形或轨迹动态可控 |
| `.src.cpp/.src.s`证明目标参数被活跃消费 | `P2F SEMANTIC GATE PASS` | 不能写冻结`.fcode`后PAR-only控制已通过 |
| SeqSimu进程exit 0、`SeqSimuEnd`、`ERROR=0` | `SIMULATION COMPLETED` | 不能写目标数值因果已通过 |
| 目标通道按预注册公式变化且非目标不变 | 对应能力的`OFFLINE NUMERIC PASS` | 不能写物理轴、真实ADC、真实k-space或成像PASS |
| 只有离线P2F/SeqSimu证据 | 逻辑通道离线能力 | 不能外推实机安全、RAW、重建或图像质量 |

状态必须使用以下含义：

- `PASS`：预先定义的必备证据全部满足；
- `FAIL`：获得了有效、完整证据，且至少一个硬条件明确不满足；
- `UNKNOWN`：关键证据缺失、身份不明、结果不可读或实验没有完整结束；
- `BLOCKED`：当前缺少必要厂家材料、授权、工具或安全前提，无法合法验证；
- `SKIP`：该检查不适用于本案例，并已说明原因。

“任务中断”“文件找不到”“只看到GUI曲线”都应判为`UNKNOWN`，不能判`FAIL`，更不能猜`PASS`。

## 3. 统一审查顺序

每次实验必须按以下顺序审查。前一级失败或身份不明，不进入后一级。

1. 冻结实验身份和工具链；
2. 审查源码diff；
3. 审查`.src`及厂家头文件调用；
4. 审查P2F四个动作和四个工件；
5. 审查`.src.cpp/.src.s`中的实际展开；
6. 区分编译基础PAR和当次运行`proj.par`；
7. 审查`.rfwave/.gwave/PhaseGain`等运行资产；
8. 运行前冻结输入和非目标文件SHA；
9. 审查SeqSimu退出、`inf.log`和各通道原始数值；
10. 复核非目标文件、进程、网络、RAW和厂家日志；
11. 生成manifest、acceptance和safety结论；
12. 最后才写“支持什么”和“不支持什么”。

## 4. 源码层

### 4.1 源码diff

**作用**

回答“本轮到底改变了哪个可证伪因素”。它是建立单变量因果的第一道门。

**必须检查**

- 基线和变体的完整路径、大小、SHA-256；
- 逐行diff和二进制diff摘要；
- 改动的稳定符号、变量、函数调用、循环条件或表索引；
- 是否同时改到了注释、路径、换行、波表或其他业务字段；
- 本次改动是否与预注册的唯一变量一致。

**通过标准**

- 只有一个业务变量或一个结构性目标发生变化；
- 所有机械差异都能解释；
- 没有未登记的第二个控制量变化；
- diff保存为本次证据包的一部分。

**常见误判**

- 同时修改RF、梯度、ADC和循环，却仍称“单变量”；
- 只口头描述改动，没有保存diff；
- 从上一次变体继续改，导致继承了不应存在的旧差异；
- 只比较文件名，没有比较内容SHA。

**记录字段**

```text
baseline_src / baseline_sha256
variant_src / variant_sha256
business_change
changed_symbols
changed_line_count
unexpected_diff_count
diff_file
```

### 4.2 `.src`

**作用**

表达厂家DSL中的设计意图：控制器块、主循环、事件顺序、参数声明、波表调用、地址递增和RX触发。

**必须检查**

- `systemSel:3`对应的厂家语法和头文件；
- `main/gradS/gradR/gradP/tx1/rx1`各控制块是否存在、各自职责是否单一；
- 主循环、分支、`goto`、Trigger和停止条件；
- RF、梯度、RX事件调用的先后关系；
- 参数声明是`common/global`还是源码局部常量；
- 波表类型、波表号、LUT地址和每次循环的地址递增；
- 同一个事件数是否只有一个所有者；
- 是否引入未经厂家材料证明的API、结束语法或控制器组合。

**通过标准**

- 源码结构来自当前Slim Pro已经可编译的厂家/项目骨架；
- 目标参数有明确消费者；
- 每个循环、地址和事件都有边界；
- 没有为“让它编译”而加入虚假RF、梯度或RX；
- 设计目标可以在后续数值输出中被证伪。

**常见误判**

- 源码出现参数名，就宣称它可在运行PAR中控制；
- 源码调用`GradP`，就宣称已经采到不同ky；
- 把逻辑`GradR/GradP/GradS`直接称为物理Gx/Gy/Gz；
- 把循环执行64次等同于采集64条不同k-space线；
- 自行发明厂家没有证据的trajectory或mask接口。

### 4.3 厂家头文件、`systemSel.hw`与P2F身份

它们不是序列业务文件，但决定P2F如何解释同一份`.src`。

**必须冻结**

- `P2F_x32.exe`完整路径、版本、大小、SHA；
- 所有实际参与的headers路径、大小、SHA；
- `systemSel.hw`路径、内容身份和SHA；
- P2F完整命令行、当前目录、环境变量和退出码；
- 源码、波表和输出目录的绝对路径。

**常见误判**

- 换了头文件或`systemSel.hw`，仍与旧基线直接比较；
- 只记录“用了P2F”，没有记录具体二进制和命令；
- 用旧型号、旧DLL或其他谱仪的规则解释当前Slim Pro结果。

## 5. P2F中间层

### 5.1 `.src.cpp`

**作用**

显示P2F展开后的高层控制逻辑，最适合核对循环、调用实参、分支、波表地址和参数消费者。

**必须检查**

- 源码主循环是否仍按预期存在；
- 目标参数是否仍出现在活跃条件或调用实参中；
- P2F是否插入、删除、复制或改写了事件；
- LUT地址、增益、duration和地址递增是否正确；
- RF、梯度、RX的调用拓扑是否与基线一致；
- 非目标参数和控制器块是否漂移。

**通过标准**

- 目标变量仍被目标调用活跃消费；
- 没有回退到旧立即数；
- 事件数量和相对顺序与`.src`设计一致；
- 波表地址不越界、不重叠且递增正确。

**常见误判**

- 只检查`.src`，不检查P2F展开；
- 在`.src.cpp`看到变量名，但不确认它是否位于活跃路径；
- 只搜索字符串，不核对具体消费者和控制流；
- 把`.src.cpp`存在误写成SeqSimu一定直接消费它。当前这一点仍是`UNKNOWN`。

### 5.2 `.src.s`

**作用**

显示仿真控制层的参数读取、比较、地址消费和控制器调度，是区分“运行时读取”与“编译时固化”的关键证据。

**必须检查**

- 目标字段是否从参数位置、RAM或表地址读取；
- 是否仍存在旧立即数；
- 循环比较、事件计数和地址递增使用哪个值；
- 每个PhaseGain pair或LUT的消费次数；
- 控制器触发、TimerCmp和结束条件；
- 目标变体之外的指令结构是否不变。

**运行时参数候选的最低判据**

```text
.src声明并消费
→ P2F PAR导出
→ .src.cpp保留活跃消费者
→ .src.s从参数/RAM读取而非旧立即数
```

这四项只证明“具备运行时读取结构”，还必须做冻结`.fcode`的PAR-only SeqSimu对照，才能升级为动态可调。

**常见误判**

- `.par`出现字段就认定运行时可调；
- `.src.s`仍比较立即数64，却宣称`noViews`已参数化；
- 只看一个字符串命中，不核对调用和循环位置；
- 把汇编层语义推断写成SeqSimu数值PASS。

### 5.3 P2F生成的编译基础`.par`

**作用**

记录本轮P2F导出的`common/global`参数、默认值、类型和部分源码/资源引用。它是“编译层参数权威”，不是完整运行包。

**必须检查**

- 目标字段是否唯一导出；
- 类型、默认值、数组长度和顺序；
- `:SRC`或等价源码身份；
- 波表、PhaseGain、TX/RX RAM和JSON运行资产是否存在；
- 与基线相比的新增、删除和漂移字段；
- 文件是否只是P2F基础PAR，还是已经合并过运行资产。

**通过标准**

- 目标字段唯一且类型/默认值正确；
- 非目标编译字段没有未解释变化；
- 缺失的运行资产被明确列为后续合并项；
- 不把基础PAR直接冒充完整`proj.par`。

**常见误判**

- P2F PAR出现参数，就宣称冻结`.fcode`下动态可调；
- 发现P2F PAR没有PhaseGain，便错误判断P2F失败；
- 把上一次运行PAR覆盖成P2F基础PAR，丢失波表和运行表；
- 把运行后的参数修改回写到P2F基础PAR，破坏编译基线。

### 5.4 `.fcode`

**作用**

当前主要用作控制器执行工件的身份锚点。

**必须检查**

- 文件存在、非空、路径、大小、时间和SHA；
- 是否由本轮冻结源码和工具链生成；
- PAR-only实验前后SHA是否保持不变；
- 不同场景是否意外加载了不同`.fcode`。

**可以据此判断**

- 编译工件身份是否一致；
- “固定编译工件、只改运行参数”的条件是否满足。

**不能据此判断**

- 不靠肉眼从二进制推导RF、梯度或轨迹；
- 当前没有证据证明SeqSimu是否直接消费`.fcode`，保持`UNKNOWN`；
- `.fcode`生成成功不等于SeqSimu、硬件或成像成功。

## 6. 运行输入层

### 6.1 当次运行`proj.par`

**作用**

它回答“这一次SeqSimu到底加载了哪套源码、参数、表和路径”。它必须与P2F基础PAR分开保存。

**必须检查**

- 以本轮P2F PAR为编译字段权威；
- 本次唯一参数值，例如`noViews`、RF参数、ADC点数和dwell；
- PhaseGain数组长度、完整pair顺序和活动前缀；
- `:SRC`、`.src.s`、RF/梯度波表和JSON资产的绝对路径；
- 所有路径存在且位于本次隔离根；
- 是否存在旧目录、同名旧文件或厂家库回退；
- 参数类型、重复字段、数组长度和未解释差异；
- 文件SHA是否与预注册值一致。

**通过标准**

- 所有输入能在同一隔离根内闭合；
- 目标字段与预注册完全一致；
- 没有根外依赖、旧路径回退、重复字段或类型漂移；
- 运行前后文件SHA不变，除非实验明确允许SeqSimu生成新的运行副本。

**常见误判**

- 文件选择框加载了旧目录中的同名`proj.par`；
- 窗口显示一个目标值，就忽略其他路径仍指向旧资产；
- 把PhaseGain置零但仍执行RX，误写成“跳采”；
- 运行不同场景时更换`.fcode`，却宣称固定工件参数化。

### 6.2 `.gwave/.rfwave`

**作用**

保存外部逐点波形。它们是数值输入，不是“已经被使用”的自动证明。

**必须检查**

- 文件格式、头部、点数和每点时间间隔；
- LUT数量、起始地址、结束地址和是否重叠；
- 每个点的原始值、符号、极值和闭合点；
- 持续时间、点数与源码调用参数的关系；
- 原始有符号积分、绝对积分和预期面积比；
- 波表号、gain、waveNo和`.src`消费者；
- `proj.par`中的实际绝对路径；
- 文件大小和SHA。

**通过标准**

- 静态格式与厂家手册一致；
- LUT地址不重叠、不越界；
- 数值、点数、极性、积分与预注册设计一致；
- `.src → .src.cpp/.src.s → proj.par`均能追到同一波表；
- SeqSimu原始通道逐点复现后，才可写“波表被实际消费”。

**常见误判**

- `.gwave`通过44/44静态检查，就写成P2F或SeqSimu PASS；
- 只看波形图，不保存点值和积分；
- 两个LUT地址重叠或边界点计算不一致；
- 不区分原始波表值、gain后的逻辑值和物理梯度单位；
- 根据`.rfwave`包络直接宣称翻转角或B1正确。

## 7. SeqSimu完成与通道数值层

### 7.1 `inf.log`

**作用**

回答仿真是否完整结束、是否有错误、发生了多少事件以及各事件的时间标签。

**必须检查**

- `tcc`和`mri_c`实际PID、完整路径和数字退出码；
- 是否存在且仅存在一次`SeqSimuEnd`；
- `ERROR`总数是否为0；
- Trigger、RF、GradS、GradR、GradP、RX的事件数；
- 每个事件的起止时间、标签和所属scan/view；
- 日志是否属于本次唯一Start；
- 是否存在崩溃、异常码、截断或旧日志拼接。

**通过标准**

- `tcc exit=0`、`mri_c exit=0`；
- `SeqSimuEnd=1`；
- `ERROR=0`；
- 事件数和时间与预注册设计一致。

**常见误判**

- GUI显示曲线就忽略`ERROR`；
- 只有`mri_c exit0`，但没有`SeqSimuEnd`；
- 日志来自多次Start，无法区分哪一轮；
- 程序异常退出却把部分输出当作完整仿真。

### 7.2 RF原始文件

优先文件：

```text
rfdatarealNo1.txt
rfdataimagNo1.txt
inf.log
```

**主要判断**

- RF事件数、每事件点数、起止时间；
- 复波形`z(t)=real(t)+i·imag(t)`；
- 幅度比例、相位旋转、相位斜率/频偏；
- 波形包络和时长；
- 目标事件外RF是否不变。

**典型计算**

- 相位0°→90°：检查`z90 ≈ i·z0`；
- 衰减变化：检查复幅比是否等于预注册理论值；
- 频偏变化：从展开相位斜率计算有效频偏；
- 时长变化：检查起点、终点、点数和共同区间样本。

**常见误判**

- 只看real或只看imag；
- 只看峰值，不看完整复波形；
- 将逻辑RF幅度写成真实B1或翻转角；
- 忽略闭合点与采样间隔导致的点数差一。

### 7.3 `gradsData.txt`

**作用**

检查逻辑GradS通道的逐点幅值、极性、时长、面积和与RF的相对时序。

**必须检查**

- 事件数、每叶点数和边界；
- 正负极性、峰值、平台、爬升/下降；
- 每叶有符号面积和总面积；
- 与RF起止时间的差；
- 幅度或极性实验中的逐点比例/反号；
- 非目标GradR/GradP/RX是否不变。

**常见误判**

- 把逻辑GradS直接叫物理Gz；
- 只看峰值，不看整叶比例和面积；
- 用公共Delay共同移动RF和GradS，却宣称改变了相对对齐；
- 忽略爬坡和补偿叶。

### 7.4 `gradrData.txt`

**作用**

检查逻辑GradR中的预聚相、主读出叶、匹配补偿、自定义LUT及其与RX的关系。

**必须检查**

- 每个叶片的分组、事件数、幅度、极性、时长和面积；
- 主读出叶与匹配补偿是否按预注册比例联动；
- 读出窗边界和RX窗的共同时间轴；
- 自定义`.gwave`时是否逐点复现目标LUT；
- 与GradP双轴实验的共同起止时间。

**常见误判**

- 把逻辑GradR直接叫物理Gx；
- 只比较一个峰值，忽略预聚相和补偿叶；
- ADC dwell变化导致读出持续时间变化，却误写成GradR幅度变化；
- 只证明RX门位于读出区，就宣称每个ADC点严格同步。

### 7.5 `gradpData.txt`

**作用**

检查逻辑GradP波形、PhaseGain pair消费、RX前累计有符号面积及相对逻辑ky顺序。

**必须检查**

- 每个view是否消费完整的两个PhaseGain值；
- 编码叶和回绕叶的事件边界、幅值、极性和面积；
- RX中心前累计有符号面积`M_RX`；
- `new_view → source_view → PhaseGain pair → M_RX → relative ky rank`映射；
- 交换、倒序、中心优先或mask候选中的集合、顺序和唯一性；
- 回绕闭合残差与SeqSimu文本量化容差。

**常见误判**

- GradP事件存在但全部为0，仍宣称不同ky；
- 只重排一个值而拆散完整pair；
- PhaseGain置0但RX仍运行，误称“少采”；
- 把非均匀相对逻辑ky写成标准等间距Cartesian；
- 未校准就换算成绝对`mm⁻¹`。

### 7.6 `rxdata1.txt`

**作用**

检查RX/ADC配置点数、dwell、门控窗口和与梯度共同时间区的关系。

**必须检查**

- RX事件数；
- 每事件有效点数、预丢弃点数和总配置点数；
- `samplePeriod`/dwell；
- RX起止时间和窗口长度；
- 首样点、末样点和闭合点约定；
- 每个采样点时间戳是否真实存在；
- RX点是否位于GradR/GradP共同有效区。

**结论边界**

- 有RX门起止、点数和dwell：最多证明“ADC配置/窗口同步”；
- 有每个ADC点时间戳或等价机器证据，并逐点对齐双轴梯度：才可证明“离线逐点严格同步”；
- 两者都不能证明真实硬件ADC已采样。

**常见误判**

- `8×20 µs`直接当成已证实的首末样点时刻；
- 把配置点数、有效点数和`preDiscard`混为一谈；
- RX事件数不变就认定采样内容不变；
- 用窗口覆盖代替逐采样点同步证据。

### 7.7 `GradMatSel.txt`

**作用**

提供逻辑GradS/GradR/GradP到梯度矩阵或方向选择的线索。

**必须检查**

- 基线与变体内容、事件数和时序是否一致；
- 每个事件对应的矩阵选择值；
- 是否存在未预注册的方向切换。

**常见误判**

- 仅凭名称或矩阵选择值就把逻辑GradR/GradP称为物理Gx/Gy；
- 未取得物理矩阵、轴符号和标定单位就推导绝对轨迹。

在厂家物理矩阵和标定未闭合前，所有结论继续使用“逻辑GradS/GradR/GradP”。

### 7.8 `shapenum.txt`

**作用**

检查各通道事件拓扑、事件块数、边界和分组。

**必须检查**

- 各通道事件数量；
- 每个事件的点数或边界索引；
- 与`inf.log`标签数量是否一致；
- 与RF/GradS/GradR/GradP/RX原始文件能否逐事件对齐；
- 变体是否只改变预期通道或预期事件数量。

**常见误判**

- 只比较文件总行数；
- 把事件数相同误写成事件内容相同；
- 不核对关闭点、边界点或空事件。

## 8. 身份、验收与安全文件

### 8.1 `manifest.json`

**作用**

证明源码、工具链、编译工件、运行输入、波表和结果属于同一次实验。

**至少记录**

```text
experiment_id / objective / baseline_id / variant_id
source path,size,sha256
source_diff path,sha256
P2F path,size,sha256,command,cwd
headers[] path,size,sha256
systemSel.hw path,size,sha256
build artifacts path,size,sha256
runtime proj.par path,size,sha256
waves/tables path,size,sha256,address,count,dwell
expected values and tolerances
process PIDs, paths, exit codes
result files path,size,sha256
non-target pre/post SHA
safety pre/post snapshots
```

**通过标准**

- 每个文件路径唯一并存在；
- 所有SHA在实验前后符合冻结规则；
- 不混用不同时间、不同根目录或不同工具链的文件；
- 能从任一结果反向追到唯一输入。

**常见误判**

- 从不同实验拼接SRC、PAR、波表和结果；
- 只记录文件名，不记录绝对路径和SHA；
- 运行结束后才补写预期值；
- 把历史证据复制到新目录就当成新实验结果。

### 8.2 `acceptance.json`

**作用**

把预注册条件、实测数值、容差、原始来源和裁决固定成机器可读记录。

每项至少包含：

```json
{
  "check_id": "example",
  "status": "PASS | FAIL | UNKNOWN | SKIP",
  "expected": null,
  "observed": null,
  "tolerance": null,
  "source_files": [],
  "calculation": "",
  "reason": ""
}
```

**必须区分**

- `automated_verdict`：文件、SHA、公式和阈值的机器检查结果；
- `scientific_verdict`：经主控复核后的能力结论，自动工具不得擅自从`PENDING_REVIEW`升级。

**常见误判**

- 只记录最终PASS，没有记录期望值、实测值和容差；
- 容差看完结果后临时放宽；
- 目标通道通过，但忽略非目标漂移；
- 将`UNKNOWN`强制归入`FAIL`或`PASS`。

### 8.3 `safety-final.json`

**作用**

证明本轮离线实验没有越过设备边界，并且后台现场已经清理。

**至少记录**

- 运行前后厂家相关进程PID、完整路径和启动时间；
- 本轮创建和终止的P2F、SeqSimu、tcc、mri_c PID；
- 专用设备网络断开与恢复状态；
- 设备地址可达性和连接数；
- 临时监测器、防火墙规则及其清理结果；
- RAW目录文件数、总字节和最新时间前后比较；
- 厂家日志文件数、大小和最新时间前后比较；
- 未触碰的ParSetup、ComSrvHost等既有环境进程；
- `cleanup_status`和任何残留项。

**通过标准**

- 只清理本轮PID和临时规则；
- 本轮P2F/SeqSimu/tcc/mri_c进程归零；
- 专用网络恢复到预注册状态；
- 设备连接为0；
- RAW与厂家运行日志无变化；
- 没有打开运行控制台或触发Run/Abort。

**常见误判**

- 功能数值正确就忽略后台残留；
- 按进程名批量结束，误伤厂家既有环境；
- 根据托盘图标数量判断真实进程数量；
- 未记录RAW和日志基线，事后无法证明没有副作用。

## 9. 如何形成一条完整因果链

### 9.1 编译时固定量

若目标量只能通过修改`.src`并重新P2F改变，应形成：

```text
唯一源码diff
→ .src中唯一消费者
→ .src.cpp/.src.s中出现对应新值或结构
→ P2F四工件全部新生成且身份闭合
→ 当次proj.par加载新工件
→ SeqSimu目标数值按预期变化
→ 非目标通道不变
```

结论只能写：

> 该控制量可通过修改`.src`并重新P2F，在离线SeqSimu中改变目标逻辑通道。

### 9.2 冻结编译工件下的运行时参数

若目标是证明PAR-only控制，应形成：

```text
.src声明并活跃消费变量
→ P2F PAR唯一导出字段
→ .src.cpp/.src.s从参数位置读取
→ .src/.src.cpp/.src.s/.fcode SHA全部冻结
→ 只改当次proj.par中的一个字段或完整表pair
→ SeqSimu目标数值按公式变化
→ 非目标文件SHA或数值不变
```

任何一个冻结工件SHA变化，都不能称为同一固定`.fcode`的参数化控制。

### 9.3 外部波表控制

```text
.src import和目标控制器消费波表
→ .src.cpp/.src.s保留波表号、地址、duration和gain
→ proj.par加载唯一波表路径
→ 波表静态点值、地址和积分预注册
→ SeqSimu目标通道逐点复现
→ 重新积分得到预期面积或面积比
```

只有静态`.gwave`正确，没有SeqSimu逐点复现，最多写`STATIC READY`。

### 9.4 ADC与梯度同步

```text
RxChannelAcquirePara点数/dwell + RxStart窗口设计
→ .src.cpp/.src.s保留RX参数与触发结构
→ proj.par加载正确samplePoint/samplePeriod
→ rxdata1证明窗口/点数/dwell
→ gradr/gradp证明共同有效区
→ 有逐点时间戳时逐点配对
```

无逐点时间戳时，结论必须停在“ADC窗口同步”。

## 10. 两个当前专项的判读重点

### 10.1 Generic Cartesian

必须同时证明：

1. `.src`只有`loopnum < noViews`拥有事件次数；
2. `.src.cpp/.src.s`读取`noViews`而不是立即数64；
3. Full64、63、R2-shaped、R4-shaped使用同一`.fcode` SHA；
4. 运行PAR使用完整PhaseGain pair活动前缀；
5. Trigger、RX、GradP事件数等于`noViews`；
6. 实测相对ky集合和顺序逐项等于manifest；
7. 未选择的source view不产生RX。

当前G14的`M_RX`间距非均匀，因此通过后也只能称“按source-view索引选择相对逻辑ky”，不能称标准等间距Cartesian或真实R2/R4加速。

### 10.2 双轴`.gwave`+ADC

必须同时证明：

1. 冻结`.src/.gwave`身份与44/44静态检查；
2. P2F四项exit 0且四工件语义正确；
3. GradR和GradP分别消费地址0与160的160点LUT；
4. `gradrData/gradpData`逐点复现各自LUT；
5. 重新计算的有符号面积比为4:3，容差来自文本量化误差；
6. RX为1窗、8点、20 µs dwell；
7. RX窗口位于两轴共同有效区；
8. 只有存在逐ADC点时间证据时，才升级为逐点同步。

该实验通过后仍只证明逻辑GradR/GradP离线LUT输出，不证明物理Gx/Gy、绝对单位、Gmax、slew、Spiral或实机安全。

## 11. 日常快速审查：五组材料

没有时间逐项展开时，最低限度依次检查：

1. `manifest.json`：文件是否来自同一次实验；
2. 源码diff：是否只有一个目标变化；
3. `.src.cpp + .src.s`：P2F是否按预期理解，参数是否被固化；
4. `proj.par + .rfwave/.gwave`：当次到底加载了什么；
5. `inf.log + acceptance.json + 目标通道原始数值`：是否零错误、目标符合公式、非目标不变。

一句话判读标准：

> `.src`表达设计，`.src.cpp/.src.s`证明P2F如何理解，`.par/波表`证明当次真正加载的输入，SeqSimu原始数值证明实际执行结果，manifest证明所有材料属于同一次实验。

## 12. 参考资料

- 当前安全边界：`../../00-authority/SCOPE_AND_SAFETY.md`。
- 当前证据状态：`../../00-authority/CURRENT_EVIDENCE.md`。
- 当前验收标准：`../../00-authority/ACCEPTANCE.md`。
- 本证据集索引：`../DOC_INDEX.md`。
