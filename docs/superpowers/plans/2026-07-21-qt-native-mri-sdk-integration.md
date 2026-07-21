# Qt 原生 MRI SDK 对接实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 Windows 上构建并运行 Qt 6 客户端，使用户通过现有按钮动态加载 `mridll.dll`，完成真实初始化、状态监控、扫描、急停、关闭和 RAW 文件验收。

**Architecture:** `MriSdkLoader` 负责 DLL 生命周期和逐函数返回码，`DeviceBridge` 负责非阻塞设备状态机，`MainWindow` 只负责按钮与状态展示。测试先构建一个 x64 假 SDK DLL 验证调用顺序和失败收束，再按加载、初始化、Run、RAW 输出四层执行真实设备验收。

**Tech Stack:** Windows x64、MSYS2 UCRT64、GCC、CMake、Ninja、Qt 6 Widgets/Test、CTest、Windows Dynamic-Link Library API。

## Global Constraints

- 正式运行入口只能是 `scenario_nmr_client.exe`，不启动 HTML、Python GUI 或 Flask。
- SDK 通过用户现有“加载 SDK”按钮选择，不把 `mridll.dll`、设备配置、RAW 数据或构建产物提交到 Git。
- 首次实机扫描固定使用 `C:\MRIScanner\Scan\PTScan.par` 和 `D:\mri_data\par0423-3`。
- 真实模式禁止静默回退 Demo，所有失败必须包含阶段、函数名和返回码。
- 未核验的场景参数不写入 SDK；真实模式不伪造 Pause/Resume。
- 扫描轮询不得阻塞 Qt UI 线程；异常、超时和退出均执行 Abort/CloseSys。
- 自动测试先于生产代码，并且每个新行为必须先看到对应测试因缺少该行为而失败。

---

### Task 1: 安装并验证 x64 Qt 构建工具链

**Files:**
- Modify: `.gitignore`
- Create: `client/TOOLCHAIN.md`

**Interfaces:**
- Consumes: Windows Package Manager 和 MSYS2 官方 UCRT64 软件仓库。
- Produces: `C:\msys64\ucrt64\bin\cmake.exe`、`ninja.exe`、`g++.exe`、Qt 6 Widgets/Test，以及固定版本记录。

- [ ] **Step 1: 安装 MSYS2**

```powershell
winget install --id MSYS2.MSYS2 --exact --accept-package-agreements --accept-source-agreements
```

Expected: `Successfully installed`，并存在 `C:\msys64\usr\bin\bash.exe`。

- [ ] **Step 2: 更新 MSYS2 基础系统**

```powershell
& 'C:\msys64\usr\bin\bash.exe' -lc 'pacman -Syu --noconfirm'
& 'C:\msys64\usr\bin\bash.exe' -lc 'pacman -Syu --noconfirm'
```

Expected: 两次命令均正常结束，第二次显示没有待处理的核心升级。

- [ ] **Step 3: 安装 UCRT64 编译链与 Qt 6**

```powershell
& 'C:\msys64\usr\bin\bash.exe' -lc 'pacman -S --needed --noconfirm mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-qt6-base'
```

Expected: 所有软件包安装成功；Qt 包来自 `ucrt64` 仓库。

- [ ] **Step 4: 验证工具链架构和版本**

```powershell
& 'C:\msys64\ucrt64\bin\g++.exe' --version
& 'C:\msys64\ucrt64\bin\cmake.exe' --version
& 'C:\msys64\ucrt64\bin\ninja.exe' --version
& 'C:\msys64\ucrt64\bin\qmake6.exe' -query QT_VERSION
```

Expected: 四条命令成功，Qt 版本为 6.x，目标为 x86_64 UCRT64。

- [ ] **Step 5: 记录实际安装版本并忽略构建目录**

`client/TOOLCHAIN.md` 必须记录以上四条命令的实际输出和复现命令；`.gitignore` 增加：

```gitignore
client/build/
client/dist/
client/test-output/
```

- [ ] **Step 6: 提交工具链记录**

```powershell
git add .gitignore client/TOOLCHAIN.md
git commit -m "build: document Qt 6 UCRT64 toolchain"
```

---

### Task 2: 建立假 SDK 与加载失败测试

**Files:**
- Create: `client/src/app/MriSdkTypes.h`
- Modify: `client/src/app/MriSdkLoader.h`
- Modify: `client/src/app/MriSdkLoader.cpp`
- Modify: `client/CMakeLists.txt`
- Create: `client/tests/fake_mri_sdk.cpp`
- Create: `client/tests/test_mri_sdk_loader.cpp`

