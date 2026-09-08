# Research: Ideate — Offline Visual Correctness Testing for the Renderer

**Input:** docs/research/render_offline_test/explore.md

## Candidates

### Candidate 1: FrameCapture PNG Writer
**Home module/system:** DiaBgfx (new `Capture/FrameCaptureWriter.h/cpp`)
**Size:** S
**Description:** A thin utility that accepts a `FrameCaptureResult` (RGBA8 pixel pointer + dimensions) and writes it to a PNG file on disk. Uses a lightweight single-header PNG encoder (e.g. `stb_image_write.h`, already common in bgfx-adjacent codebases) to avoid a heavy dependency. Output path follows PD-009: `Cluiche/out/<AppName>/captures/<tag>_<framenum>.png`.

This is the lowest-level building block — nothing else can work without it. It does not do comparison; it just persists what the engine already captures in-memory. The writer runs on a worker thread (hand-off from render thread) to avoid stalling the frame.

**Primary value:** Turns the existing in-memory capture pipeline into durable artefacts that scripts, humans, and LLMs can consume.

---

### Candidate 2: Reference Baseline Store + Bless Workflow
**Home module/system:** CluicheTest (`Captures/` directory convention) + `dia` CLI command
**Size:** S
**Description:** A convention and tooling layer that separates "captured frames" from "blessed reference frames." When a test stage runs, captures land in `out/CluicheTest/captures/run/<tag>.png`. A `dia capture bless <tag>` CLI command promotes a run capture to `out/CluicheTest/captures/reference/<tag>.png` (or a checked-in path under `Cluiche/Assets/CluicheTest/captures/reference/`). Re-running the stage then compares against the reference.

The bless step is intentionally manual the first time; after that it's automated. Reference PNGs are small enough to check into git for a handful of test scenes (a single 1280×720 RGBA8 frame is ~3.5 MB uncompressed, ~100–300 KB as PNG).

**Primary value:** Establishes the "known good" ground truth that all comparison candidates depend on. Without this, comparison is meaningless.

---

### Candidate 3: Pixel Diff Engine (C++ in-process)
**Home module/system:** DiaBgfx (new `Capture/FrameDiff.h/cpp`) or a new `DiaCaptureTest` module
**Size:** S–M
**Description:** A C++ component that takes two RGBA8 pixel buffers of equal dimensions and produces a `FrameDiffResult`: total differing pixels, max channel delta, percentage of pixels above a configurable threshold, and optionally a diff-image buffer (red channel = magnitude of difference, useful for writing a visual diff PNG). The threshold is configurable per test tag (e.g. shadow tests get a tighter budget than AA-heavy scenes).

No external library required — the algorithm is a per-pixel loop. Can optionally compute per-region stats (divide frame into a configurable grid, report per-cell max delta) for faster LLM triage (16 numbers vs. a full pixel buffer).

**Primary value:** Produces a structured, numeric verdict — pass/fail + magnitude — that scripts and LLMs can read without any image upload.

---

### Candidate 4: JSON Stats Report Writer
**Home module/system:** CluicheTest (`TestStageModuleBase` extension) or `DiaCaptureTest`
**Size:** S
**Description:** After a diff, serialise the `FrameDiffResult` plus test metadata (stage name, frame number, renderer backend, timestamp, git commit hash) to a JSON file at `out/CluicheTest/captures/reports/<tag>_report.json`. Schema:

```json
{
  "tag": "mesh3d_render_system",
  "frame": 60,
  "backend": "dx11",
  "commit": "856e56ec",
  "pass": false,
  "diff": {
    "total_pixels": 921600,
    "differing_pixels": 4200,
    "differing_pct": 0.46,
    "max_delta": 12,
    "threshold": 4,
    "regions": [
      { "row": 1, "col": 2, "differing_pct": 3.1, "max_delta": 12 }
    ]
  }
}
```

The regions grid (e.g. 4×4) makes LLM triage ultra-cheap: paste 16 numbers, ask "which region has the problem and what kind of artifact could cause a delta of 12 in that area?"

**Primary value:** The primary AI-triage interface. An LLM can read this JSON and give a diagnosis without ever seeing the image.

---

### Candidate 5: Offline Python Diff + Report Script
**Home module/system:** `Tools/` (new `render_diff.py`)
**Size:** S
**Description:** A Python script that runs outside the engine and operates purely on the files written by C1–C4. Given two PNG files (or a captures directory with run/ and reference/ subdirectories), it:
- Computes pixel diff using `Pillow` / `numpy` (no GPU needed)
- Computes SSIM using `scikit-image` for perceptual comparison
- Generates a colour-coded diff PNG (hot = large delta)
- Writes an extended JSON report including SSIM score, per-channel histograms, and a human-readable summary
- Optionally accepts a `--prompt-for-ai` flag that outputs a compact text summary suitable for pasting into an LLM conversation

