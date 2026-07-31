# System Spec: RenderTestPlugin

## Parent Application
@docs/specs/applications/cluicheeditor/cluicheeditor.md

**Research:** @docs/research/render_offline_test/summary.md
**UI Mockup:** @docs/research/render_offline_test/render_test_debugger_mockup.html

## Purpose

RenderTestPlugin is a dockable CluicheEditor panel for inspecting the output of `dia check render-diff` runs. After a render test pipeline executes, it writes PNG captures and a JSON diff report to `out/CluicheTest/captures/`. This plugin reads those files and presents them in a structured visual workspace: a run history browser, a three-up frame inspector (reference / run / diff), a 4×4 region grid overlay, per-region diff metrics, an expectations editor, an AI triage panel, a metrics view, and a render targets browser.

The plugin is **offline-only** — it reads completed test output; it does not connect to a live game. "Inspect and fix" is the primary loop: developer runs tests in the terminal, switches to the editor to diagnose failures, authors or updates expectations, then re-runs.

**Target users:** Engine developers and technical artists who author visual regression tests and need to investigate pixel-level render failures without manually comparing PNG files.

## Responsibilities

- Own the `RenderTestPlugin` `IEditorPlugin` subclass: registration, lifecycle (`OnLoad` / `OnUnload`), dockable panel UI (`dia://rendertest/`)
- Scan `out/CluicheTest/captures/` for run report directories; build and maintain the run history list
- Load the JSON diff report (`render_diff_report.json`) and PNG captures (`reference.png`, `run.png`, `diff.png`) for the selected run
- Render the three-up frame inspector: reference, run, and diff canvases with wipe-slider, channel isolator (R/G/B/A/Depth), scroll-to-zoom, and drag-to-pan
- Compute and display the 4×4 region diff grid over a thumbnail of the diff image; colour cells by severity (hot = >2% diff, warm = >0.3%)
- Show the worst-region callout and expose a threshold slider that live-updates region colouring
- Own the expectations editor tab: load `.render_expectations.json` from `Assets/CluicheTest/captures/reference/<tag>/` (alongside the reference PNG), display pass/fail per rule, allow add/edit/delete of rules, write changes back on save
- Own the AI triage tab: display the raw JSON report, provide an "Ask Claude" action that sends the report (no images) to the DiaChatPlugin orchestrator via `DiaEditorAPI::ExecuteAction("chat.send_context")`, display the streamed verdict, and offer a "Save verdict to report" action
- Own the metrics tab: display renderer stats from the JSON report (draw calls, GPU mesh count, frame time, shadow map resolution, GPU upload) with sparklines across the last N runs
- Own the render targets tab: display named intermediate render targets captured in the run report, with per-target channel mode buttons (RGB, R, G, B, A, Linear, Remap, etc.)
- Display titlebar context for the selected run: stage tag, frame number, backend name, and commit hash alongside the pass/fail badge
- Expose two titlebar buttons: `dia check render-diff` (re-run the diff on the current selection without capturing) and `⊕ New Capture` (capture + diff + refresh run list)
- Expose a "BLESS" action on failing runs that shells `dia capture bless <tag>` via DiaCLI; the same command is exposed as a `DiaEditorAPI` action (`rendertest.bless`) so it can be triggered from DiaChatPlugin or other tooling

## Not Responsible For

- Running the test pipeline itself — that is DiaRenderTest CLI (C1–C8 + C10)
- Capturing frames at runtime — that is `DIA_CAPTURE` / `FrameCaptureRingBuffer` in DiaBgfx
- AI model configuration or backend selection — that is DiaChatPlugin
- Writing diff reports or computing pixel statistics — that is `Tools/render_diff.py`
- Displaying live render targets from a running game — this plugin is post-run / offline only

## Architecture

### Overview

```
RenderTestPlugin (IEditorPlugin)
    ├─ RunHistoryPanel       — scans out/CluicheTest/captures/, lists runs
    ├─ FrameInspectorPanel   — loads PNGs, renders three-up canvas, wipe/channel controls
    ├─ DiffAnalysisPanel     — 4×4 region grid, threshold slider, worst-region callout
    └─ BottomTabBar
         ├─ ExpectationsTab  — loads/saves .render_expectations.json, rule CRUD
         ├─ AITriageTab      — JSON report viewer, "Ask Claude" → DiaEditorAPI, verdict save
         ├─ MetricsTab       — stat cards + sparklines from report JSON
         └─ RenderTargetsTab — intermediate RT thumbnails with channel toggles
```

