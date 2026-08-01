# Project B｜04 DLL、自动化接口、数据与副作用

## 1. 三条外部接口必须区分

```text
原生 Qt SDK 链
  MainWindow → DeviceBridge → MriSdkLoader → mridll.dll

运行时资产链（仅 c77 分支）
  stage script → mri-runtime/manifest → MriRuntimeResolver → 原生 Qt SDK 链

eggcontroller 代理链（8e4 阶段）
  MainWindow → EggControllerProcess/QProcess → Python proxy
  → 外部 eggcontrollerV2::MainWindow_V3.samplingBtn_click_sync()
  → 返回 RAW/K-space PNG/最终 PNG 路径
```

代理链不是“Qt 替换 DLL”；它把外部自动化程序作为黑盒进程调用。外部程序内部是否加载 DLL，取决于传入 `--egg-root` 的实际代码和资产。

## 2. 原生 Qt SDK 链

### 加载和初始化

`c236e00:client/src/app/MriSdkLoader.cpp::load` 使用 `LoadLibraryW` 严格绑定导出；缺 DLL/导出即失败，不再回退 Demo。

`initialize` 的顺序为：

```text
Init
→ ConfigFile(init.ini)
→ SetOutputPath
→ SetChannelValid("1")
→ SetOutputPrefix
→ SetSaveMode(1)
→ SetParameterFile
→ SetSystemSel
→ 校准相关函数
```

任一步失败会报告 `MriSdkResult` 并尝试 CloseSys。`prepareScan` 会在 Run 前重载参数/通道。证据：`c236e00:client/src/app/MriSdkLoader.cpp:125-218` 及稳定符号 `prepareScan`。

### Run/状态/RAW

`DeviceBridge::startScan()` 拍摄输出目录 RAW 快照、调用 Run 一次、用 QTimer 轮询；正常完成后等待新/更新的非空 RAW，异常状态或超时才 Abort。Pause/Resume 明确不发设备命令。

### 安全限制

- 单次 DLL 调用同步、无调用级 timeout。
- c236 读取连接码但不以其阻断 Ready；c77 分支后来把实机成功码 0 纳入判断。
- 当前 `33be` 的 `DeviceBridge.cpp` 与 `c236e00` 为同一 blob，不含 c77 的 identity/precheck gate。

## 3. 运行时资产链（B1-R）

`c77aeb4:client/runtime/mri-runtime-manifest.json` 固定：

| 资产 | 历史身份 |
|---|---|
| `mridll.dll` | 34,896,384 bytes；SHA-256 `D32AF2B676A4956A3D9AB8707B49F47083328A5CE9236FBB5324E44C28054CE8` |
| `hw_cfg` | 455 files；206,656 bytes；manifest hash `A8BFF731985A8886EEB53191A6AFD9F5F037931A841A50A4960738595FC45F6F` |
| `init.ini` | SHA-256 `644D2F4DAD06E5FD5AC6DF7161C63A4164F5B56F926C66DC77D3892CAD411956` |
| `PTScan.par` | 21,684 bytes；SHA-256 `6FD62B50A56B802D070AE52737A57516FECE927FCE28BDA17979D4C046C36783` |

初始 Git 提交曾包含另一份 35,006,464-byte DLL，SHA-256 `E964A3EBEF019424188DA3AAFA5270626ED15163843C4211DC8C97AA954FFF3C`，并在 `f502549` 删除。D32 与 E964 明确不是同一文件；这才是历史上可证实的 DLL 资产身份变化。

`MriRuntimeResolver::resolve` 校验 DLL/init/PTScan/hash 与 `hw_cfg` 清单、生成 proof 并支持调用前复核；manifest 是 SDK runtime manifest，不是 Mock 结果 manifest。

**VERIFIED FACT**：当前 `33be` 没有这些 runtime 文件/类。当前 `C:\MRIScanner\Scan\PTScan.par` 也已不匹配历史清单；不能把 c77 的“固定基线”投影为当前外部状态。

## 4. eggcontroller 代理链（B1-P）

### Qt 端

`8e4ad0b:client/src/app/EggControllerProcess.cpp::start` 启动配置中的 program/arguments/workingDirectory，异步读取 JSONL：

