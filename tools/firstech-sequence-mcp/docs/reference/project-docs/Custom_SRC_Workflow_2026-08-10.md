> **Historical method reference.** Current safety and acceptance rules in `../../authority/` govern. Dated actions are context only and do not authorize execution.

---
title: 菲特 Slim Pro 自定义 SRC 制作与验证中间过程指南
date: 2026-08-10
updated: 2026-08-10
object: firstech-mri-sequence-source-workflow
scope: 纯离线源码、P2F与SeqSimu
status: working-guide
authority: 当前仓库安全/验收文档与同批次原始证据
---

# 菲特 Slim Pro 自定义 SRC 制作与验证中间过程指南

## 1. 先回答：以后是不是只关注制作`.src`？

**主线是制作`.src`，但不能只交一个`.src`。**

`.src`决定事件结构、循环、控制器分工和参数如何被消费，是序列设计的核心；但没有同批编译工件、波表和SeqSimu数值证据，就不能证明它可用。

以后每条序列的最小交付单位应是：

```text
sequence-package/
├─ source/
│  ├─ sequence.src
│  └─ wave/               # .rfwave / .gwave
├─ toolchain-manifest/     # P2F、headers、systemSel.hw身份
├─ build/
│  ├─ sequence.fcode
│  ├─ sequence.par
│  ├─ sequence.src.s
│  └─ sequence.src.cpp
├─ simulation/
│  ├─ inf.log
│  ├─ RF real/imag
│  ├─ GradS/GradR/GradP
│  ├─ RX/GradMatSel/shapenum
│  └─ acceptance.json
└─ manifest.json           # 路径、大小、SHA、参数、结论边界
```

## 2. 制作`.src`前先固定什么

### 2.1 固定设备与编译环境

- 谱仪：Slim Pro，`systemSel:3`；
- P2F版本与实际`P2F_x32.exe`路径；
- `include_slimScope`中的实际头文件；
- `systemSel.hw`；
- 源码和波表的绝对路径；
- 一套已经0 ERROR的厂家/项目基线。

以上任何一项改变，都必须记录成新的工具链身份，不能与旧结果混为同一基线。

### 2.2 每次只定义一个科学目标

例如：

- 只验证RF相位；
- 只验证逻辑GradS一片叶的幅度；
- 只验证`noViews`是否控制事件数；
- 只验证一段逻辑GradR/GradP LUT与ADC窗。

不要一次同时改RF、梯度、ADC和循环，否则无法判断结果来自哪一处。

## 3. 厂家`.src`的基本结构

以下是结构关系，不是可直接编译的最终源码：

```text
import TX / GRAD 波表
include common.h / grad.h / tx.h / rx.h / mainctrl.h

协议参数（可能导出到PAR）
实现常量（波表编号、地址、固定拓扑）

void main() {
    main控制：触发次数、TR、总循环
    gradS：逻辑Slice控制块
    gradR：逻辑Read控制块
    gradP：逻辑Phase控制块
    tx1：RF控制块
    rx1：ADC/RX控制块
}
```

优先从已由当前Slim Pro P2F编译过的厂家源码复制骨架和语法，不凭记忆重新发明控制块、标签、结束方式或函数签名。

## 4. 哪些通常属于编译时，哪些可能运行时可调

### 4.1 编译时控制

通常需要修改`.src`并重新P2F：

- 控制器块是否存在；
- RF、梯度、RX事件的调用顺序；
- 主循环、分支、goto和地址递增；
- 新增参数声明；
- `import`的波表类型和拓扑；
- 字面量时序与没有导出到PAR的变量；
- 每个view消费多少个PhaseGain值。

### 4.2 运行时候选

全局/common变量或表被P2F导出到PAR后，可能在冻结编译工件下改变：

- RF相位表、衰减、频偏、时长；
- PhaseGain完整pair的数值和顺序；
- ADC点数、dwell；
- 某些梯度增益；
- 新Generic G14中的`noViews`。

