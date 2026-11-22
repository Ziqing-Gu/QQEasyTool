# QQEasyTool

## 1. 产品介绍 / Product

Qing Audio / Ziqing Gu · **1.09 Stable** · 2026-11-22

基于 [QQDeBreath ARA/VST3/AU](https://github.com/Ziqing-Gu/QQDeBreath-ARA-VST3-AU) 的人声区域编辑插件，增加 Others 区域与独立 Gain/EQ。完整功能公开体验，无期限或功能限制。Analyze 当前只自动识别 Breath，自动齿音识别尚未提供。

A vocal-region editor derived from QQDeBreath ARA/VST3/AU, adding Others regions and individual Gain/EQ. Full functionality for public feedback without expiry or feature limits. Analyze currently detects Breath only; automatic sibilance detection is not included.

### 功能

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

### Features

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

后续插件维护集中于 QQEasyTool；QQDeBreath 插件保留 1.29，独立版另行维护。Future plug-in development focuses on QQEasyTool; DeBreath plug-ins remain at 1.29 and standalone development is separate.

## 2. 许可证 / License

### ⚠️ 禁止商业使用 / NO COMMERCIAL USE

### Qing Audio 非商业源码共享许可证 1.0

### 本项目源码公开，但不属于 OSI 认可的开源软件

> **禁止任何商业使用。** 仅允许个人、学习、教育、研究、评估、爱好及其他非商业用途。发布原版、二进制版或修改版时，必须同时免费公开完整对应源代码，保留作者、版权和许可证声明，醒目标明原项目名称、作者、来源链接、修改者、修改日期及修改内容，并使整个修改版继续采用同一许可证。完整条款见 [LICENSE](LICENSE)。
>
> **NO COMMERCIAL USE.** Use is permitted only for personal, educational, research, evaluation, hobby, charitable, and other non-commercial purposes. Any distributed original, binary, or modified version must provide the complete corresponding source without charge, preserve authorship, copyright, and license notices, prominently identify the original project, author, source URL, modifier, date, and changes, and license the entire modified work under the same terms. See [LICENSE](LICENSE).
>
> 许可证政策变更与后续 AI 维护说明见 [LICENSE_POLICY_CHANGE.md](LICENSE_POLICY_CHANGE.md)。 / See [LICENSE_POLICY_CHANGE.md](LICENSE_POLICY_CHANGE.md) for the policy record and future AI maintenance instructions.


本项目第一方源码采用 **Qing Audio 非商业源码共享许可证 1.0**（`LicenseRef-Qing-Audio-NC-Source-Share-1.0`）。禁止任何商业使用。发布原版、二进制版或修改版时，必须同时免费公开完整对应源代码，保留作者、版权与许可证声明，注明原项目、作者、来源链接、修改者、修改日期及修改内容，并使整个修改版继续采用同一许可证。完整条款见 [LICENSE](LICENSE)。第三方组件继续适用其各自许可证。

This project's first-party source is licensed under the **Qing Audio Non-Commercial Source-Share License 1.0** (`LicenseRef-Qing-Audio-NC-Source-Share-1.0`). Commercial use is prohibited. Distribution of the original, binary, or modified version requires the complete corresponding source at no charge, preserved authorship, copyright, and license notices, prominent identification of the original project, author, source URL, modifier, date, and changes, and the same license for the entire modified work. See [LICENSE](LICENSE). Third-party components remain under their respective licenses.

Previously distributed copies retain rights already granted; this License applies to copies supplied with it.


## 3. 最新下载与说明书 / Downloads and manuals

当前公开版本 / Current public release: **1.09 Stable** · Installer delivery updated 2026-11-22

[**最新版 Release / Latest release**](https://github.com/Ziqing-Gu/QQEasyTool/releases/latest)

| 下载 / Download | 选择说明 / Selection |
|---|---|
| [Windows 安装包 / Installer](https://github.com/Ziqing-Gu/QQEasyTool/releases/download/v1.09/QQEasyTool-1.09-Windows-x64-Setup.exe) | Windows 10/11 x64 · VST3 / ARA |
| [Intel Mac 安装包 / Installer](https://github.com/Ziqing-Gu/QQEasyTool/releases/download/v1.09/QQEasyTool-1.09-macOS-Intel-Setup.pkg) | macOS 11+ · Intel VST3 + Universal 2 AU |
| [Apple Silicon Mac 安装包 / Installer](https://github.com/Ziqing-Gu/QQEasyTool/releases/download/v1.09/QQEasyTool-1.09-macOS-Apple-Silicon-Setup.pkg) | macOS 11+ · arm64 VST3 + Universal 2 AU |
| [安装包说明（中文）](https://github.com/Ziqing-Gu/QQEasyTool/releases/download/v1.09/QQEasyTool-1.09-Installer-Guide-ZH-CN.txt) | 安装、升级、格式选择与卸载 |
| [Installer Guide (English)](https://github.com/Ziqing-Gu/QQEasyTool/releases/download/v1.09/QQEasyTool-1.09-Installer-Guide-EN.txt) | Installation, upgrade, format selection and removal |
| [图文用户手册（中文，1.08 版）](https://github.com/Ziqing-Gu/QQEasyTool/releases/download/v1.09/QQEasyTool-1.08-User-Manual-ZH-CN.pdf) | 操作适用于 1.09；安装以新版安装包说明为准 |
| [Illustrated Manual (English, 1.08 edition)](https://github.com/Ziqing-Gu/QQEasyTool/releases/download/v1.09/QQEasyTool-1.08-User-Manual-EN.pdf) | Operations apply to 1.09; use the new installer guide for installation |
| [插件对应完整源码 / Complete plug-in source](https://github.com/Ziqing-Gu/QQEasyTool/releases/download/v1.09/QQEasyTool-1.09-Complete-Source.zip) | 原 1.09 插件源码与依赖；安装器源码见下方 / Installer sources below |

直接运行 Windows EXE 或 Mac PKG；Mac 可在“自定 / Customize”中选择 VST3、AU 或两者。Apple Silicon 原生宿主选择 Apple-Silicon；Intel Mac 或 Rosetta Intel 宿主选择 Intel。两包的 AU 都是 Universal 2。两个 VST3 架构使用同一安装位置，安装时选择其一。安装前保存工程并完全退出 DAW，完成后重新扫描插件。

Run the Windows EXE or Mac PKG. Mac Customize offers VST3, AU or both. Choose Apple-Silicon for native M-series hosts, or Intel for Intel Macs / Rosetta Intel hosts. Both include the same Universal 2 AU. The two VST3 architectures share one destination; choose one. Save sessions, fully quit hosts before installation, and rescan afterward.

Windows EXE 未进行 Authenticode 签名；Mac PKG 未签名、未公证，插件保留原有 ad-hoc 签名。系统首次打开的提示见双语安装包说明。最低系统：Windows 10/11 x64、macOS 11。不提供 Linux 或 iOS 版本。

The Windows EXE is not Authenticode signed. Mac PKGs are unsigned and not notarized; plug-ins retain their original ad-hoc signatures. See the installer guides for first-opening prompts. Minimum systems: Windows 10/11 x64 and macOS 11. Linux and iOS are not provided.

图文手册保留原始 1.08 版文件名和内容，界面操作仍适用于 1.09；安装方式以新版安装包说明为准。 / The illustrated manuals retain their original 1.08 edition and still apply to 1.09 operations; follow the new installer guides for installation.

插件源码 / Plug-in source: [`d44e0923e8fa01157391fe6e6bc0016f70616090`](https://github.com/Ziqing-Gu/QQEasyTool/tree/d44e0923e8fa01157391fe6e6bc0016f70616090) · tag `v1.09`

安装器源码 / Installer source: [`591fb772041f996b6e6d55ecdbb5cb5145af7345`](https://github.com/Ziqing-Gu/QQEasyTool/tree/591fb772041f996b6e6d55ecdbb5cb5145af7345/packaging/installers) · [打包和安装验证 / Packaging and installation checks](https://github.com/Ziqing-Gu/QQEasyTool/actions/runs/45027696522)

Windows 与两类 Mac 安装包内的插件均沿用已发布的 1.09 成品，字节核验一致，未重新编译。Windows 全新安装、实际 1.08 升级、锁定拒绝/重试与卸载通过；两类 Mac 的 AU 单选、双格式安装、模拟旧元数据升级及遗留资源替换通过。此安装检查不等于真实 DAW 工程验收。

All installers reuse the published 1.09 plug-ins byte for byte, without recompiling. Windows fresh installation, actual 1.08 upgrade, locked-file rejection/retry and uninstall passed. Both Mac architectures passed AU-only selection, both-format installation, simulated older-metadata upgrade and stale-resource replacement checks. These checks do not imply real DAW project acceptance.

GitHub 自动源码 ZIP/TAR 不是安装包。 / GitHub automatic source archives are not installers.

[构建 / Build](docs/build.md) · [安装器打包 / Installer packaging](docs/installers/README.md) · [开发交接 / Handoff](AI_DEVELOPMENT_HANDOFF.md)

## 4. 逐版本更新 / Version history

### 1.09 Stable — 2026-11-22 / ARA window layout

安装包交付更新：新增 Windows EXE、Intel Mac PKG 和 Apple Silicon Mac PKG。Mac 可选 VST3/AU，沿用同一 1.09 插件成品，不改变 Stable、版本或已冻结备份。 / Installer delivery update: Windows EXE plus Intel and Apple Silicon Mac PKGs, with selectable VST3/AU on Mac. Existing 1.09 plug-in binaries, Stable status and frozen backup remain unchanged.

中文：恢复 Easy Tool 编辑器定时器中遗漏的 ARA 窗口尺寸同步调用。首次挂载、重开窗口和宿主缩放改变后，已有的有限次数同步现在能够执行，修复延迟尺寸协商造成的右侧截断。只在第 1/4 次检查触发一像素协商，第 8 次停止。音频处理、分析模型、参数、项目状态格式和操作方法保持不变。

English: Restores the missing editor-timer call to the existing bounded ARA host-layout handshake. Attachment, reopening and host scale changes can now complete deferred size negotiation, addressing right-side clipping. Two one-pixel nudges occur on checks 1/4 and synchronization stops on check 8. Audio processing, analysis, parameters, project format and user workflows are unchanged.

验证 / Validation: 新回归在原 1.08 上失败，在 1.09 上通过；隐藏原生测试宿主覆盖 2596 × 760 面板、125%/150%/200% 缩放、重开和空闲稳定性，ARA 选择/历史/后台分析与音频定位回归通过。The new regression fails on the unchanged 1.08 editor and passes on 1.09. Hidden-native-host layout, ARA selection/history/background-analysis and transport checks pass. 用户指定 Stable，不代表网友原 Studio One 工程或 Mac DAW 的实机验收。User-designated Stable does not imply acceptance in the reporter's Studio One project or a real Mac DAW.

### 1.08 Stable — Background ARA analysis and faster sparse-audio analysis / 2026-10-27

中文：修正上一版把 ARA 分析绑定到编辑器、切换音源时取消分析的问题。分析现在由 ARA 文档按音源管理，切轨或关闭插件窗口后继续，完成后只写回原音源。另一轨可启动独立任务，两条低优先级工作线程由同一插件的实例共享，额外任务排队。复用已有原始音频缓存；工作线程不调用宿主音频读取或修改 ARA 模型。文档定时器在模型线程提交结果，保留各音源参数和分析前的撤销状态；显式取消、宿主归档恢复、音源内容失效和手工区域修改阻止过期结果覆盖。组合选择从当前音源结果刷新，避免旧组合快照遮住后台完成结果。进度显示排队状态或已耗时，不再使用估算百分比。

English: ARA analysis is now owned by the document/source instead of the editor. Switching tracks or closing the editor keeps queued/running work alive; completion is committed to the originating source. Two low-priority workers are shared across instances of this plug-in, with additional jobs queued. Workers use immutable cached audio and never read host audio or mutate the ARA model. A document timer commits on the model thread, preserving per-source parameters and pre-analysis Undo. Explicit cancellation, host archive restore, invalidated audio and manual region edits reject stale work. Group views rebuild from live source regions. The UI shows queued/elapsed time.

性能 / Performance: Native analysis uses exact order-statistic selection instead of full-array sorting, reuses computed spectral statistics for fully digital-silent frames, and suppresses denormal arithmetic in filter tails. Low-level nonzero audio still receives full analysis. Detection model, thresholds and region rules remain unchanged. Ordinary VST3/AU uses the same optimized analyzer; background continuity across ARA track selection applies to ARA mode.

验证 / Validation: Eight original user WAVs were compared against the previous analyzer for each product (16 comparisons total). All region types and start/end sample/time boundaries matched. Overall measured speedup was 1.40–5.47x. These are native analyzer timings, not Cubase end-to-end UI timings. Input SHA-256 hashes were rechecked unchanged. The deterministic ARA regression includes two queued source jobs surviving selection/editor destruction, document-timer completion, group refresh, per-source parameters, cached input with no host reread, pre-analysis Undo, explicit Cancel and host archive Undo rejection. Existing selection/history, multi-instance audio, transport, waveform and product-specific probes pass. Audio processBlock and the ARA playback renderer are unchanged; factory load, internal/PE/moduleinfo 1.0.8 and installed SHA-256 parity pass.


### 1.07 — Cubase ARA selection, Reload and edit history / 2026-10-27

中文：ARA 编辑器响应宿主事件/轨道选择并切换对应波形、区域和参数，无需关闭窗口。显式 EditorView 选择不再被播放实例的区域归属过滤；选择改变跳过旧的一秒恢复间隔，直接使用已有音频缓存显示波形。Reload 保留现有编辑和参数。插件内部撤销/重做现覆盖区域编辑、Gain、Norm、EQ 和监听参数；历史按音源保存在处理器/ARA 文档中，Reload、切轨和重开窗口后仍可用。未应用预览先回滚，切轨前保存待提交修改；旧分析请求的迟到结果不会覆盖新轨。

English: The ARA editor follows host event/track selection without closing its window, restoring the selected source's waveform, regions and parameters. Explicit EditorView selection takes priority over playback-renderer assignment; selection changes bypass the previous retry delay and reuse cached source audio. Reload preserves edits. Internal Undo/Redo covers region edits, Gain, Norm, EQ and monitor parameters, with per-source history owned by the processor/ARA document rather than an editor window. Unapplied previews roll back, pending edits flush before switching, and stale analysis completions cannot replace a different source.

宿主集成 / Host integration: ARA edits queue source/modification content notifications; host archive Undo/Redo refreshes an already-open editor without echoing the restored state as a new edit. Ordinary VST3/AU custom-state edits notify the host through JUCE, and host state restore refreshes the editor. Exact Cubase history-panel entries and host-specific Undo grouping have not been validated in the user's real project. Native host history remains host-dependent; the plugin's own Undo/Redo works independently.

验证 / Validation: Real ARA EditorView regression covers immediate selection callbacks, distinct A/B waveforms, 20 rapid switches, sequence-only selection, two views, multi-event selection, pending/preview edits, unavailable source retry, late analysis rejection, Reload/reopen Undo/Redo and filtered host archive Undo/Redo with no feedback notifications. Ordinary shared editor tests cover region/Gain/Norm/EQ history, reopen, custom-state notifications and host state restore. Existing audio/transport/waveform/ARA multi-instance probes pass; processBlock and ARA renderer code remain unchanged. Windows VST3 factory load, PE/moduleinfo 1.0.7 and installed SHA-256 parity pass.



中文：修复同一 ARA 文档超过 32 个音源后最早素材被淘汰、渲染静音，以及恢复存档时清空全部有效音频缓存的问题。音源在宿主模型线程预载，按源的失活、销毁和内容更新管理缓存；不再由后台线程访问宿主模型。局部存档只恢复指定音源，支持持久 ID 重映射、工程级独立存档与未分析音源；离线渲染不再依赖 Playing 标志。编辑器复用已有缓存。保留 Voice/Breath/Noise/Others 路由、旧 Sibilance 转 Others、重叠混音、Norm/Gain/EQ、1.04 普通 VST3 定位和 1.05 界面响应优化。

English: Fixed older sources being evicted after an ARA document exceeds 32 sources, causing silent rendering, and valid raw audio being discarded during archive restore. Audio preloads on the host model thread and follows source deactivation, destruction and content updates; background threads no longer traverse host model objects. Filtered restore preserves unrelated sources and handles persistent-ID remapping, document-only archives and untouched sources. Offline render does not require Playing; the editor reuses cached audio. Voice/Breath/Noise/Others routing, legacy Sibilance-to-Others mapping, overlap mixing, Norm/Gain/EQ, 1.04 ordinary VST3 transport and 1.05 display responsiveness are preserved.

验证 / Validation: Unmodified 1.05 reproduced 32 audible / 12 silent sources from 22 renderers and 44 sources; archive restore discarded all 22 cached sources. 1.06 renders all 44 with no silent sources, retains valid caches, performs zero host reads during concurrent rendering, and passes source lifecycle/restore checks. 96 ARA DSP comparisons against frozen EasyTool 1.05 match sample-for-sample. Existing routing/state/ARA input, transport and 30 waveform reference cases pass. Actual VST3 loading and internal version 1.0.6 are verified. These automated checks are not original Studio One project acceptance.

边界 / Limits: Full-source caching scales with active audio duration and channel count. Initial loading and undo reactivation can wait for synchronous preloading. Failed reads never publish partial buffers and retry at subsequent model/load events. Previously rendered silent files require rerendering from original events. Archive schema and ordinary VST3 state format remain compatible.



### 1.06 Stable — 2026-10-24 / ARA multi-source cache and restore

中文：修复同一 ARA 文档超过 32 个音源后最早素材被淘汰、渲染静音，以及恢复存档时清空全部有效音频缓存的问题。音源在宿主模型线程预载，按源的失活、销毁和内容更新管理缓存；不再由后台线程访问宿主模型。局部存档只恢复指定音源，支持持久 ID 重映射、工程级独立存档与未分析音源；离线渲染不再依赖 Playing 标志。编辑器复用已有缓存。保留 Voice/Breath/Noise/Others 路由、旧 Sibilance 转 Others、重叠混音、Norm/Gain/EQ、1.04 普通 VST3 定位和 1.05 界面响应优化。

English: Fixed older sources being evicted after an ARA document exceeds 32 sources, causing silent rendering, and valid raw audio being discarded during archive restore. Audio preloads on the host model thread and follows source deactivation, destruction and content updates; background threads no longer traverse host model objects. Filtered restore preserves unrelated sources and handles persistent-ID remapping, document-only archives and untouched sources. Offline render does not require Playing; the editor reuses cached audio. Voice/Breath/Noise/Others routing, legacy Sibilance-to-Others mapping, overlap mixing, Norm/Gain/EQ, 1.04 ordinary VST3 transport and 1.05 display responsiveness are preserved.

验证 / Validation: Unmodified 1.05 reproduced 32 audible / 12 silent sources from 22 renderers and 44 sources; archive restore discarded all 22 cached sources. 1.06 renders all 44 with no silent sources, retains valid caches, performs zero host reads during concurrent rendering, and passes source lifecycle/restore checks. 96 ARA DSP comparisons against frozen EasyTool 1.05 match sample-for-sample. Existing routing/state/ARA input, transport and 30 waveform reference cases pass. Actual VST3 loading and internal version 1.0.6 are verified. These automated checks are not original Studio One project acceptance.

状态 / Status: Stable by explicit user instruction. Previous designated Stable: 1.04; immediate installed rollback: 1.05. This release contains Windows x64 VST3 only, as requested. No 1.06 macOS artifacts were built or reused from older versions. Original historical Candidate records remain historical; no new Candidate stage is used.

边界 / Limits: Full-source caching scales with active audio duration and channel count. Initial loading and undo reactivation can wait for synchronous preloading. Failed reads never publish partial buffers and retry at subsequent model/load events. Previously rendered silent files require rerendering from original events. Archive schema and ordinary VST3 state format remain compatible.


### 1.05 Candidate — 2026-09-22 / Gain, Norm Target and EQ responsiveness

中文：移植 QQDeBreath 1.25 已验收的交互性能优化。Gain、Norm Target 直接缩放波形缓存；全局和区域 EQ 的波形计算使用可取消后台线程，只采用最新结果。缓存区域峰值和绘制范围，动态频谱只处理当前窗口涉及的区域，连续拖动不再触发全文件静态频谱计算。ARA 区域参数更新限频，试听更新与波形计算分离。保留 EasyTool 的 Voice/Breath/Noise/Others 分轨、旧 Sibilance 类型兼容、重叠区域行为、Auto Apply 关闭时的预览/Apply 以及 1.04 播放同步规则。

English: Ported the accepted QQDeBreath 1.25 responsiveness improvements. Gain/Norm Target scale cached display data; global/selected EQ waveforms run in a cancellable latest-request worker. Cached peaks and painting intervals avoid repeated source scans, dynamic spectra visit only the current window, and continuous dragging avoids whole-file static spectra. Selected ARA parameter updates are throttled independently of waveform calculation. EasyTool Voice/Breath/Noise/Others routing, legacy Sibilance compatibility, neutral overlap behavior, manual preview/Apply and 1.04 transport semantics are preserved.

Scope: editor/display only; processor, EQ DSP, analysis and parameter format unchanged. No new audio smoothing algorithm. EQ waveform display may update after dragging. No new macOS build or DAW acceptance is claimed. Candidate awaiting user acceptance; Stable remains 1.04.

Validation: Windows x64 VST3 build and Steinberg load check passed. 30 reference cases at 44.1/48/96 kHz, each with six Gain/Target settings plus global/selected EQ, agree with 1.04 within 2.06028e-7 relative display rounding. Scalar changes submit zero full renders. Latest EQ, source replacement/clear and worker close passed. Existing routing/state/ARA input and transport probes passed. Synthetic 120-second/500-region test: 1000 scalar updates 236.457 ms; 500 EQ submissions 182.869 ms; paint 14.0675 ms. These are internal UI entry-point measurements, not host end-to-end latency.




### 1.04 更新 / What's new in 1.04 — 2026-09-20

用户于 2026-09-20 指定此版本为 Stable。/ Designated Stable by the user on 2026-09-20.

#### 中文

- 普通 VST3 的试听和宿主同步采用 QQDeBreath 1.23 Stable 已验收的逻辑：停止时点击插件波形，按播放从所选音频位置开始。
- 停止时的选择保留至首个实际播放音频块，启动时宿主时间不同、零长度块和重复停止回调不会吞掉选择。原地停止/继续保留原有试听行为。
- 后续 DAW 重新定位或循环回跳时，清除试听偏移和内部循环，指针与实际音频恢复原始工程对齐；停止时宿主报告的新定位也会取消待播放选择。
- 普通 VST3 界面使用音频线程发布的宿主位置快照，避免跨线程直接读取播放时钟。
- 保留 EasyTool 的 ARA 输入/缓存处理、Voice/Breath/Noise/Others 监听、Norm、Gain、EQ、区域编辑及工程状态格式。

#### English

- Ordinary VST3 audition/transport synchronization now uses the user-verified QQDeBreath 1.23 Stable logic. Clicking the waveform while stopped selects the audio that starts when Play is pressed.
- A stopped selection remains pending until the first actual playing audio block. Different startup clocks, zero-length blocks, and repeated stopped callbacks do not consume it. Pause/resume in place retains audition behavior.
- Later DAW seeks and cycle wraps clear audition offsets and internal loops, restoring both the cursor and actual audio to the original timeline. A new host-reported stopped position also cancels the pending selection.
- The ordinary VST3 editor reads an audio-thread transport snapshot instead of directly querying the host clock from the UI thread.
- Preserved EasyTool's ARA input/cache processing, Voice/Breath/Noise/Others monitoring, Norm, Gain, EQ, region editing, and project-state format.


### 1.03 更新 / What's new in 1.03

#### 中文

- **修复 ARA 工程重开后的爆音/噼啪声：** 工程状态恢复后，音频源由后台线程预加载到不可变缓存。
- **禁止实时音频线程直接读取 ARA 音源：** 缓存准备完成前只输出静音，避免在播放回调中发生宿主音频读取、锁竞争和磁盘/解码操作。
- **适配多音源和已保存工程状态：** 恢复的每个 ARA source 都会按 fingerprint 独立缓存，缓存完成后再安全进入实时渲染。
- **版本说明：** 未改变 Breath、Noise、Others、监听、EQ、Gain 或分析规则。

#### English

- **Fixed crackle after reopening an ARA project:** Restored ARA sources are now preloaded by a background worker into immutable audio caches.
- **Removed realtime ARA source reads:** The audio callback never reads host source data, waits for a reader, or performs large source I/O. It outputs silence until the cache is ready.
- **Supports restored multi-source state:** Each restored ARA source is cached independently by fingerprint before safe realtime rendering.
- **Behavior preserved:** Breath, Noise, Others, monitoring, EQ, Gain, and analysis rules were not changed.

### 1.02 更新 / What's new in 1.02

#### 中文

- **修复 macOS ARA 首次打开的界面尺寸握手：** 针对部分 Fender Studio / Studio One 环境，编辑器首次嵌入时只显示在左侧、需要拖动宿主窗口才恢复的问题，增加了有限次数的延迟宿主布局同步。
- **兼容宿主晚到的尺寸和缩放信息：** ARA 编辑器在挂入宿主层级、首次显示以及宿主发送缩放因子后重新布局。
- **音频行为不变：** 没有修改 Breath、Noise、Others、监听、EQ、Gain、分析或工程状态逻辑。

- **厂商标识保持为 Qing Audio：** 延续此前的统一厂商名称，并保留稳定的插件身份码。

#### English

- **Fixed the first-open macOS ARA layout handshake:** On some Fender Studio / Studio One environments, the editor could appear only in the left part of the host panel until the host window was dragged. The editor now performs a bounded deferred host-layout synchronization.
- **Handles late host bounds and scale information:** The ARA editor resynchronizes after hierarchy attachment, first visibility, and host scale-factor delivery.
- **Audio behavior unchanged:** Breath, Noise, Others, monitoring, EQ, Gain, analysis, and project-state logic were not changed.
- **Manufacturer identity:** The plug-in remains published as Qing Audio with stable plug-in identity codes.



### 1.01 更新 / What's new in 1.01

#### 中文

- **监听通道隔离：** 修复关闭 Voice、Breath、Noise 或 Others 监听后，在特定实时锁竞争情况下仍可能漏出未处理声音的问题。
- **多实例状态隔离：** 每个 QQEasyTool 插件实例使用自己的监听状态，互不影响。
- **多轨 ARA 绑定：** 每个插件实例只显示和处理分配给自己的 ARA 播放区域，不再回退到工程中的第一个音源。
- **每音源处理状态：** Breath Norm、Target、全局 Gain、全局 EQ、区域 Gain/EQ 和监听设置会跟随对应 ARA 音源保存与恢复。
- **回归验证：** 继续验证 UI 关闭/重开、Breath/Noise/Others 监听、Norm、Gain、全局 EQ、区域 EQ 和区域类型记忆。

#### English

- **Monitor-channel isolation:** Fixed a real-time lock-contention path that could leak unprocessed audio after Voice, Breath, Noise, or Others monitoring was disabled.
- **Per-instance isolation:** Each QQEasyTool instance now keeps independent monitoring state.
- **Multi-track ARA binding:** Each plug-in instance displays and processes only its assigned ARA playback regions instead of falling back to the first source in the project.
- **Per-source processing state:** Breath Norm, Target, global Gain, global EQ, region Gain/EQ, and monitor settings are stored and restored with the corresponding ARA source.
- **Regression coverage:** Retained checks for editor recreation, Breath/Noise/Others monitoring, Norm, Gain, global EQ, region EQ, and remembered region types.


### 0.99 更新 / What's new in 0.99

#### 中文

- **UI 状态保持：** 修复关闭插件 UI 后再次打开时，Norm、Target、全局 Gain 和全局 EQ 被默认值覆盖的问题。
- **项目状态优先：** 同一插件实例重新打开编辑器时，会保留 DAW 中的实时参数和项目状态。
- **区域处理保持：** 区域独立 Gain 和独立 EQ 在 UI 重新打开后也会保持不变。
- **回归验证：** 增加编辑器关闭/重开测试，并继续验证监听、Norm、Gain、全局 EQ、区域 EQ 和区域类型记忆。

#### English

- **Persistent UI state:** Fixed global defaults overwriting Norm, Target, global Gain, and global EQ after the plug-in UI was closed and reopened.
- **Project state takes priority:** Reopening the editor for the same plug-in instance now preserves live DAW parameters and project state.
- **Region processing is retained:** Per-region Gain and EQ remain unchanged across editor recreation.
- **Regression coverage:** Added an editor close/reopen test while retaining checks for monitoring, Norm, Gain, global EQ, region EQ, and remembered region types.


### 0.94 齿音实验 / Sibilance experiment — historical Test

日期未在现有记录中明确；状态：保留的实验基础，非当前主线。采用相对频谱比例检测、Breath 优先冲突处理、Sibilance 区域和独立处理、全局 Sibilance EQ/Gain/Norm，以及 Analyze 内容选择。识别准确率未达稳定要求，0.95 主线移除此功能。未在本次重新测试。
The date is not specified in the surviving record. Status: retained experimental baseline, not the current main line. It used relative spectral-ratio detection, Breath-priority conflict handling, Sibilance regions/individual processing, global Sibilance EQ/Gain/Norm and Analyze content selection. Detection accuracy was not stable enough; the main line removed it in 0.95. It was not retested in this release.



### 0.95 更新 / What's new in 0.95 — 2026-07-16

历史记录；未在本次重新验证旧版本。/ Historical record; this old version was not retested in this release.

#### 中文

- 主线版本取消 Sibilance 区域和自动齿音分析。
- Analyze 恢复为只检测 Breath，并保留 Noise、Breath 和 Others 三种区域。
- 0.94 齿音实验版本另行保存，供未来继续训练和研究。

#### English

- Removed the Sibilance region and automatic sibilance analysis from the main line.
- Returned Analyze to Breath-only detection while retaining Noise, Breath, and Others.
- Preserved the 0.94 sibilance experiment separately for future training and research.


### 0.97 更新 / What's new in 0.97 — 2026-07-16

历史记录；未在本次重新验证旧版本。/ Historical record; this old version was not retested in this release.

#### 中文

- 修复区域独立 EQ 和独立 Gain 只改变波形显示、但监听不到实际变化的问题。

#### English

- Fixed per-region EQ and Gain changing the waveform display without being audible in live monitoring.


### 0.98 更新 / What's new in 0.98 — 2026-07-16

历史记录；未在本次重新验证旧版本。/ Historical record; this old version was not retested in this release.

#### 中文

- 精简主界面，移除单独的 Draw 类型按钮组。
- 右键切换区域类型后会记住该类型；下一次 Shift 拖动画区时自动沿用。
- 统一 Global EQ 与 Region EQ 的单行控制布局和频谱窗口大小。

#### English

- Simplified the main UI by removing the separate Draw type button group.
- Remembered the type selected by right-click cycling and reused it for the next Shift-drag region.
- Aligned the one-row Global EQ and Region EQ controls and spectrum sizing.


### 1.0 更新 / What's new in 1.0 — 2026-07-28

历史记录；未在本次重新验证旧版本。/ Historical record; this old version was not retested in this release.

#### 中文

- 修复取消 Voice、Breath、Noise 或 Others 监听后，在音频线程遇到短暂锁竞争时仍可能漏出未处理声音的问题。
- 为普通 VST3 监听增加安全静音回退，并验证 Breath、Noise、Others 监听开关可以独立生效。
- 修复多轨 ARA 工程中不同 QQEasyTool 实例可能绑定到同一个波形或错误音源的问题。
- ARA 选择现在会与当前插件实例实际分配的播放区域相交；多轨环境不再回退到工程中的第一个音源。
- 将监听、Norm、Target、全局 Gain、全局 EQ 和区域处理状态按 ARA 音源独立保存、恢复和渲染。
- 保留并通过 0.99 的 UI 关闭/重开状态测试，以及 Gain、Norm、全局/区域 EQ、Others 和区域类型记忆测试。

#### English

- Fixed a lock-contention path that could leak unprocessed audio after Voice, Breath, Noise, or Others monitoring was disabled.
- Added a safe-silence fallback for ordinary VST3 preview monitoring and verified independent Breath, Noise, and Others monitor switches.
- Fixed different QQEasyTool instances in a multi-track ARA project binding to the same waveform or the wrong source.
- ARA selection is now intersected with the playback regions assigned to the current plug-in instance; multi-track projects no longer fall back to the first source.
- Stored, restored, and rendered monitor, Norm, Target, global Gain, global EQ, and region-processing state independently for each ARA source.
- Retained and passed the 0.99 editor-recreation coverage together with Gain, Norm, global/region EQ, Others, and remembered-region-type checks.


### 齿音研究 / Sibilance research

0.94 曾进行齿音频谱规则实验，准确率未达到稳定要求；0.95 起主线移除此功能。未来研究从保留的 0.94 实验基础继续。 / Version 0.94 experimented with sibilance rules; accuracy was insufficient and the feature was removed from the main line in 0.95. Future research uses that preserved experimental baseline.

## 5. 历史下载 / Historical downloads

[全部历史版本 / All releases](https://github.com/Ziqing-Gu/QQEasyTool/releases)

原 1.09 手工安装用 ZIP 资产保留在同一 Release 供历史使用；当前默认下载为上方三份安装包。 / Original 1.09 manual-install ZIP assets remain in the same Release for historical use; the three installers above are the current delivery.
