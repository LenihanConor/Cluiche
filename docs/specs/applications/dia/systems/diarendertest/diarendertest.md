# System Spec: DiaRenderTest

## Parent Application
@docs/specs/applications/dia/dia.md

**Status:** `Approved`

**Research:** @docs/research/render_offline_test/summary.md

---

## Purpose

DiaRenderTest is the offline visual correctness pipeline for the Dia renderer. It captures frames from any `ICanvas`-backed renderer, diffs them against blessed reference frames, emits structured JSON reports, and evaluates hand-authored scene expectation files — enabling AI and human triage of rendering regressions without manual screen-grabs.

The system is split across two layers:

- **`DiaBgfx` extension (C1):** `FrameCaptureWriter` — a reusable PNG writer that persists any `FrameCaptureResult` to disk. Lives in DiaBgfx because it is general-purpose observability infrastructure, not test-specific.
- **`DiaCaptureTest` (new Dia module, C3/C4/C10):** diff engine, JSON report serialiser, and scene expectation evaluator. Test-semantics layer; any game or tool can depend on it.
- **`Tools/` Python scripts (C5/C6/C7):** offline SSIM analysis, `dia check render-diff` CLI entry point, and AI prompt template. Follow `dia_modules.py` precedent.
- **`CluicheTest` convention (C2/C8):** bless workflow, per-frame metrics JSON, and `.expectations.json` files. App-side; not part of the Dia module.

```
[Test stage fires DIA_CAPTURE]
        ↓
DiaBgfx::FrameCaptureWriter  ← C1: write RGBA8 → PNG
        ↓ out/CluicheTest/captures/run/<tag>.png
dia capture bless <tag>       ← C2: promote run → reference
        ↓ out/CluicheTest/captures/reference/<tag>.png
DiaCaptureTest::FrameDiff     ← C3: pixel diff, 4×4 region grid
        ↓
DiaCaptureTest::CaptureReport ← C4: JSON report with metadata
        ↓ out/CluicheTest/captures/reports/<tag>_report.json
Tools/render_diff.py          ← C5: SSIM, histogram, diff PNG, side-by-side
        ↓
dia check render-diff         ← C6: one-liner CI/developer entry point
DiaCaptureTest::ExpectationEvaluator ← C10: hand-authored rules → AI auto-bless
Tools/render_ai_report.py     ← C7: structured AI prompt, optional Claude API call
CluicheTest metrics JSON      ← C8: draw calls, GPU mesh count per stage run
```

---

## Responsibilities

### DiaBgfx — `FrameCaptureWriter` (C1)
- Accept a `Dia::Graphics::FrameCaptureResult` (RGBA8, width, height, pitch) and write a PNG to a caller-supplied path using `stb_image_write`
- Write is dispatched to a worker thread; render thread is not stalled
- Live under `Dia/DiaBgfx/Capture/FrameCaptureWriter.h/.cpp`

### DiaCaptureTest — new module (C3, C4, C10)
- Provide `Dia::CaptureTest::FrameDiff` — in-process pixel diff of two RGBA8 buffers; configurable per-channel threshold; produces `FrameDiffResult` with: total pixels, differing pixel count, differing %, max delta, and per-cell stats for a configurable N×N region grid (default 4×4)
- Provide `Dia::CaptureTest::CaptureReportWriter` — serialises `FrameDiffResult` + `CaptureMetadata` (tag, frame number, backend, git commit) to a JSON file conforming to the report schema
- Provide `Dia::CaptureTest::ExpectationEvaluator` (C10) — loads a `.expectations.json` file, evaluates each rule against a `FrameCaptureResult` + `FrameDiffResult`, returns pass/fail per rule; supports numeric region rules and metric rules
- Provide `Dia::CaptureTest::MetricsWriter` — serialises per-frame metric values (draw calls, GPU mesh count, frame time) to a JSON file alongside the capture report; usable from any app's stage base class
- Provide `DiaCaptureTest.vcxproj` static library project registered in `Cluiche.sln`
- Provide `dia.capturetest.architecture.module.md` YAML module documentation

