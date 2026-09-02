# 分层验收与停止条件

本文记录验收标准，不包含或授权分发厂家二进制、受限手册或可部署运行时。

## A. 包身份

- 对不可变发布包生成清单后，在模块根运行 `py -3.11 .\scripts\verify_package.py .\package-manifest.json .` 返回 PASS。
- 文件集合、大小和 SHA-256 全部一致；无额外 DLL/EXE/LIB/PDB/RAW/旧 SDK。
- ZIP 外部 SHA-256 与发送方提供值一致。

失败即停止，不运行任何厂家程序。

## B. 纯软件与负向测试

- Python 依赖安装成功；`pytest`、`ruff check`、`python -m compileall` 全部通过。
- 路径越界、UNC、symlink/junction、厂家安装目录写入、重复 run ID 均被拒绝。
- 假 P2F 的非零退出、缺工件、超时被结构化记录并判失败。
- 解析器能识别唯一 `SeqSimuEnd`、真实 ERROR、缺失通道、非数字行和切片上限。
- V1 仿真后端硬 `blocked`；即使有人填写 bridge/gate 路径，`simulate_sequence` 也不得启动子进程。

## C. Windows 目标机静态核验

- 只读探针生成 P2F、当前 DLL、SeqSimu/tcc/mri_c（如存在）、include、systemSel 的实际路径、版本、大小和 SHA。
- 当前 DLL 的 PE 位数和 exports 通过静态字节解析获得；全程没有 `LoadLibrary`、`ctypes.CDLL` 或厂家进程启动。
- 保存进程、网卡、目标 IP 路由和 TCP 基线；不得修改系统网络设置。
- 如果当前 DLL/ABI/Socket 协议任一不明，结论明确为 blocker，不猜参数。

## D. 隔离 P2F 编译

- 使用白名单项目中已知序列的副本；输出只在新的 run 目录。
- compile 和三个 dump 均 exit 0，且实际生成的 FCODE、PAR、ASM、CPP 四件工件齐全并有 SHA。
- 编译源、include、systemSel、P2F SHA/PE（以及可得的文件版本）身份均可追溯；run manifest 至少记录源文件、快照和 P2F SHA。
- 原始项目文件和厂家安装目录前后不变，SpectrometerIDE 启动次数为 0。
- A–C 和直接 P2F 预检通过后配置项目 MCP，信任项目并重启/新建 Codex 任务；`/mcp` 或 `codex mcp list` 可见 9 个白名单工具。
- 通过 MCP Inspector/Codex 调用的结果与直接 CLI 结果一致。

A–D 全通过后，才可写：`Windows MCP compile path = TARGET-ENV-VERIFIED`。

## E. 离线仿真（单独门控）

本层不随本包自动放行。至少满足：

> 架构 blocker：在线 Codex 主机通常存在到目标 IP 的默认路由，而严格 `host-no-route` 门会因此阻塞。不得削弱此门来“跑通”。应先由用户/管理员在包外预置并验证可信的目标-IP 出站隔离，或使用隔离 worker/VM；本 MCP 不修改网络。该方案未闭合时，E 保持 BLOCKED。

- 当前 DLL/桥接器位数、ABI、固定依赖目录和本地 Socket 协议已实证。
- bridge 使用 Windows Job Object（`KILL_ON_JOB_CLOSE`）包含本轮全部子进程；进程组或按名称清理不能代替。
- 编译 ID 与编译 manifest SHA 成对保留并在 stage 前核对；运行输入来自 `stage_simulation_input` 返回的冻结 ID + stage manifest SHA，SRC/PAR/FCODE/S/CPP/波表属于同一工作区并全部有 SHA。
- 无头 CLI bridge 独立通过后，再接 MCP；MCP 本体不加载 DLL。
- bridge 回传并核对实际加载 DLL 的规范路径/SHA、vendor return、全部 compiler exit 和 child exit 状态。
- 已知人工零错误基线只运行一次，无自动重试；tcc/mri_c 退出 0（如适用）、唯一 `SeqSimuEnd=1`、`ERROR=0`、预期通道与点数齐全。
- 运行前后无新增谱仪连接、无 RAW、无厂家安装目录/日志异常变化；只终止本轮子进程。
- MCP 摘要和文件哈希与 CLI bridge 一致。

任一失败：保留证据、停止后端、不得转而用 IDE/UI automation 绕过。

## 数据层验收

每次分析结果必须带：输入 run ID、文件 SHA、解析规则、点数、单位和 UNKNOWN 项。逻辑积分字段必须写明 `coordinate_space=logical_GradR_GradP`、`gradient_units=relative_or_unknown`、`physical_calibration=false`。在真实采集和校准完成前，不得称“真实可重建轨迹”。
