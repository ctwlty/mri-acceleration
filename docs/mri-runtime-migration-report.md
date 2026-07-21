# MRI Runtime 迁移与 Release-Prep 报告

日期：2026-07-21
隔离分支：`codex/migrate-verified-mri-runtime-impl`
迁移基线：`c236e00`
工作树：`C:\Users\Administrator\.codex\visualizations\2026\07\21\019f8476-a80e-7eb1-b7c0-d915064ef076\mri-runtime-impl`

## 迁移结论

Qt 客户端已具备“严格 staging → 应用相对解析 → 编译内信任基线 → typed execution gate → fresh precheck/ticket → 单次 Run”的闭环。供应商资产不进入 Git；发布时只从明确来源复制最小运行集合，并在复制前后校验。

本报告覆盖实机前 release-prep，不授权或记录真实 Run/Abort。真实硬件闭环必须在独立全分支审查关闭所有 Critical/Important 后，由主审查流程执行恰好一次 PTScan。

## 迁入与排除边界

迁入 `client/dist/mri-runtime`：

| 目标 | 锁定数量 / 大小 | 锁定 SHA-256 |
| --- | ---: | --- |
| `mridll.dll` | 1 / 34,896,384 bytes | `D32AF2B676A4956A3D9AB8707B49F47083328A5CE9236FBB5324E44C28054CE8` |
| `hw_cfg/` | 455 / 206,656 bytes | manifest `A8BFF731985A8886EEB53191A6AFD9F5F037931A841A50A4960738595FC45F6F` |
| `hw_cfg/init.ini`（上项子集） | 1 / 301 bytes | `644D2F4DAD06E5FD5AC6DF7161C63A4164F5B56F926C66DC77D3892CAD411956` |
| `profiles/PTScan.par` | 1 / 21,684 bytes | `6FD62B50A56B802D070AE52737A57516FECE927FCE28BDA17979D4C046C36783` |
| `mri-runtime-manifest.json` | 1 / 545 bytes | tracked 文件 `3F7997BA073C820AD1476DB4313430615D1824C0DD5C2D1250BB3BADB618B754` |

排除 `mridll.dll.backup_20250303`、所有未选 `.par`、Python GUI/Flask、重建/后处理、HSL 串口、源项目、缓存/日志/RAW、build/dist 产物，以及 `vcruntime140*.dll`、`msvcp140.dll` 等 MSVC 系统运行库。Qt/MinGW 依赖由 `windeployqt6` 处理；MSVC v14 x64 Runtime 必须通过系统安装程序提供，staging 只检查可解析性和 PE x64，不复制系统 DLL。

## staged 包和部署命令

```text
client/dist/
├── scenario_nmr_client.exe
├── mri_sdk_verify.exe
├── Qt6*.dll、platforms/、Qt/MinGW 依赖
└── mri-runtime/
    ├── mri-runtime-manifest.json
    ├── mridll.dll
    ├── hw_cfg/                  # 完整 455 文件
    └── profiles/PTScan.par
```

```powershell
# Release
powershell.exe -NoProfile -ExecutionPolicy Bypass -File client\scripts\build.ps1 `
  -Configuration Release -BuildRoot '<worktree>\client\build-task4-release'

# 先以 QtOnly 包执行不含设备资产的 native GUI smoke
powershell.exe -NoProfile -ExecutionPolicy Bypass -File client\scripts\deploy.ps1 `
  -BuildRoot '<worktree>\client\build-task4-release' -QtOnly

# 审计通过后，把真实资产 staging 到同一隔离包；该命令不启动程序或加载 DLL
powershell.exe -NoProfile -ExecutionPolicy Bypass -File client\scripts\stage-mri-runtime.ps1 `
  -MriSdkRoot 'C:\Users\Administrator\Desktop\eggcontrol\eggcontrollerV2\Iface\mriRely' `
  -ParameterFile 'C:\MRIScanner\Scan\PTScan.par' `
  -Destination '<worktree>\client\dist\mri-runtime'
