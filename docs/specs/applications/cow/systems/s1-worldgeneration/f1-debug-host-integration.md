# Feature Spec: F-1 Debug Host Integration

**Parent:** @docs/specs/applications/cow/systems/s1-worldgeneration/worldgenerationpipeline.md

**Gameplay Domains:** simulation

## Summary

F-1 Debug Host Integration exposes WLD's generation controls and live telemetry through F-1's in-game debug host UI, enabling CoW's testbed iteration workflow: designers adjust seed/parameters, click Regenerate, see results within seconds, export/import authored worlds, and monitor generation progress live. This feature wires `WorldGenerationModule` APIs to F-1's debug UI (seed input, biome weight sliders, placement param sliders, Regenerate/Export/Import/Cancel buttons, progress bar, retry telemetry display).

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

1. **Parameter inputs exposed** — Seed (integer input), biome distribution weights (sliders per biome type), placement params (Nexus count, dynasty spacing, etc. as sliders)
2. **Action buttons** — Regenerate (triggers `GenerateWorld()`), Export (triggers `ExportWorld()`), Import (triggers `ImportWorld()`), Cancel (triggers `CancelGeneration()`)
3. **Live progress display** — Progress bar or text showing current attempt number, elapsed time, validation status ("Generating...", "Validating attempt 2/10...", "Complete" / "Failed")
4. **Retry telemetry** — Display final retry count on success, or failure diagnostics (timeout/unsatisfiable/error/cancel) on failure
5. **File path inputs** — Export/Import file path text inputs with default values (`Cluiche/out/CoW/worlds/<seed>.wld.json`)
6. **Non-blocking UI** — Generation runs on background thread (SimPU); UI remains responsive during generation (Cancel button clickable, progress updates live)

## Non-Goals

1. **Visual rendering** — Feature 7 (Layered Debug Rendering) handles world visualization; this feature handles controls only
2. **Real-time parameter preview** — Sliders update parameters but don't auto-regenerate (designer must click Regenerate; auto-regen would be too slow)
3. **Parameter validation** — UI accepts any integer/float input; Feature 1-2 handle validation (e.g., negative seed, impossible placement params)
4. **Undo/redo** — Each Regenerate produces new world, discarding previous; no history stack (future enhancement if needed)
5. **Save/Load UI** — This feature handles world export/import only; E1 (save/load) handles live session persistence (separate UI)

## Architecture

### UI Layout

**Panel name:** "World Generation" (dockable panel in F-1 debug host, same pattern as event log / inspector panels)

**Sections:**

