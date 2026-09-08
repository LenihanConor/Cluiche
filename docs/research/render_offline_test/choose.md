# Research: Choice — Offline Visual Correctness Testing for the Renderer

**Date:** 2026-06-22
**Chosen candidates:** C1–C8 + C10 (DiaRenderTest CLI Pipeline) and C11 (RenderTestPlugin) as separate backlog items

## Rationale

C1–C8 form a strict dependency chain that delivers the full offline AI-triage pipeline at low cost (all S or S–M). C10 (Scene Expectation Files + AI Auto-Bless) closes the greenfield gap — without it the bless step requires manual human inspection every time a new test stage is added. Together these ship as one system: "DiaRenderTest CLI Pipeline."

C11 (RenderTestPlugin for CluicheEditor) is the visual authoring and debugging layer. The mockup produced during this research session defines it clearly enough to spec now. It is sequenced after the CLI pipeline (depends on C1–C4 + C10 being on disk) and scoped as a separate backlog item so it doesn't block the core pipeline.

C9 (Headless/Offscreen Mode) is explicitly excluded — the risk/cost of WARP integration is not justified until the CLI pipeline is proven in normal GPU runs.

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|------------------|
| C9 — Headless Mode | M–L, high risk (WARP determinism on Windows), not needed to prove the core pipeline; revisit if CI becomes a hard requirement |

## Pre-Spec Commitments

- **Output directory:** all captures and reports under `Cluiche/out/CluicheTest/captures/` (PD-009)
- **Reference storage:** both PNG (human-inspectable) and JSON stats (LLM-triage); checked into `Cluiche/Assets/CluicheTest/captures/reference/`
- **Bless workflow:** manual first run via `dia capture bless <tag>`; subsequent new stages use C10 AI auto-bless
- **Diff granularity:** 4×4 region grid as the primary AI-triage unit (16 numbers per frame)
- **No STL in public diff API** (PD-004); DiaCore containers or primitives only in `FrameDiffResult`
- **Python tooling in `Tools/`** following `dia_modules.py` precedent
- **C11 editor plugin** is deferred until C1–C4 + C10 are shipped; mockup at `docs/research/render_offline_test/render_test_debugger_mockup.html` serves as the visual spec

## Next Steps

- Backlog item 1: `/spec-system DiaRenderTest` — covers C1–C8 + C10 as CLI pipeline
- Backlog item 2: `/spec-system RenderTestPlugin` — covers C11 as a CluicheEditor plugin
- Attach `docs/research/render_offline_test/summary.md` as research context to both specs
