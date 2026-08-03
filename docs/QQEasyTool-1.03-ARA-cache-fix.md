# QQEasyTool 1.03 ARA cache fix

## 中文

1. 修复部分 DAW 在关闭并重新打开工程后，ARA 音频出现噼啪声的问题。
2. 工程恢复和 ARA 播放渲染器创建时，会请求后台缓存预加载。
3. 音频线程只读取已经完成的不可变缓存；缓存尚未完成时输出静音，不再直接调用 `ARAAudioSourceReader`。
4. 不改变 Breath、Noise、Others、EQ、Gain、监听或分析逻辑。

## English

1. Fixed crackle reported by some DAWs after reopening an ARA project.
2. Project restoration and playback-renderer creation now request background cache warmup.
3. The realtime audio thread only reads immutable completed caches; it outputs silence until warmup finishes and never calls `ARAAudioSourceReader`.
4. Breath, Noise, Others, EQ, Gain, monitoring, and analysis behavior are unchanged.