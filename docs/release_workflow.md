# Release workflow / 发布流程
Plan A: build and verify local Windows output.
Plan B: one-time complete source snapshot; freeze after completion and never inspect/refresh it in later plans.
Plan C: synchronize public source and bilingual history; reuse the verified local Windows output, build three macOS formats from the same code, verify all four packages, and deliver an actual desktop folder.
Plan D: publish verified assets in this same public repository and maintain real direct-download links in README.
Windows cloud CI is optional: manually dispatch build_windows=true only when explicitly requested. Documentation corrections reuse approved binaries and skip CI.

Plan C 复用已验证本机 Windows 成品，GitHub 默认只构建三类 macOS。Plan D 发布本仓库 Release 与逐项下载入口。Plan B 完成后冻结，不能再次读取或刷新。
本次补交不改变代码、不重新构建或安装。用户明确要求 QQEasyTool 不附加独立说明书。
