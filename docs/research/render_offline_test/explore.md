# Research: Explore — Offline Visual Correctness Testing for the Renderer

**Session date:** 2026-06-22
**Folder:** docs/research/render_offline_test/

## Problem Space Overview

Visual-correctness testing for a GPU renderer is fundamentally a "does this frame look right?" question. The challenge is that "right" must be defined in advance (a reference image or a set of expected properties), and comparisons need to be cheap enough to run frequently without requiring a human to visually inspect every result.

The goal here is to lower the token cost of involving AI in renderer quality work. Currently the workflow is: run the app → screen-grab → paste into conversation → describe what you see. Each round is expensive because the AI must interpret a full-frame image in context. If instead the engine emits structured artefacts — pixel diffs against a known baseline, per-region colour histograms, draw-call counters — then AI (or a human) can triage quickly: is the diff zero? Is the histogram within tolerance? Are draw calls unchanged? Only failures need visual inspection.

The secondary goal is to make this workflow autonomous and repeatable: a test stage that captures, compares, and emits a pass/fail without any human in the loop, so the system can be run offline (CI or just a local `dia run`) and produce a report that's small enough to attach to a conversation.

## Existing Approaches

**In the industry:**
- **Reference image comparison** — render a known scene, compare pixel-for-pixel or with a tolerance budget. Used by WebGPU CTS, OGRE, Godot engine, Unreal's automation framework.
- **Perceptual diff (pdiff / SSIM)** — compare images accounting for human visual sensitivity; suppresses false positives from sub-pixel rasterisation differences.
- **Region of interest (ROI) probes** — instead of whole-frame diff, sample N rectangular windows to check colour means and variances. Much cheaper to store and compare.
- **G-buffer channel dumps** — capture depth, normals, albedo separately for targeted debugging.
- **Golden file pipelines** — reference frames checked into source control; failures produce diff images; CI marks test red.
- **Frame capture + AI captioning** — render a frame, save as PNG, attach to an LLM request with a structured prompt asking specific questions ("is the shadow on the left or right of the cube?"). Dramatically cheaper than free-form screen-grabs.
- **Shader unit tests** — test shader logic offline using CPU-side math equivalents; no GPU required.
- **Headless rendering / offscreen surfaces** — render to an offscreen framebuffer and read back without opening a window. Used for CI.

**C++ game engine precedents:**
- Godot: `create_capture()` + `compare_to_image()` in GDScript tests.
- bgfx itself: frame debug capture via `bgfx::requestScreenShot` (already wired in this engine).
- Unreal Engine: Gauntlet test framework with screenshot comparison per actor.

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| Capture granularity | Whole frame · Region · Specific render target | Whole frame easiest to start; regions more stable to camera drift |
| Comparison method | Exact pixel diff · Tolerance budget · Perceptual (SSIM) · Histogram · AI-described | Exact is brittle on AA; SSIM needs library; histogram is cheap and robust |
| Reference storage | Checked-in PNG files · JSON of ROI stats · AI-generated description | PNGs are large; JSON stats are tiny; descriptions enable LLM checks without image upload |
| Output format | PNG diff image · JSON report · Markdown summary · SARIF | JSON + Markdown feeds into `dia check` family easily |
| Automation level | Manual trigger · Stage-integrated · CI hook | Stage-integrated is least friction |
| AI role | Primary judge · Triage reviewer · Off | For cheap token usage: AI reads JSON diff report, not raw frame |
| Headless mode | Offscreen bgfx target (DirectX WARP) · Normal window | WARP removes GPU dependency for CI |

## Known Tradeoffs

