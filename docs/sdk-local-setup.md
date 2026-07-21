# SDK 本地放置约定

本仓库不提交大体积 SDK、DLL、设备运行目录、扫描原始数据和构建产物。不同电脑如果已经具备 SDK，可以直接在本机使用。

## 推荐目录

Windows 实机调试时，将 SDK 放在项目外或本地忽略目录中，例如：

```text
C:\NMRSDK\
D:\NMRSDK\
<repo>\local_sdk\
```

客户端运行时通过界面的 `加载 SDK` 选择本机 `mridll.dll`。

## 不提交内容

- `mridll.dll`
- `NMRDLL.dll`
- `*.lib`
- `*.exe`
- `*.pdb`
- `bin/`
- `obj/`
- `client/build/`
- `rawData/`
- `*.raw`
- `*.fid`
- `*.nii.gz`
- `Log/`

## 当前策略

源码仓库只保留：

- Qt/C++ 客户端源码
- C# SDK 包装层源码
- `testDLL` 示例源码
- 产品/技术文档
- Harness 变更记录

真实设备 SDK 文件由每台电脑本地准备，不进入 Git。