### Data Flow

```
[dia check render-diff]
    → writes out/CluicheTest/captures/<tag>/<timestamp>/
         render_diff_report.json   (diff stats, region grid, renderer metrics)
         reference.png             (blessed reference frame)
         run.png                   (captured frame from this run)
         diff.png                  (per-pixel absolute delta, amplified)
         render_targets/           (optional named RT PNGs)

RenderTestPlugin (on load / on "New Capture" completion)
    → scans captures dir → RunHistoryPanel list
    → on run selected: reads JSON + PNGs → all panels update
    → AI Triage "Ask Claude" → passes JSON report text to DiaChatPlugin via DiaEditorAPI
    → BLESS action → shells `dia capture bless <tag>` → re-scans
```

### IEditorPlugin Subclass

```cpp
class RenderTestPlugin : public Dia::Editor::IEditorPlugin {
public:
    static constexpr Dia::Core::StringCRC kPluginId{"render_test_plugin"};

    Dia::Core::StringCRC   GetPluginId()  const override { return kPluginId; }
    const char*            GetUIPath()    const override { return "dia://rendertest/"; }
    const char*            GetDisplayName() const override { return "Render Test"; }
    Dia::Editor::DockMode  GetDockMode()  const override { return Dia::Editor::DockMode::kDockable; }

    void OnLoad(Dia::Editor::EditorModel& model)   override;
    void OnUnload(Dia::Editor::EditorModel& model) override;

private:
    RunHistoryScanner   mScanner;
    WebUIBridgeHandlers mHandlers;
};
```

### WebUIBridge Message Contract

The React UI communicates via the editor's WebUIBridge. Messages use StringCRC-keyed handlers:

| JS → C++ (request) | C++ → JS (push) |
|---|---|
| `rendertest.list_runs` | `rendertest.runs_updated` (run list JSON) |
| `rendertest.select_run {tag, timestamp}` | `rendertest.run_loaded` (report + image paths) |
| `rendertest.bless {tag}` | `rendertest.bless_complete` |
| `rendertest.save_expectations {tag, rules[]}` | `rendertest.expectations_saved` |
| `rendertest.ask_claude {report_json}` | (delegates to DiaChatPlugin stream) |
| `rendertest.rerun_diff {tag, timestamp}` | `rendertest.runs_updated` |
| `rendertest.new_capture` | `rendertest.capture_started`, `rendertest.runs_updated` |
| `rendertest.toggle_heatmap` | `rendertest.heatmap_state {enabled}` |

### Capture Directory Layout

```
out/CluicheTest/captures/
    <tag>/
        latest -> <timestamp>/   (symlink or .latest sentinel file)
        <timestamp>/
            render_diff_report.json
            reference.png
            run.png
            diff.png
            render_targets/
                <name>.png       (one per named RT)
```

### AI Triage Integration

The AI Triage tab does not call an LLM directly. It uses `DiaEditorAPI::ExecuteAction("chat.send_context", payload)` where `payload` is the JSON report text. DiaChatPlugin receives this as a pre-filled message and streams the verdict back. This keeps all LLM backend management inside DiaChatPlugin and makes the triage feature a zero-config consumer.

If DiaChatPlugin is not loaded, the "Ask Claude" button is disabled with tooltip "DiaChatPlugin not loaded."

## Features

