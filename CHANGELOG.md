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

## 1.04 更新 / What's new in 1.04 — 2026-09-20

## 1.05 Candidate — 2026-09-22 / Gain, Norm Target and EQ responsiveness

中文：移植 QQDeBreath 1.25 已验收的交互性能优化。Gain、Norm Target 直接缩放波形缓存；全局和区域 EQ 的波形计算使用可取消后台线程，只采用最新结果。缓存区域峰值和绘制范围，动态频谱只处理当前窗口涉及的区域，连续拖动不再触发全文件静态频谱计算。ARA 区域参数更新限频，试听更新与波形计算分离。保留 EasyTool 的 Voice/Breath/Noise/Others 分轨、旧 Sibilance 类型兼容、重叠区域行为、Auto Apply 关闭时的预览/Apply 以及 1.04 播放同步规则。

English: Ported the accepted QQDeBreath 1.25 responsiveness improvements. Gain/Norm Target scale cached display data; global/selected EQ waveforms run in a cancellable latest-request worker. Cached peaks and painting intervals avoid repeated source scans, dynamic spectra visit only the current window, and continuous dragging avoids whole-file static spectra. Selected ARA parameter updates are throttled independently of waveform calculation. EasyTool Voice/Breath/Noise/Others routing, legacy Sibilance compatibility, neutral overlap behavior, manual preview/Apply and 1.04 transport semantics are preserved.

Scope: editor/display only; processor, EQ DSP, analysis and parameter format unchanged. No new audio smoothing algorithm. EQ waveform display may update after dragging. No new macOS build or DAW acceptance is claimed. Candidate awaiting user acceptance; Stable remains 1.04.

Validation: Windows x64 VST3 build and Steinberg load check passed. 30 reference cases at 44.1/48/96 kHz, each with six Gain/Target settings plus global/selected EQ, agree with 1.04 within 2.06028e-7 relative display rounding. Scalar changes submit zero full renders. Latest EQ, source replacement/clear and worker close passed. Existing routing/state/ARA input and transport probes passed. Synthetic 120-second/500-region test: 1000 scalar updates 236.457 ms; 500 EQ submissions 182.869 ms; paint 14.0675 ms. These are internal UI entry-point measurements, not host end-to-end latency.


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

## 1.02 Update / What's new in 1.02 - 2026-08-03

### 中文

- 修复部分 Fender Studio / Studio One 环境下 macOS ARA 首次打开时界面只显示左侧、需要拖动宿主窗口才恢复的问题。
- 在 ARA 层级挂接、首次显示和宿主缩放比例传递后，增加了有界的延迟布局同步。
- 音频处理、分析、监听、EQ、Gain、区域编辑和工程状态行为保持不变。

- **厂商标识保持为 Qing Audio：** 延续此前的统一厂商名称，并保留稳定的插件身份码。

- **Manufacturer identity:** The plug-in remains published as Qing Audio with stable plug-in identity codes.

### English

- Fixed the first-open macOS ARA layout handshake on some Fender Studio / Studio One environments.
- Added bounded deferred synchronization after ARA hierarchy attachment, first visibility, and host scale-factor delivery.
- Kept audio processing, analysis, monitoring, EQ, Gain, region editing, and project-state behavior unchanged.
# QQEasyTool Changelog / Update Record

## 1.01 Update / What's new in 1.01

### Chinese

- Unified the displayed plug-in manufacturer name as Qing Audio.
- Updated the plug-in version display and build metadata while preserving stable plug-in identity codes.

### English

- Unified the displayed plug-in manufacturer name as Qing Audio.
- Updated the plug-in version display and build metadata while preserving stable plug-in identity codes.

# QQEasyTool Changelog / 更新记录

## 1.0 更新 / What's new in 1.0 — 2026-07-28

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

## 0.99 更新 / What's new in 0.99 — 2026-07-23

### 中文

- 修复关闭并重新打开插件 UI 后，全局默认设置覆盖当前 Norm、Target、全局 Gain 和全局 EQ 的问题。
- 将“应用初始全局默认设置”的生命周期从编辑器窗口改为插件处理器实例，确保同一实例只自动应用一次。
- 验证区域独立 Gain 和独立 EQ 在 UI 重建后保持不变。
- 新增编辑器重建专项回归测试；完整监听、Norm、Gain、全局 EQ、区域 EQ 和区域类型记忆测试全部通过。
- 更新为 QQEasyTool 0.99 的中英文双语项目说明和 GitHub 构建配置。

### English

- Fixed global defaults overwriting the current Norm, Target, global Gain, and global EQ after closing and reopening the plug-in UI.
- Moved the initial-global-default claim from the editor-window lifetime to the processor-instance lifetime, so it is automatically applied only once per instance.
- Verified that per-region Gain and EQ survive editor recreation.
- Added an editor-recreation regression test; the full monitoring, Norm, Gain, global EQ, region EQ, and remembered-region-type checks all pass.
- Updated the bilingual QQEasyTool 0.99 project documentation and GitHub build configuration.

## 0.98 更新 / What's new in 0.98 — 2026-07-16

### 中文

- 精简主界面，移除单独的 Draw 类型按钮组。
- 右键切换区域类型后会记住该类型；下一次 Shift 拖动画区时自动沿用。
- 统一 Global EQ 与 Region EQ 的单行控制布局和频谱窗口大小。

### English

- Simplified the main UI by removing the separate Draw type button group.
- Remembered the type selected by right-click cycling and reused it for the next Shift-drag region.
- Aligned the one-row Global EQ and Region EQ controls and spectrum sizing.

## 0.97 更新 / What's new in 0.97 — 2026-07-16

### 中文

- 修复区域独立 EQ 和独立 Gain 只改变波形显示、但监听不到实际变化的问题。

### English

- Fixed per-region EQ and Gain changing the waveform display without being audible in live monitoring.

## 0.95 更新 / What's new in 0.95 — 2026-07-16

### 中文

- 主线版本取消 Sibilance 区域和自动齿音分析。
- Analyze 恢复为只检测 Breath，并保留 Noise、Breath 和 Others 三种区域。
- 0.94 齿音实验版本另行保存，供未来继续训练和研究。

### English

- Removed the Sibilance region and automatic sibilance analysis from the main line.
- Returned Analyze to Breath-only detection while retaining Noise, Breath, and Others.
- Preserved the 0.94 sibilance experiment separately for future training and research.

## 0.94 齿音实验 / Sibilance experiment — historical Test

日期未在现有记录中明确；状态：保留的实验基础，非当前主线。采用相对频谱比例检测、Breath 优先冲突处理、Sibilance 区域和独立处理、全局 Sibilance EQ/Gain/Norm，以及 Analyze 内容选择。识别准确率未达稳定要求，0.95 主线移除此功能。未在本次重新测试。
The date is not specified in the surviving record. Status: retained experimental baseline, not the current main line. It used relative spectral-ratio detection, Breath-priority conflict handling, Sibilance regions/individual processing, global Sibilance EQ/Gain/Norm and Analyze content selection. Detection accuracy was not stable enough; the main line removed it in 0.95. It was not retested in this release.