- `stage` → `stageChanged`
- `error` → `failed`
- `result` → `EggControllerArtifacts { taskId, rawPath, kspaceImagePath, finalImagePath }`

工件校验要求绝对、存在、非空、图像可解码，并检查 task ID 与文件名/目录绑定。`MainWindow::showEggControllerArtifacts` 加载两图，记录 RAW/图的路径、大小、mtime、SHA-256。

### Python 端

`8e4ad0b:client/tools/eggcontroller_proxy.py::run_once`：

```text
切换到 egg_root / 加入 sys.path
→ 导入 MainWindow_V3.Ui_MainWindow
→ ui.demoCheckbox.setChecked(False)
→ 记录 output/*.raw 快照
→ samplingBtn_click_sync() 恰好一次
→ 要求 success count +1 且 registerList 非空
→ 取 K-space/final 路径
→ 找本次新增/更新 RAW
→ 输出 task/raw/kspace/final JSON
```

### 静态断点和真值边界

- **VERIFIED FACT**：`8e4ad0b` 提交树中的 `MainWindow_V3.py` 没有 `demoCheckbox`；proxy 的直接访问会失败。
- **VERIFIED FACT**：同一提交树 `eggcontrollerV2/Iface/HCController.py` 注释 `consolev3`、实际导入 `console_mock`；`console_mock` 不加载硬件 DLL。
- **VERIFIED FACT**：proxy 单测以自建 FakeUi 提供 `demoCheckbox` 和假工件，未导入真实 UI。
- **VERIFIED FACT**：本机 Desktop 外部参考副本目前的 `HCController.py` 改为导入 `consolev3`，而其 `consolev3.py` 通过 `ctypes.CDLL` 加载 `Iface\mriRely\mridll.dll`；这是外部可变目录事实，不是 `8e4ad0b` Git 提交事实。
- **UNKNOWN**：历史某次运行究竟传入哪一个 egg-root 版本、是否含 demo 开关修补，以及是否真实完成设备链；本轮没有运行时记录绑定。

因此：源码可证明进程协议和一次入口设计，不能仅凭 `8e4ad0b` 提交证明真实硬件链闭环。

## 5. 当前 Mock 数据与结果包

`91343be:client/src/app/MockResultPackage.cpp` 固定写入：

1. `parameter-snapshot.json`
2. `mock-source.json`
3. `standard-mock-result.png`
4. `mock-qc.json`
5. `audit-events.json`
6. `task-note.txt`
7. `manifest.json`

writer 使用 staging、`QSaveFile`、逐文件回读/hash、最终目录 rename，拒绝同名覆盖。生产保存路径在 `write()` 成功后直接 `markPackaged()`；不会先调用公开的 `verify()`。后续 `loadHistory()` 才调用 `verify()`，交叉校验 run/snapshot/sample/template/softwareCommit、图像与 QC hash，并只投影磁盘上通过或带明确问题状态的实际 manifest。

- 输入图是 QRC 的 `:/mock-reconstruction.png`，明确是 Mock 资产，不是 RAW 重建结果。
- QC 由 `ImageQualityEvaluator::evaluate` 对该 Mock PNG 计算。
- 默认 Mock 链不写真实 RAW，也不调用原生 SDK 或 eggcontroller。
- 当前生产 UI 没有 `m_eggController->start`，所以外部代理工件不会进入当前 Mock 结果包链。

## 6. 副作用总表

| 路径 | 可达条件 | 副作用 | 当前默认 UI |
|---|---|---|---|
| Mock | 正常 01–12 | 可写探针、结果包/manifest、剪贴板/打开目录 | 可达 |
| native auto-connect | 显式 CLI | LoadLibrary、Init/config/calibration | 不自动，可达旁路 |
| verifier scan | 独立工具显式 `--scan` | Run/轮询/可能 Abort/RAW | 不属于 GUI，但发布源码可构建 |
| eggcontroller | 8e4 UI 或未来重新接线 | 外部 Python/可能的内部设备副作用 | 当前只配置，不 start |
| runtime staging | c77 分支脚本 | 复制/校验外部 DLL/config/par | 当前不存在 |
