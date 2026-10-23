# Release workflow / 发布流程

Current Stable: 1.06; previous designated Stable: 1.04; immediate installed rollback: 1.05.
1.06 follows the user's explicit Windows-only request: validated Windows build, installation, Stable designation, a new frozen Plan B, public source sync and Windows Release upload. No Mac artifacts are produced and no full cross-platform Plan C is claimed.

Plan B is a one-time source snapshot. Once complete, never access, enumerate, rehash or refresh it during later work. Reuse completed builds/tests; documentation-only corrections do not trigger new versions, builds or backups.

The workflow requires explicit manual build_macos=true for Mac jobs and build_windows=true for Windows cloud reproduction. Both default to false. This 1.06 publication uses [skip ci]; no Actions run was triggered.

Follow the user's current Plan specification for future full cross-platform Plan C/D requests. Keep existing numerical version history and old Candidate labels as historical records. No separate user manual is added.

当前 1.06 按用户要求只交付 Windows：完成本机验证、覆盖、Stable、新 Plan B 以及 GitHub 源码和成品上传。未运行 Mac 构建，不宣称完整跨平台 Plan C。后续 Mac 云构建须明确手动选择 build_macos=true。已完成的备份保持冻结；仅改发布文档不重建、不重备份。
