# Scenario NMR Qt Client

Qt 6/C++ 桌面客户端以应用相对路径解析经过校验的 MRI 运行包。启动或自动建链只会加载并初始化 SDK；只有用户明确选择“设备基线（已实机验证 PTScan）”、完成当次预检并取得一次性 ticket 后，Run 才可能可用。所有现有科研场景继续保持 `Run HOLD`。

正式运行不依赖 HTML、Python GUI、Flask、重建代码或 HSL 串口程序。

## 构建环境

详细版本与安装命令见 [TOOLCHAIN.md](TOOLCHAIN.md)。当前验证组合为 MSYS2 UCRT64、GCC 16.1.0、CMake 4.4.0、Ninja 1.13.2 和 Qt 6.11.1 Widgets/Test。若仓库路径包含中文字符，请把 `BuildRoot` 指向纯英文路径，避免 Qt `moc` 无法创建生成文件。

供应商 `mridll.dll` 还要求系统已安装 **x64 Microsoft Visual C++ v14 Redistributable**，并能解析以下三个 x64 DLL：

- `vcruntime140.dll`
- `vcruntime140_1.dll`
- `msvcp140.dll`

staging 会检查它们是否存在且 PE machine 为 x64，但不会把这些系统 DLL 复制进发布包。请用微软安装程序部署/修复 VC++ Runtime，不要从其他机器手工复制系统 DLL。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File client\scripts\build.ps1 `
  -Configuration Debug -BuildRoot C:\tmp\scenario-nmr-debug

powershell.exe -NoProfile -ExecutionPolicy Bypass -File client\scripts\build.ps1 `
  -Configuration Release -BuildRoot C:\tmp\scenario-nmr-release
```

Debug 构建运行完整 CTest；Release 构建不编译测试目标。

## 经过验证的运行资产

唯一允许迁入发布包的设备资产如下。`hw_cfg` 的目录哈希是按每个文件的 `相对路径|字节数|SHA-256` 记录排序并以 UTF-8 汇总得到的完整清单哈希；相对路径分隔符固定为 `\`，记录以 `\n` 连接且末尾不追加换行，Hidden/System 文件也计入。

| 资产 | 数量 / 大小 | SHA-256 |
| --- | ---: | --- |
| `mridll.dll` | 1 文件 / 34,896,384 bytes | `D32AF2B676A4956A3D9AB8707B49F47083328A5CE9236FBB5324E44C28054CE8` |
| `hw_cfg/` | 455 文件 / 206,656 bytes | 清单 `A8BFF731985A8886EEB53191A6AFD9F5F037931A841A50A4960738595FC45F6F` |
| `hw_cfg/init.ini`（包含于上项） | 1 文件 / 301 bytes | `644D2F4DAD06E5FD5AC6DF7161C63A4164F5B56F926C66DC77D3892CAD411956` |
| `profiles/PTScan.par` | 1 文件 / 21,684 bytes | `6FD62B50A56B802D070AE52737A57516FECE927FCE28BDA17979D4C046C36783` |

staging 还会复制 tracked 的 `mri-runtime-manifest.json`。它只是包结构/诊断元数据，不能替换编译进客户端的生产信任基线。

明确排除：`mridll.dll.backup_20250303`、其他 `.par`、Python GUI/Flask、重建与后处理代码、HSL 串口代码、日志/RAW、源项目，以及 MSVC/UCRT 系统 DLL。运行资产和构建/发布目录均被 Git 忽略，不得提交。

## 部署与 staged 布局

设备运行包必须同时给出 SDK 根目录和所选参数文件；仅验证 Qt 原生启动时必须显式使用 `-QtOnly`。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File client\scripts\deploy.ps1 `
  -BuildRoot C:\tmp\scenario-nmr-release `
  -MriSdkRoot 'C:\path\to\mriRely' `
  -ParameterFile 'C:\MRIScanner\Scan\PTScan.par'

# 不含设备资产的 Qt-only 冒烟包
powershell.exe -NoProfile -ExecutionPolicy Bypass -File client\scripts\deploy.ps1 `
  -BuildRoot C:\tmp\scenario-nmr-release -QtOnly
```

`deploy.ps1` 调用 `windeployqt6` 并复制所需 Qt/MinGW 依赖；设备资产由 `stage-mri-runtime.ps1` 在复制前后各校验一次。预期布局：

```text
client/dist/
├── scenario_nmr_client.exe
├── mri_sdk_verify.exe
├── Qt6*.dll、platforms/ 及 Qt/MinGW 依赖
└── mri-runtime/
    ├── mri-runtime-manifest.json
    ├── mridll.dll
    ├── hw_cfg/
    │   └── init.ini（以及完整的其余 454 个文件）
    └── profiles/
        └── PTScan.par
```

