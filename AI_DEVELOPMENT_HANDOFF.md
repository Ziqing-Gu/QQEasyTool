# QQEasyTool authoritative development handoff

Current binary-source commit: a22a7015f0c233894201a5ee4ff76a2252e2c276 (tag v1.06).
Current release: https://github.com/Ziqing-Gu/QQEasyTool/releases/tag/v1.06
Current completion: Windows installed with hash parity; Plan B completed/frozen; public source and four Windows/source/guide Release assets verified. No Mac artifacts or CI runs. This Windows-only delivery does not claim full cross-platform Plan C.
No original Studio One 7 project acceptance is claimed.

Current Stable: 1.06 (internal 1.0.6), user-designated on 2026-10-24.
Current development: 1.06 Stable; Windows only. No new Candidate stage.
Scope: ARA cache/lifecycle/archive fixes; preserve EasyTool DSP/routing and 1.05 display.
Status: Windows build, four regressions and actual VST3 factory loading passed; user-authorized installation and Stable. No macOS builds.
Immediate installed rollback: 1.05 existing delivery.
Previous user-designated Stable: 1.04. Historical rollback for 1.04 was 1.03.
Current 1.06 delivery: Windows x64 VST3 only. No 1.06 Mac builds; older Mac packages remain historical.
Toolchain: JUCE 8.0.13; CMake; MSVC 2022 / Xcode.
Historical 1.04 build code commit: 631e27e3c1d9e4019dab3626c94cffcb60266d43. Historical 1.05 build source: 696cdecee6aaca0d8dd02a8ce7b8614c900bf7bb (tag v1.05).

## 2026-09-20 — 1.04 Stable
Based on 1.03; ported the accepted QQDeBreath 1.23 ordinary VST3 transport logic.
User requirement: click the waveform while stopped, then Play starts the selected audio; later DAW relocation restores project alignment.
Cause: internal audition offsets survived host relocation; early reset variants could lose the stopped selection at Play startup.
Implementation: hold selection until the first actual playing block anchors the host clock; clear offsets on later relocation; publish the audio clock snapshot for the ordinary VST3 editor.
Preserved EasyTool's ARA input/cache path, region processing, Voice/Breath/Noise/Others monitoring, Norm, Gain, EQ and state format.
Validation: Windows transport probes at 44.1/48/96 kHz, existing route/state/ARA-input tests and loading checks passed; three macOS builds, architecture and signature checks passed.
Stable is by explicit user designation. No new real macOS/ARA host acceptance is claimed. Host-reported transport information is required to observe stopped relocation.
Rollback to 1.03 if needed; do not undo the separate ARA cache fix in that version.
Earlier history remains in CHANGELOG.md, README.md, DEVELOPMENT_HISTORY.md and SIBILANCE_DEVELOPMENT_HISTORY.md.

## 2026-09-20 — Plan C / D remediation
No plug-in source or binary changed. Completed download links, bilingual release history, installation details, actual desktop packaging and remote asset verification.
No standalone user manual is added, as explicitly requested.
Earlier work used obsolete plan definitions, revisited completed Plan B and ran unnecessary Windows CI. Those past actions remain recorded; this pass does not access Plan B or repeat builds.
Plan B is inherited only from its prior completion record and stays frozen. Future Plan C reuses local Windows output and builds three Mac formats; Windows cloud CI requires explicit build_windows=true dispatch.
Next work: user-requested development only; preserve transport behavior and record a new candidate/baseline before changing code.

## 1.05 Candidate — 2026-09-22 / Gain, Norm Target and EQ responsiveness

中文：移植 QQDeBreath 1.25 已验收的交互性能优化。Gain、Norm Target 直接缩放波形缓存；全局和区域 EQ 的波形计算使用可取消后台线程，只采用最新结果。缓存区域峰值和绘制范围，动态频谱只处理当前窗口涉及的区域，连续拖动不再触发全文件静态频谱计算。ARA 区域参数更新限频，试听更新与波形计算分离。保留 EasyTool 的 Voice/Breath/Noise/Others 分轨、旧 Sibilance 类型兼容、重叠区域行为、Auto Apply 关闭时的预览/Apply 以及 1.04 播放同步规则。

English: Ported the accepted QQDeBreath 1.25 responsiveness improvements. Gain/Norm Target scale cached display data; global/selected EQ waveforms run in a cancellable latest-request worker. Cached peaks and painting intervals avoid repeated source scans, dynamic spectra visit only the current window, and continuous dragging avoids whole-file static spectra. Selected ARA parameter updates are throttled independently of waveform calculation. EasyTool Voice/Breath/Noise/Others routing, legacy Sibilance compatibility, neutral overlap behavior, manual preview/Apply and 1.04 transport semantics are preserved.

Scope: editor/display only; processor, EQ DSP, analysis and parameter format unchanged. No new audio smoothing algorithm. EQ waveform display may update after dragging. No new macOS build or DAW acceptance is claimed. Candidate awaiting user acceptance; Stable remains 1.04.