1. **Parameters**
   - Seed: integer text input (default: random)
   - Biome Weights: one slider per Phase 2 biome type (labels TBD, defaults from `GenParams`)
   - Placement Params: sliders for Nexus count, dynasty spacing, etc. (exact params TBD per Feature 1 Open Q #1)

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

**Mockup (ASCII):**

```
┌─ World Generation ──────────────────────────────────┐
│ Seed:       [ 12345678                 ] [Random]   │
│                                                      │
│ Biome Weights:                                       │
│   Forest:   [====|----------------] 0.40             │
│   Mountain: [========|------------] 0.30             │
│   ...                                                │
│                                                      │
│ Placement Params:                                    │
│   Nexus Count:      [=====|-----------] 8            │
│   Dynasty Spacing:  [======|----------] 15.0         │
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

### Event Wiring

**Regenerate button click:**

```cpp
void OnRegenerateClicked() {
    // Read parameters from UI
    GenParams params = ReadParamsFromUI();
    uint64_t seed = ReadSeedFromUI();
    
    // Publish event (handled by WorldGenerationModule on SimPU)
    PublishEvent(WorldRegenerationRequested{seed, params});
    
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
    float progress = static_cast<float>(event.attemptNumber) / mMaxRetryAttempts;
    SetProgressBar(progress);
    
    // Update status text
    SetStatusText("Validating attempt " + std::to_string(event.attemptNumber) + "/" + std::to_string(mMaxRetryAttempts) + "...");
}
```

**Completion (subscribed to `WorldGenerationComplete` event from Feature 3):**

```cpp
void OnWorldGenerationComplete(const WorldGenerationComplete& event) {
    mCurrentWorld = event.world;
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
    mCurrentWorld = nullptr;  // Clear partial state
}
```

**Export button click:**

```cpp
void OnExportClicked() {
    if (!mCurrentWorld) {
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
    std::string filePath;
};

// Triggered by Import button
struct WorldImportRequested {
    std::string filePath;
};

// Triggered by Cancel button
struct WorldGenerationCancelled {
    // Empty payload (just a signal)
};
```

**Subscribed by F-1 Debug Host UI (from Features 3/4/5):**

```cpp
struct WorldGenerationComplete {
    WorldStructure world;
    uint64_t seed;
    uint32_t retryAttempts;
};

struct WorldGenerationFailed {
    FailureReason reason;
    std::string message;
};

struct WorldGenerationRetry {  // Optional, from Feature 3 Open Q #2
    uint32_t attemptNumber;
    ValidationResult lastValidation;
};

struct WorldExportComplete {  // Optional, from Feature 4
    std::string filePath;
};

struct WorldImportComplete {  // Optional, from Feature 4
    WorldStructure world;
    std::string filePath;
};
```

## Dependencies

### Upstream Dependencies

| System/Feature | Why needed | Status |
|----------------|-----------|---------|
| **F-1 Harness** | Debug host framework (panel registration, UI primitives like sliders/buttons/text inputs) | Approved (backlog) |
| **Feature 1 (Placeholder Generation Algorithm)** | Provides `GenParams` struct (biome weights, placement params) | Draft (being specced) |
| **Feature 3 (Retry Mechanism)** | Provides `GenerateWorld()` API and `WorldGenerationComplete` event | Draft (being specced) |
| **Feature 4 (JSON Export/Import)** | Provides `ExportWorld()` / `ImportWorld()` APIs | Draft (being specced) |
| **Feature 5 (Timeout and Failure Handling)** | Provides `CancelGeneration()` API and `WorldGenerationFailed` event | Draft (being specced) |
| **Phase 2 data design** | Biome enum names (for slider labels) | GDD in-progress (names TBD — placeholder uses arbitrary labels) |

### Dia Engine Dependencies

- **DiaMessageBus** (event pub/sub for cross-thread communication)
- **DiaUI** (sliders, buttons, text inputs, progress bar, panel framework — assumes F-1 has these primitives)
- **DiaCore** (file path utilities, logging)

## Acceptance Criteria

### AC1 — Regenerate Workflow
Open S-1 stage in F-1 debug host. Adjust seed, click Regenerate. **Pass:** Status updates to "Generating...", then "Complete (N attempts)" after generation finishes. Repeat with different seed. **Pass:** New world generated (different biome distribution visible in Feature 7 rendering, if implemented).

### AC2 — Parameter Adjustment Workflow
Adjust biome weight slider (e.g., increase Forest weight to 0.8), click Regenerate. **Pass:** Generated world has more Forest biome cells than before (observable via Feature 7 rendering or manual inspection of exported JSON).

### AC3 — Export Workflow
Generate world, enter export path `C:/test.wld.json`, click Export. **Pass:** File written to disk, status shows "Exported to C:/test.wld.json" (or similar feedback). Open file in text editor. **Pass:** Valid JSON matching Feature 4 schema.

### AC4 — Import Workflow
Export world to file, hand-edit JSON (change one Hearth position), enter import path, click Import. **Pass:** World reloaded with edited Hearth position (observable via Feature 7 rendering or manual inspection). Status shows "Imported from <path>".

### AC5 — Cancel Workflow
Click Regenerate, immediately click Cancel (before generation completes). **Pass:** Status updates to "Cancelling...", then "Failed: CANCEL". No world returned (export button disabled or shows error if clicked).

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

## Inherited Binding Decisions

### From Platform (Cluiche)

| ID | Decision | How it applies |
|----|----------|----------------|
| PD-002 | ProcessingUnit/Phase/Module architecture | F-1 debug host UI runs on MainPU (UI thread); `WorldGenerationModule` runs on SimPU (generation thread); event bus handles cross-thread communication |
| PD-009 | All generated non-binary output lives under `Cluiche/out/<AppName>/` | Default export path is `Cluiche/out/CoW/worlds/` (user can override, but default must comply) |

### From Application (CoW)

| ID | Decision | How it applies |
|----|----------|----------------|
| AD-002 | Classify cross-cutting needs | F-1 Debug Host Integration is **Feature-local** (lives in S-1 stage's UI code); if other CoW testbeds need similar debug panels, extract common patterns to **CoW-Shared** UI utilities. Not Dia (too CoW-specific: uses WLD events and CoW world structure), not Baseline |
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

`Approved` — 2026-10-11

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