### Tools/ — Python scripts (C5, C6, C7)
- `Tools/render_diff.py` — offline SSIM (scikit-image), per-channel histogram, colour-coded diff PNG, side-by-side composite PNG; accepts `--run` and `--ref` directory paths
- `Tools/render_ai_report.py` (C7) — reads a JSON report, produces a compact structured prompt for minimal token consumption; optionally calls the Claude API and writes the verdict back into the report JSON; `--ai-report` flag
- DiaCLI `dia check render-diff` (C6) — entry point that runs `render_diff.py`, parses the report, prints human-readable summary, exits non-zero on failure
- DiaCLI `dia capture bless <tag>` (C2) — promotes `out/CluicheTest/captures/run/<tag>.png` to `out/CluicheTest/captures/reference/<tag>.png`
- DiaCLI `dia capture list` — lists all run captures with their bless status

### CluicheTest convention (C2, C8)
- Per-stage `.expectations.json` files under `Cluiche/Assets/CluicheTest/captures/expectations/<tag>.json`
- Per-stage metrics JSON written by `TestStageModuleBase` at stage end: draw calls, GPU mesh count, frame time
- Blessed reference PNGs under `Cluiche/Assets/CluicheTest/captures/reference/` (checked in)

## Non-Responsibilities

- **Visual debugging UI** — owned by `RenderTestPlugin` (CluicheEditor, separate spec)
- **Headless / offscreen rendering** — WARP mode deferred; not required to run this pipeline
- **GPU performance profiling** — use RenderDoc or PIX; C8 metrics are draw-call counts only
- **Shader unit tests** — out of scope; test shader logic through stage renders
- **Multi-app capture coordination** — one app at a time; no cross-app diff
- **Video / multi-frame temporal comparison** — out of scope

---

## Public Interfaces

### `Dia::Bgfx::FrameCaptureWriter` (C1)

```cpp
// Dia/DiaBgfx/Capture/FrameCaptureWriter.h
namespace Dia { namespace Bgfx {

class FrameCaptureWriter
{
public:
    // Write result to path asynchronously (fire-and-forget worker thread).
    // Returns immediately. path must be valid for the duration of the write.
    static void WriteAsync(const Dia::Graphics::FrameCaptureResult& result,
                           const char* path);

    // Synchronous variant — blocks caller until write completes.
    static bool WriteSync(const Dia::Graphics::FrameCaptureResult& result,
                          const char* path);
};

} }
```

### `Dia::CaptureTest::FrameDiffResult` (C3)

```cpp
// Dia/DiaCaptureTest/FrameDiff.h
namespace Dia { namespace CaptureTest {

struct RegionStat
{
    unsigned int row;
    unsigned int col;
    float        differingPct;
    unsigned int maxDelta;
};

struct FrameDiffResult
{
    bool         pass;
    unsigned int totalPixels;
    unsigned int differingPixels;
    float        differingPct;
    unsigned int maxDelta;
    unsigned int threshold;

    static constexpr unsigned int kMaxRegions = 16; // 4x4 grid
    RegionStat   regions[kMaxRegions];
    unsigned int regionCount;
};

class FrameDiff
{
public:
    // gridDim: number of rows/cols in the region grid (default 4 → 4×4)
    static FrameDiffResult Diff(const void* refPixels,
                                const void* runPixels,
                                unsigned int width,
                                unsigned int height,
                                unsigned int threshold,
                                unsigned int gridDim = 4);
};

} }
```

### `Dia::CaptureTest::CaptureReportWriter` (C4)

```cpp
// Dia/DiaCaptureTest/CaptureReport.h
namespace Dia { namespace CaptureTest {

struct CaptureMetadata
{
    const char*  tag;          // stage name / capture tag
    unsigned int frameNumber;
    const char*  backend;      // "dx11", "dx12", "vulkan"
    const char*  commitHash;   // 8-char git short hash, or ""
};

class CaptureReportWriter
{
public:
    // Writes JSON report to path. Returns false on IO error.
    static bool Write(const char*           path,
                      const CaptureMetadata& meta,
                      const FrameDiffResult& diff);
};

} }
```

### `Dia::CaptureTest::ExpectationEvaluator` (C10)

