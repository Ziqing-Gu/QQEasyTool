# Building QQEasyTool / 编译说明

CMake >= 3.22; C++17; JUCE 8.0.13; Visual Studio 2022 or Xcode; recursive ARA SDK submodules.

Windows 交付复用本机 Plan A 成品；Plan C 默认仅构建三类 Mac 成品。Windows delivery reuses the validated local Plan A build; Actions defaults to three Mac jobs. Windows CI is opt-in with build_windows=true.

## 中文

- CMake 3.22 或更高版本
- 支持 C++17 的编译器
- JUCE
- Celemony ARA SDK（仓库中的 `external/ARA_SDK` 子模块）

克隆仓库及子模块：

```bash
git clone --recurse-submodules https://github.com/Ziqing-Gu/QQEasyTool.git
```

Windows VST3：

```powershell
cmake -S . -B build -A x64 -DQQDEBREATH_COPY_AFTER_BUILD=OFF
cmake --build build --config Release --target QQEasyTool_VST3
```

macOS VST3 / AU：

```bash
cmake -S . -B build -G Xcode -DQQDEBREATH_COPY_AFTER_BUILD=OFF
cmake --build build --config Release --target QQEasyTool_VST3
cmake --build build --config Release --target QQEasyTool_AU
```

Windows VST3 的标准安装位置通常是：

```text
C:\Program Files\Common Files\VST3\QQEasyTool.vst3
```

macOS 的标准安装位置通常是：

```text
/Library/Audio/Plug-Ins/VST3/QQEasyTool.vst3
/Library/Audio/Plug-Ins/Components/QQEasyTool.component
```

## English

- CMake 3.22 or newer
- A compiler with C++17 support
- JUCE
- Celemony ARA SDK (the `external/ARA_SDK` submodule)

Clone the repository and its submodules:

```bash
git clone --recurse-submodules https://github.com/Ziqing-Gu/QQEasyTool.git
```

Windows VST3:

```powershell
cmake -S . -B build -A x64 -DQQDEBREATH_COPY_AFTER_BUILD=OFF
cmake --build build --config Release --target QQEasyTool_VST3
```

macOS VST3 / AU:

```bash
cmake -S . -B build -G Xcode -DQQDEBREATH_COPY_AFTER_BUILD=OFF
cmake --build build --config Release --target QQEasyTool_VST3
cmake --build build --config Release --target QQEasyTool_AU
```

The standard Windows VST3 installation path is usually:

```text
C:\Program Files\Common Files\VST3\QQEasyTool.vst3
```

The standard macOS installation paths are usually:

```text
/Library/Audio/Plug-Ins/VST3/QQEasyTool.vst3
/Library/Audio/Plug-Ins/Components/QQEasyTool.component
```
