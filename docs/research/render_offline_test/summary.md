# Research Summary — Offline Visual Correctness Testing for the Renderer

**Session folder:** docs/research/render_offline_test/
**Date:** 2026-06-22

## One-Line Answer

Build a CLI pipeline (C1–C8 + C10) that captures frames, diffs against blessed references, emits JSON reports, and uses hand-authored scene expectations + AI auto-bless to bootstrap new test stages — all without manual screen-grabs.

## Journey

1. **Explored:** The engine already has async bgfx frame-capture (RGBA8, BGRA→RGBA, yflip handled), a `DIA_CAPTURE` macro wired into `TestStageModuleBase`, and render fence sync — the hard parts are done. The missing pieces are a file writer, a diff engine, a reference store, and a report format.
2. **Ideated:** 11 candidates generated, ranging from a 1-file PNG writer (S) to a full headless WARP mode (M–L) and a CluicheEditor visual debugger plugin (L).
3. **Evaluated:** C1–C8 + C10 scored highest as a dependency chain (all S or S–M); C9 scored lowest on cost/risk and was excluded; C11 was separated as a distinct backlog item.
4. **Chose:** Two backlog items: DiaRenderTest CLI Pipeline (C1–C8 + C10) and RenderTestPlugin editor (C11); C9 explicitly deferred.

## Chosen Work Items

### Item 1: DiaRenderTest CLI Pipeline
**Candidates:** C1 + C2 + C3 + C4 + C5 + C6 + C7 + C8 + C10
**Home modules:** DiaBgfx (C1, C3), CluicheTest/TestStageModuleBase (C2, C4, C8, C10), Tools/ (C5, C6, C7)
**Suggested spec type:** System
**Estimated size:** M (all candidates are S individually; pipeline integration adds coordination)

### Item 2: RenderTestPlugin (CluicheEditor)
**Candidates:** C11
**Home module:** CluicheEditor (new `RenderTestPlugin`)
**Suggested spec type:** System
**Estimated size:** L
**Depends on:** DiaRenderTest CLI Pipeline (C1–C4 + C10 must ship first)
**Mockup:** docs/research/render_offline_test/render_test_debugger_mockup.html

## Key Insights from Exploration

- The engine's `FrameCaptureRingBuffer` already handles all the fiddly bgfx async readback details (BGRA swap, yflip, generation tokens). C1 is literally "add `stb_image_write` and call it."
- Pixel diff alone is not enough for greenfield stages — C10 (hand-authored expectations + AI auto-bless) is required to bootstrap the first reference without manual inspection.
- The AI-triage token cost is minimised by the 4×4 region grid: 16 numbers in a JSON report let Claude diagnose most artifacts without receiving any image.
- Python (C5) handles SSIM and histogram analysis that is too expensive to implement in C++; `Tools/render_diff.py` follows the `dia_modules.py` precedent.
- C9 (headless WARP) is the only candidate that would make this CI-grade; it's excluded because WARP determinism on Windows is non-trivial and the pipeline doesn't need it to be useful.
- The editor plugin (C11) design is fully defined by the mockup — wipe slider, region grid, expectation authoring, AI triage panel, render targets. It should be spec'd from the mockup directly.

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| C9 — Headless Mode | M–L, high WARP integration risk, not needed to prove the core pipeline |

## References

- docs/research/render_offline_test/explore.md
- docs/research/render_offline_test/ideate.md
- docs/research/render_offline_test/evaluate.md
- docs/research/render_offline_test/choose.md
- docs/research/render_offline_test/render_test_debugger_mockup.html