但是“PAR里能看到字段”不等于运行时可调。必须建立：

```text
.src变量/表
→ .src.cpp展开
→ .src.s参数读取
→ .par字段
→ SeqSimu输出发生唯一预期变化
```

目前已经动态证明的是G14的RF相位、衰减、频偏和PhaseGain pair置换，以及CSJ_SE_PRE的ADC点数/接收窗。Generic G14的`noViews`只通过P2F静态门，动态控制仍UNKNOWN。

## 5. 标准制作流程

### 第一步：选基线

只选择满足以下条件的源码：

- 身份和SHA固定；
- 依赖闭合；
- 当前Slim Pro P2F可编；
- 有0 ERROR的SeqSimu基线；
- 目标通道有非零数值输出。

不要用报错序列直接作为新功能基线；它可以提供线索，但不能提供干净因果。

### 第二步：做唯一源码差异

- 一次只改一个变量、一个调用或一个循环条件；
- 保存逐行diff；
- 不改厂家安装目录和历史原件；
- 在独立目录中制作候选。

### 第三步：执行P2F四项门

同一源码、头文件、`systemSel.hw`和P2F身份下生成：

1. `.fcode`；
2. `.par`；
3. `.src.s`；
4. `.src.cpp`。

四项都必须exit0、非空并记录SHA。不能只看到文件存在就算通过，还要核对：

- PAR是否唯一导出目标字段；
- CPP是否保留预期控制结构；
- S是否从参数读取，而不是把值常量化；
- 事件拓扑和地址递增是否没有意外变化。

### 第四步：构造运行PAR

P2F基础PAR与当次SeqSimu运行PAR要分开：

- 以本轮P2F PAR为编译字段权威；
- 只合并明确需要的PhaseGain、TX/RX RAM、波表和JSON运行资产；
- 所有绝对路径必须存在并指向本轮隔离根；
- 禁止回退到旧工程中的同名工件。

### 第五步：SeqSimu单变量验证

必须同时满足：

- `tcc/mri_c exit0`；
- `SeqSimuEnd`；
- `inf.log ERROR=0`；
- 目标通道数值按预注册公式变化；
- 非目标通道、事件数和时序保持；
- 原始输出与输入工件可由manifest唯一绑定。

### 第六步：写结论边界

SeqSimu通过后只写“逻辑通道的离线控制通过”。没有实机证据时，不写：

- 物理Gx/Gy/Gz；
- 真实B1/翻转角；
- 真实ADC；
- 真实k-space；
- 成像或加速成功。

## 6. 各通道制作时看什么

### 6.1 RF / tx1

- 波形选择：`TxChannelShapeSel`与`.rfwave`；
- 幅度：`TxAttReg/Ram`；
- 相位：`TxPhaseOffsetReg/Ram`；
- 频偏：`TxFreqOffsetReg/Ram`；
- 时长和门控：`TxStart`。

验收用RF real/imag复数数据，不只看GUI截图。

### 6.2 逻辑GradS / GradR / GradP

- 梯形/计算波形：`CreateGradCalcGainReg/List`；
- LUT波形：`CreateGradListGainReg/List`；
- 增益与表地址：`GradScaleReg/Ram`、gain、waveNo；
- 逻辑轴到物理轴：`GradMatSel`。

没有物理矩阵和标定前，只称逻辑GradS/GradR/GradP。

### 6.3 ADC / rx1

- 点数与dwell：`RxChannelAcquirePara(samplePeriod, samplePoint)`；
- 接收门：`RxStart`；
- 起点：`WaitTrigger + TimerCmp`及rx专属偏移；
- 与梯度同步：必须在同一时间轴核对RX采样时刻和GradR/GradP值。

只有门的起止时间、没有逐采样点时间戳时，只能证明“接收窗同步”，不能宣称“ADC逐点严格同步”。

## 7. 两个优先源码模板

### 7.1 Generic Cartesian GRE

目标：一次P2F后固定工件，由运行参数决定Cartesian采样。