```cpp
// Dia/DiaCaptureTest/ExpectationEvaluator.h
namespace Dia { namespace CaptureTest {

struct RuleResult
{
    const char*  ruleId;
    bool         pass;
    float        actualValue;
    float        expectedValue;
    const char*  op; // ">", "<", "==", "near"
};

struct EvaluationResult
{
    bool        allPass;
    RuleResult  results[32];
    unsigned int ruleCount;
};

class ExpectationEvaluator
{
public:
    // Load .expectations.json from path.
    bool Load(const char* path);

    // Evaluate all rules against the capture result and diff.
    // Metric rules (draw_calls, gpu_mesh_count) require metricJson != nullptr.
    EvaluationResult Evaluate(const Dia::Graphics::FrameCaptureResult& capture,
                              const FrameDiffResult& diff,
                              const char* metricJson = nullptr) const;
};

} }
```

### `Dia::CaptureTest::MetricsWriter` (C8)

```cpp
// Dia/DiaCaptureTest/MetricsWriter.h
namespace Dia { namespace CaptureTest {

struct MetricEntry
{
    const char*  key;
    float        value;
};

class MetricsWriter
{
public:
    // Write metrics to path as JSON. Returns false on IO error.
    static bool Write(const char*       path,
                      const char*       tag,
                      unsigned int      frameNumber,
                      const MetricEntry entries[],
                      unsigned int      entryCount);
};

} }
```

### `.expectations.json` schema (C10)

```json
{
  "tag": "mesh3d_render_system",
  "rules": [
    { "id": "frame_not_black",   "type": "brightness", "region": [0,0], "op": ">",    "value": 0.02 },
    { "id": "cube_centre_lit",   "type": "brightness", "region": [2,2], "op": ">",    "value": 0.30 },
    { "id": "shadow_edge_tight", "type": "diff_delta", "region": [1,2], "op": "<=",   "value": 4    },
    { "id": "draw_calls",        "type": "metric",     "key": "draw_calls", "op": "==", "value": 3  }
  ]
}
```

### JSON report schema (C4)

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

---

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| diarendertest-png-writer | `FrameCaptureWriter` in DiaBgfx — async + sync PNG write from `FrameCaptureResult`; `stb_image_write` dependency | TBD | Approved |
| diarendertest-diff-engine | `DiaCaptureTest` module — `FrameDiff`, `FrameDiffResult`, `CaptureReportWriter`; JSON report schema | TBD | Approved |
| diarendertest-expectations | `ExpectationEvaluator`, `.expectations.json` schema, AI auto-bless flow (C10) | TBD | Approved |
| diarendertest-python-tools | `render_diff.py` (SSIM + histogram + diff PNG), `render_ai_report.py`, `dia check render-diff`, `dia capture bless` | TBD | Approved |
| diarendertest-metrics-writer | `DiaCaptureTest::MetricsWriter` (C8) — per-frame metric serialisation usable from any stage base class | TBD | Approved |
| diarendertest-cluichetest-integration | Wire `TestStageModuleBase` to `MetricsWriter`, per-stage `.expectations.json` files, reference PNGs for existing 3D stages | TBD | Approved |

---

## Dependencies on Other Systems

| System | Role |
|--------|------|
| **DiaBgfx** | `FrameCaptureResult` pixel data source; `FrameCaptureWriter` lives here (C1) |
| **DiaGraphics** | `FrameCaptureResult`, `FrameCaptureToken` — interface types |
| **DiaObservation** | `DIA_CAPTURE` macro already triggers captures in `TestStageModuleBase`; `DIA_LOG_*` in writer and diff engine |
| **DiaCore** | JSON serialiser (report writer), IO (file write), `StringCRC` for capture tags |
| **DiaCLI** | Hosts `dia check render-diff`, `dia capture bless`, `dia capture list` commands |
| **CluicheTest** | `TestStageModuleBase` — integration point for metrics JSON and capture firing |

**Dependents:**
- **RenderTestPlugin (CluicheEditor)** — reads JSON reports and PNG captures produced by this system; deferred spec