**Interfaces:**
- Consumes: 用户选择的 DLL 绝对路径。
- Produces: `MriSdkResult MriSdkLoader::load(const QString&)`、`void MriSdkLoader::unload()`、`MriSdkSessionState MriSdkLoader::sessionState() const`、可配置返回码和调用日志的 `fake_mri_sdk.dll`。

- [ ] **Step 1: 定义测试期望的结果类型与假 SDK 控制接口**

`MriSdkTypes.h` 定义：

```cpp
#pragma once
#include <QString>

enum class MriSdkSessionState { Unloaded, Loaded, Initializing, Ready, Scanning, Stopping, Fault, Closed };

struct MriSdkResult {
    bool ok = false;
    QString stage;
    QString function;
    int code = 0;
    QString message;
    static MriSdkResult success(const QString& stage);
    static MriSdkResult failure(const QString& stage, const QString& function, int code, const QString& message);
};

struct MriSdkConfig {
    QString initPath;
    QString parameterPath;
    QString outputPath;
    QByteArray outputPrefix = "PTMRIData";
    int systemSelection = 3;
};
```

`fake_mri_sdk.cpp` 除生产导出外提供：

```cpp
extern "C" __declspec(dllexport) void FakeReset();
extern "C" __declspec(dllexport) void FakeSetFailure(const char* functionName, int code);
extern "C" __declspec(dllexport) const char* FakeCalls();
extern "C" __declspec(dllexport) void FakeSetScanStatus(int status);
```

- [ ] **Step 2: 写加载行为测试**

`test_mri_sdk_loader.cpp` 使用 Qt Test 验证空路径失败、缺失文件失败、完整假 DLL 成功，并断言失败后 `isLoaded()` 为 false、会话没有进入 `Loaded`。

```cpp
void MriSdkLoaderTest::missingDllDoesNotFallBackToDemo()
{
    MriSdkLoader loader;
    const auto result = loader.load(QDir::temp().filePath("missing-mridll.dll"));
    QVERIFY(!result.ok);
    QVERIFY(!loader.isLoaded());
    QVERIFY(loader.sessionState() != MriSdkSessionState::Loaded);
    QCOMPARE(result.function, QStringLiteral("LoadLibrary"));
}
```

- [ ] **Step 3: 运行测试并确认失败原因**

```powershell
& 'C:\msys64\ucrt64\bin\cmake.exe' -S client -B client/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
& 'C:\msys64\ucrt64\bin\cmake.exe' --build client/build
& 'C:\msys64\ucrt64\bin\ctest.exe' --test-dir client/build -R mri_sdk_loader --output-on-failure
```

Expected: 测试因旧 `load()` 返回 `bool` 且会回退 Demo 而失败或编译失败。

- [ ] **Step 4: 实现最小加载结果和严格导出绑定**

将 `load()` 改为返回 `MriSdkResult`。空路径、`LoadLibraryW` 失败或必需导出缺失时返回失败并保持 `Unloaded/Fault`；只有全部必需导出绑定成功才进入 `Loaded`。`unload()` 清空所有函数指针、调用 `FreeLibrary` 并回到 `Unloaded`。

- [ ] **Step 5: 运行加载测试至通过**

```powershell
& 'C:\msys64\ucrt64\bin\cmake.exe' --build client/build
& 'C:\msys64\ucrt64\bin\ctest.exe' --test-dir client/build -R mri_sdk_loader --output-on-failure
```

Expected: `mri_sdk_loader` 全部通过。

- [ ] **Step 6: 提交严格加载行为**

```powershell
git add client/CMakeLists.txt client/src/app/MriSdkTypes.h client/src/app/MriSdkLoader.h client/src/app/MriSdkLoader.cpp client/tests/fake_mri_sdk.cpp client/tests/test_mri_sdk_loader.cpp
git commit -m "test: enforce strict MRI SDK loading"
```

---

### Task 3: 按参考实现完成初始化与校准序列

**Files:**
- Modify: `client/src/app/MriSdkTypes.h`
- Modify: `client/src/app/MriSdkLoader.h`
- Modify: `client/src/app/MriSdkLoader.cpp`
- Modify: `client/tests/fake_mri_sdk.cpp`
- Modify: `client/tests/test_mri_sdk_loader.cpp`

**Interfaces:**
- Consumes: `MriSdkConfig`，其路径均为已验证的绝对路径。
- Produces: `MriSdkResult MriSdkLoader::initialize(const MriSdkConfig&)`、`MriSdkResult MriSdkLoader::prepareScan()`、`MriSdkStatus MriSdkLoader::status() const`。

