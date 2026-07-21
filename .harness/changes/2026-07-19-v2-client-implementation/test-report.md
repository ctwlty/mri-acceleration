# Test Report

| Gate | Command | Result | Evidence |
| --- | --- | --- | --- |
| configure | `cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)"` | Pass | Build files generated |
| build | `cmake --build build -j` | Pass | `scenario_nmr_client` linked successfully |
| project-build | `cmake --build build -j` in `/Users/martin/Project/nuclear_system/client` | Pass | Target project build reached 100% |

## Skipped Gates

- Real SDK runtime: Mac cannot load Windows `mridll.dll` and真实采集仍保持 HOLD。
- Visual screenshot: 本轮验证到 Qt 编译通过；完整视觉验收下一轮用原型或本机窗口检查。
