# 当前事实、未知项与结论边界

日期：2026-08-11。此文档是为本次 MCP 交接提炼的当前证据；包内历史项目资料仅作参考。

## 已验证事实

1. Windows 目标机上，直接 P2F CLI 已成功完成编译和 `-dump par/asm/cpp`。可执行文件位于经核验的 `<SpectrometerIDE-install-root>\bin\P2F_x32.exe`；命令形态为：

   ```text
   P2F_x32.exe -i <include_slimScope> <source.src>
   P2F_x32.exe -i <include_slimScope> -dump par <source.src>
   P2F_x32.exe -i <include_slimScope> -dump asm <source.src>
   P2F_x32.exe -i <include_slimScope> -dump cpp <source.src>
   ```

   Windows Codex 仍须重新核对本机路径、版本、SHA 和依赖，不能硬编码既有哈希。

2. GUI Compile 与直接 P2F 的核心输出已做过等价性验证。因此 MCP 编译不需要也不应启动 SpectrometerIDE。

3. 人工 SeqSimu 离线仿真链已有多次成功基线，可见逻辑 GradS/GradR/GradP/RX 等结果。

4. 双轴固定案例的 `GradP` 经过 `waveNo` 修复后已恢复 160 点 payload。末端额外零行裁决、ADC 逐点时间戳和普适序列映射仍未因此自动闭合。

5. DLL v2.26 手册记录：

   ```c
   int Simulate(int port, int tr, int loop,
                const char *simulatePath, const char *prjpath)
   ```

   同一说明还要求上层创建 Socket 服务器接收数据。因此它不是“一次 P/Invoke 即得到完整结果”的接口。

6. 历史归档 DLL 曾静态导出 `Simulate/SimulatePause/SimulateResume/SimulateAbort`。这些只证明历史二进制，不证明当前安装版本。

## 当前未知或未闭合

- 当前 Windows 安装 `mridll.dll` 的真实路径、SHA、签名、PE 位数和导出。
- 当前 DLL 的 `Simulate` ABI、调用约定、依赖搜索、Socket 消息/完成协议和错误语义。
- `Simulate()` 返回 0 与完整仿真结束之间的关系；不能把“进程创建成功”当作结果成功。
- 当前 DLL/SeqSimu 动态加载后是否会尝试连接谱仪或修改厂家目录。
- `tcc/mri_c` 是否有稳定、受支持、无头命令入口；实际路径和参数。
- 逻辑 GradR/GradP 到物理 Gx/Gy 的轴映射、符号、单位、比例、硬件延迟和涡流修正。
- ADC 每个采样点的真实时间戳。共享采样窗不等于逐点同步已验证。

## 证据解释规则

- v3.3 手册从 v3.0/DLL 7.2.0 起删除部分“内部函数接口说明”。手册没有 `Simulate` 不能被解释为导出一定不存在，也不能被解释为仍受官方支持。
- 历史 DLL、EXE、SDK ZIP 不得复制到现场安装目录，也不得替换当前 DLL。
- `SeqSimuEnd=1`、`ERROR=0`、必需通道齐全、数值检查通过以及输入/输出哈希同属成功证据；缺一项就不能称仿真通过。
- 解析器产生的是逻辑相对积分。除非物理校准证据完成，否则结论边界停在“可用于序列时序/形状检查”。
