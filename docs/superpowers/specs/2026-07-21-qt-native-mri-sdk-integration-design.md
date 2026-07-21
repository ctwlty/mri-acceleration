# Qt 原生 MRI SDK 对接与实机验证设计

## 1. 目标

在 Windows 上构建并运行现有 Qt 6/C++ 桌面客户端，保留用户通过“加载 SDK”按钮选择 `mridll.dll` 的交互，由 Qt 进程直接完成谱仪 SDK 加载、初始化、状态读取、扫描启动、扫描监控、急停和关闭。

本次交付不以 HTML、Python GUI 或 Flask 服务作为运行入口，也不接入参考项目中的自动进样电机。参考项目 `C:\Users\Administrator\Documents\Codex\2026-07-21\m\work\eggcontrollerV2` 仅作为已验证的 SDK 调用顺序、运行资产和验收行为基准。

## 2. 已确认的运行条件

- 目标平台为 64 位 Windows。
- 参考 SDK 位于 `eggcontrollerV2\Iface\mriRely\mridll.dll`，PE 架构为 x64。
- DLL SHA-256 为 `D32AF2B676A4956A3D9AB8707B49F47083328A5CE9236FBB5324E44C28054CE8`。
- `mridll.dll` 已确认导出当前 Qt 客户端和参考控制器需要的全部关键函数。
- 初始化配置位于 DLL 相邻的 `hw_cfg\init.ini`，配置的谱仪地址为 `192.168.0.101`。
- 本机存在参考参数文件 `C:\MRIScanner\Scan\PTScan.par`。
- 本机存在扫描输出目录 `D:\mri_data\par0423-3`。
- 谱仪地址可 Ping 通；端口 6000 在 SDK 初始化前不直接接受 TCP 连接，因此最终连接状态以 SDK 返回值为准。
- 用户已确认设备正常运行，并允许使用已跑通的 `PTScan.par` 进行真实设备验证。

## 3. 范围

### 3.1 包含

- 安装并固定 Qt 6、CMake、Ninja/MinGW 64 位构建工具链。
- 构建和启动 `client/` 中的 Qt Widgets 应用。
- 保留“加载 SDK”文件选择入口。
- 从所选 DLL 路径推导 SDK 运行资产目录，并以绝对路径调用 SDK。
- 严格绑定并校验真实控制链路所需导出函数。
- 实现清晰的设备会话状态机。
- 按参考项目顺序完成初始化、基线校准、扫描、轮询、急停和关闭。
- 使用测试 DLL 自动验证调用顺序、返回码处理、超时与关闭行为。
- 使用真实 `mridll.dll` 执行加载、初始化、连接状态读取和一次完整基线扫描。
- 以 SDK 状态码、日志和新增 `.raw` 文件作为实机验收证据。

### 3.2 不包含

- HTML 原型或 Python/Flask 运行入口。
- COM5 自动进样电机控制。
- 将尚未核验的场景参数写入 SDK。
- 修改谱仪固件或 SDK DLL。
- 声称 SDK 不支持的 Pause/Resume 是真实设备能力。
- 临床诊断工作流。

## 4. 方案选择

采用 Qt/C++ 原生动态加载方案。Qt 应用通过 Windows 动态库 API 加载用户选择的 `mridll.dll`，在进程内调用 SDK。

不采用 Python 服务桥接，因为它会引入第二套运行时、进程间通信和重复状态管理。不采用仅修补 Demo 的方案，因为它无法形成可信的真实设备验收链路。

## 5. 架构

### 5.1 `MriSdkLoader`

职责仅限于 SDK 生命周期和原始函数调用：

- 加载和卸载 DLL。
- 绑定并校验函数地址。
- 对所有 SDK 返回码形成统一结果。
- 执行初始化、基线配置、Run、状态读取、Abort 和 CloseSys。
- 记录最后一次错误的阶段、函数名和返回码。

当用户明确选择 DLL 后，任何加载或绑定失败都返回失败。禁止自动切换到 Demo 并返回成功。

### 5.2 `DeviceBridge`

职责是设备会话编排：

- 管理 `Unloaded`、`Loaded`、`Initializing`、`Ready`、`Scanning`、`Stopping`、`Fault` 和 `Closed` 状态。
- 验证操作是否允许在当前状态执行。
- 将 SDK 结果转换为 UI 日志和状态信号。
- 用定时器轮询 `ScanStatus`，不阻塞 UI 线程。
- 在完成、异常或超时时收束扫描会话。
- 记录扫描开始前的输出目录快照，并在完成后确认新增 `.raw` 文件。

### 5.3 `MainWindow`

主窗口只负责交互和展示：

- “加载 SDK”选择 `mridll.dll`。
- 选择成功后展示 DLL 路径和加载状态。
- “一键建链”执行真实初始化，不提前伪造连接成功。
- “开始采集”仅在 `Ready` 状态可用。
- “急停”仅在扫描或停止中的状态可用。
- Pause/Resume 在真实 SDK 模式下禁用并标注“SDK 不支持”。
- 显示真实连接状态、温度、扫描状态码、扫描进度、错误阶段和 RAW 输出结果。

## 6. SDK 路径解析

用户选择：

```text
<sdk-root>\mridll.dll
```

客户端推导：

```text
init.ini  = <sdk-root>\hw_cfg\init.ini
par file  = C:\MRIScanner\Scan\PTScan.par
output    = D:\mri_data\par0423-3
prefix    = PTMRIData
```

所有路径在调用 SDK 前转为绝对路径并验证存在性。参数文件和输出目录允许在高级设置中覆盖，但首次实机验收固定使用以上已验证路径。

SDK 文件不复制进 Git，也不提交构建产物、RAW 数据或设备运行目录。