---

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| DRT-001 | `FrameCaptureWriter` lives in DiaBgfx, not DiaCaptureTest | PNG write is general-purpose observability infrastructure reusable by any system using `ICanvas`; test-specific logic belongs in DiaCaptureTest | Module placement | Accepted | Yes |
| DRT-002 | 4×4 region grid is the default diff granularity | 16 numbers per frame is the minimum for meaningful AI triage; configurable for finer analysis without breaking the schema | Diff engine | Accepted | Yes |
| DRT-003 | JSON report schema is stable from v1.0; breaking changes require a version bump field | Downstream scripts and the RenderTestPlugin editor depend on the schema; silent breakage is unacceptable | Report schema | Accepted | Yes |
| DRT-004 | Reference PNGs stored under `Cluiche/Assets/CluicheTest/captures/reference/` (checked in); run captures and reports under `Cluiche/out/CluicheTest/captures/` (gitignored) | PD-009: generated output under `out/`; references are source assets and belong in `Assets/` | Output paths | Accepted | Yes |
| DRT-005 | Bless step is manual first-run; subsequent new stages use C10 AI auto-bless | Manual bless requires human sign-off on the first capture; AI auto-bless is gated on all expectation rules passing — not a fully automated black-box | Bless workflow | Accepted | Yes |
| DRT-006 | No STL containers in `FrameDiffResult`, `EvaluationResult`, or any DiaCaptureTest public header | PD-004; fixed-size arrays with explicit count fields are the pattern | DiaCaptureTest public API | Accepted | Yes |
| DRT-007 | Python tooling uses `Pillow`, `numpy`, `scikit-image`; added to `dia env` SDK manifest. `stb_image_write.h` lives in `External/stb/`, registered in `dia env` SDK manifest — not sourced from bgfx's internal `3rdparty/` tree | Standard scientific Python stack + explicit stb dependency; `dia env setup` installs both; no fragile coupling to bgfx internals | Tools/ + DiaBgfx | Accepted | Yes |
| DRT-008 | C9 (headless/WARP mode) is explicitly out of scope for this system | Risk/cost of WARP determinism on Windows is non-trivial; deferred until the CLI pipeline is proven in normal GPU runs | Scope boundary | Accepted | Yes |

---

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|-----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Capture tags used as `StringCRC` keys internally; string form used in JSON output |
| PD-004 | Platform | No STL containers in public APIs | `FrameDiffResult`, `EvaluationResult` use fixed-size arrays + count fields (DRT-006) |
| PD-005 | Platform | x64 only | All `.vcxproj` targets x64 |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaCaptureTest.vcxproj` + `.vcxproj.filters` maintained manually; registered in `Cluiche.sln` |
| PD-007 | Platform | C++20 required | All sources compiled under `/std:c++20`; `std::span` usable for pixel buffer views |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | `DiaCaptureTest.vcxproj` inherits; no per-project overrides |
| PD-009 | Platform | Generated output under `Cluiche/out/<AppName>/` | Run captures and reports at `out/CluicheTest/captures/`; references in `Assets/` (DRT-004) |
| AD-001 | Dia App | Module YAML frontmatter documentation | `dia.capturetest.architecture.module.md` required |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004 |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | `Dia::CaptureTest::` for new module; `Dia::Bgfx::` for writer extension |

---

## Open Design Questions

_All questions resolved._

| # | Question | Decision |
|---|----------|----------|
| 1 | Where does `stb_image_write.h` live? | `External/stb/stb_image_write.h` — explicit dependency, registered in `dia env` SDK manifest. Do NOT source from bgfx's internal `3rdparty/` tree (fragile coupling to bgfx's internal layout). |
| 2 | AI auto-bless invocation — API call or prompt file? | Prompt file only. `render_ai_report.py` writes `<tag>_ai_prompt.txt` — a structured prompt the developer pastes into a conversation. No `ANTHROPIC_API_KEY` dependency, works offline, no network failure modes. |
| 3 | Metrics JSON coupling — CluicheTest or DiaCaptureTest? | `DiaCaptureTest::MetricsWriter` — any stage base class in any app can use it. CluicheTest's `TestStageModuleBase` calls it; CluicheTest-specific logic stays in CluicheTest. |

---

## Status

`Approved` — In Progress. Plan: @docs/specs/applications/dia/systems/diarendertest/diarendertest.plan.md