- **Exact pixel diff vs. tolerance** — exact comparison breaks on driver updates, MSAA jitter, temporal AA. A tolerance budget (e.g. ≤1% of pixels differ by >2/255) is much more stable.
- **Storing PNG references** — large, slow to diff in git, but humans can inspect them directly. JSON stats are tiny but can't be visually compared.
- **AI as primary judge is token-expensive** — AI is cheapest as a triage step when a structured diff report already flags the suspicious frames; it only needs to look at outliers.
- **Headless rendering on Windows** — bgfx supports DirectX WARP (software rasteriser) which gives deterministic output regardless of GPU model, useful for CI but slow.
- **Test-stage coupling** — embedding capture in test stages ties the comparison to stage execution; a dedicated "golden run" mode is cleaner but requires more infrastructure.

## Known Pitfalls (C++ / game engine context)

- bgfx screenshot readback is async (1–2 frames delay); polling without a fence can capture the wrong frame.
- BGRA vs. RGBA byte order must be normalised before comparison (this engine already handles it in `FrameCaptureRingBuffer`).
- Y-flip differences between APIs (D3D vs. Vulkan vs. OpenGL) can cause identical scenes to compare as 100% different (this engine already handles yflip in `FrameCaptureRingBuffer`).
- Reference images generated on one GPU/driver may differ on another due to floating-point precision differences in shaders.
- Writing files from the render thread requires synchronisation with the filesystem; safer to hand off pixel data to a worker thread.
- Checking large PNG files into git LFS requires setup; JSON stats sidestep this entirely.

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| DiaBgfx (`Canvas`, `FrameCaptureRingBuffer`) | Already has 4-slot async screenshot pipeline; pixel data available as RGBA8 in-memory — just needs a writer |
| DiaGraphics (`ICanvas`, `FrameCaptureResult`) | Clean interface for requesting and polling captures; already surfaced as virtual methods |
| DiaObservation (`DIA_CAPTURE` macro, `CaptureMetadata`) | High-level capture trigger already wired into `TestStageModuleBase::FireCapture()` |
| DiaBgfx3D (`Canvas3D`, `MeshRenderer`) | Produces the 3D frames under test; has trace zones and draw-call metrics already |
| DiaCore (containers, JSON, IO) | Can hold diff results and write JSON reports; `DiaIO` for file output |
| CluicheTest (`TestStageModuleBase`, `Mesh3DRenderSystemTestStageModule`) | Test stages already coordinate frame capture with render fences; have pass/fail checkpoints |

### What is Already Built

The foundation is surprisingly complete:
- `RequestFrameCapture()` → `PollFrameCapture()` async pipeline exists in `Canvas`
- BGRA→RGBA conversion and yflip already handled in `FrameCaptureRingBuffer`
- `DIA_CAPTURE(tag, context)` macro already triggered on test pass/fail in `TestStageModuleBase`
- Render fence sync (`mRenderFence.FetchLatest()`) ensures captures are frame-accurate

**What is missing:** a pixel writer, a comparison engine, a reference store, and a report format.

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-001 StringCRC | Capture tags and test names should use StringCRC keys; OK in internal logic, string form fine in JSON output |
| PD-004 No STL in public APIs | Pixel comparison and diff output structs must use DiaCore containers or primitives in any public header |
| PD-007 C++20 | `std::span`, `std::byte` available for pixel buffer views without owning memory |
| PD-009 out/ directory | Frame captures and reports should land in `Cluiche/out/CluicheTest/captures/` |
| PD-010 .diagame / .diastage | Stage metadata could reference a `captures/` directory; doesn't break any existing schema |

## Open Questions for Ideation

- Should the pixel writer live in DiaBgfx (close to the data) or in a new `DiaCaptureTest` module (clean separation)?
- Is the comparison engine a C++ library component, a Python script, or both?
- What is the right reference storage format for the AI triage use case — PNG (visually inspectable) or JSON stats (tiny, LLM-friendly)?
- How does the system handle first-run (no reference exists yet) — auto-generate, or require an explicit "bless this frame" step?
- Should AI analysis happen inline (the test stage queries an LLM and writes a verdict) or offline (a human/CI step reads the report and optionally invokes the LLM)?
- Is SSIM necessary, or is a simple tolerance pixel-count diff good enough to start?
