# ⚠️ 禁止商业使用 / NO COMMERCIAL USE

## Qing Audio 非商业源码共享许可证 1.0

### 本项目源码公开，但不属于 OSI 认可的开源软件

> **禁止任何商业使用。** 仅允许个人、学习、教育、研究、评估、爱好及其他非商业用途。发布原版、二进制版或修改版时，必须同时免费公开完整对应源代码，保留作者、版权和许可证声明，醒目标明原项目名称、作者、来源链接、修改者、修改日期及修改内容，并使整个修改版继续采用同一许可证。完整条款见 [LICENSE](LICENSE)。
>
> **NO COMMERCIAL USE.** Use is permitted only for personal, educational, research, evaluation, hobby, charitable, and other non-commercial purposes. Any distributed original, binary, or modified version must provide the complete corresponding source without charge, preserve authorship, copyright, and license notices, prominently identify the original project, author, source URL, modifier, date, and changes, and license the entire modified work under the same terms. See [LICENSE](LICENSE).
>
> 许可证政策变更与后续 AI 维护说明见 [LICENSE_POLICY_CHANGE.md](LICENSE_POLICY_CHANGE.md)。 / See [LICENSE_POLICY_CHANGE.md](LICENSE_POLICY_CHANGE.md) for the policy record and future AI maintenance instructions.

# QQEasyTool

