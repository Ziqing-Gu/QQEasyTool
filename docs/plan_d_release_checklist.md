# Plan D Release Checklist

Plan D must include all Plan A, Plan B, and Plan C deliverables, plus:

1. Build and verify Windows x64 VST3.
2. Build and verify macOS Apple Silicon VST3.
3. Build and verify macOS Intel x86_64 VST3.
4. Build and verify macOS Universal 2 AU/ARA.
5. Create one release folder under D:\Codex\Outputs with `Win` and `Mac` subfolders. Put the Windows VST3 archive in `Win`, put the Apple Silicon VST3, Intel VST3, and Universal 2 AU archives in `Mac`, and keep the Chinese/English installation guides at the release-folder root.
6. Include Chinese and English installation guides covering both Windows and macOS installation. Name them `QQEasyTool <version> Windows与macOS安装使用说明（中文）.txt` and `QQEasyTool-<version>-Windows-macOS-INSTALL.txt`.
7. Verify architectures and SHA-256 checksums internally, but do not place architecture proof text files or `SHA256SUMS.txt` in the desktop end-user package.
8. Update the bilingual GitHub README with the current version and release changes.
