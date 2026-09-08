# Research: Evaluate — Offline Visual Correctness Testing for the Renderer

**Input:** docs/research/render_offline_test/ideate.md

## Scoring Criteria

- **Engine Value (0.25):** Improves DiaBgfx/DiaGraphics reusability or capability for any future renderer
- **Game Value (0.20):** Directly reduces friction in testing CluicheTest rendering quality
- **Implementation Cost (0.25):** Inverse of effort — 5 = very cheap (hours), 1 = very expensive (weeks)
- **Risk (0.15):** Inverse of uncertainty — 5 = well-understood, 1 = highly uncertain
- **Cluiche Fit (0.15):** Aligns with module structure, PD decisions, existing `dia check` family

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|-----------|:---:|:---:|:---:|:---:|:---:|:---:|
| C1 PNG Writer | 5 | 4 | 5 | 5 | 5 | **4.85** |
| C2 Baseline Store + Bless | 3 | 5 | 5 | 5 | 5 | **4.60** |
| C3 C++ Pixel Diff Engine | 5 | 5 | 4 | 4 | 5 | **4.60** |
| C4 JSON Stats Report | 4 | 5 | 5 | 5 | 5 | **4.75** |
| C5 Python Diff Script | 2 | 5 | 4 | 4 | 4 | **3.75** |
| C6 `dia check render-diff` CLI | 2 | 5 | 4 | 4 | 5 | **3.80** |
| C7 AI Prompt Template | 1 | 5 | 5 | 4 | 3 | **3.55** |
| C8 Per-Frame Metrics JSON | 4 | 4 | 5 | 5 | 5 | **4.55** |
| C9 Headless Mode | 5 | 3 | 1 | 2 | 4 | **2.95** |

## Top 3 Candidates

### Rank 1: C1 — PNG Writer (score: 4.85)
**Why:** The single highest-value, lowest-risk change in the entire pipeline. The engine already captures RGBA8 pixels in-memory with correct byte order and yflip — adding `stb_image_write` (a 1-file header, zero-dependency, already in the bgfx repo) and a `FrameCaptureWriter` class is a few hours of work. Every other candidate in the pipeline is blocked on this one existing. Perfectly fits DiaBgfx.
**Watch out for:** Write timing — must hand off from the render thread to avoid stalling `bgfx::frame()`. A fire-and-forget worker queue is the right pattern.

### Rank 2: C4 — JSON Stats Report (score: 4.75)
**Why:** The primary interface for both AI triage and CI. Once C3 produces a `FrameDiffResult`, serialising it to JSON with stage metadata is trivial (DiaCore already has a JSON writer). The region grid (4×4 = 16 numbers) is the key insight: an LLM can diagnose a shadow artifact from "region [1,2] has 3.1% differing pixels, max delta 12" in a single short prompt with no image attached.
**Watch out for:** Schema stability — once reports are being produced and scripts consume them, changing the schema is a breaking change. Define it carefully up front.

### Rank 3: C2 — Baseline Store + Bless (score: 4.60, tied with C3)
**Why:** The bless workflow is the UX linchpin. Without it, there's no reference to compare against and the whole pipeline produces nothing useful. `dia capture bless <tag>` as a CLI command is a small amount of Python/DiaCLI work and establishes the "known good" convention that the rest of the system relies on. C3 (pixel diff) is equally scored but depends on C2 existing first.
**Watch out for:** Reference drift — if the engine changes intentionally (new shader, different light), references must be re-blessed. Document the bless step clearly so it doesn't feel like a chore.

## Pipeline Dependency Order

The candidates form a strict dependency chain rather than independent choices. The real decision is how far along the chain to build:

```
C1 (PNG Writer)
  └─ C2 (Baseline Store)
       └─ C3 (C++ Pixel Diff)
            └─ C4 (JSON Report)       ← minimum useful AI-triage endpoint
                 ├─ C5 (Python SSIM)
                 │    └─ C6 (dia check render-diff)
                 │         └─ C7 (AI Prompt Template)
                 └─ C8 (Per-Frame Metrics JSON)   ← independent of C5/C6/C7

C9 (Headless Mode)  ← fully independent, deferred
```

**Stopping at C4** gives you: captures on disk, references blessed, numeric diff, JSON report readable by both humans and LLMs. That is the minimum viable offline pipeline.

**Stopping at C8** gives you everything C4 offers plus: SSIM and diff PNGs from Python (C5), a `dia check` one-liner (C6), a structured AI prompt template (C7), and a zero-image metrics verdict (C8). This is the full useful stack.

**C9** is a separate investment (M–L, WARP integration) that makes the pipeline CI-grade but adds significant risk and cost. Recommended as a follow-on spec after C1–C8 are proven.

## Recommendation

Build C1 through C8 as a single system — **"DiaRenderTest pipeline"** — in dependency order. The cumulative cost is low (C1–C4 is a day or two of work; C5–C8 adds another day), and each step is independently useful. C4 is the earliest point where the AI-triage goal is met. C8 adds the zero-image metrics check which is the cheapest possible correctness signal and worth including. C9 should be a separate spec; the risk of WARP determinism issues and offscreen surface wiring on Windows is non-trivial and shouldn't block shipping the core pipeline.

The system aligns with PD-004 (DiaCore containers in the C++ diff engine), PD-009 (all output under `out/CluicheTest/`), and fits naturally into the existing `dia check` family (PD-006). The Python script (C5–C7) lives in `Tools/` following the precedent of `dia_modules.py`.
