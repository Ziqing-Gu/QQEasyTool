# QQEasyTool

[中文](#中文) | [English](#english)
当前版本 / Current version: **1.03**

## 1.03 更新 / What''s new in 1.03

### 中文

- **修复 ARA 工程重开后的爆音/噼啪声：** 工程状态恢复后，音频源由后台线程预加载到不可变缓存。
- **禁止实时音频线程直接读取 ARA 音源：** 缓存准备完成前只输出静音，避免在播放回调中发生宿主音频读取、锁竞争和磁盘/解码操作。
- **适配多音源和已保存工程状态：** 恢复的每个 ARA source 都会按 fingerprint 独立缓存，缓存完成后再安全进入实时渲染。
- **版本说明：** 未改变 Breath、Noise、Others、监听、EQ、Gain 或分析规则。

### English

- **Fixed crackle after reopening an ARA project:** Restored ARA sources are now preloaded by a background worker into immutable audio caches.
- **Removed realtime ARA source reads:** The audio callback never reads host source data, waits for a reader, or performs large source I/O. It outputs silence until the cache is ready.
- **Supports restored multi-source state:** Each restored ARA source is cached independently by fingerprint before safe realtime rendering.
- **Behavior preserved:** Breath, Noise, Others, monitoring, EQ, Gain, and analysis rules were not changed.
## 1.02 更新 / What's new in 1.02

### 中文

- **修复 macOS ARA 首次打开的界面尺寸握手：** 针对部分 Fender Studio / Studio One 环境，编辑器首次嵌入时只显示在左侧、需要拖动宿主窗口才恢复的问题，增加了有限次数的延迟宿主布局同步。
- **兼容宿主晚到的尺寸和缩放信息：** ARA 编辑器在挂入宿主层级、首次显示以及宿主发送缩放因子后重新布局。
- **音频行为不变：** 没有修改 Breath、Noise、Others、监听、EQ、Gain、分析或工程状态逻辑。

- **厂商标识保持为 Qing Audio：** 延续此前的统一厂商名称，并保留稳定的插件身份码。

### English

- **Fixed the first-open macOS ARA layout handshake:** On some Fender Studio / Studio One environments, the editor could appear only in the left part of the host panel until the host window was dragged. The editor now performs a bounded deferred host-layout synchronization.
- **Handles late host bounds and scale information:** The ARA editor resynchronizes after hierarchy attachment, first visibility, and host scale-factor delivery.
- **Audio behavior unchanged:** Breath, Noise, Others, monitoring, EQ, Gain, analysis, and project-state logic were not changed.
- **Manufacturer identity:** The plug-in remains published as Qing Audio with stable plug-in identity codes.


## 1.01 更新 / What's new in 1.01

### 中文

- **监听通道隔离：** 修复关闭 Voice、Breath、Noise 或 Others 监听后，在特定实时锁竞争情况下仍可能漏出未处理声音的问题。
- **多实例状态隔离：** 每个 QQEasyTool 插件实例使用自己的监听状态，互不影响。
- **多轨 ARA 绑定：** 每个插件实例只显示和处理分配给自己的 ARA 播放区域，不再回退到工程中的第一个音源。
- **每音源处理状态：** Breath Norm、Target、全局 Gain、全局 EQ、区域 Gain/EQ 和监听设置会跟随对应 ARA 音源保存与恢复。
- **回归验证：** 继续验证 UI 关闭/重开、Breath/Noise/Others 监听、Norm、Gain、全局 EQ、区域 EQ 和区域类型记忆。

### English

- **Monitor-channel isolation:** Fixed a real-time lock-contention path that could leak unprocessed audio after Voice, Breath, Noise, or Others monitoring was disabled.
- **Per-instance isolation:** Each QQEasyTool instance now keeps independent monitoring state.
- **Multi-track ARA binding:** Each plug-in instance displays and processes only its assigned ARA playback regions instead of falling back to the first source in the project.
- **Per-source processing state:** Breath Norm, Target, global Gain, global EQ, region Gain/EQ, and monitor settings are stored and restored with the corresponding ARA source.
- **Regression coverage:** Retained checks for editor recreation, Breath/Noise/Others monitoring, Norm, Gain, global EQ, region EQ, and remembered region types.

## 0.99 更新 / What's new in 0.99

### 中文

- **UI 状态保持：** 修复关闭插件 UI 后再次打开时，Norm、Target、全局 Gain 和全局 EQ 被默认值覆盖的问题。
- **项目状态优先：** 同一插件实例重新打开编辑器时，会保留 DAW 中的实时参数和项目状态。
- **区域处理保持：** 区域独立 Gain 和独立 EQ 在 UI 重新打开后也会保持不变。
- **回归验证：** 增加编辑器关闭/重开测试，并继续验证监听、Norm、Gain、全局 EQ、区域 EQ 和区域类型记忆。

### English

- **Persistent UI state:** Fixed global defaults overwriting Norm, Target, global Gain, and global EQ after the plug-in UI was closed and reopened.
- **Project state takes priority:** Reopening the editor for the same plug-in instance now preserves live DAW parameters and project state.
- **Region processing is retained:** Per-region Gain and EQ remain unchanged across editor recreation.
- **Regression coverage:** Added an editor close/reopen test while retaining checks for monitoring, Norm, Gain, global EQ, region EQ, and remembered region types.

## 中文

### 项目简介

QQEasyTool 是在 [QQDeBreathTool 的 ARA-VST3-AU 版本](https://github.com/Ziqing-Gu/QQDeBreath-ARA-VST3-AU)基础上制作的实验性音频编辑插件。

相较于 QQDeBreathTool，目前 QQEasyTool 的主要扩展是增加 `Others` 区域。用户可以把喷麦、特殊噪声或任何需要单独处理的声音标记为 Others，并为每个 Others 区域独立设置 EQ 和 Gain。

当前版本为 `1.03`，仍处于测试阶段。现阶段 Analyze 只自动检测 Breath；齿音（Sibilance）的自动检测和处理尚未包含在当前版本中。未来希望加入自动检测和处理齿音的能力，只是目前还没有足够的时间继续开发这部分功能。

### 当前功能

- 支持 ARA 工作流，并可构建 VST3；macOS 工程同时包含 AU/ARA 构建目标。
- Analyze 当前只自动检测 Breath。
- Breath 支持全局 Norm、可调 Target、全局 Gain 和全局 EQ。
- 每个 Breath 区域支持独立 Gain 和独立 EQ。
- `Others` 是用户自定义区域，每个区域支持独立 Gain 和独立 EQ，但没有全局处理。
- `Noise`、`Breath` 和 `Others` 使用不同颜色显示；点击区域会显示当前区域类型。
- 支持分别监听 Voice、Noise、Breath 和 Others。
- 支持移动区域、删除区域、切换区域类型，以及 `Ctrl+Z` / `Ctrl+Shift+Z` 撤销与重做。
- Shift 拖动画新区域时，会沿用上一次选择或切换的区域类型。
- Global EQ 与 Region EQ 使用一致的紧凑布局，并共享 Auto Apply 状态。

### 齿音功能计划

曾经在 0.94 实验分支中尝试过基于自适应频谱规则的齿音检测。由于目前识别准确率还不够稳定，0.95 之后的主线版本取消了 Sibilance 区域和自动齿音分析，只保留 Breath、Noise 和 Others。

如果未来继续开发齿音功能，应从保存的 0.94 Sibilance 基础版本继续研究，而不是直接改变当前稳定主线。

### 构建要求

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

### 重要说明

- 这是测试版本，请先在工程副本中验证，不建议直接用于不可恢复的重要项目。
- 全局默认预设只应应用于新的插件实例或用户主动执行的 Load；关闭并重开同一 UI 不应覆盖当前项目状态。
- ARA、VST3 和 AU 的宿主行为可能不同，请在目标 DAW 中分别验证。
- JUCE 和 ARA SDK 各自受其许可证约束；分发前请确认相关授权条件。

## English

### Overview

QQEasyTool is an experimental audio editing plug-in built on the [ARA-VST3-AU edition of QQDeBreathTool](https://github.com/Ziqing-Gu/QQDeBreath-ARA-VST3-AU).

Compared with QQDeBreathTool, the main extension currently provided by QQEasyTool is the `Others` region type. Users can mark plosives, unusual noises, or any sound requiring special treatment as Others, then apply independent EQ and Gain to each Others region.

The current version is `1.03` and remains in testing. Analyze currently detects Breath only. Automatic sibilance detection and processing are not included in the current version. The long-term goal is to add automatic sibilance detection and treatment, but there has not yet been enough development time to complete that feature.

### Current Features

- Supports an ARA workflow and VST3 builds; the macOS project also provides an AU/ARA target.
- Analyze currently detects Breath only.
- Breath provides global Norm, an adjustable Target, global Gain, and global EQ.
- Every Breath region can have independent Gain and EQ.
- `Others` is a user-defined region type. Each region can have independent Gain and EQ, without global processing.
- `Noise`, `Breath`, and `Others` use different colors, and selecting a region displays its type.
- Voice, Noise, Breath, and Others can be monitored independently.
- Regions can be moved, deleted, or cycled between types, with `Ctrl+Z` / `Ctrl+Shift+Z` undo and redo.
- Shift-drag creation reuses the most recently selected or cycled region type.
- Global EQ and Region EQ use matching compact layouts and share the Auto Apply state.

### Sibilance Roadmap

Version 0.94 previously experimented with adaptive spectral rules for sibilance detection. Because the detection accuracy was not yet reliable enough, the main line removed the Sibilance region and automatic sibilance analysis from version 0.95 onward, retaining Breath, Noise, and Others.

Future sibilance research should continue from the archived 0.94 Sibilance base instead of directly changing the current stable main line.

### Build Requirements

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

### Important Notes

- This is a test build. Validate it in a copy of your project before using it in work that cannot be recovered.
- Global defaults should apply only to a fresh plug-in instance or an explicit Load action. Closing and reopening the same UI must not overwrite the current project state.
- ARA, VST3, and AU host behavior can differ, so each target DAW should be tested separately.
- JUCE and the ARA SDK are governed by their respective licenses. Confirm the applicable terms before distribution.