推荐职责：

- `noViews`：唯一控制主循环次数；
- PhaseGain完整pair：控制每个view的相对ky和回绕；
- 外部builder：把`ky_order/mask`转换成前`2×noViews`个有效PhaseGain值；
- RF与ADC参数：控制激发和每条读出。

验收顺序固定为：

```text
Full64等价
→ 63条（尾部少1）
→ 指定单缺线
→ R2-shaped（32条）
→ R4-shaped（16条）
```

前一级失败，不进入下一级。PhaseGain置0但仍产生RX，不算跳采。

### 7.2 双轴`.gwave`+ADC诊断序列

目标：证明逻辑GradR(t)、GradP(t)能按逐点LUT输出，ADC在共同有效时间区采样。

最小结构：

- 一个`.gwave`包含两个互不重叠LUT；
- GradR和GradP分别消费自己的LUT；
- 两轴同一Trigger、同一起点、相同点数；
- RX只配置一窗、固定点数与dwell；
- 不为“让它编译”添加虚假RF。

当前候选只完成静态44/44检查；P2F结果在中断后UNKNOWN。恢复时先核对原始结果，不从头重做。

## 8. 通用验收与STOP规则

### PASS必须包含

- 输入身份唯一；
- 唯一业务diff；
- P2F四工件完整；
- SeqSimu 0 ERROR；
- 目标数值符合公式；
- 非目标数值和拓扑不变；
- 进程清理、网络恢复、RAW与厂家日志不变。

### 立即STOP

- 源码、头文件、systemSel或波表身份不一致；
- 目标字段出现多次或有隐藏消费者；
- CPP/S显示目标值已常量化却仍按PAR-only试验；
- P2F任何一项失败；
- SeqSimu崩溃、非0、无`SeqSimuEnd`或有ERROR；
- 目标之外的通道发生未解释变化；
- 需要猜物理轴、单位、ADC首样点或轨迹坐标；
- 出现设备连接、运行控制台、Run/Abort或RAW迹象。

STOP后先读证据，不自动换参数反复尝试。

## 9. 建议的后续顺序

1. 恢复双轴候选P2F门证据；
2. 收口实验23–27，补齐RF时长、逻辑GradS/GradR、ADC dwell；
3. 执行Generic Cartesian Full64→63→R2/R4动态门；
4. 双轴`.gwave`通过P2F后，再做一次SeqSimu逐点验证；
5. 只有双轴波形和ADC同步门通过，才开始Spiral trajectory generator；
6. 真实设备、RAW和成像另开实机阶段，重新授权。

## 10. 每次新`.src`的完成清单

- [ ] 目标只包含一个可证伪能力；
- [ ] 基线源码、头文件、P2F、systemSel、波表SHA已冻结；
- [ ] 唯一源码diff已保存；
- [ ] `.fcode/.par/.src.s/.src.cpp`四项生成并核对；
- [ ] 编译时与运行时参数边界已写明；
- [ ] 运行PAR没有旧路径回退；
- [ ] SeqSimu原始数值通过预注册公式；
- [ ] 非目标通道不变；
- [ ] 结论只写到证据支持的层级；
- [ ] manifest、原始输出和清理记录已保存。

完成这十项，才可以把该`.src`称为“可复现的离线序列模板”。

## 11. 中间过程文件判读矩阵（后续必查）

以后判断一个序列是否成立，必须按下列顺序逐层核对：

```text
源码意图
→ P2F编译展开
→ 当次运行参数与波表
→ SeqSimu原始数值
→ 文件身份与验收结论
```

不能跳层。例如，`.par`中出现一个参数，只能证明它被导出，不能单独证明它真正控制了波形。

### 11.1 核心文件与判断内容