- [ ] **Step 1: 增加状态结构和初始化顺序测试**

`MriSdkTypes.h` 增加：

```cpp
struct MriSdkStatus {
    int connection = 0;
    double temperature = 0.0;
    int scan = 0;
    int currentScan = 0;
    int totalScans = 0;
};
```

测试用绝对临时路径创建 `init.ini`、`PTScan.par` 和输出目录，调用 `initialize()` 后从 `FakeCalls()` 断言以下前缀完全一致：

```text
Init|ConfigFile|SetOutputPath|SetChannelValid:1|SetOutputPrefix:PTMRIData|SetSaveMode:1|SetParameterFile|SetSystemSel:3|SetAllPreempValue|SetAllGraAnalogDelay|SetSingleGraGmax:0:2240|SetSingleGraGmax:1:2080|SetSingleGraGmax:2:2980|SetPreempCross:1|SetPreempValue:0:6:200|SetPreempValue:0:7:500|SetPreempValue:0:8:800|SetPreempValue:0:9:1000
```

- [ ] **Step 2: 运行初始化测试并确认失败**

```powershell
& 'C:\msys64\ucrt64\bin\cmake.exe' --build client/build
& 'C:\msys64\ucrt64\bin\ctest.exe' --test-dir client/build -R mri_sdk_loader --output-on-failure
```

Expected: 失败，因为校准导出尚未绑定，且旧实现的通道值和顺序与参考实现不同。

- [ ] **Step 3: 绑定校准函数并实现统一调用检查**

新增精确函数类型：

```cpp
using SetOutputPrefixFunc = int (*)(const char*);
using SetAllPreempValueFunc = int (*)();
using SetAllGraAnalogDelayFunc = int (*)();
using SetSingleGraGmaxFunc = int (*)(int, float);
using SetPreempCrossFunc = int (*)(int);
using SetPreempValueFunc = int (*)(int, int, float);
```

`initialize()` 先检查三个路径，进入 `Initializing`，再严格按设计文档顺序调用。任一非零返回值立即形成 `MriSdkResult::failure(...)`，调用 `CloseSys()`，进入 `Fault`，不执行后续调用。

- [ ] **Step 4: 增加逐步骤失败收束测试**

以 `FakeSetFailure("ConfigFile", 12)` 和 `FakeSetFailure("SetParameterFile", 7)` 分别验证：结果包含准确函数名/返回码，失败点之后的函数未被调用，`CloseSys` 被调用一次。

- [ ] **Step 5: 运行初始化测试至通过**

```powershell
& 'C:\msys64\ucrt64\bin\cmake.exe' --build client/build
& 'C:\msys64\ucrt64\bin\ctest.exe' --test-dir client/build -R mri_sdk_loader --output-on-failure
```

Expected: 初始化顺序、失败收束和状态读取测试全部通过。

- [ ] **Step 6: 提交初始化链路**

```powershell
git add client/src/app/MriSdkTypes.h client/src/app/MriSdkLoader.h client/src/app/MriSdkLoader.cpp client/tests/fake_mri_sdk.cpp client/tests/test_mri_sdk_loader.cpp
git commit -m "feat: initialize MRI SDK with verified calibration sequence"
```

---

### Task 4: 实现非阻塞设备状态机与 RAW 验收

**Files:**
- Modify: `client/src/app/DeviceBridge.h`
- Modify: `client/src/app/DeviceBridge.cpp`
- Create: `client/tests/test_device_bridge.cpp`
- Modify: `client/CMakeLists.txt`

**Interfaces:**
- Consumes: `MriSdkLoader`、`MriSdkConfig`、Qt `QTimer` 和输出目录。
- Produces: `MriSdkResult loadSdk(const QString&)`、`MriSdkResult connectDevice(const MriSdkConfig&)`、`MriSdkResult startScan()`、`void abortScan()`、`void refreshStatus()` 和 `sessionStateChanged(MriSdkSessionState)`。

- [ ] **Step 1: 写状态机与扫描结果测试**

测试覆盖：

```cpp
void DeviceBridgeTest::runIsRejectedBeforeReady();
void DeviceBridgeTest::successfulScanTransitionsReadyScanningReady();
void DeviceBridgeTest::faultStatusAbortsAndTransitionsFault();
void DeviceBridgeTest::completedScanRequiresNewNonEmptyRawFile();
void DeviceBridgeTest::timeoutAbortsAndTransitionsFault();
```