```

正式启动：

```powershell
Start-Process '<worktree>\client\dist\scenario_nmr_client.exe'
Start-Process '<worktree>\client\dist\scenario_nmr_client.exe' -ArgumentList '--auto-connect'
```

release-prep 不执行以上正式启动。`--auto-connect` 仅允许 Load/Init，不允许 Run/Abort。

## 路径优先级与信任边界

每个 CLI 字段独立优先于对应 bundled default：

- `--sdk` > `<app>/mri-runtime/mridll.dll`
- `--init` > `<app>/mri-runtime/hw_cfg/init.ini`
- `--par` > `<app>/mri-runtime/profiles/PTScan.par`
- `--output` > `<app>/mri-output`

覆盖一个字段不会关闭其余 bundled 字段的 manifest/资产校验。CLI 是路径选择，不是授权；只有实际解析的 DLL/init/完整 `hw_cfg`/PTScan 全部符合编译内生产基线时，resolver 才能 mint 不透明身份凭据。manifest 是诊断元数据，不能自行提升信任。

## HOLD、基线和 scene unlock

所有当前科研场景的类型均为 `ExecutionGate::Hold`，只能浏览与 `DRY_RUN`。VerifiedBaseline 是互斥的设备执行来源，不属于科研场景分类。Ready 也不等于可 Run：必须显式选择 VerifiedBaseline，并以当前连接、idle `ScanStatus`、可写输出和固定资产身份完成 fresh precheck，取得实例/generation/source/identity 绑定的一次性 ticket。

SDK/config/mode/scene/run/completion/abort/fault 任一变化都会使 ticket 失效。建链/重连将桥和 UI combo 同步复位为 HOLD，并刷新按钮；启动/auto-connect 不 Run、不 Abort。

未来场景解锁至少要求：独立实机证据；固定参数/运行资产及哈希；SDK 映射和物理边界审查；该场景独立的输出验收标准；HOLD/TOCTOU/错误路径自动回归；安全审查无 Critical/Important；随后才可引入 `VerifiedScene`，不得复用 baseline ticket。

## 调用与安全顺序

1. Resolver 校验 manifest、资产、目录清单和输出，并在 `LoadLibrary` 前复验路径绑定身份。
2. Load 严格绑定导出；Init 执行 `Init → ConfigFile → SetOutputPath → SetChannelValid → SetOutputPrefix → SetSaveMode → SetParameterFile → SetSystemSel → pre-emphasis/gradient calibration`。任一步失败执行 `CloseSys`。
3. precheck 要求 Ready、VerifiedBaseline、固定身份、连接非 0、`ScanStatus=0`、输出可写，记录温度/连接/状态并签发一次性 ticket。
4. Run 前再次检查完整身份、连接、idle 和输出，随后消费 ticket，执行 `SetParameterFile → SetChannelValid → Run`。
5. 轮询 1/2/4；3 或活动后的 0 表示完成，继续验证新增/更新的非空 RAW。-1/5/6 或 timeout 最多一次 Abort；正常完成不 Abort。
6. 重配或退出 `CloseSys`；若 shutdown 时仍 Scanning/Stopping 且尚未 Abort，才先执行一次 Abort。

## 回滚

- 未扫描：关闭新客户端，把整个 `client/dist` 换回上一已验证包；不混用 runtime 子目录。
- 扫描中：只用当前会话的一次急停并等待停止，不重复 Run/Abort；保留失败包与日志。
- 代码：回到迁移前提交/分支；设备资产、build、dist 仍为 ignored，不从 Git 恢复或提交。
- 父分支/父工作树：本迁移只在隔离 worktree 提交，不 merge、不移动父分支指针、不写父工作树。

## 2026-07-21 实机前审计状态

审计过程中发现 Task 1/2 的目录哈希实现相对原始规范发生漂移：实现把路径改为 `/` 并追加末尾换行，因此一度对未变化的真实资产得到 `E60BB535...11C`。生产信任基线没有修改。PowerShell staging、Qt resolver 和所有 test fixture 已统一回原规范：Hidden/System/`-Force` 文件全部计入；相对路径分隔符固定为 `\`；按相对路径稳定排序；记录 UTF-8、以 `\n` 连接且末尾无换行。含 hidden/nested 文件的固定小 fixture hash 为 `1B611088F0D8B2C58D14A35942CF9F50DE058BDEB7C12A6D167FCFF24A61DA47`，两端回归通过。

修复后，指定真实来源和 staged copy 均得到锁定清单 `A8BFF731985A8886EEB53191A6AFD9F5F037931A841A50A4960738595FC45F6F`，且均为 455 文件 / 206,656 bytes。完整 staged runtime 为 458 文件 / 35,125,269 bytes；DLL/init/PTScan 大小与哈希逐项匹配，发布包中 `vcruntime140.dll`、`vcruntime140_1.dll`、`msvcp140.dll` 复制数为 0。

fresh Debug 构建后 CTest 8/8 通过；fresh Release 21/21 构建通过。Windows native GUI smoke 仅在 `-QtOnly` 包执行并通过；之后才 staging 真实资产，且未再启动 GUI、未加载真实 DLL。Release 路径、命令、可执行文件哈希及剩余实机步骤见 `.superpowers/sdd/task-4-prehardware-report.md`。
