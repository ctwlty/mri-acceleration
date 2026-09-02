# 范围与安全边界（当前包最高优先级）

## 允许的工具面

- `inspect_toolchain`：读取已配置文件和只读系统基线，不加载 DLL。
- `compile_sequence`：在新 run 目录内运行固定 P2F 命令和三类 dump。
- `stage_simulation_input`：复制并哈希同一批次的 SRC、PAR、FCODE、S/CPP 与波表依赖。
- `probe_simulation_backend`：只报告 V1 硬阻塞状态，不执行 DLL。
- `simulate_sequence`：V1 仅核对冻结输入 ID/SHA，随后必定返回安全阻塞；本包没有可启动的动态后端。
- `analyze_simulation`、`read_channel_slice`、`compare_simulations`：只读分析。
- `compute_logical_gradient_moment`：只返回相对逻辑积分，不落盘。

Codex 自己在白名单项目目录中编辑 `.src/.gwave/.par`；MCP 不提供 `edit_sequence`，也不提供任意命令执行。

## 永久禁止进入本 MCP 的能力

- 设备连接、初始化、参数写入、频率/匀场/梯度控制、Run、PrepareRun、扫描 Abort、CloseSys。
- 调用或暴露 `Init`、`ConfigFile`、`SetParameterFile`、`SetFcodeFilePath`、`Run`、`PrepareRun`、`Abort` 等厂家实机接口。
- 启动完整 SpectrometerIDE 或用 UI automation 点击 Start。
- 读取、复制或生成真实 RAW；修改厂家安装目录。
- 修改 NIC、IP、路由、DNS、VPN 或 Windows 防火墙。
- 任意 DLL 路径、任意导出函数名、任意 shell、任意进程终止。

这些不是“首次版本暂不做”，而是研发离线 MCP 的架构边界。实机采集以后通过另一个明确授权、可审计的适配层接入项目 A/B。

## 为什么 V1 动态仿真硬阻塞

项目已有证据表明：

- SeqSimu 构造路径可能访问 NMRDLL 的系统频率、型号和 Gmax 等接口。
- SpectrometerIDE 曾在专用网卡断开后仍经其他路由连接谱仪服务地址；公开证据不记录具体设备地址。
- v2.26 手册记录过 `Simulate(...)`，但当前安装 DLL 的导出、ABI、依赖和 Socket 数据协议尚未在目标机闭合。

因此“厂家手册说仿真无需连接谱仪”不能代替当前安装版本的安全验收。只检查已有 TCP 连接也不能证明 DLL 启动后不会尝试连接。

## 静态检查规则

- 检查 PE 位数/导出只能读文件字节，或使用 `dumpbin /exports`、`llvm-readobj --coff-exports`、本包 `pe_probe.py`。
- 禁止用 `LoadLibrary`、`ctypes.CDLL`、P/Invoke 或运行目标程序来“检查导出”。加载 DLL 可能执行初始化代码。
- 不允许以包内历史哈希替代目标机重新计算。

## 若以后获准连接 simbridge

- MCP 永不直接加载厂家 DLL；桥接器必须是独立、位数匹配的固定可执行文件。
- 只允许固定白名单：`Simulate`、`SimulatePause`、`SimulateResume`、`SimulateAbort`；其中 Abort 仅能取消本次离线仿真 worker，不能映射到扫描 Abort。
- `Simulate` 只消费已冻结的 simulation input ID，不能接受任意项目路径。
- Socket 只绑定 `127.0.0.1`/`::1`，禁止 `0.0.0.0` 和局域网地址。
- 一次只运行一轮；禁止自动重试；超时只终止本轮已记录 PID/进程树。
- 禁止 `taskkill /IM`、按进程名扫杀或终止不属于本轮的进程。
- 动态前后必须保存连接、进程、安装目录和 RAW 目录差异证据；出现外部连接迹象立即判失败，并保持后端阻塞。

## 数据声明

仿真输出只能声明为：

```text
coordinate_space = logical_GradR_GradP
gradient_units = relative_or_unknown
physical_axis_mapping = unknown
physical_calibration = false
adc_pointwise_timestamps = unknown_or_verified
```

在轴映射、梯度单位、比例、延迟和 ADC 逐点时间戳均被实证前，禁止使用“物理 Gx/Gy”“绝对 k-space”“可重建真实采样轨迹已闭合”等表述。