成功测试用 `QSignalSpy` 断言状态序列，并让假 DLL 在 Run 时向配置的输出目录写入非空 `PTMRIData_fake.raw`。

- [ ] **Step 2: 运行设备桥测试并确认失败**

```powershell
& 'C:\msys64\ucrt64\bin\cmake.exe' --build client/build
& 'C:\msys64\ucrt64\bin\ctest.exe' --test-dir client/build -R device_bridge --output-on-failure
```

Expected: 失败，因为旧桥接层没有会话状态机、定时轮询和 RAW 输出判定。

- [ ] **Step 3: 实现状态守卫和定时轮询**

`DeviceBridge` 保存 `MriSdkSessionState m_state`、`QTimer m_pollTimer`、`QElapsedTimer m_scanElapsed`、`QSet<QString> m_rawFilesBeforeScan`、`MriSdkConfig m_config`。`startScan()` 仅允许从 `Ready` 进入 `Scanning`；`refreshStatus()` 对 1/2/4 保持扫描，对 3/0 验证新 RAW，对 -1/5/6 调用 Abort 并进入 `Fault`。

- [ ] **Step 4: 实现超时、急停和幂等关闭**

扫描超时调用 `Abort()`；急停从 `Scanning` 进入 `Stopping`，状态停止后回到 `Ready`；析构函数停止定时器并执行一次安全关闭。所有拒绝和错误通过 `operationFailed(MriSdkResult)` 与日志信号送往 UI。

- [ ] **Step 5: 运行设备桥和全部自动测试**

```powershell
& 'C:\msys64\ucrt64\bin\cmake.exe' --build client/build
& 'C:\msys64\ucrt64\bin\ctest.exe' --test-dir client/build --output-on-failure
```

Expected: 加载器与设备桥测试全部通过。

- [ ] **Step 6: 提交设备状态机**

```powershell
git add client/CMakeLists.txt client/src/app/DeviceBridge.h client/src/app/DeviceBridge.cpp client/tests/test_device_bridge.cpp client/tests/fake_mri_sdk.cpp
git commit -m "feat: add MRI device session state machine"
```

---

### Task 5: 将真实会话状态接回现有 Qt 界面

**Files:**
- Create: `client/src/app/DeviceActionAvailability.h`
- Create: `client/src/app/DeviceActionAvailability.cpp`
- Modify: `client/src/app/MainWindow.h`
- Modify: `client/src/app/MainWindow.cpp`
- Modify: `client/src/main.cpp`
- Create: `client/tests/test_device_actions.cpp`
- Modify: `client/CMakeLists.txt`

**Interfaces:**
- Consumes: `DeviceBridge` 状态、状态指标和错误信号。
- Produces: 保留现有“加载 SDK”“一键建链”“开始采集”“急停”按钮的真实控制行为。

- [ ] **Step 1: 增加 UI 状态断言测试或可检查的状态映射函数**

将按钮可用性提取到 `DeviceActionAvailability.h/.cpp` 的纯函数：

```cpp
struct DeviceActionAvailability {
    bool canLoadSdk;
    bool canConnect;
    bool canRun;
    bool canAbort;
};

DeviceActionAvailability actionsForState(MriSdkSessionState state);
```

Qt Test 覆盖 `Unloaded`、`Loaded`、`Ready`、`Scanning`、`Fault` 的四个按钮布尔值，先运行并确认函数不存在导致失败。

- [ ] **Step 2: 实现按钮状态映射并连接真实操作**

加载按钮只执行 `loadSdk(dllPath)`；建链按钮构造：

```cpp
MriSdkConfig config;
config.initPath = QFileInfo(dllPath).dir().filePath("hw_cfg/init.ini");
config.parameterPath = QStringLiteral("C:/MRIScanner/Scan/PTScan.par");
config.outputPath = QStringLiteral("D:/mri_data/par0423-3");
config.outputPrefix = "PTMRIData";
config.systemSelection = 3;
```

开始采集调用真实 `startScan()`，急停调用真实 `abortScan()`。移除构造函数中的空 DLL Demo 初始化和场景 `HOLD` 对基线扫描入口的阻断。

- [ ] **Step 3: 禁用伪 Pause/Resume 并显示真实指标**

真实模式下暂停/继续按钮禁用，tooltip 为“当前 SDK 未提供暂停/继续接口”。页头和页脚显示 SDK 路径、连接码、温度、ScanStatus、当前/总扫描号、最后错误和 RAW 文件路径。

- [ ] **Step 4: 运行全部测试并构建 Release**

