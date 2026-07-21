# Qt 原生 MRI SDK 与真实设备验证记录

验证日期：2026-07-21（Asia/Shanghai）

## 验证对象

- Qt 客户端：`scenario_nmr_client.exe`
- 原生验证器：`mri_sdk_verify.exe`（与 GUI 复用同一个 `DeviceBridge`）
- SDK：`mridll.dll`
- SDK SHA-256：`D32AF2B676A4956A3D9AB8707B49F47083328A5CE9236FBB5324E44C28054CE8`
- 初始化配置：参考项目 `Iface\mriRely\hw_cfg\init.ini`
- 扫描参数：`C:\MRIScanner\Scan\PTScan.par`
- 输出目录：`D:\mri_data\par0423-3`

## 环境

- Windows x64
- MSYS2 UCRT64
- GCC 16.1.0
- CMake 4.4.0
- Ninja 1.13.2
- Qt 6.11.1

## 真实初始化结果

Qt 进程通过 `LoadLibraryW` 直接加载 64 位 `mridll.dll`，并完成参考项目中的初始化、参数加载和基线校准序列。

最终初始化输出：

```text
LOG SDK 初始化和基线校准完成，连接码=0，ScanStatus=0
INITIALIZED connection=0 temperature=37.7188 scanStatus=0 current=0 total=0
```

本 SDK 构建中 `SetPreempCross(1)` 返回 1，表示已应用的启用状态，不是失败码。实机探针确认其余校准调用均返回 0；实现只对 `SetPreempCross` 接受 0/1，其余初始化、配置和校准步骤仍严格检查返回码。

## 真实扫描结果

执行一次 `PTScan.par` 扫描，设备进入活动态后约 55 秒完成：

```text
LOG Run 已执行，开始轮询 ScanStatus
LOG 扫描完成，RAW 文件：D:/mri_data/par0423-3/PTMRIData00_1.raw
SCAN_COMPLETED raw=D:/mri_data/par0423-3/PTMRIData00_1.raw
POST_REVIEW_REAL_SCAN_EXIT=0
```

输出文件：

- 路径：`D:\mri_data\par0423-3\PTMRIData00_1.raw`
- 大小：5,374,219 字节
- 最后写入：2026-07-21 16:00:36（本地时间）
- SHA-256：`F5818CBC102C98F277981C6498DB82FA7E2CBC64B6308A3319FE677A86C8D6B3`

设备使用固定 RAW 文件名并覆盖旧文件，因此验收逻辑比较路径、大小和修改时间，而不是只要求出现新文件名。

## 发现并修复的问题

1. Qt `QFileInfo` 输出 `/` 路径分隔符，而旧 SDK 只接受 Windows `\`。同一最小探针使用 `\` 时 `Init=0`，使用 `/` 时 `Init=7 / GetLastErr=1009`。所有传给 SDK 的配置、参数和输出路径现已转换为原生 Windows 分隔符。
2. `Run` 后首轮 `ScanStatus` 可能仍是旧的空闲值 0。状态机现在等待本次扫描观察到 1/2/4 活动态后，才接受 0 为完成；状态 3 仍可直接完成。
3. 设备覆盖 `PTMRIData00_1.raw`。RAW 验证现在接受新增或元数据发生变化的非空文件。
4. 用户 Abort 后保持 `Stopping` 并继续轮询，设备确认停止后才恢复 `Ready`；关闭路径保证同一会话不重复发送 Abort。
5. 扫描完成与 RAW 文件最终写盘可能存在时间差。完成状态后使用有界等待窗口检查文件，不在首次缺失时立即误报失败。

## 自动化与部署验证

- Debug CTest 覆盖严格 DLL 加载、原生路径、初始化/校准语义、状态机启动竞态、同名 RAW 覆盖、按钮状态映射和假 SDK 端到端扫描。
- Release 部署使用 `windeployqt6` 并递归补齐 UCRT64 依赖。
- 部署后的验证器在仅保留 Windows 系统 PATH 时可启动。
- 部署后的 GUI 完成 3 秒离屏启动冒烟检查。
