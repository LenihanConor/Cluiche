# Feature Spec: F-1 Debug Host Integration

**Parent:** @docs/specs/applications/cow/systems/s1-worldgeneration/worldgenerationpipeline.md
> **Amended 2026-10-18 (Feature 9 re-scope, @regions-and-sites.md): the panel's Validation section lists every registered guarantee; the Dynasty Starts fixed control is replaced by a Sites overlay with a toggle per registered kind (`SiteKindInfo`); adds the `regions` and `sites` queries and the climate offsets from Feature 8. Full text updated when this feature is built.*

**Gameplay Domains:** simulation

## Summary

F-1 Debug Host Integration exposes WLD's generation controls and live telemetry through F-1's in-game debug host UI, enabling CoW's testbed iteration workflow: designers adjust seed/parameters, click Regenerate, see results within seconds, export/import authored worlds, and monitor generation progress live. This feature wires `WorldGenerationModule` APIs to F-1's debug UI (seed input, terrain and climate sliders, placement param sliders, Regenerate/Export/Import/Cancel buttons, progress bar, retry telemetry display).

*Amended 2026-10-15:* F-1's debug host for data panels is the DiaDebugPanel (`IDebugDomain`), so this panel is the card of Feature 7's `WorldGenDebugDomain`, built from its `GetJSONState` / `OnCommand`. Besides the controls it now explains the data: a **Cell Inspector** (every field of the selected cell) and a **Validation** section (each guarantee's result and evidence summary, plus the per-attempt retry history). Export/Import is this testbed's save/load of a world.

GDD decision 3 chose in-game parameter tuning over a standalone editor — this feature implements that decision.

## Problem

Without debug host integration:
- **No iteration workflow** — Designers cannot tweak parameters and regenerate without editing code or config files (unacceptable for fast iteration)
- **Blind generation** — No live feedback on retry attempts, validation failures, or timeout progress (designers don't know if generation is stalled or progressing)
- **Manual export/import** — No UI for export/import paths, forcing command-line or hardcoded paths (error-prone)
- **Feature 1-5 inaccessible** — All WLD capabilities (generation, validation, retry, export/import, timeout) exist but are unusable without UI

With debug host integration:
- **Fast iteration** — Adjust seed → Regenerate → inspect in <10 seconds (meets GDD §7's <5-second generation + <5-second feedback target)
- **Live telemetry** — Progress bar shows "Attempt 2/10: validating...", retry count, elapsed time (designers know generation is working, not hung)
- **Authorable worlds** — Export/Import buttons with file path inputs enable hand-editing workflow (export → edit JSON → re-import)
- **Safe cancellation** — Cancel button triggers Feature 5's clean discard-and-return (designers can abort bad seeds without restarting app)

## Goals

1. **Parameter inputs exposed** — Seed (integer input), climate offsets (`temperatureOffset`, `moistureOffset` sliders; replace per-biome weights, Feature 8), terrain shape sliders (`WorldShapeParams`, Feature 1). Placement params for dragon-layer sites come later with the dragon layer
2. **Action buttons** — Regenerate (triggers `GenerateWorld()`), Export (triggers `ExportWorld()`), Import (triggers `ImportWorld()`), Cancel (triggers `CancelGeneration()`)
3. **Live progress display** — Progress bar or text showing current attempt number, elapsed time, validation status ("Generating...", "Validating attempt 2/10...", "Complete" / "Failed")
4. **Retry telemetry** — Display final retry count on success, or failure diagnostics (timeout/unsatisfiable/error/cancel) on failure
5. **File path inputs** — Export/Import file path text inputs with default values (`Cluiche/out/CoW/worlds/<seed>.wld.json`)
6. **Non-blocking UI** — Generation runs on background thread (SimPU); UI remains responsive during generation (Cancel button clickable, progress updates live)
7. **Cell Inspector** *(added 2026-10-15)* — For the cell selected in Feature 7 (click) or typed here (x, y), shows:
   - coordinate and cell index;
   - `biome`, `terrain`, `region`, `elevation`, `latitude`, `waterSource`;
   - any site on the cell (type and id);
   - its six neighbours by direction `0..5`, as coordinates or "edge";
   - Design Reference: species affinity for its biome, only if A1 data is externalized (was a Feature 7 tooltip).
8. **Validation section** *(added 2026-10-15)* — From `GetLastReport()`:
   - outcome, attempt count and outer seed;
   - one row per guarantee: pass/fail, diagnostics, and an evidence count ("10 proof paths, 0 violations"), with each violating cell selectable;
   - one row per attempt: index, sub-seed, failed guarantee ids, elapsed ms.
9. **Validate button** *(added 2026-10-15)* — Runs Feature 2 on the current world (e.g. after a hand-edited import) and replaces the Validation section and Feature 7's evidence with the result. Does not change the world
10. **Map size** *(added 2026-10-15; amended 2026-10-19, product review)* — A picker for Small 66 × 42 (default), Standard 80 × 52 and Large 104 × 64, calling `cow.worldgen.set_size` (parent spec, Map Sizes)

## Non-Goals

1. **Visual rendering** — Feature 7 (Layered Debug Rendering) handles world visualization; this feature handles controls only
2. **Automatic regeneration** — *(Amended 2026-10-19):* Terrain and climate sliders auto-regenerate via live preview (SD-S1-004); designer clicks Regenerate only for sites/validation. This non-goal originally stated sliders don't auto-regenerate, but that was superseded by the live-shaping decision
3. **Parameter validation** — UI accepts any integer/float input; Feature 1-2 handle validation (e.g., negative seed, impossible placement params)
4. **Undo/redo** — Each Regenerate produces new world, discarding previous; no history stack (future enhancement if needed)
5. **Session Save/Load UI** — World save/load *is* Export/Import here; E1 (save/load) handles live session persistence (separate UI)

## Architecture

### UI Layout

**Panel name:** "World Generation": the DiaDebugPanel card of `WorldGenDebugDomain` (amended 2026-10-15). Buttons send domain commands (`regenerate`, `export`, `import`, `cancel`, `validate`, `cells.select.cell` (DiaCellLayers; was `selectCell`), `setColourMode`, `toggle`). The domain turns each into the request events below; the panel never calls the module directly.

**Sections:**

1. **Parameters** *(amended 2026-10-19 for Feature 8 climate offsets, Feature 9 dragon-layer placement)*
   - Seed: integer text input (default: random)
   - Terrain sliders: `WorldShapeParams` (Land, Continents, Islands, Peninsulas, etc., Feature 1)
   - Climate sliders: `temperatureOffset`, `moistureOffset` (Feature 8; replaced per-biome weights)
   - Placement Params: *(dragon layer only, not S-1)* sliders for dynasty count, spacing, etc. added when the dragon layer is specced

2. **Actions**
   - Regenerate button (triggers `GenerateWorld()` with current params)
   - Export button (triggers `ExportWorld()` with current world and file path)
   - Import button (triggers `ImportWorld()` with file path)
   - Cancel button (triggers `CancelGeneration()`, only enabled during generation)

3. **Status**
   - Progress text: "Idle" / "Generating..." / "Validating attempt N/M..." / "Complete (N attempts)" / "Failed: <reason>"
   - Progress bar (0-100%, maps to retry progress: attempt N of M = N/M * 100%)
   - Elapsed time: "3.2s" (updates live during generation)

4. **File Paths**
   - Export path: text input (default: `Cluiche/out/CoW/worlds/<seed>.wld.json`)
   - Import path: text input (default: empty, or last exported path)

5. **View** *(added 2026-10-15)*
   - Colour by: Biome / Terrain / Region / Elevation / Latitude / Water Source, plus the legend for that mode
   - Drawer toggles (Feature 7): Tiles, Grid Lines, Region Edges, Sites, Dynasty Starts, Guarantee Evidence, Selection

6. **Cell Inspector** *(added 2026-10-15)* — selected cell's fields, site, neighbours (Goal 7)

7. **Validation** *(added 2026-10-15)* — Validate button; per-guarantee rows; per-attempt rows (Goal 8)

**Mockup (ASCII, sections 1–4; sections 5–7 below):** *(amended 2026-10-19: climate offsets replace Biome Weights; Placement Params are dragon-layer, not shown in S-1)*

```
┌─ World Generation ──────────────────────────────────┐
│ Seed:       [ 12345678                 ] [Random]   │
│                                                      │
│ Terrain:                                             │
│   Land:         [======|----------] 0.42             │
│   Mountainous:  [=======|---------] 0.55             │
│   Rivers:       [======|-----------] 0.50            │
│   ... (19 sliders total)                             │
│                                                      │
│ Climate:                                             │
│   Temp Offset:  [-------|----------] 0.00            │
│   Moist Offset: [-------|----------] 0.00            │
│                                                      │
│ [Regenerate] [Export] [Import] [Cancel]             │
│                                                      │
│ Status: Validating attempt 2/10...                   │
│ Progress: [============================  ] 70%       │
│ Elapsed: 3.2s                                        │
│                                                      │
│ Export Path: [C:/GitHub/Cluiche/out/CoW/worlds/...] │
│ Import Path: [                                     ] │
└──────────────────────────────────────────────────────┘
```

```
┌─ View ───────────────────────────────────────────────┐
│ Colour by: (•)Biome ( )Terrain ( )Region             │
│            ( )Elevation ( )Latitude ( )Water         │
│ Legend: ■ Forest ■ Mountain ■ Desert ■ ...           │
│ [x]Tiles [ ]Grid [ ]Regions [x]Sites [ ]Dynasty      │
│ [ ]Evidence [x]Selection                             │
├─ Cell Inspector ─────────────────────────────────────┤
│ Cell (23, 17)  index 1145        [x: 23] [y: 17]     │
│ Biome Forest · Terrain Plain · Region Verdant Lowl.  │
│ Elevation 0.42 · Latitude 0.61 · Water None          │
│ Site: Hearth #3                                      │
│ Neighbours 0:(24,17) 1:(23,16) 2:(22,17)             │
│            3:(22,18) 4:(23,18) 5:(24,18)             │
├─ Validation ─────────────────── [Validate] ──────────┤
│ Success after 3 attempts · seed 12345678             │
│ ✓ HearthNexus   10 proof paths, 0 violations         │
│ ✓ DynastySpace  min 9 ≥ 8, 0 violations              │
│ ✓ WorldThrone   6/6 types, 6 proof paths             │
│ Attempts: #0 sub 12345678 ✗ HearthNexus   41 ms      │
│           #1 sub 12345679 ✗ WorldThrone   39 ms      │
│           #2 sub 12345680 ✓               44 ms      │
└──────────────────────────────────────────────────────┘
```

Column 23 is odd, so it sits half a cell down (odd-q). The inspector reads neighbours from `HexTopology::NeighbourAt`, never from its own maths.

### Event Wiring

**Regenerate button click:** *(amended 2026-10-19: uses DiaAPI commands per SD-S1-002, not direct bus events)*

```cpp
void OnRegenerateClicked() {
    // Read parameters from UI
    Json::Value args;
    args["seed"] = ReadSeedFromUI();
    // ... add params ...
    
    // Send command to WorldGenDebugDomain (SD-S1-002)
    // Domain queues WorldRegenerationRequested on SimPU
    SendCommand("WorldGen", "regenerate", args);
    
    // Update UI to "Generating..." state
    SetStatusText("Generating...");
    SetProgressBar(0.0f);
    mStartTime = std::chrono::steady_clock::now();
}
```

**Progress updates (subscribed to `WorldGenerationRetry` event from Feature 3):**

```cpp
void OnWorldGenerationRetry(const WorldGenerationRetry& event) {
    // Update progress bar: attempt N of M
    float progress = static_cast<float>(event.attemptNumber) / event.maxAttempts;
    SetProgressBar(progress);
    
    // Update status text
    SetStatusText("Validating attempt " + std::to_string(event.attemptNumber) + "/" + std::to_string(event.maxAttempts) + "...");
}
```

**Completion (subscribed to `WorldGenerationComplete` event from Feature 3):**

```cpp
void OnWorldGenerationComplete(const WorldGenerationComplete& event) {
    // The event is a notification (amended 2026-10-14); the world and report are read
    // from the module on SimPU when the domain builds its JSON state.
    SetStatusText("Complete (" + std::to_string(event.retryAttempts + 1) + " attempts)");
    SetProgressBar(1.0f);
    
    // Update export path default to include seed
    SetExportPathDefault("Cluiche/out/CoW/worlds/" + std::to_string(event.seed) + ".wld.json");
}
```

**Failure (subscribed to `WorldGenerationFailed` event from Feature 5):**

```cpp
void OnWorldGenerationFailed(const WorldGenerationFailed& event) {
    SetStatusText("Failed: " + event.message);
    SetProgressBar(0.0f);
    // The previous world stays current (Feature 5 Goal 5a); the Validation section shows the failed attempts.
}
```

**Export button click:**

```cpp
void OnExportClicked() {
    if (module.GetCurrentWorld() == nullptr) {
        ShowErrorMessage("No world to export (generate a world first)");
        return;
    }
    
    std::string filePath = ReadExportPathFromUI();
    PublishEvent(WorldExportRequested{filePath});
    
    // WorldGenerationModule handles export on SimPU, publishes WorldExportComplete
}
```

**Import button click:**

```cpp
void OnImportClicked() {
    std::string filePath = ReadImportPathFromUI();
    if (filePath.empty()) {
        ShowErrorMessage("Import path cannot be empty");
        return;
    }
    
    PublishEvent(WorldImportRequested{filePath});
    
    // WorldGenerationModule handles import on SimPU, publishes WorldImportComplete
}
```

**Cancel button click:**

```cpp
void OnCancelClicked() {
    PublishEvent(WorldGenerationCancelled{});
    SetStatusText("Cancelling...");
}
```

### Thread Safety

**UI thread (MainPU):** Reads user input, publishes events, subscribes to completion/failure events, updates UI.

**Generation thread (SimPU):** `WorldGenerationModule` subscribes to `WorldRegenerationRequested`, runs `GenerateWorld()`, publishes progress/completion/failure events.

**Synchronization:** Event bus handles cross-thread communication (MainPU → SimPU via request events, SimPU → MainPU via completion/failure events). No shared mutable state.

## Public Interfaces

### APIs

**None (this feature is UI-only, uses existing `WorldGenerationModule` APIs from Features 1-5).**

### Events

**Published by F-1 Debug Host UI (this feature):**

```cpp
// Triggered by Regenerate button
struct WorldRegenerationRequested {
    uint64_t seed;
    GenParams params;
};

// Triggered by Export button
struct WorldExportRequested {
    char filePath[260];   // Fixed buffer: bus messages must be trivially copyable
};

// Triggered by Import button
struct WorldImportRequested {
    char filePath[260];   // Fixed buffer: bus messages must be trivially copyable
};

// Triggered by Cancel button
struct WorldGenerationCancelled {
    // Empty payload (just a signal)
};
```

**Subscribed by F-1 Debug Host UI (from Features 3/4/5):**

```cpp
// Notifications only (amended 2026-10-15): the world is read via GetCurrentWorld(),
// the attempt detail via GetLastReport().
struct WorldGenerationComplete {
    uint64_t seed;
    uint32_t retryAttempts;
    uint32_t worldRevision;
};

struct WorldGenerationFailed {
    FailureReason reason;
    char message[128];    // Fixed buffer (truncated); distinguishing text per GDD decision 5
};

struct WorldGenerationRetry {  // From Feature 3 (Open Q #2 resolved 2026-10-15)
    uint32_t attemptNumber;
    uint32_t maxAttempts;
    uint32_t failedGuaranteeCount;
};

struct WorldExportComplete {  // Optional, from Feature 4
    char filePath[260];   // Fixed buffer: bus messages must be trivially copyable
};

struct WorldImportComplete {  // Optional, from Feature 4; world via GetCurrentWorld()
    char filePath[260];   // Fixed buffer: bus messages must be trivially copyable
    uint32_t worldRevision;
};
```

## Dependencies

### Upstream Dependencies

| System/Feature | Why needed | Status |
|----------------|-----------|---------|
| **F-1 Harness** | Debug host framework (panel registration, UI primitives like sliders/buttons/text inputs) | Approved (backlog) |
| **Feature 1 (Terrain Generator, @terrain-generator.md)** | Provides `GenParams` struct (`WorldShapeParams` incl. climate offsets, placement params) | Approved 2026-10-18 |
| **Feature 3 (Retry Mechanism)** | Provides `GenerateWorld()` API and `WorldGenerationComplete` event | Approved |
| **Feature 4 (JSON Export/Import)** | Provides `ExportWorld()` / `ImportWorld()` APIs | Approved |
| **Feature 5 (Timeout and Failure Handling)** | Provides `CancelGeneration()` API and `WorldGenerationFailed` event | Approved |
| **Phase 2 data design** | Biome enum names (for slider labels) | GDD in-progress (names TBD — placeholder uses arbitrary labels) |
| **Feature 2 (Reachability Validator)** | Evidence counts and violating cells for the Validation section; `Validate()` for the Validate button | Approved (amended 2026-10-15) |
| **Feature 3 (Retry Mechanism)** | `GetLastReport()` for the Validation section | Approved (amended 2026-10-15) |
| **Feature 7 (Layered Debug Rendering)** | `WorldGenDebugDomain` hosts this panel; selection and colour mode state | Approved (amended 2026-10-15) |
| **DiaDebugDomain** | DiaDebugPanel card from `GetJSONState` / `OnCommand` | Done |

### Dia Engine Dependencies

- **DiaMessageBus** (event pub/sub for cross-thread communication)
- **DiaVisualDebugger / DiaDebugPanel** (panel card, commands; replaces the assumed DiaUI panel framework, amended 2026-10-15)
- **DiaCore** (file path utilities, logging)

## Acceptance Criteria

### AC1 — Regenerate Workflow
Open S-1 stage in F-1 debug host. Adjust seed, click Regenerate. **Pass:** Status updates to "Generating...", then "Complete (N attempts)" after generation finishes. Repeat with different seed. **Pass:** New world generated (different biome distribution visible in Feature 7 rendering, if implemented).

### AC2 — Parameter Adjustment Workflow
Raise the `moistureOffset` slider (e.g., to 0.2), click Regenerate. **Pass:** Generated world has more moisture-loving biome cells (Forest, Rainforest, Wetland) and fewer Desert cells than before (observable via Feature 7 rendering or manual inspection of exported JSON).

### AC3 — Export Workflow
Generate world, enter export path `C:/test.wld.json`, click Export. **Pass:** File written to disk, status shows "Exported to C:/test.wld.json" (or similar feedback). Open file in text editor. **Pass:** Valid JSON matching Feature 4 schema.

### AC4 — Import Workflow
Export world to file, hand-edit JSON (change one Hearth position), enter import path, click Import. **Pass:** World reloaded with edited Hearth position (observable via Feature 7 rendering or manual inspection). Status shows "Imported from <path>".

### AC5 — Cancel Workflow
Click Regenerate, immediately click Cancel (before generation completes). **Pass:** Status updates to "Cancelling...", then "Failed: CANCEL". No world returned (export button disabled or shows error if clicked).

### Amendments (2026-10-19, F-3 World Assets)

- **Baked terrain output:** `cow.world.terrain.bake <id>` writes to `Assets/CoW/Global/WorldGen/Baked/<id>.wld.json` via the `source` alias (dia-needs #16), not `out/`.
- **World bundle reference:** SD-S1-010 gains `"world_bundle": "<id>"` as a stage world option (feature 6 panel shows it when loading a stage's world config).

### AC6 — Progress Updates
Click Regenerate with seed that triggers multiple retries (force via hand-edited impossible parameters). **Pass:** Progress bar and status text update live ("Validating attempt 2/10...", progress bar at 20%, elapsed time counting up). Proves live telemetry works.

### AC7 — Failure Feedback
Force timeout (Feature 5 AC1's artificial delay). **Pass:** Status shows "Failed: TIMEOUT" after timeout duration. Proves failure diagnostics displayed.

Force unsatisfiable seed (Feature 5 AC3's impossible parameters). **Pass:** Status shows "Failed: unsatisfiable seed" after retry exhaustion.

### AC8 — Default Export Path
Generate world with seed 12345. **Pass:** Export path text input auto-fills with `Cluiche/out/CoW/worlds/12345.wld.json` (or similar default per Feature 4 Open Q #1). Proves default path logic.

### AC9 — Random Seed Button
Click "Random" button next to seed input. **Pass:** Seed input updates to random value (e.g., system time hash or RNG draw). Click Regenerate. **Pass:** World generated with random seed.

### AC10 — UI Responsiveness During Generation
Click Regenerate (long-running generation, e.g., timeout test seed). While generation in progress, drag F-1 debug host window, click other panels. **Pass:** UI remains responsive (no freeze), proving generation runs on background thread. Cancel button clickable.

### AC11 — Cell Inspector *(added 2026-10-15)*
Click a cell in Feature 7, then type a different coordinate. **Pass:**
- Each time, the inspector shows that cell's index and all six fields, matching `GetCurrentWorld()->cells[y * width + x]`.
- A cell holding a site shows the site.
- The neighbours match `HexTopology::NeighbourAt` for all six directions, with "edge" for `kNoCell`.

Select a Hearth's cell, export, and read the JSON. **Pass:** the inspector's values equal the exported values for that cell.

### AC12 — Validation Section *(added 2026-10-15)*
Generate with a seed that needs ≥2 attempts. **Pass:**
- One row per attempt, with sub-seed and failed guarantee ids matching `GetLastReport()`.
- One row per guarantee, with evidence counts equal to the sizes of its `proofPaths` and `violatingCells` / `violatingPairs`.

Force an unsatisfiable seed. **Pass:** the outcome reads "Failed: unsatisfiable", all attempts are listed, and the previous world is still shown and inspectable.

### AC13 — Validate After Import *(added 2026-10-15)*
Export a passing world, hand-edit one Hearth onto an isolated cell, import, and click Validate. **Pass:**
- HearthNexus shows ✗ with one violation.
- Clicking the violation selects that cell, and the inspector shows it.
- Feature 7's evidence layer marks it.
- The world itself is unchanged (`GetWorldRevision()` same as after the import).

### AC14 — Small Map *(added 2026-10-15)*
**Pass:** the panel shows "Small 66 × 42", and a generated world has `dimensions == {66, 42}`.

## Inherited Binding Decisions

### From Platform (Cluiche)

| ID | Decision | How it applies |
|----|----------|----------------|
| PD-002 | ProcessingUnit/Phase/Module architecture | F-1 debug host UI runs on MainPU (UI thread); `WorldGenerationModule` runs on SimPU (generation thread); event bus handles cross-thread communication |
| PD-009 | All generated non-binary output lives under `Cluiche/out/<AppName>/` | Default export path is `Cluiche/out/CoW/worlds/` (user can override, but default must comply) |

### From Application (CoW)

| ID | Decision | How it applies |
|----|----------|----------------|
| AD-002 | Classify cross-cutting needs | The panel is part of `WorldGenDebugDomain`, which is **CoW-Shared** with `WorldGenerationModule` (amended 2026-10-15; S-2.5/S-3 load S-1 worlds and reuse the inspector). Not Dia (too CoW-specific: uses WLD events and CoW world structure), not Baseline |
| AD-003 | GDD owns design intent | GDD defines "in-game parameter tuning, not standalone editor" (decision 3), <5-second regeneration metric (§7), and fast iteration workflow (§4). This spec defines UI layout (panel sections, button actions, progress display) |
| AD-005 | One stage per testbed; modes chosen in-panel | S-1 is one stage; if S-1 later gains modes (e.g., "Single Cell" vs. "Living Ecology" per GDD testbed roster), they're chosen via a mode dropdown in this debug panel, not as separate stages |

### From System (WorldGenerationPipeline)

| ID | Decision | How it applies |
|----|----------|----------------|
| N/A | No system-level binding decisions in parent spec | System spec has Open Design Questions but no SD- binding decisions |

## Open Design Questions

| # | Question | Impact | Resolution |
|---|----------|--------|------------|
| 1 | What are the exact parameter inputs (biome types, placement param names/ranges)? | Affects UI layout (slider count, labels, default values); does not block implementation (use placeholder sliders, update labels when Phase 2 finalizes) | GDD open question 1 and Feature 1 Open Q #1 defer this; start with placeholder sliders (e.g., "Biome 1", "Biome 2"), update to real names (Forest, Mountain, etc.) when Phase 2 data design completes |
| 2 | Should progress updates be per-retry-attempt (coarse) or per-generation-phase (fine)? | Affects progress bar granularity (coarse: jumps 10% per attempt; fine: smooth 1% increments per generation phase); coarse is simpler, fine is smoother | Start with per-retry-attempt (reuses Feature 3's `WorldGenerationRetry` event if implemented); add finer updates if designers request smoother progress |
| 3 | Should UI auto-export on successful generation (save last world automatically)? | Convenience vs. clutter (auto-export fills `out/CoW/worlds/` with many files); manual export gives designers control | Manual export only (matches GDD decision 3's "in-game iteration" — designers export when they want to freeze a result, not every generation) |
| 4 | Should Cancel button be always-visible or only-during-generation? | Always-visible but disabled when idle is clearer (button grayed out); only-during-generation is cleaner but requires dynamic layout | Always-visible (enabled only during generation, disabled when idle); matches standard UI patterns |

## Status

`Approved` — 2026-10-11. Amended 2026-10-15: panel is the `WorldGenDebugDomain` card in DiaDebugPanel; adds View (colour mode, toggles), Cell Inspector, Validation section, Validate button and read-only Small map size (Goals 7–10, AC11–AC14); event structs match the notification shapes; failure keeps the previous world.

---

## Implementation Notes

When implementation begins (after approval), create a plan file `f1-debug-host-integration.plan.md` alongside this spec. The plan must include an **Implementation Patterns** section documenting:

- UI panel registration (how to add "World Generation" panel to F-1 debug host framework)
- Parameter input layout (slider ranges, default values, label generation from `GenParams` struct)
- Event subscription (which events to subscribe to, handler signatures, UI update patterns)
- Progress calculation (how to map retry attempts to progress bar percentage)
- File path input validation (check path exists for import, create directory for export if missing)
- Thread-safe UI updates (how to ensure UI updates from SimPU events happen on MainPU thread)

These patterns are validated against actual implementation before tasks are marked Done.

**Dependency note:** F-1 Harness spec (debug host framework) must define panel registration API and UI primitives (sliders, buttons, text inputs, progress bar) before this feature can be implemented. If F-1 spec incomplete, coordinate with F-1 implementers on API shape.
