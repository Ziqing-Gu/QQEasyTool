# Sibilance Development History / 齿音开发历史

## 中文

QQEasyTool 0.95 及之后的当前主线只自动分析 Breath，并保留 Noise、Breath 和用户自定义的 Others 区域。Sibilance 区域和自动齿音分析已从 0.95 主线中移除。

未来如需继续开发齿音检测、标注训练、De-Esser 或独立齿音版本，标准起点是冻结保存的 QQEasyTool 0.94 源代码：

```text
QQEasyTool-0.94-Sibilance-Base (historical archive identifier; private storage path omitted)
```

该 0.94 版本包含相对频谱比例检测器、Breath 优先的冲突处理、Sibilance 区域、全局 Sibilance EQ/Gain/Norm、独立区域处理，以及 Analyze 检测内容选择窗口。

请不要从 0.95 之后的 Breath-only 主线重新恢复已经删除的齿音逻辑；未来应直接复制 0.94 冻结源码，作为新的齿音研究分支起点。

## English

QQEasyTool 0.95 and later on the current main line analyze Breath only while retaining Noise, Breath, and user-defined Others regions. Sibilance regions and automatic sibilance analysis were removed from the main line in 0.95.

The canonical starting point for future sibilance detection, labeling, training, De-Esser work, or a dedicated sibilance edition is the archived QQEasyTool 0.94 source:

```text
QQEasyTool-0.94-Sibilance-Base (historical archive identifier; private storage path omitted)
```

That 0.94 version contains the relative spectral-ratio detector, Breath-priority conflict handling, Sibilance regions, global Sibilance EQ/Gain/Norm, per-region processing, and the Analyze content-selection dialog.

Do not reconstruct the removed sibilance logic from the Breath-only 0.95+ main line. Copy the frozen 0.94 source and use it as the starting point for a new sibilance research branch.

维护说明：该路径仅作为历史档案标识，不授权后续 Plan 访问已完成的 Plan B。/ Maintenance note: this historical archive reference does not authorize later release stages to inspect a completed Plan B backup.
