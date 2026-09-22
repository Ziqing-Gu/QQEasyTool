# QQEasyTool authoritative development handoff

Current Stable: 1.04 (internal 1.0.4), designated by the user on 2026-09-20.
Current Candidate: 1.05 (internal 1.0.5), based on 1.04 Stable.
Scope: port the user-accepted QQDeBreath 1.25 Gain/Norm Target and EQ Auto Apply UI responsiveness improvements; preserve EasyTool audio/transport/routing/legacy-type compatibility.
Status: Windows build and focused automated validation passed; user explicitly authorized direct installed VST3 overwrite without a host-process precheck.
Rollback baseline: [machine-local paths retained in private verification records]
Previous stable / rollback: 1.03.
Platforms: Windows x64 VST3; macOS arm64 VST3, x86_64 VST3 and Universal 2 AU; shared ARA-capable plug-in.
Toolchain: JUCE 8.0.13; CMake; MSVC 2022 / Xcode.
Historical 1.04 build code commit: 631e27e3c1d9e4019dab3626c94cffcb60266d43. Current 1.05 source is the verified active workspace; its public build commit will be recorded during Plan C.

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