默认输出目录是 `<app>/mri-output`，解析器会创建并探测可写性。

## 启动与 CLI 优先级

使用完整 staged 包时无需传设备路径：

```powershell
Start-Process client\dist\scenario_nmr_client.exe
Start-Process client\dist\scenario_nmr_client.exe -ArgumentList '--auto-connect'
```

路径解析按字段独立处理：显式 `--sdk`、`--init`、`--par`、`--output` 只覆盖同名字段；未覆盖字段继续使用 `<app>/mri-runtime` 或 `<app>/mri-output`。因此优先级是“该字段 CLI > 该字段 bundled default”，不是全有或全无。

```powershell
Start-Process client\dist\scenario_nmr_client.exe -ArgumentList @(
  '--auto-connect',
  '--sdk', 'C:\path\to\mridll.dll',
  '--init', 'C:\path\to\hw_cfg\init.ini',
  '--par', 'C:\path\to\PTScan.par',
  '--output', 'D:\mri_data\par0423-3'
)
```

CLI 覆盖只改变路径，不绕过身份校验或 Run gate。若解析失败，GUI 保持打开并记录错误，但不会调用 SDK。`--auto-connect` 最多完成 Load/Init；它不会调用 Run 或 Abort。

## HOLD、基线与按钮语义

- 启动、加载 SDK、初次建链、重连、改变配置、切换场景、完成/终止/故障后，fresh precheck 都会失效；重连还会把执行模式和按钮视觉同步复位到科研场景 `HOLD`。
- 当前所有科研场景只能浏览、显示参数和执行 `DRY_RUN`，不能触发 Run。
- VerifiedBaseline 不是科研分类。只有固定运行资产与 PTScan 身份匹配、会话 Ready、连接有效、`ScanStatus=0`、输出可写，并且当次 precheck 通过时，“开始采集”才启用。
- ticket 绑定桥实例、generation、执行来源和完整资产身份，只能使用一次；任何状态或身份变化都要求重新预检。
- Pause/Resume 被禁用，因为当前 SDK 没有对应接口。Abort 只在 Scanning/Stopping 可用。

未来某个科研场景只有在完成独立实机验证、固定并复核参数/资产哈希、SDK 字段映射与物理边界、增加该场景的回归和安全审查、定义可验收 RAW 结果，并通过独立评审后，才可从 `Hold` 改为 `VerifiedScene`。不得通过改 UI 文案、CLI 路径或复用 baseline ticket 解锁。

## SDK 调用与安全顺序

1. Resolver 校验 manifest、DLL/init/完整 `hw_cfg`/PTScan 身份和输出可写性；在 `LoadLibrary` 前再次验证路径绑定身份。
2. Load 后严格绑定所需导出。Init 顺序为 `Init → ConfigFile → SetOutputPath → SetChannelValid → SetOutputPrefix → SetSaveMode → SetParameterFile → SetSystemSel → pre-emphasis/gradient calibration`；任一步失败即 `CloseSys`。
3. 用户选择 VerifiedBaseline 后执行 precheck；Run 前再次验证完整身份、连接、idle 状态和输出可写性。
4. ticket 消费后才执行 `SetParameterFile → SetChannelValid → Run`，随后轮询 `ScanStatus`。
5. 状态 1/2/4 表示活动；3 或观察到活动后的 0 表示完成，并继续等待新增或更新的非空 RAW。-1/5/6 或扫描超时触发一次 Abort；正常完成不 Abort。
6. 退出/重配执行 `CloseSys`；仅当仍处于 Scanning/Stopping 且尚未 Abort 时，shutdown 才先发一次 Abort。

`mri_sdk_verify.exe` 只做加载和初始化，不提供 `--scan`，不会调用 Run 或 Abort。

## 回滚

1. 未开始扫描时直接关闭新客户端；若正在扫描，只通过当前会话的一次“急停”等待设备确认停止，不重复 Run/Abort。
2. 保留失败包及日志用于审计，把 `client/dist` 整体替换为上一个已验证发布包；不要混用两个包的 `mri-runtime`。
3. 重新核对 VC++ Runtime、四项资产身份和输出目录，再从 HOLD 开始。
4. 回滚代码时回到迁移前提交/分支；运行资产始终留在 ignored 发布目录，父工作树不需要也不应被修改。

更完整的迁移与发布证据见 [`../docs/mri-runtime-migration-report.md`](../docs/mri-runtime-migration-report.md)。