Validation: Windows x64 VST3 build and Steinberg load check passed. 30 reference cases at 44.1/48/96 kHz, each with six Gain/Target settings plus global/selected EQ, agree with 1.04 within 2.06028e-7 relative display rounding. Scalar changes submit zero full renders. Latest EQ, source replacement/clear and worker close passed. Existing routing/state/ARA input and transport probes passed. Synthetic 120-second/500-region test: 1000 scalar updates 236.457 ms; 500 EQ submissions 182.869 ms; paint 14.0675 ms. These are internal UI entry-point measurements, not host end-to-end latency.


1.05 installation: directly overwritten in the system VST3 directory as authorized, without a host-process scan. Installed bundle matches the verified build and output byte-for-byte; class IDs preserved. Prior 1.04 bundle saved at [machine-local paths retained in private verification records]

## 2026-09-22 — 1.05 Plan B / C / D authorized
User requested Plan B, followed by Plan C and D after DeBreath completes. DeBreath 1.25 Plan C/D completion was verified from its existing output records. Inherit the completed Windows 1.05 build/tests/installation without rebuilding or reinstalling. The user authorized public release of 1.05; no new explicit Stable designation or real-host acceptance is inferred. Stable remains 1.04.
Plan B snapshot: [machine-local paths retained in private verification records]

## 2026-09-22 — 1.05 public build source
Current 1.05 binary source: 696cdecee6aaca0d8dd02a8ce7b8614c900bf7bb in Ziqing-Gu/QQEasyTool. Windows reuses the verified 1.05 build; the public source matches its compiled source, allowing only CRLF/LF normalization. ARA SDK commit 0005c532bb03eb210e7bb490329fc6d7ba1792c7 and JUCE 8.0.13 are preserved. Plan B is frozen and not revisited. Publication is authorized; Stable remains 1.04 until explicitly designated otherwise.

1.05 delivery: all three macOS builds passed architecture and ad-hoc signature verification. Windows Actions was skipped and the existing Windows output reused. Six actual desktop files (four platform archives and two guides) were verified. No separate manual is included, as requested. Release v1.05 contains seven assets including complete source with ARA SDK and JUCE; uploaded names, sizes and hashes match. No new Stable designation or host acceptance is inferred.


## 1.06 Stable — 2026-10-24 / ARA multi-source cache and restore

中文：修复同一 ARA 文档超过 32 个音源后最早素材被淘汰、渲染静音，以及恢复存档时清空全部有效音频缓存的问题。音源在宿主模型线程预载，按源的失活、销毁和内容更新管理缓存；不再由后台线程访问宿主模型。局部存档只恢复指定音源，支持持久 ID 重映射、工程级独立存档与未分析音源；离线渲染不再依赖 Playing 标志。编辑器复用已有缓存。保留 Voice/Breath/Noise/Others 路由、旧 Sibilance 转 Others、重叠混音、Norm/Gain/EQ、1.04 普通 VST3 定位和 1.05 界面响应优化。

English: Fixed older sources being evicted after an ARA document exceeds 32 sources, causing silent rendering, and valid raw audio being discarded during archive restore. Audio preloads on the host model thread and follows source deactivation, destruction and content updates; background threads no longer traverse host model objects. Filtered restore preserves unrelated sources and handles persistent-ID remapping, document-only archives and untouched sources. Offline render does not require Playing; the editor reuses cached audio. Voice/Breath/Noise/Others routing, legacy Sibilance-to-Others mapping, overlap mixing, Norm/Gain/EQ, 1.04 ordinary VST3 transport and 1.05 display responsiveness are preserved.

验证 / Validation: Unmodified 1.05 reproduced 32 audible / 12 silent sources from 22 renderers and 44 sources; archive restore discarded all 22 cached sources. 1.06 renders all 44 with no silent sources, retains valid caches, performs zero host reads during concurrent rendering, and passes source lifecycle/restore checks. 96 ARA DSP comparisons against frozen EasyTool 1.05 match sample-for-sample. Existing routing/state/ARA input, transport and 30 waveform reference cases pass. Actual VST3 loading and internal version 1.0.6 are verified. These automated checks are not original Studio One project acceptance.

状态 / Status: Stable by explicit user instruction. Previous designated Stable: 1.04; immediate installed rollback: 1.05. This release contains Windows x64 VST3 only, as requested. No 1.06 macOS artifacts were built or reused from older versions. Original historical Candidate records remain historical; no new Candidate stage is used.

边界 / Limits: Full-source caching scales with active audio duration and channel count. Initial loading and undo reactivation can wait for synchronous preloading. Failed reads never publish partial buffers and retry at subsequent model/load events. Previously rendered silent files require rerendering from original events. Archive schema and ordinary VST3 state format remain compatible.

