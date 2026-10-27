# QQEasyTool development history

## 1.05 Candidate — 2026-09-22 / Gain, Norm Target and EQ responsiveness

中文：移植 QQDeBreath 1.25 已验收的交互性能优化。Gain、Norm Target 直接缩放波形缓存；全局和区域 EQ 的波形计算使用可取消后台线程，只采用最新结果。缓存区域峰值和绘制范围，动态频谱只处理当前窗口涉及的区域，连续拖动不再触发全文件静态频谱计算。ARA 区域参数更新限频，试听更新与波形计算分离。保留 EasyTool 的 Voice/Breath/Noise/Others 分轨、旧 Sibilance 类型兼容、重叠区域行为、Auto Apply 关闭时的预览/Apply 以及 1.04 播放同步规则。

English: Ported the accepted QQDeBreath 1.25 responsiveness improvements. Gain/Norm Target scale cached display data; global/selected EQ waveforms run in a cancellable latest-request worker. Cached peaks and painting intervals avoid repeated source scans, dynamic spectra visit only the current window, and continuous dragging avoids whole-file static spectra. Selected ARA parameter updates are throttled independently of waveform calculation. EasyTool Voice/Breath/Noise/Others routing, legacy Sibilance compatibility, neutral overlap behavior, manual preview/Apply and 1.04 transport semantics are preserved.

Scope: editor/display only; processor, EQ DSP, analysis and parameter format unchanged. No new audio smoothing algorithm. EQ waveform display may update after dragging. No new macOS build or DAW acceptance is claimed. Candidate awaiting user acceptance; Stable remains 1.04.

Validation: Windows x64 VST3 build and Steinberg load check passed. 30 reference cases at 44.1/48/96 kHz, each with six Gain/Target settings plus global/selected EQ, agree with 1.04 within 2.06028e-7 relative display rounding. Scalar changes submit zero full renders. Latest EQ, source replacement/clear and worker close passed. Existing routing/state/ARA input and transport probes passed. Synthetic 120-second/500-region test: 1000 scalar updates 236.457 ms; 500 EQ submissions 182.869 ms; paint 14.0675 ms. These are internal UI entry-point measurements, not host end-to-end latency.


## 2026-09-20 — 1.04 Stable

Ported ordinary VST3 transport synchronization from the user-approved QQDeBreath 1.23 Stable baseline. Stopped waveform selection survives Play startup; later DAW relocation restores the original timeline. ARA playback/input and region processing remain unchanged. Transport tests at 44.1/48/96 kHz, existing route/state/ARA-input probes and Steinberg loading checks passed. The user designated this version Stable and requested Plan B, C and D. The approved Windows binary remains unchanged.

Previous development entries remain in CHANGELOG.md and SIBILANCE_DEVELOPMENT_HISTORY.md.


## 1.06 Stable — 2026-10-24 / ARA multi-source cache and restore

中文：修复同一 ARA 文档超过 32 个音源后最早素材被淘汰、渲染静音，以及恢复存档时清空全部有效音频缓存的问题。音源在宿主模型线程预载，按源的失活、销毁和内容更新管理缓存；不再由后台线程访问宿主模型。局部存档只恢复指定音源，支持持久 ID 重映射、工程级独立存档与未分析音源；离线渲染不再依赖 Playing 标志。编辑器复用已有缓存。保留 Voice/Breath/Noise/Others 路由、旧 Sibilance 转 Others、重叠混音、Norm/Gain/EQ、1.04 普通 VST3 定位和 1.05 界面响应优化。

English: Fixed older sources being evicted after an ARA document exceeds 32 sources, causing silent rendering, and valid raw audio being discarded during archive restore. Audio preloads on the host model thread and follows source deactivation, destruction and content updates; background threads no longer traverse host model objects. Filtered restore preserves unrelated sources and handles persistent-ID remapping, document-only archives and untouched sources. Offline render does not require Playing; the editor reuses cached audio. Voice/Breath/Noise/Others routing, legacy Sibilance-to-Others mapping, overlap mixing, Norm/Gain/EQ, 1.04 ordinary VST3 transport and 1.05 display responsiveness are preserved.

验证 / Validation: Unmodified 1.05 reproduced 32 audible / 12 silent sources from 22 renderers and 44 sources; archive restore discarded all 22 cached sources. 1.06 renders all 44 with no silent sources, retains valid caches, performs zero host reads during concurrent rendering, and passes source lifecycle/restore checks. 96 ARA DSP comparisons against frozen EasyTool 1.05 match sample-for-sample. Existing routing/state/ARA input, transport and 30 waveform reference cases pass. Actual VST3 loading and internal version 1.0.6 are verified. These automated checks are not original Studio One project acceptance.

状态 / Status: Stable by explicit user instruction. Previous designated Stable: 1.04; immediate installed rollback: 1.05. This release contains Windows x64 VST3 only, as requested. No 1.06 macOS artifacts were built or reused from older versions. Original historical Candidate records remain historical; no new Candidate stage is used.

边界 / Limits: Full-source caching scales with active audio duration and channel count. Initial loading and undo reactivation can wait for synchronous preloading. Failed reads never publish partial buffers and retry at subsequent model/load events. Previously rendered silent files require rerendering from original events. Archive schema and ordinary VST3 state format remain compatible.



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


Current release: Stable; new Plan B completed and frozen. Reuse the delivered Windows binary. Mac builds and final release verification are recorded separately. History retains 64 steps per source in the current session; no full undo-chain persistence across restart is claimed.
