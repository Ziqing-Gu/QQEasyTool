# QQEasyTool Changelog / 更新记录

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