| 文件或记录 | 主要判断内容 | 通过信号 | 常见误判 |
|---|---|---|---|
| `sequence.src` | 控制块、主循环、事件顺序、参数声明、波表调用及地址递增 | 与设计目标一致，本轮只有预定改动 | 只要源码中有变量或API，就宣称已掌握控制 |
| 源码`diff` | 本轮到底改了什么 | 只改一个能力或一个可证伪因子 | 同时改RF、梯度、ADC或循环，导致结果无法归因 |
| `headers` / `systemSel.hw` / P2F身份 | 谱仪型号、头文件版本、编译器版本及参数 | 路径、大小、SHA与冻结基线一致 | 换了头文件或`systemSel.hw`仍与旧基线混用 |
| `.rfwave` / `.gwave` | 波形点、点数、周期、LUT地址、极性、积分面积及引用路径 | 地址不重叠，点数/数值/面积与设计一致，SHA固定 | 格式静态正确，就宣称P2F或SeqSimu已经消费该波表 |
| P2F生成`.par` | 哪些`common/global`变量真正导出，类型和默认值是否正确 | 目标字段唯一、类型/默认值正确、其他字段未漂移 | 参数出现在PAR就当作冻结`.fcode`下必然可调 |
| `.src.cpp` | P2F展开后的高层逻辑、循环条件、调用实参、波表地址 | 预期参数仍被活跃调用消费，事件拓扑未意外变化 | 只看`.src`，不查P2F是否改写或常量化了语义 |
| `.src.s` | 仿真控制层如何读参数、比较循环、消费地址和调用各控制器 | 目标参数从参数位置读取，不是旧的立即数；地址/计数结构正确 | `noViews`等已被固化仍宣称可只改PAR |
| `.fcode` | 执行工件身份，是否本轮新生成，参数化对照时是否冻结 | 非空、路径和SHA记录完整；PAR-only对照时SHA不变 | 试图靠肉眼从`.fcode`推导轨迹，或每个运行方案都换`.fcode`却称为参数化 |
| SeqSimu当次`proj.par` | 本次实际加载哪份`.src/.src.s`、波表、PhaseGain和其他运行资产 | 所有绝对路径唯一、存在且指向本轮隔离根，参数值与预注册相同 | 打开了旧的同名PAR或回退到厂家序列库 |
| `inf.log` | 仿真是否真正完成、ERROR数、Trigger/事件时间和标签 | 进程退出0、存在`SeqSimuEnd`、`ERROR=0`，事件数/时间符合预期 | GUI显示了曲线就忽略日志中的ERROR或不完整结束 |
| RF/GradS/GradR/GradP/RX原始数值 | 幅度、相位、极性、时间、点数、面积和事件数是否真正按公式改变 | 目标通道按预注册公式变化，非目标通道逐字节或逐数值不变 | 只看GUI“形状差不多”，不核对原始数值和积分 |
| `GradMatSel.txt` | 逻辑GradS/GradR/GradP到方向矩阵的选择线索 | 与基线相同；结论仍按逻辑轴表述 | 未做物理矩阵和标定，就把GradR/GradP直接称为物理Gx/Gy |
| `shapenum.txt` | 各通道事件拓扑、块数和边界 | 事件数与`inf.log`和各原始数值文件一致 | 只比较总行数，没有确认每个通道的事件分组 |
| `manifest.json` | 本次所有输入/输出的路径、大小、SHA、工具链和参数身份 | 能唯一追到同一次源码、编译、运行参数和仿真结果 | 将不同批次的SRC、PAR、波表和结果拼成一条“成功链” |
| `acceptance.json` | 事件数、幅值比、时间差、积分、目标/非目标差分和PASS/FAIL | 所有预注册数值条件同时通过，并能回指原始文件 | 只看P2F/SeqSimu进程`exit0`，不验证科学数值 |
| `safety-final.json` / 清场记录 | 实验进程、网络隔离/恢复、RAW与厂家日志是否变化 | 本轮进程和临时规则归零，网络恢复，RAW/厂家日志不变 | 功能数值正确就忽略后台残留和设备副作用 |

### 11.2 通道与SeqSimu原始文件对照

