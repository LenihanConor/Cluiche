**Spec:** @docs/specs/applications/dia/systems/diarendertest/diarendertest.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | png-writer — `FrameCaptureWriter` in DiaBgfx: add `stb_image_write.h` to `External/bimg/3rdparty/stb/`, create `Dia/DiaBgfx/Capture/FrameCaptureWriter.h/.cpp` (WriteAsync + WriteSync), add `StbImageWriteImpl.cpp` to DiaBgfx, update DiaBgfx.vcxproj + .vcxproj.filters | GoogleTests: write PNG to temp path, verify file exists and is non-zero bytes (sync); fire WriteAsync, sleep briefly, verify file | Pending | sonnet | |
| 2 | diff-engine — create `DiaCaptureTest` module: `FrameDiff.h/.cpp`, `CaptureReport.h/.cpp`, `DiaCaptureTest.vcxproj`, `DiaCaptureTest.vcxproj.filters`, register in `Cluiche.sln`, create `dia.capturetest.architecture.module.md` | GoogleTests: Diff identical buffers → pass/0% diff; Diff buffers with one changed pixel → fail + correct region stats; CaptureReportWriter writes valid JSON file | Pending | sonnet | |
| 3 | metrics-writer — add `MetricsWriter.h/.cpp` to DiaCaptureTest, add to vcxproj/.filters | GoogleTests: Write entries to temp path, read back and verify JSON keys/values | Pending | sonnet | |
| 4 | expectations — add `ExpectationEvaluator.h/.cpp` to DiaCaptureTest, add to vcxproj/.filters | GoogleTests: Load .expectations.json, evaluate brightness rule pass, evaluate brightness rule fail, metric rule | Pending | sonnet | |
| 5 | python-tools — `Tools/render_diff.py` (SSIM + histogram + diff PNG), `Tools/render_ai_report.py`, `dia check render-diff` subcommand in `Dia/DiaCLI/dia_cli/cli/check.py`, `Dia/DiaCLI/dia_cli/cli/capture.py` (bless + list) | `dia test cli` smoke run; manual `--help` on all new subcommands | Pending | sonnet | |
| 6 | cluichetest-integration — wire `TestStageModuleBase` to call `MetricsWriter` at `FireCapture()`, add `.expectations.json` files for existing 3D stages under `Cluiche/Assets/CluicheTest/captures/expectations/`, add `out/CluicheTest/captures/` paths to `.gitignore` | `dia run cluichetest` builds and runs; verify metrics JSON written at stage end | Pending | sonnet | |
