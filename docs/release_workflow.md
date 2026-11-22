# Release workflow / 发布流程

Current Stable: 1.09; previous Stable/rollback: 1.08. Windows build, tests and installation are inherited. Plan B is complete and frozen; never inspect or refresh it during publication.

本版按公开源码 Plan C/D 发布。默认 push 只构建 Apple Silicon VST3、Intel VST3 和 Universal 2 AU；Windows CI 仅明确选择 build_windows=true 时运行。已完成的阶段只继承，不重复构建或备份。

This release uses the public-source Plan C/D route. Source pushes build only the three Mac formats; Windows cloud reproduction is opt-in. Public source, versioned artifacts, bilingual guides and unchanged applicable 1.08 illustrated manuals are delivered together. Preserve historical versions and license terms.