## 7. SDK 调用顺序

### 7.1 加载阶段

1. `LoadLibrary` 加载用户选择的 x64 `mridll.dll`。
2. 绑定所有必需导出函数。
3. 如果任何必需函数缺失，立即卸载 DLL 并进入 `Fault`。

### 7.2 初始化阶段

按参考项目 `Iface/consolev3.py` 的已验证顺序执行：

1. `Init(init.ini)`
2. `ConfigFile(init.ini)`
3. `SetOutputPath(output)`
4. `SetChannelValid("1")`
5. `SetOutputPrefix("PTMRIData")`
6. `SetSaveMode(1)`
7. `SetParameterFile(PTScan.par, false)`
8. `SetSystemSel(3)`
9. `SetAllPreempValue()`
10. `SetAllGraAnalogDelay()`
11. `SetSingleGraGmax(0, 2240.0)`
12. `SetSingleGraGmax(1, 2080.0)`
13. `SetSingleGraGmax(2, 2980.0)`
14. `SetPreempCross(1)`
15. `SetPreempValue(0, 6, 200.0)`
16. `SetPreempValue(0, 7, 500.0)`
17. `SetPreempValue(0, 8, 800.0)`
18. `SetPreempValue(0, 9, 1000.0)`
19. 读取 `GetConnectStatus`、`GetTemperature` 和 `ScanStatus`，形成建链结果。

每个有返回值的函数都必须检查返回码。任一步骤失败时停止后续初始化，记录失败函数和返回码，调用 `CloseSys` 收束已打开的会话。

### 7.3 扫描阶段

1. 再次调用 `SetParameterFile(PTScan.par, false)`，避免 DLL 状态过期导致 `Run()` 返回 7。
2. 再次调用 `SetChannelValid("1")`。
3. 记录输出目录中现有 `.raw` 文件及时间戳。
4. 调用 `Run()`；非零返回码立即进入 `Fault`。
5. 使用 Qt 定时器轮询 `ScanStatus()`。
6. 状态 1 表示扫描中，状态 2 表示传输中，状态 4 表示写数据中。
7. 状态 3 或 0 表示完成，随后检查新增 `.raw` 文件。
8. 状态 -1、5 或 6 表示异常，调用 `Abort()` 并进入 `Fault`。
9. 超过配置的扫描超时时间时调用 `Abort()` 并进入 `Fault`。

### 7.4 停止与关闭

- 用户点击急停时调用 `Abort()`，轮询状态直到停止或达到停止超时。
- 应用关闭时，如果正在扫描则先调用 `Abort()`，随后调用 `CloseSys()`，最后卸载 DLL。
- 析构和显式关闭必须幂等，防止重复关闭导致崩溃。

## 8. 错误处理与安全

- 真实 SDK 模式禁止静默回退 Demo。
- UI 不得在未检查 SDK 返回值时显示“已连接”。
- SDK 调用错误统一包含阶段、函数名、返回码和路径上下文。
- UI 线程不得等待扫描完成；状态轮询由定时器驱动。
- 初次验收只使用参考项目已跑通的 `PTScan.par`。
- 当前场景模板继续作为展示和工作流上下文，不调用未经核验的 `SetParameter` 字段。
- 真实模式下禁用 Pause/Resume，避免把 UI 状态变化误认为设备动作。
- 扫描异常、超时或应用退出均执行 Abort/CloseSys 收束。
- 不修改参考 SDK 资产，不把 DLL、RAW 或本机路径资产加入 Git。

## 9. 测试设计

### 9.1 自动测试

构建一个仅用于测试的 x64 假 SDK DLL，导出与真实 DLL 相同的控制函数，并把调用序列写入内存或测试日志。测试覆盖：

- 所有必需导出存在时加载成功。
- 缺少必需导出时加载失败且不进入 Demo。
- 初始化调用顺序和参数与参考实现一致。
- 任一步骤返回非零时停止后续调用并关闭会话。
- Run 前重新加载参数文件和通道配置。
- 状态 1/2/4 保持扫描，3/0 完成，-1/5/6 触发 Abort。
- 扫描超时触发 Abort。
- CloseSys 与卸载可重复调用且不崩溃。

### 9.2 构建与 UI 冒烟测试

- CMake 配置成功。
- Qt 客户端编译和链接成功。
- Windows 部署目录包含 Qt 运行库并能独立启动。
- 主窗口可选择 DLL，失败信息可见，按钮状态随会话状态变化。

### 9.3 真实设备验收

按风险逐层推进：

1. 加载真实 DLL 并验证导出函数。
2. 执行 Init/ConfigFile 和基线配置。
3. 读取连接状态、温度和空闲扫描状态。
4. 执行一次 `PTScan.par` 基线扫描。
5. 记录 Run 返回码和完整状态序列。
6. 验证输出目录中新建非空 `.raw` 文件。
7. 正常关闭 SDK，会话结束后应用保持稳定。

若任何层失败，不继续下一层；保存日志并报告具体函数、返回码和环境证据。

## 10. 验收标准

- Qt 桌面程序无需 HTML、Python GUI 或 Flask 即可启动。
- 用户可通过现有“加载 SDK”按钮选择真实 `mridll.dll`。
- DLL 或配置错误不会被 Demo 状态掩盖。
- Qt 日志显示真实初始化步骤和返回结果。
- 连接状态来自 SDK，而非硬编码。
- Run、ScanStatus、Abort、CloseSys 均由 Qt/C++ 直接调用。
- 一次真实基线扫描成功结束，并在输出目录生成新的非空 `.raw` 文件。
- 失败和超时路径能够安全 Abort/CloseSys。
- 自动测试、构建验证和真实设备验收结果均形成可复核记录。

