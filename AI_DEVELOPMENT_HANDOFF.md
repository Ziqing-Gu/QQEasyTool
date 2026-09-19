# QQEasyTool authoritative development handoff

Current Stable: 1.04 (internal 1.0.4), designated by the user on 2026-09-20.
Current Candidate: none.
Previous stable / rollback: 1.03.
Platforms: Windows x64 VST3; macOS arm64 VST3, x86_64 VST3 and Universal 2 AU; shared ARA-capable plug-in.
Toolchain: JUCE 8.0.13; CMake; MSVC 2022 / Xcode.
Build code commit: 631e27e3c1d9e4019dab3626c94cffcb60266d43. The v1.04 tag is unchanged; corrective commits update documents/workflow only.

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
