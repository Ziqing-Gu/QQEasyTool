## 1.04 更新 / What's new in 1.04 — 2026-09-20

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