| # | Feature | Description | Status |
|---|---------|-------------|--------|
| F-00 | Titlebar Context | Stage tag, frame number, backend name, commit hash, pass/fail badge; two action buttons: `dia check render-diff` (re-run diff) and `⊕ New Capture` (capture + diff + refresh) | Draft |
| F-01 | Run History Browser | Scrollable list of past runs grouped by tag; pass/fail dot; timestamp; BLESS button on failing runs | Draft |
| F-02 | Three-Up Frame Inspector | Reference / Run / Diff canvases with shared pan+zoom; wipe slider sweeps between reference and run; channel isolator (RGB, R, G, B, A, Depth) | Draft |
| F-03 | 4×4 Region Diff Grid | Overlay on diff thumbnail; cells coloured by diff%; worst-region callout; threshold slider live-updates colouring; heatmap toggle overlays per-pixel delta on the diff canvas | Draft |
| F-04 | Expectations Editor | Load/save `.render_expectations.json`; add/edit/delete rules; pass/fail status per rule from loaded report | Draft |
| F-05 | AI Triage Panel | JSON report viewer; "Ask Claude" dispatches via DiaEditorAPI; streamed verdict display; "Save verdict to report" | Draft |
| F-06 | Metrics Tab | Stat cards (draw calls, GPU mesh count, frame time, shadow map res, GPU upload) with run-history sparklines | Draft |
| F-07 | Render Targets Tab | Named intermediate RT thumbnails with channel mode toggles (RGB/R/G/B/A/Remap/Linear/Inv) | Draft |
| F-08 | Capture Actions | `dia check render-diff` re-runs diff on current selection; `⊕ New Capture` captures + diffs + refreshes run list; both show progress indicator | Draft |

## Public Interface

No C++ public API beyond `IEditorPlugin`. All functionality is surfaced via the WebUIBridge message contract above. Other plugins interact with RenderTestPlugin only through DiaEditorAPI actions if needed.

## Inherited Binding Decisions

| Source | ID | Decision | Impact on RenderTestPlugin |
|--------|----|----------|-----------------------------|
| Platform | PD-001 | StringCRC for all IDs | `kPluginId`, WebUIBridge handler keys all use `StringCRC` |
| Platform | PD-004 | No STL in public APIs | `IEditorPlugin` overrides and WebUIBridge payloads use DiaCore types or primitives only |
| Platform | PD-006 | VS project files are source of truth | Plugin added to `CluicheEditor.vcxproj` and `.vcxproj.filters` |
| Platform | PD-009 | Generated output under `out/<AppName>/` | All captures and reports live under `out/CluicheTest/captures/` |
| CluicheEditor | AED-001 | DiaEditor is a pure library; CluicheEditor owns app flow | Plugin implements `IEditorPlugin` — no DiaApplicationFlow dependency |
| CluicheEditor | AED-002 | Plugins specified in `.diaapp` manifest `editor` section | `RenderTestPlugin` declared in `editor-plugins.diaapp` |
| CluicheEditor | AED-003 | Each system owns its editor under `<System>/Editor/` | Plugin source lives in `CluicheEditor/RenderTestPlugin/` (CluicheEditor-side plugin, not a Dia system plugin) |
| CluicheEditor | AED-005 | React + DiaUICEF (CEF) for UI | Plugin UI is a React+TypeScript panel served at `dia://rendertest/` |

## Open Design Questions

_None._

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| RTP-001 | Plugin is offline-only; reads `out/CluicheTest/captures/` | Keeps scope clear; live capture is DiaBgfx responsibility; avoids WebSocket dependency | Entire plugin | Accepted | Yes |
| RTP-002 | AI triage delegates to DiaChatPlugin via DiaEditorAPI; no direct LLM calls | Single backend config; zero extra dependencies; degrades gracefully if DiaChatPlugin absent | AI Triage tab | Accepted | Yes |
| RTP-003 | 4×4 region grid is the primary diff unit (16 numbers per frame) | Cost-minimised AI triage; matches research decision from choose.md; no image bytes sent to LLM | Diff analysis | Accepted | Yes |
| RTP-004 | `.render_expectations.json` lives alongside the reference PNG in `Assets/CluicheTest/captures/reference/<tag>/` | Keeps test authoring self-contained; expectations and reference always travel together | Expectations | Accepted | Yes |
| RTP-005 | BLESS is a DiaCLI command (`dia capture bless <tag>`) exposed as a `DiaEditorAPI` action (`rendertest.bless`) | CLI is the canonical tool; editor button is a convenience trigger; scripting from DiaChatPlugin requires the DiaEditorAPI exposure | BLESS action | Accepted | Yes |
| RTP-006 | Render target capture is automatic on failing runs only; passing runs skip RT capture | RTs are a diagnostic tool — only needed when something is wrong; keeps disk cost proportional to failure count; one branch in the CLI pipeline | RT capture | Accepted | Yes |

## Status

`Approved`