```powershell
& 'C:\msys64\ucrt64\bin\cmake.exe' --build client/build
& 'C:\msys64\ucrt64\bin\ctest.exe' --test-dir client/build --output-on-failure
& 'C:\msys64\ucrt64\bin\cmake.exe' -S client -B client/build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
& 'C:\msys64\ucrt64\bin\cmake.exe' --build client/build-release
```

Expected: 自动测试全部通过，生成 `client/build-release/scenario_nmr_client.exe`。

- [ ] **Step 5: 提交 UI 接线**

```powershell
git add client/src/app/DeviceActionAvailability.h client/src/app/DeviceActionAvailability.cpp client/src/app/MainWindow.h client/src/app/MainWindow.cpp client/src/main.cpp client/tests/test_device_actions.cpp client/CMakeLists.txt
git commit -m "feat: connect Qt controls to real MRI session"
```

---

### Task 6: 部署 Qt 运行库并执行真实设备分层验收

**Files:**
- Create: `client/scripts/deploy.ps1`
- Create: `client/scripts/verify-real-sdk.ps1`
- Create: `docs/verification/2026-07-21-real-mri-sdk-verification.md`
- Modify: `client/README.md`

**Interfaces:**
- Consumes: Release 可执行文件、MSYS2 UCRT64 Qt 运行库、真实 `mridll.dll`、已确认的 init/par/output 路径。
- Produces: 可启动的 `client/dist/scenario_nmr_client.exe` 和可复核的真实设备验证报告。

`verify-real-sdk.ps1` 负责在运行前后分别保存 `D:\mri_data\par0423-3\*.raw` 的文件名、长度和修改时间，并输出新增非空文件清单；它不直接调用 SDK，也不绕过 Qt 界面的加载、建链和扫描按钮。

- [ ] **Step 1: 编写部署脚本并先验证缺失运行库时启动失败**

`deploy.ps1` 清理并创建 `client/dist`，复制 Release EXE，调用：

```powershell
& 'C:\msys64\ucrt64\bin\windeployqt6.exe' --release --no-translations --compiler-runtime 'client\dist\scenario_nmr_client.exe'
```

脚本随后复制 UCRT64 GCC 运行库依赖并用 `Start-Process -PassThru` 启动 3 秒，确认进程保持存活后安全关闭测试实例。

- [ ] **Step 2: 运行部署脚本并启动 Qt 应用**

```powershell
powershell -ExecutionPolicy Bypass -File client/scripts/deploy.ps1
Start-Process -FilePath (Resolve-Path 'client/dist/scenario_nmr_client.exe') -WorkingDirectory (Resolve-Path 'client/dist')
```

Expected: 原生 Qt 主窗口出现，无 HTML/Python/Flask 进程。

- [ ] **Step 3: 执行真实 DLL 加载与初始化验收**

在 UI 中选择：

```text
C:\Users\Administrator\Documents\Codex\2026-07-21\m\work\eggcontrollerV2\Iface\mriRely\mridll.dll
```

点击“一键建链”。记录每个初始化函数返回码、`GetConnectStatus`、温度和空闲 `ScanStatus`。任何失败立即停止，不执行 Run。

- [ ] **Step 4: 执行一次真实 PTScan 基线扫描**

扫描前记录 `D:\mri_data\par0423-3\*.raw` 的文件名、长度和时间；点击“开始采集”；记录完整 ScanStatus 序列。状态进入 3 或 0 后，验证新增 `.raw` 文件长度大于 0。

- [ ] **Step 5: 验证急停和关闭收束**

若扫描在正常验证中已完成，不额外启动第二次扫描；通过假 SDK 自动测试覆盖 Abort。关闭应用，确认进程退出且再次启动仍能重新加载 SDK。只有真实扫描需要停止时才在实机上点击急停。

- [ ] **Step 6: 写入验证报告**

报告必须包含工具链版本、DLL 哈希、路径、初始化返回码、状态序列、RAW 文件名/大小/时间、应用退出结果，以及未执行或失败步骤的准确原因。

- [ ] **Step 7: 执行最终验证并提交**

```powershell
& 'C:\msys64\ucrt64\bin\ctest.exe' --test-dir client/build --output-on-failure
git diff --check
git status --short
git add client/scripts client/README.md docs/verification/2026-07-21-real-mri-sdk-verification.md
git commit -m "docs: record native Qt MRI SDK verification"
```

Expected: 自动测试全部通过、Qt Release 可启动、实机报告包含可复核证据、Git 只包含源码/文档而不包含 DLL/RAW/构建目录。
