# QQEasyTool 1.09 installer recipes

These recipes package the existing released product binaries; they do not compile the plug-in. Product source/tag stays `d44e0923e8fa01157391fe6e6bc0016f70616090` / `v1.09`. `packaging/installers/inputs.json` locks released payload hashes and versions. The original complete source release archive corresponds to those plug-ins; installer sources are included here and at `591fb772041f996b6e6d55ecdbb5cb5145af7345`.

Windows: Python 3 and Inno Setup 6. Run `python packaging/installers/windows/build.py --payload PATH_CONTAINING_QQEasyTool.vst3 --output OUTPUT_DIR --work TEMP_DIR --iscc PATH_TO_ISCC.exe`. Run the companion `verify.py --installer EXE --previous PATH_TO_1.08_BUNDLE --work TEMP_DIR --receipt RECEIPT.json` for isolated install/upgrade/lock/uninstall validation. Work/output paths must be explicitly selected. The normal installer uses the system VST3 directory and requires administrator authorization. It does not create shortcuts.

Mac: `.github/workflows/package-installers.yml` is a manually dispatched, package-only workflow on native ARM and Intel runners. It downloads hash-locked existing 1.09 VST3/AU archives, verifies signatures and architectures, packages selectable formats with `pkgbuild`/`productbuild`, expands and hashes the payloads, then tests installation/upgrade on the disposable CI runner. It does not run on a user's Mac for testing. macOS 11 minimum; native VST3 plus unchanged Universal 2 AU. Scripts reject active hosts, symbolic-link target paths and newer installed versions. No system security setting is changed.

Installer outputs are unsigned and not notarized; existing plug-in ad-hoc signatures are preserved. The two Mac VST3 variants share one installation path. Choose the package matching the host process; Intel is also offered for an Intel/Rosetta host on Apple Silicon.

[Successful packaging and installation run](https://github.com/Ziqing-Gu/QQEasyTool/actions/runs/45027696522). Historical build workflow and binaries remain unchanged. Refer to the bilingual installer guides in this directory for user-facing steps.
