# Affected Files

| File | Reason |
| --- | --- |
| `client/src/app/MriSdkLoader.h` | SDK 动态加载与 demo 回退 |
| `client/src/app/MriSdkLoader.cpp` | 真实 DLL 绑定和 demo 逻辑 |
| `client/src/app/DeviceBridge.h` | 设备桥接接口重构 |
| `client/src/app/DeviceBridge.cpp` | 设备控制与状态同步 |
| `client/src/app/MainWindow.h` | SDK 状态展示 |
| `client/src/app/MainWindow.cpp` | SDK 加载按钮、状态与默认初始化 |
| `client/CMakeLists.txt` | 新增 loader 源文件 |
| `client/README.md` | 测试步骤 |
