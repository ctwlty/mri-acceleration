# Windows Qt 6 构建工具链

本项目使用 MSYS2 UCRT64 的 64 位 Qt 工具链，避免与 Anaconda 自带的 Qt 5 混用。

## 已验证版本

| 组件 | 版本 |
| --- | --- |
| GCC | 16.1.0 (`mingw-w64-ucrt-x86_64-gcc 16.1.0-5`) |
| CMake | 4.4.0 (`mingw-w64-ucrt-x86_64-cmake 4.4.0-1`) |
| Ninja | 1.13.2 (`mingw-w64-ucrt-x86_64-ninja 1.13.2-1`) |
| Qt | 6.11.1 (`mingw-w64-ucrt-x86_64-qt6-base 6.11.1-1`) |
| Qt prefix | `C:\msys64\ucrt64` |

## 安装

```powershell
winget install --id MSYS2.MSYS2 --exact --accept-package-agreements --accept-source-agreements --silent
& 'C:\msys64\usr\bin\bash.exe' -lc 'pacman -Syu --noconfirm'
& 'C:\msys64\usr\bin\bash.exe' -lc 'pacman -Syu --noconfirm'
& 'C:\msys64\usr\bin\bash.exe' -lc 'pacman -S --needed --noconfirm mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-qt6-base'
```

## 配置与构建

```powershell
& 'C:\msys64\ucrt64\bin\cmake.exe' -S client -B client/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
& 'C:\msys64\ucrt64\bin\cmake.exe' --build client/build
& 'C:\msys64\ucrt64\bin\ctest.exe' --test-dir client/build --output-on-failure
```

构建和运行时让 `C:\msys64\ucrt64\bin` 位于 `PATH` 前部，以确保加载 Qt 6 UCRT64 运行库。