| 要判断的能力 | 优先查看文件 | 主要数值 |
|---|---|---|
| RF幅度/相位/频偏/时长 | `rfdatarealNo1.txt` + `rfdataimagNo1.txt` + `inf.log` | 复幅、相位旋转、相位斜率、点数、起止时间 |
| 逻辑GradS | `gradsData.txt` + `inf.log` + `GradMatSel.txt` | 逐点幅值、极性、平台/爬坡时长、面积、与RF的相对时间 |
| 逻辑GradR | `gradrData.txt` + `rxdata1.txt` + `inf.log` | 预聚相/读出叶幅值、面积、持续时间、与RX窗的对齐 |
| 逻辑GradP / 相对ky | `gradpData.txt` + `PhaseGain` + `rxdata1.txt` + `inf.log` | RX中心前累计有符号面积、pair消费、事件数、相对ky集合/顺序 |
| ADC点数/dwell/窗口 | `rxdata1.txt` + `inf.log` + 对应`gradrData.txt/gradpData.txt` | `samplePoint`、`samplePeriod`、RX起止、首样点约定、与梯度有效区间的关系 |
| 事件数与拓扑 | `shapenum.txt` + `inf.log` + 各通道原始文件 | Trigger/RF/GradS/GradR/GradP/RX数是否同步变化 |

### 11.3 Generic Cartesian专项判断

Generic Cartesian只在以下五项同时成立时才算参数化通过：

1. `.src`中事件数只有一个所有者：`loopnum < noViews`；
2. `.src.cpp/.src.s`证明主循环读取`noViews`，没有回到立即数`64`；
3. 运行PAR中`noViews=N`，前`2×N`个PhaseGain值是完整pair，未消费尾部不产生RX；
4. Full64、63、R2-shaped和R4-shaped使用同一份`.fcode` SHA；
5. SeqSimu中Trigger、RX和GradP事件数都等于`N`，实际相对ky集合/顺序与manifest逐项一致。

下列情况不算跳采或R2/R4：

- 只把PhaseGain设为0，但RX仍执行64次；
- 只更换`.fcode`实现不同顺序，却宣称是同一固定工件的参数化；
- 只对比PAR文字，没有对比RX/GradP事件数和相对ky数值。

### 11.4 双轴`.gwave`+ADC专项判断

该能力只在以下证据闭合后才能放行：

1. `.src`明确`import GRAD`，GradR与GradP分别消费两个不同LUT；
2. `.gwave`中两个LUT地址不重叠，点数、值、积分和SHA与设计一致；
3. `.src.cpp/.src.s`证明LUT地址、duration、gain与RX参数未被意外替换；
4. `gradrData.txt/gradpData.txt`逐点复现两条LUT，共同起止时间正确，重新积分的面积比与设计相同；
5. `rxdata1.txt`证明ADC点数、dwell和窗口位置正确，采样区间位于两轴共同有效区内；
6. 只有得到每个ADC点时刻或等价机器证据，才能称“逐点严格同步”。如果只有RX门起止，结论只能到“ADC窗口同步”。

44/44静态检查只能证明候选文件内部自洽，不等于P2F PASS，更不等于SeqSimu数值能力PASS。

### 11.5 日常快速检查：只看五组材料

如果不需要深入每个细节，至少查看：

1. `manifest.json`：确认SRC、工具链、PAR、波表和结果属于同一批；
2. 源码`diff`：确认只改了一个目标；
3. `.src.cpp + .src.s`：确认P2F正确理解源码，运行参数没有被意外固化；
4. 当次`proj.par + .rfwave/.gwave`：确认真正加载了什么；
5. `inf.log + acceptance.json + 目标通道原始数值`：确认仿真零错误、目标数值符合公式、非目标未漂移。

一句话标准：

> `.src`表达了设计，`.src.cpp/.src.s`证明P2F正确理解，`.par/波表`证明当次真正加载的输入，SeqSimu原始数值证明它确实这样执行，manifest证明所有证据属于同一次实验。