[中文](#中文) | [English](#english)
当前稳定版本 / Current stable version: **1.08 Stable**（用户指定 / user-designated）







## 2026-10-27 — Maintenance direction / 后续维护方向

用户决定把后续 ARA/VST3/AU 功能与修复集中到 QQEasyTool。QQDeBreath 插件版停留在 1.29；它可能是最后一版。保留现有源码、成品和历史下载，除非用户以后明确重新授权，不再修改 QQDeBreath 插件产品。本次已授权的 1.29 发布收尾继续完成。独立应用不在此次停止维护范围内。

Future ARA/VST3/AU development is focused on QQEasyTool. QQDeBreath plug-ins remain at 1.29, which may be the final plug-in release. Existing source, binaries and historical downloads remain available. Further QQDeBreath plug-in product changes require a new explicit user request. The already-authorized 1.29 publication is completed; the standalone application is separate.

## 1.08 Stable — Background ARA analysis and faster sparse-audio analysis / 2026-10-27

中文：修正上一版把 ARA 分析绑定到编辑器、切换音源时取消分析的问题。分析现在由 ARA 文档按音源管理，切轨或关闭插件窗口后继续，完成后只写回原音源。另一轨可启动独立任务，两条低优先级工作线程由同一插件的实例共享，额外任务排队。复用已有原始音频缓存；工作线程不调用宿主音频读取或修改 ARA 模型。文档定时器在模型线程提交结果，保留各音源参数和分析前的撤销状态；显式取消、宿主归档恢复、音源内容失效和手工区域修改阻止过期结果覆盖。组合选择从当前音源结果刷新，避免旧组合快照遮住后台完成结果。进度显示排队状态或已耗时，不再使用估算百分比。

English: ARA analysis is now owned by the document/source instead of the editor. Switching tracks or closing the editor keeps queued/running work alive; completion is committed to the originating source. Two low-priority workers are shared across instances of this plug-in, with additional jobs queued. Workers use immutable cached audio and never read host audio or mutate the ARA model. A document timer commits on the model thread, preserving per-source parameters and pre-analysis Undo. Explicit cancellation, host archive restore, invalidated audio and manual region edits reject stale work. Group views rebuild from live source regions. The UI shows queued/elapsed time.

性能 / Performance: Native analysis uses exact order-statistic selection instead of full-array sorting, reuses computed spectral statistics for fully digital-silent frames, and suppresses denormal arithmetic in filter tails. Low-level nonzero audio still receives full analysis. Detection model, thresholds and region rules remain unchanged. Ordinary VST3/AU uses the same optimized analyzer; background continuity across ARA track selection applies to ARA mode.

验证 / Validation: Eight original user WAVs were compared against the previous analyzer for each product (16 comparisons total). All region types and start/end sample/time boundaries matched. Overall measured speedup was 1.40–5.47x. These are native analyzer timings, not Cubase end-to-end UI timings. Input SHA-256 hashes were rechecked unchanged. The deterministic ARA regression includes two queued source jobs surviving selection/editor destruction, document-timer completion, group refresh, per-source parameters, cached input with no host reread, pre-analysis Undo, explicit Cancel and host archive Undo rejection. Existing selection/history, multi-instance audio, transport, waveform and product-specific probes pass. Audio processBlock and the ARA playback renderer are unchanged; factory load, internal/PE/moduleinfo 1.0.8 and installed SHA-256 parity pass.

## 1.07 — Cubase ARA selection, Reload and edit history / 2026-10-27

中文：ARA 编辑器响应宿主事件/轨道选择并切换对应波形、区域和参数，无需关闭窗口。显式 EditorView 选择不再被播放实例的区域归属过滤；选择改变跳过旧的一秒恢复间隔，直接使用已有音频缓存显示波形。Reload 保留现有编辑和参数。插件内部撤销/重做现覆盖区域编辑、Gain、Norm、EQ 和监听参数；历史按音源保存在处理器/ARA 文档中，Reload、切轨和重开窗口后仍可用。未应用预览先回滚，切轨前保存待提交修改；旧分析请求的迟到结果不会覆盖新轨。

English: The ARA editor follows host event/track selection without closing its window, restoring the selected source's waveform, regions and parameters. Explicit EditorView selection takes priority over playback-renderer assignment; selection changes bypass the previous retry delay and reuse cached source audio. Reload preserves edits. Internal Undo/Redo covers region edits, Gain, Norm, EQ and monitor parameters, with per-source history owned by the processor/ARA document rather than an editor window. Unapplied previews roll back, pending edits flush before switching, and stale analysis completions cannot replace a different source.

宿主集成 / Host integration: ARA edits queue source/modification content notifications; host archive Undo/Redo refreshes an already-open editor without echoing the restored state as a new edit. Ordinary VST3/AU custom-state edits notify the host through JUCE, and host state restore refreshes the editor. Exact Cubase history-panel entries and host-specific Undo grouping have not been validated in the user's real project. Native host history remains host-dependent; the plugin's own Undo/Redo works independently.

验证 / Validation: Real ARA EditorView regression covers immediate selection callbacks, distinct A/B waveforms, 20 rapid switches, sequence-only selection, two views, multi-event selection, pending/preview edits, unavailable source retry, late analysis rejection, Reload/reopen Undo/Redo and filtered host archive Undo/Redo with no feedback notifications. Ordinary shared editor tests cover region/Gain/Norm/EQ history, reopen, custom-state notifications and host state restore. Existing audio/transport/waveform/ARA multi-instance probes pass; processBlock and ARA renderer code remain unchanged. Windows VST3 factory load, PE/moduleinfo 1.0.7 and installed SHA-256 parity pass.



中文：修复同一 ARA 文档超过 32 个音源后最早素材被淘汰、渲染静音，以及恢复存档时清空全部有效音频缓存的问题。音源在宿主模型线程预载，按源的失活、销毁和内容更新管理缓存；不再由后台线程访问宿主模型。局部存档只恢复指定音源，支持持久 ID 重映射、工程级独立存档与未分析音源；离线渲染不再依赖 Playing 标志。编辑器复用已有缓存。保留 Voice/Breath/Noise/Others 路由、旧 Sibilance 转 Others、重叠混音、Norm/Gain/EQ、1.04 普通 VST3 定位和 1.05 界面响应优化。

English: Fixed older sources being evicted after an ARA document exceeds 32 sources, causing silent rendering, and valid raw audio being discarded during archive restore. Audio preloads on the host model thread and follows source deactivation, destruction and content updates; background threads no longer traverse host model objects. Filtered restore preserves unrelated sources and handles persistent-ID remapping, document-only archives and untouched sources. Offline render does not require Playing; the editor reuses cached audio. Voice/Breath/Noise/Others routing, legacy Sibilance-to-Others mapping, overlap mixing, Norm/Gain/EQ, 1.04 ordinary VST3 transport and 1.05 display responsiveness are preserved.

验证 / Validation: Unmodified 1.05 reproduced 32 audible / 12 silent sources from 22 renderers and 44 sources; archive restore discarded all 22 cached sources. 1.06 renders all 44 with no silent sources, retains valid caches, performs zero host reads during concurrent rendering, and passes source lifecycle/restore checks. 96 ARA DSP comparisons against frozen EasyTool 1.05 match sample-for-sample. Existing routing/state/ARA input, transport and 30 waveform reference cases pass. Actual VST3 loading and internal version 1.0.6 are verified. These automated checks are not original Studio One project acceptance.

边界 / Limits: Full-source caching scales with active audio duration and channel count. Initial loading and undo reactivation can wait for synchronous preloading. Failed reads never publish partial buffers and retry at subsequent model/load events. Previously rendered silent files require rerendering from original events. Archive schema and ordinary VST3 state format remain compatible.


## 1.06 Stable — 2026-10-24 / ARA multi-source cache and restore

中文：修复同一 ARA 文档超过 32 个音源后最早素材被淘汰、渲染静音，以及恢复存档时清空全部有效音频缓存的问题。音源在宿主模型线程预载，按源的失活、销毁和内容更新管理缓存；不再由后台线程访问宿主模型。局部存档只恢复指定音源，支持持久 ID 重映射、工程级独立存档与未分析音源；离线渲染不再依赖 Playing 标志。编辑器复用已有缓存。保留 Voice/Breath/Noise/Others 路由、旧 Sibilance 转 Others、重叠混音、Norm/Gain/EQ、1.04 普通 VST3 定位和 1.05 界面响应优化。

English: Fixed older sources being evicted after an ARA document exceeds 32 sources, causing silent rendering, and valid raw audio being discarded during archive restore. Audio preloads on the host model thread and follows source deactivation, destruction and content updates; background threads no longer traverse host model objects. Filtered restore preserves unrelated sources and handles persistent-ID remapping, document-only archives and untouched sources. Offline render does not require Playing; the editor reuses cached audio. Voice/Breath/Noise/Others routing, legacy Sibilance-to-Others mapping, overlap mixing, Norm/Gain/EQ, 1.04 ordinary VST3 transport and 1.05 display responsiveness are preserved.

验证 / Validation: Unmodified 1.05 reproduced 32 audible / 12 silent sources from 22 renderers and 44 sources; archive restore discarded all 22 cached sources. 1.06 renders all 44 with no silent sources, retains valid caches, performs zero host reads during concurrent rendering, and passes source lifecycle/restore checks. 96 ARA DSP comparisons against frozen EasyTool 1.05 match sample-for-sample. Existing routing/state/ARA input, transport and 30 waveform reference cases pass. Actual VST3 loading and internal version 1.0.6 are verified. These automated checks are not original Studio One project acceptance.

状态 / Status: Stable by explicit user instruction. Previous designated Stable: 1.04; immediate installed rollback: 1.05. This release contains Windows x64 VST3 only, as requested. No 1.06 macOS artifacts were built or reused from older versions. Original historical Candidate records remain historical; no new Candidate stage is used.

边界 / Limits: Full-source caching scales with active audio duration and channel count. Initial loading and undo reactivation can wait for synchronous preloading. Failed reads never publish partial buffers and retry at subsequent model/load events. Previously rendered silent files require rerendering from original events. Archive schema and ordinary VST3 state format remain compatible.

## 1.05 Candidate — 2026-09-22 / Gain, Norm Target and EQ responsiveness

中文：移植 QQDeBreath 1.25 已验收的交互性能优化。Gain、Norm Target 直接缩放波形缓存；全局和区域 EQ 的波形计算使用可取消后台线程，只采用最新结果。缓存区域峰值和绘制范围，动态频谱只处理当前窗口涉及的区域，连续拖动不再触发全文件静态频谱计算。ARA 区域参数更新限频，试听更新与波形计算分离。保留 EasyTool 的 Voice/Breath/Noise/Others 分轨、旧 Sibilance 类型兼容、重叠区域行为、Auto Apply 关闭时的预览/Apply 以及 1.04 播放同步规则。

English: Ported the accepted QQDeBreath 1.25 responsiveness improvements. Gain/Norm Target scale cached display data; global/selected EQ waveforms run in a cancellable latest-request worker. Cached peaks and painting intervals avoid repeated source scans, dynamic spectra visit only the current window, and continuous dragging avoids whole-file static spectra. Selected ARA parameter updates are throttled independently of waveform calculation. EasyTool Voice/Breath/Noise/Others routing, legacy Sibilance compatibility, neutral overlap behavior, manual preview/Apply and 1.04 transport semantics are preserved.

Scope: editor/display only; processor, EQ DSP, analysis and parameter format unchanged. No new audio smoothing algorithm. EQ waveform display may update after dragging. No new macOS build or DAW acceptance is claimed. Candidate awaiting user acceptance; Stable remains 1.04.

Validation: Windows x64 VST3 build and Steinberg load check passed. 30 reference cases at 44.1/48/96 kHz, each with six Gain/Target settings plus global/selected EQ, agree with 1.04 within 2.06028e-7 relative display rounding. Scalar changes submit zero full renders. Latest EQ, source replacement/clear and worker close passed. Existing routing/state/ARA input and transport probes passed. Synthetic 120-second/500-region test: 1000 scalar updates 236.457 ms; 500 EQ submissions 182.869 ms; paint 14.0675 ms. These are internal UI entry-point measurements, not host end-to-end latency.



## 下载 / Downloads — 1.08 Stable

[Latest Release](https://github.com/Ziqing-Gu/QQEasyTool/releases/latest) · [v1.08](https://github.com/Ziqing-Gu/QQEasyTool/releases/tag/v1.08)

- [QQEasyTool-Windows-x64-1.08.zip](https://github.com/Ziqing-Gu/QQEasyTool/releases/download/v1.08/QQEasyTool-Windows-x64-1.08.zip)
- [QQEasyTool-macOS-Apple-Silicon-1.08.zip](https://github.com/Ziqing-Gu/QQEasyTool/releases/download/v1.08/QQEasyTool-macOS-Apple-Silicon-1.08.zip)
- [QQEasyTool-macOS-Intel-1.08.zip](https://github.com/Ziqing-Gu/QQEasyTool/releases/download/v1.08/QQEasyTool-macOS-Intel-1.08.zip)
- [QQEasyTool-macOS-AU-Universal-1.08.zip](https://github.com/Ziqing-Gu/QQEasyTool/releases/download/v1.08/QQEasyTool-macOS-AU-Universal-1.08.zip)
- [QQEasyTool-1.08-Complete-Source.zip](https://github.com/Ziqing-Gu/QQEasyTool/releases/download/v1.08/QQEasyTool-1.08-Complete-Source.zip)
- [QQEasyTool-1.08-Windows-macOS-INSTALL-ZH-CN.txt](https://github.com/Ziqing-Gu/QQEasyTool/releases/download/v1.08/QQEasyTool-1.08-Windows-macOS-INSTALL-ZH-CN.txt)
- [QQEasyTool-1.08-Windows-macOS-INSTALL.txt](https://github.com/Ziqing-Gu/QQEasyTool/releases/download/v1.08/QQEasyTool-1.08-Windows-macOS-INSTALL.txt)
- [QQEasyTool-1.08-User-Manual-ZH-CN.pdf](https://github.com/Ziqing-Gu/QQEasyTool/releases/download/v1.08/QQEasyTool-1.08-User-Manual-ZH-CN.pdf)
- [QQEasyTool-1.08-User-Manual-EN.pdf](https://github.com/Ziqing-Gu/QQEasyTool/releases/download/v1.08/QQEasyTool-1.08-User-Manual-EN.pdf)

Windows x64 VST3、macOS arm64 / Intel VST3、Universal 2 AU。选择与宿主架构匹配的包。
GitHub 自动生成的 Source code (zip) / Source code (tar.gz) 不是可安装插件；Complete-Source.zip 包含 ARA SDK 与 JUCE。Automatic source archives are not installable plug-ins.

两份新中英文手册对应 QQEasyTool 1.08，完整功能公开体验，无到期时间或功能限制。 / New Chinese and English manuals cover 1.08. Full functionality for public feedback, without expiry or feature limits.


## 1.04 更新 / What's new in 1.04 — 2026-09-20

用户于 2026-09-20 指定此版本为 Stable。/ Designated Stable by the user on 2026-09-20.

### 中文

- 普通 VST3 的试听和宿主同步采用 QQDeBreath 1.23 Stable 已验收的逻辑：停止时点击插件波形，按播放从所选音频位置开始。
- 停止时的选择保留至首个实际播放音频块，启动时宿主时间不同、零长度块和重复停止回调不会吞掉选择。原地停止/继续保留原有试听行为。
- 后续 DAW 重新定位或循环回跳时，清除试听偏移和内部循环，指针与实际音频恢复原始工程对齐；停止时宿主报告的新定位也会取消待播放选择。
- 普通 VST3 界面使用音频线程发布的宿主位置快照，避免跨线程直接读取播放时钟。
- 保留 EasyTool 的 ARA 输入/缓存处理、Voice/Breath/Noise/Others 监听、Norm、Gain、EQ、区域编辑及工程状态格式。

### English

- Ordinary VST3 audition/transport synchronization now uses the user-verified QQDeBreath 1.23 Stable logic. Clicking the waveform while stopped selects the audio that starts when Play is pressed.
- A stopped selection remains pending until the first actual playing audio block. Different startup clocks, zero-length blocks, and repeated stopped callbacks do not consume it. Pause/resume in place retains audition behavior.
- Later DAW seeks and cycle wraps clear audition offsets and internal loops, restoring both the cursor and actual audio to the original timeline. A new host-reported stopped position also cancels the pending selection.
- The ordinary VST3 editor reads an audio-thread transport snapshot instead of directly querying the host clock from the UI thread.
- Preserved EasyTool's ARA input/cache processing, Voice/Breath/Noise/Others monitoring, Norm, Gain, EQ, region editing, and project-state format.

## 1.03 更新 / What's new in 1.03

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

当前版本为 `1.04`，仍处于测试阶段。现阶段 Analyze 只自动检测 Breath；齿音（Sibilance）的自动检测和处理尚未包含在当前版本中。未来希望加入自动检测和处理齿音的能力，只是目前还没有足够的时间继续开发这部分功能。

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

The current version is `1.04` and remains in testing. Analyze currently detects Breath only. Automatic sibilance detection and processing are not included in the current version. The long-term goal is to add automatic sibilance detection and treatment, but there has not yet been enough development time to complete that feature.

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

## 许可证 / License

本项目第一方源码采用 **Qing Audio 非商业源码共享许可证 1.0**（`LicenseRef-Qing-Audio-NC-Source-Share-1.0`）。禁止任何商业使用。发布原版、二进制版或修改版时，必须同时免费公开完整对应源代码，保留作者、版权与许可证声明，注明原项目、作者、来源链接、修改者、修改日期及修改内容，并使整个修改版继续采用同一许可证。完整条款见 [LICENSE](LICENSE)。第三方组件继续适用其各自许可证。

This project's first-party source is licensed under the **Qing Audio Non-Commercial Source-Share License 1.0** (`LicenseRef-Qing-Audio-NC-Source-Share-1.0`). Commercial use is prohibited. Distribution of the original, binary, or modified version requires the complete corresponding source at no charge, preserved authorship, copyright, and license notices, prominent identification of the original project, author, source URL, modifier, date, and changes, and the same license for the entire modified work. See [LICENSE](LICENSE). Third-party components remain under their respective licenses.

Previously distributed copies retain rights already granted; this License applies to copies supplied with it.

## 0.94 齿音实验 / Sibilance experiment — historical Test

日期未在现有记录中明确；状态：保留的实验基础，非当前主线。采用相对频谱比例检测、Breath 优先冲突处理、Sibilance 区域和独立处理、全局 Sibilance EQ/Gain/Norm，以及 Analyze 内容选择。识别准确率未达稳定要求，0.95 主线移除此功能。未在本次重新测试。
The date is not specified in the surviving record. Status: retained experimental baseline, not the current main line. It used relative spectral-ratio detection, Breath-priority conflict handling, Sibilance regions/individual processing, global Sibilance EQ/Gain/Norm and Analyze content selection. Detection accuracy was not stable enough; the main line removed it in 0.95. It was not retested in this release.


## 0.95 更新 / What's new in 0.95 — 2026-07-16

历史记录；未在本次重新验证旧版本。/ Historical record; this old version was not retested in this release.

### 中文

- 主线版本取消 Sibilance 区域和自动齿音分析。
- Analyze 恢复为只检测 Breath，并保留 Noise、Breath 和 Others 三种区域。
- 0.94 齿音实验版本另行保存，供未来继续训练和研究。

### English

- Removed the Sibilance region and automatic sibilance analysis from the main line.
- Returned Analyze to Breath-only detection while retaining Noise, Breath, and Others.
- Preserved the 0.94 sibilance experiment separately for future training and research.

## 0.97 更新 / What's new in 0.97 — 2026-07-16

历史记录；未在本次重新验证旧版本。/ Historical record; this old version was not retested in this release.

### 中文

- 修复区域独立 EQ 和独立 Gain 只改变波形显示、但监听不到实际变化的问题。

### English

- Fixed per-region EQ and Gain changing the waveform display without being audible in live monitoring.

## 0.98 更新 / What's new in 0.98 — 2026-07-16

历史记录；未在本次重新验证旧版本。/ Historical record; this old version was not retested in this release.

### 中文

- 精简主界面，移除单独的 Draw 类型按钮组。
- 右键切换区域类型后会记住该类型；下一次 Shift 拖动画区时自动沿用。
- 统一 Global EQ 与 Region EQ 的单行控制布局和频谱窗口大小。

### English

- Simplified the main UI by removing the separate Draw type button group.
- Remembered the type selected by right-click cycling and reused it for the next Shift-drag region.
- Aligned the one-row Global EQ and Region EQ controls and spectrum sizing.

## 1.0 更新 / What's new in 1.0 — 2026-07-28

历史记录；未在本次重新验证旧版本。/ Historical record; this old version was not retested in this release.

### 中文

- 修复取消 Voice、Breath、Noise 或 Others 监听后，在音频线程遇到短暂锁竞争时仍可能漏出未处理声音的问题。
- 为普通 VST3 监听增加安全静音回退，并验证 Breath、Noise、Others 监听开关可以独立生效。
- 修复多轨 ARA 工程中不同 QQEasyTool 实例可能绑定到同一个波形或错误音源的问题。
- ARA 选择现在会与当前插件实例实际分配的播放区域相交；多轨环境不再回退到工程中的第一个音源。
- 将监听、Norm、Target、全局 Gain、全局 EQ 和区域处理状态按 ARA 音源独立保存、恢复和渲染。
- 保留并通过 0.99 的 UI 关闭/重开状态测试，以及 Gain、Norm、全局/区域 EQ、Others 和区域类型记忆测试。

### English

- Fixed a lock-contention path that could leak unprocessed audio after Voice, Breath, Noise, or Others monitoring was disabled.
- Added a safe-silence fallback for ordinary VST3 preview monitoring and verified independent Breath, Noise, and Others monitor switches.
- Fixed different QQEasyTool instances in a multi-track ARA project binding to the same waveform or the wrong source.
- ARA selection is now intersected with the playback regions assigned to the current plug-in instance; multi-track projects no longer fall back to the first source.
- Stored, restored, and rendered monitor, Norm, Target, global Gain, global EQ, and region-processing state independently for each ARA source.
- Retained and passed the 0.99 editor-recreation coverage together with Gain, Norm, global/region EQ, Others, and remembered-region-type checks.

## 维护与构建 / Maintenance and builds

权威交接记录 / Authoritative handoff: [AI_DEVELOPMENT_HANDOFF.md](AI_DEVELOPMENT_HANDOFF.md)。
现行发布规则 / Current release workflow: [release_workflow.md](docs/release_workflow.md)。
Windows 复用已验证的本机 Plan A 成品；GitHub 默认只生成三类 macOS。Windows CI 只在明确要求时以 build_windows=true 手动启用。
Windows delivery reuses verified local Plan A output; GitHub defaults to three macOS builds. Windows cloud reproduction is opt-in with build_windows=true.