The script is runnable standalone (`python Tools/render_diff.py --run out/captures/run/ --ref out/captures/reference/`) and also callable from a `dia check render-diff` command.

**Primary value:** SSIM and histogram analysis that is too expensive to implement in C++ but trivial in Python. Decouples the heavy analysis from the engine binary.

---

### Candidate 6: `dia check render-diff` CLI Command
**Home module/system:** DiaAPI (`dia` CLI, new `render-diff` subcommand)
**Size:** S
**Description:** A `dia` CLI entry point that orchestrates the offline workflow: run the Python diff script, parse the JSON report, print a human-readable summary to stdout, and exit non-zero if any test fails its threshold. Integrates with the existing `dia check` family (alongside `deps`, `cppcheck`, `spec-sync`). Can be run standalone or as a CI step.

Also adds `dia capture bless <tag>` (from C2) and `dia capture list` to the CLI surface.

**Primary value:** Makes the whole pipeline a one-liner (`dia check render-diff`) and gives it a home in the existing tooling vocabulary. Lowers the friction to "run it every time."

---

### Candidate 7: Structured AI Prompt Template + `--ai-report` Mode
**Home module/system:** `Tools/render_diff.py` extension (or standalone `Tools/render_ai_report.py`)
**Size:** S
**Description:** A script mode that takes a JSON diff report and produces a compact, structured natural-language prompt designed for minimal token usage when pasted into Claude. The prompt embeds only the relevant numbers (regions above threshold, max delta, pass/fail), attaches the diff PNG if available, and asks targeted questions ("Is this a shadow projection error, a UV mapping error, or a vertex transform error?"). The script can optionally call the Claude API directly (via `anthropic` Python SDK) and write the LLM verdict back into the JSON report.

This closes the loop: capture → diff → structured report → AI verdict → human decision.

**Primary value:** Directly addresses the "lower token usage" goal by pre-structuring what the AI sees. A 200-token prompt with 16 region numbers beats a 2000-token free-form screen-grab description every time.

---

### Candidate 8: Per-Frame Metrics JSON (No Image Required)
**Home module/system:** CluicheTest (`TestStageModuleBase`) + existing metric hooks
**Size:** S
**Description:** At the end of each test stage run, serialise the per-frame metric timeseries already collected by the engine (draw call counts, GPU mesh count, frame time) to a JSON file alongside the capture report. This doesn't require image comparison at all — it checks that the renderer is *doing the right work* (e.g. exactly 3 mesh draw calls for the 3-cube scene) even if visual comparison isn't conclusive.

Pairs with C4 (JSON report) to give a two-axis verdict: "visual diff passed AND draw calls matched expected."

**Primary value:** A zero-image correctness signal. Cheapest possible LLM question: "draw calls this run = 3, expected = 3, pass."

---

### Candidate 9: Headless / Offscreen Render Mode
**Home module/system:** DiaBgfx (`Canvas`) + `dia run` CLI flag
**Size:** M–L
**Description:** Wire up bgfx's DirectX WARP (software rasteriser) backend and an offscreen render target so `dia run cluichetest --headless --stages mesh3d_render_system` can execute without opening a window and without needing a physical GPU. The engine renders N frames, fires captures, writes reports, and exits. Output is identical across machines (WARP is deterministic), making this suitable for CI.

This is the most infra-heavy candidate but unlocks true CI-grade automation: pixel-exact results on any Windows machine regardless of GPU.

**Primary value:** Removes the GPU-and-display requirement from the test loop. Enables running in CI or on a dev machine without launching the full app.

---

## Coverage Map

The candidates span the full design space from explore.md:

| Design Axis | Candidates that address it |
|-------------|---------------------------|
| Capture granularity (whole frame) | C1, C2, C3 |
| Capture granularity (regions / stats) | C3, C4, C5, C7 |
| Comparison method (exact + tolerance) | C3 |
| Comparison method (perceptual SSIM) | C5 |
| Comparison method (metrics, no image) | C8 |
| Reference storage (PNG + JSON) | C1, C2, C4 |
| Output format (JSON report + diff PNG) | C4, C5 |
| Automation / CLI | C6 |
| AI triage (low token) | C4, C7 |
| Headless / CI | C9 |

Scope range: C1 through C8 are all S or S–M, meaning the core pipeline (C1→C2→C3→C4→C5→C6→C7) could be built incrementally. C9 (headless) is the only M–L and is independent enough to defer.
