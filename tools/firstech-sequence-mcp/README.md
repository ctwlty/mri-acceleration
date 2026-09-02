# Firstech 序列 MCP 实现指南

本模块包含可公开发布的离线 Firstech 序列 MCP 实现、测试、辅助脚本和运行约束。不包含厂家二进制文件、受限手册、设备凭据或机器专用配置。

## 架构

```text
Codex
  └─ STDIO MCP 服务（本模块；不加载厂家 DLL）
       ├─ 静态检查
       ├─ P2F 适配器 → 独立编译运行目录 + 清单
       ├─ 仿真输入冻结器 → 不可变的暂存输入
       ├─ 预留仿真工具（V1 强制阻断；不启动动态进程）
       └─ 纯解析器 → 摘要 / 有界切片 / 逻辑梯度矩
```

Codex 直接编辑配置项 `source_roots` 所允许目录内的序列文件。本 MCP 不提供任意编辑器、Shell、DLL 加载器、设备控制或 RAW 数据处理工具。

## 在 Windows 上安装

推荐使用 `uv`：

```powershell
Set-Location .\tools\firstech-sequence-mcp
uv sync --dev --frozen --no-editable
$env:FIRSTECH_MCP_CONFIG = "<private-config-path>"
.\.venv\Scripts\pytest.exe
.\.venv\Scripts\ruff.exe check .
.\.venv\Scripts\python.exe -m compileall -q src scripts
```

也可以使用标准 Python 3.11 或更高版本：

```powershell
py -3.11 -m venv .venv
.\.venv\Scripts\python.exe -m pip install .
.\.venv\Scripts\python.exe -m pip install "pytest>=8.4,<9" "ruff>=0.12,<1"
.\.venv\Scripts\python.exe -m pytest
.\.venv\Scripts\ruff.exe check .
.\.venv\Scripts\python.exe -m compileall -q src scripts
```

不要将本模块安装到厂家 Python 或运行时环境中，也不要把文件复制到 `<SpectrometerIDE-install-root>`。

## 验证发布包

如果发布压缩包包含生成的 `package-manifest.json`，请在安装依赖前，从解压后的模块根目录执行验证：

```powershell
py -3.11 .\scripts\verify_package.py .\package-manifest.json .
```

仓库不跟踪发布清单，因为清单中的哈希值会随每次源码修订而变化。对不可变的发布压缩包，可使用 `scripts/build_manifest.py` 生成清单；为避免自引用哈希循环，清单不会收录自身。

## 只读目标环境探测

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\probe_toolchain.ps1 `
  -ConfigPath "<private-config-path>" `
  -OutputPath ".\evidence\toolchain-probe.json"
```

探测脚本通过 `pe_probe.py` 读取 PE 字节，不调用 `LoadLibrary`。启用 P2F 调用前应先检查生成的 JSON。该探测不会启用仿真器。

## 手动启动 MCP

```powershell
$env:FIRSTECH_MCP_CONFIG = "<private-config-path>"
.\.venv\Scripts\python.exe -m firstech_sequence_mcp.server
```

STDOUT 仅用于 MCP 协议帧。厂家子进程输出会被捕获到运行文件中，诊断信息写入 STDERR。

## 推荐工具调用顺序

1. `inspect_toolchain()`
2. Codex 在允许的源码根目录下编辑序列。
3. `compile_sequence(source_path)`
4. 保留编译运行 ID 和编译清单 SHA。将运行时 PAR 文件放在已编译 SRC 文件旁，并确保依赖项使用工作区内的相对路径；随后调用 `stage_simulation_input(compile_run_id, expected_compile_manifest_sha256, runtime_par_path)`，并保留返回的暂存 ID 和暂存清单 SHA。
5. `probe_simulation_backend()`
6. 在 Windows E 层通过单独验收前，保持 `simulate_sequence(simulation_input_id, expected_manifest_sha256)` 为阻断状态。
7. 获得有效仿真运行后，调用 `analyze_simulation`、`read_channel_slice`、`compare_simulations` 和 `compute_logical_gradient_moment`。

## 运行身份与证据

每个会改变状态的工具都会创建新的 `run_root/<run_id>`，并使用 `exist_ok=False` 防止覆盖。清单记录输入、哈希、命令、退出码、日志、输出哈希、状态和证据限制。失败的运行也会保留；工具不会静默重试或覆盖已有运行。

## 仿真桥接状态

本模块不附带厂家桥接二进制文件。`simulation.backend = "blocked"` 是安全且符合预期的初始状态。`docs/reference/SIMBRIDGE_CONTRACT.md` 定义了受限的 JSON 进程契约；只有在完成当前 DLL、ABI、Socket 和安全性验证后，Windows Codex 才可实现该契约。桥接程序不得暴露任何真实设备功能。

在可联网的 Codex 主机上，常规默认路由可能导致严格的 `host-no-route` 预检无法通过。这属于架构阻断条件，不能通过删除检查来规避。必须先在本模块之外设计并验证隔离的工作节点或虚拟机，或者预先批准的目标 IP 出站隔离方案。

完成 A–C 阶段并通过 P2F 直接预检后，将 `.codex.config.toml.example` 复制到受信任序列项目的 `.codex/config.toml`，重启 Codex Desktop 或新建任务，并在完成 D 阶段前使用 `/mcp` 或 `codex mcp list` 验证配置。
