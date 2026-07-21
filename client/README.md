# Scenario NMR Qt Client

Qt 6/C++ 桌面客户端通过“加载 SDK”按钮动态加载 64 位 `mridll.dll`，并在同一进程内执行谱仪初始化、基线校准、扫描状态监控、Run、Abort 和 CloseSys。

正式运行不依赖 HTML、Python GUI 或 Flask。

## 构建环境

详细版本与安装命令见 [TOOLCHAIN.md](TOOLCHAIN.md)。当前验证组合：

- MSYS2 UCRT64
- GCC 16.1.0
- CMake 4.4.0
- Ninja 1.13.2
- Qt 6.11.1 Widgets/Test

若仓库路径包含中文字符，请把 `BuildRoot` 指定到纯英文路径，避免 MSYS2 Qt `moc` 无法创建生成文件：

```powershell
powershell -ExecutionPolicy Bypass -File client\scripts\build.ps1 -Configuration Debug -BuildRoot C:\tmp\scenario-nmr-debug
powershell -ExecutionPolicy Bypass -File client\scripts\build.ps1 -Configuration Release -BuildRoot C:\tmp\scenario-nmr-release
```

## 部署与启动

```powershell
powershell -ExecutionPolicy Bypass -File client\scripts\deploy.ps1 -BuildRoot C:\tmp\scenario-nmr-release
Start-Process client\dist\scenario_nmr_client.exe
```

部署脚本调用 `windeployqt6`，并对部署后的 GUI 执行 3 秒离屏启动冒烟测试。

要让 Qt 程序启动后直接加载 SDK 并建链，使用：

```powershell
Start-Process client\dist\scenario_nmr_client.exe -ArgumentList @(
  '--auto-connect',
  '--sdk', 'C:\path\to\mridll.dll',
  '--init', 'C:\path\to\hw_cfg\init.ini',
  '--par', 'C:\MRIScanner\Scan\PTScan.par',
  '--output', 'D:\mri_data\par0423-3'
)
```

`--init` 可省略，此时默认使用 DLL 相邻的 `hw_cfg\init.ini`。自动建链成功后 GUI 保持运行，由用户在界面中发起采集或急停。

## 真实 SDK 操作

1. 点击“加载 SDK”，选择参考运行目录中的 `mridll.dll`。
2. 点击“一键建链”。客户端使用 DLL 相邻的 `hw_cfg\init.ini`。
3. 首次基线固定使用 `C:\MRIScanner\Scan\PTScan.par`。
4. RAW 输出目录固定为 `D:\mri_data\par0423-3`。
5. 建链成功后点击“开始采集”。状态 1/2/4 持续轮询，3/0 完成，-1/5/6 执行 Abort 并报告错误；用户急停后保持 `Stopping`，直到设备确认停止才恢复就绪。
6. 扫描完成后在有界等待期内检查新增或被本次采集更新的非空 `.raw` 文件；兼容设备延迟写盘和覆盖固定输出文件名。

真实模式下 Pause/Resume 被禁用，因为当前 SDK 未提供对应接口。

## 原生自动验收工具

`mri_sdk_verify.exe` 复用与 GUI 完全相同的 `DeviceBridge`，用于自动化分层验证，不是正式用户入口。

仅初始化：

```powershell
client\dist\mri_sdk_verify.exe `
  --sdk "C:\path\to\mridll.dll" `
  --init "C:\path\to\hw_cfg\init.ini" `
  --par "C:\MRIScanner\Scan\PTScan.par" `
  --output "D:\mri_data\par0423-3"
```

该验收工具只做加载和初始化，不提供 `--scan`，也不会调用 Run 或 Abort。扫描只能由 GUI 中经过运行包身份验证和当次预检后授权。

## 自动测试

Debug 构建运行六个 CTest 目标：严格 DLL 加载、初始化与校准序列、设备状态机、按钮状态映射、主窗口自动建链、GUI 进程级启动，以及假 SDK 原生端到端扫描。测试 DLL 不连接真实设备。进程级测试还会确认自动建链只初始化一次、不会在启动时误触发采集，且 GUI 事件循环持续运行。
