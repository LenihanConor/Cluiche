# Feature Spec: Test Stage Visual Feedback

## Parent System
@docs/specs/applications/cluichetest/systems/teststages/teststages.md

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-004, PD-006, PD-007 |
| Application | @docs/specs/applications/cluichetest/cluichetest.md | AD-001, AD-004, AD-005 |
| System | @docs/specs/applications/cluichetest/systems/teststages/teststages.md | SD-TS-001, SD-TS-002, SD-TS-003, SD-TS-004, SD-TS-005 |
| Sibling | @docs/specs/applications/cluichetest/systems/teststages/test-stage-infrastructure.md | Depends on Boot menu (Tasks 4-5) and checkpoint system |

## Problem Statement

When running test stages manually (not via the pytest orchestrator), there's no visual indication of whether checkpoints are passing or what frame the stage is on. After returning to Boot, there's no record of which stages passed or failed. Developers must check logs or the orchestrator output. This feature adds: (a) a bottom-bar HUD overlay visible during any test stage showing live checkpoint status, and (b) inline pass/fail badges on the Boot menu from the last manual run.

## Acceptance Criteria

### A — In-Stage HUD (Bottom Bar)

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-VF1 | A bottom bar appears during any active test stage | Visual — bar visible when stage is running |
| AC-VF2 | Bar shows: stage name, checkpoint name(s) with pending/pass/fail icon, current frame count | Visual — all elements present |
| AC-VF3 | Pending checkpoints show hourglass icon, passed show checkmark, failed show X | Icons update in real-time as checkpoints resolve |
| AC-VF4 | Frame count shows `current/budget` (budget from stage metadata) | Frame counter increments each frame |
| AC-VF5 | When all checkpoints pass, bar shows "PASS" label with green highlight | Visual — state change on completion |
| AC-VF6 | If frame budget exceeded without all checkpoints passing, bar shows "TIMEOUT" with red highlight | Visual — timeout state |
| AC-VF7 | HUD does not appear when running under orchestrator automation (no visual noise in CI) | Check: HUD disabled when AutomationModule detects orchestrator connection |
| AC-VF8 | HUD renders via debug draw / ImGui (not HTML/Ultralight) | Code review — no web UI dependency |
| AC-VF14 | A red ✕ button on the right of the HUD bar navigates back to Boot when clicked | Click button → stage exits → Boot menu appears |
| AC-VF15 | The ✕ button is always visible regardless of pass/fail/timeout state — exit is always user-triggered, never automatic | Button present in all HUD states |
| AC-VF16 | When running under orchestrator, the ✕ button is hidden (AC-VF7 applies equally) | No exit button in CI |

### B — Boot Menu Badges

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-VF9 | Each stage entry in Boot menu shows a badge: checkmark (passed), X (failed), dash (not yet run) | Visual — badges appear next to stage names |
| AC-VF10 | Badge state persists across stage transitions within a session | Run stage → return to Boot → badge reflects result |
| AC-VF11 | Summary line at bottom: "Last run: N/M passed" | Visual — count visible |
| AC-VF12 | Badge state resets on application restart (session-scoped, not persisted to disk) | Restart app → all badges show dash |
| AC-VF13 | Clicking a stage with a badge still navigates to it (badge is informational, not blocking) | Navigation still works regardless of badge state |

## Design

### Architecture

```
┌─────────────────────────────────────────────────┐
│ TestResultsRegistry (session-scoped singleton)   │
│  - stores per-stage: pass/fail/not-run + frames │
│  - queried by HUD module and Boot menu          │
└────────────┬─────────────────────┬──────────────┘
             │                     │
   ┌─────────▼─────────┐   ┌──────▼──────────────┐
   │ TestStageHUDModule │   │ BootUIPageModule     │
   │ (renders bottom    │   │ (queries registry,   │
   │  bar via ImGui)    │   │  exposes badges to   │
   └────────────────────┘   │  JS via binding)     │
                            └─────────────────────┘
```

### TestResultsRegistry

A session-scoped singleton that test stage modules write to and the HUD/Boot menu read from.

```cpp
namespace CluicheTest {

struct StageResult {
    enum class State { kNotRun, kRunning, kPassed, kFailed, kTimeout };
    State state = State::kNotRun;
    unsigned int settleFrame = 0;
    unsigned int budgetFrames = 0;
};

class TestResultsRegistry : public Dia::Core::Singleton<TestResultsRegistry>
{
public:
    void SetRunning(const Dia::Core::StringCRC& stageName, unsigned int budgetFrames);
    void SetPassed(const Dia::Core::StringCRC& stageName, unsigned int frame);
    void SetFailed(const Dia::Core::StringCRC& stageName, unsigned int frame);
    void SetTimeout(const Dia::Core::StringCRC& stageName);

    const StageResult* GetResult(const Dia::Core::StringCRC& stageName) const;
    void GetAllResults(Dia::Core::Containers::DynamicArrayC<StageResultEntry, 16>& out) const;

    // Live state for HUD
    const Dia::Core::StringCRC& GetActiveStage() const;
    unsigned int GetActiveFrameCount() const;
};

} // namespace CluicheTest
```

### TestStageHUDModule

A module that lives on MainPU (for rendering access) alongside every test stage. It reads checkpoint state from AutomationService and frame data from TestResultsRegistry each frame, then renders the bottom bar.

```cpp
class TestStageHUDModule : public Dia::ApplicationFlow::Module
{
public:
    static constexpr Dia::Core::StringCRC kTypeId{"TestStageHUDModule"};

protected:
    StartResult DoStart() override;
    void DoUpdate(float deltaTime) override;
    StopResult DoStop() override;

private:
    void RenderBottomBar();
    bool IsOrchestratorConnected() const;

    Dia::ApplicationFlow::ModuleRef<AutomationModule> mAutomation{this};
};
```

**Key behaviours:**
- `DoStart`: Check if orchestrator is connected (heartbeat active). If yes, skip all rendering (AC-VF7).
- `DoUpdate`: Query registered checkpoints from AutomationService, query frame count from registry, render via ImGui/debug draw.
- Renders at screen-space bottom, full width, ~30px height.

### Bottom Bar Layout

```
┌──────────────────────────────────────────────────────────────────┐
│ StageName │ ⏳ checkpoint_1 │ ✓ checkpoint_2 │ frame: N/M │ [✕] │
└──────────────────────────────────────────────────────────────────┘
```

- Stage name: left-aligned, bold
- Checkpoints: listed left-to-right, icon + name
  - ⏳ (yellow) = pending
  - ✓ (green) = passed
  - ✗ (red) = failed
- Frame counter: right-aligned, `current/budget`
- **✕ button**: far right, red, always visible; executes `dia.automation.navigate_to Boot`
- Background: dark semi-transparent
- On completion: entire bar flashes green ("PASS") or red ("TIMEOUT") for 2 seconds; ✕ button remains clickable throughout

### Boot Menu Integration

BootUIPageModule already queries `dia.manifest.stages` for the stage list. For badges, it additionally queries TestResultsRegistry and passes result state to JS:

```cpp
// In BootUIPageModule::DoStart, after stage list:
auto& registry = TestResultsRegistry::GetInstance();
for (each stage)
{
    auto* result = registry.GetResult(stageName);
    // Expose to JS: { name: "RigidBody2D", badge: "pass" | "fail" | "none" }
}
```

The HTML renders badges inline:
- ✓ green circle = passed
- ✗ red circle = failed  
- — gray = not yet run
- Summary line: "Last run: N/M passed"

### Stage Module Integration

Each test stage module calls the registry at key moments:

```cpp
StartResult RigidBody2DTestModule::DoStart()
{
    TestResultsRegistry::GetInstance().SetRunning(StringCRC("RigidBody2DStage"), 200);
    // ... existing setup ...
}

void RigidBody2DTestModule::DoUpdate(float deltaTime)
{
    // ... existing logic ...
    if (mSettled)
    {
        TestResultsRegistry::GetInstance().SetPassed(StringCRC("RigidBody2DStage"), mFrameCount);
    }
}
```

Timeout detection lives in the HUD module (it watches frame count vs budget and calls `SetTimeout` if exceeded).

### Manifest Entry for HUD Module

The HUD module is added to every test stage's `.diaapp` manifest (not the stage module itself — it's a separate module on MainPU):

```json
{
    "modules": [
        { "type": "RigidBody2DTestModule", "instance": "rigidbody2d_stage" },
        { "type": "TestStageHUDModule", "instance": "test_hud" }
    ]
}
```

### Orchestrator Suppression

When the pytest orchestrator connects, it enables the heartbeat monitor. The HUD module checks `AutomationService::IsHeartbeatActive()` — if true, orchestrator is driving the app, and the HUD stays hidden. This keeps CI clean.

## Files Touched

| File | Change |
|------|--------|
| `Cluiche/CluicheTest/Modules/TestStages/TestResultsRegistry.h` | New — singleton for session-scoped results |
| `Cluiche/CluicheTest/Modules/TestStages/TestResultsRegistry.cpp` | New — implementation |
| `Cluiche/CluicheTest/Modules/TestStages/TestStageHUDModule.h` | New — HUD rendering module |
| `Cluiche/CluicheTest/Modules/TestStages/TestStageHUDModule.cpp` | New — bottom bar ImGui rendering |
| `Cluiche/CluicheTest/Modules/BootUIPageModule.cpp` | Query TestResultsRegistry, expose badges to JS |
| Boot UI HTML (deployed asset) | Render badge icons next to stage names + summary line |
| Each test stage `.diaapp` manifest | Add `TestStageHUDModule` instance |
| Each test stage module `.cpp` | Add `SetRunning`/`SetPassed` calls to registry |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` | Add new files |
| `Cluiche/CluicheTest/CluicheTest.vcxproj.filters` | Add filter entries |

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create TestResultsRegistry singleton (.h/.cpp) | Compiles, SetRunning/SetPassed/GetResult work | Todo | sonnet | Session-scoped, no disk persistence |
| 2 | Create TestStageHUDModule (.h/.cpp) | Compiles, module registered | Todo | sonnet | ImGui bottom bar rendering |
| 3 | Implement bottom bar rendering (stage name + checkpoints + frame counter) | Bar visible during stage, checkpoints update live | Todo | sonnet | ImGui overlay at screen bottom |
| 4 | Implement completion states (PASS green flash / TIMEOUT red) | Visual state change on completion | Todo | sonnet | 2s highlight then static |
| 5 | Implement orchestrator suppression | HUD hidden when heartbeat active | Todo | haiku | Check AutomationService::IsHeartbeatActive() |
| 6 | Add registry calls to each test stage module (SetRunning/SetPassed) | Registry populated during stage run | Todo | sonnet | Touch all 5 stage modules |
| 7 | Implement timeout detection in HUD module | SetTimeout called when frame > budget | Todo | haiku | HUD owns the timeout logic |
| 8 | Update BootUIPageModule to query registry + expose badges to JS | Badge data available in JS | Todo | sonnet | Bridge pattern same as stage list |
| 9 | Update Boot UI HTML to render badges + summary line | Badges visible next to stage names | Todo | sonnet | ✓/✗/— icons + "N/M passed" |
| 10 | Add TestStageHUDModule to each test stage .diaapp manifest | HUD appears in every test stage | Todo | haiku | One module entry per stage manifest |
| 11 | Add vcxproj + filters entries | Builds in VS | Todo | haiku | |
| 12 | Add ✕ exit button to HUD bar — executes `dia.automation.navigate_to Boot` on click; hidden under orchestrator | Button navigates to Boot, absent in CI | Todo | haiku | AC-VF14, AC-VF15, AC-VF16 |
| 13 | Add DIA_LOG_* calls to TestResultsRegistry (SetPassed, SetTimeout) and HUD button handler | Logs visible in trace viewer on state change | Todo | haiku | Observability |
| 14 | Add DIA_TRACE_ZONE to TestStageHUDModule::DoUpdate | HUD render cost visible in profiler | Todo | haiku | Observability |
| 15 | Write GoogleTests T1–T9 for TestResultsRegistry + TestStageHUDModule | All tests pass via `dia run googletest` | Todo | sonnet | Tests live in CluicheTest/Modules/TestStages/Testing/ |
| 16 | Verify: manual run RigidBody2D → see HUD → click ✕ → Boot → see badge | End-to-end visual confirmation including exit button | Todo | sonnet | |

## Tests

### TestResultsRegistry (unit tests — GoogleTest)

| # | Test | What it verifies |
|---|------|-----------------|
| T1 | `SetRunning` stores state kRunning + budget | State readable after write |
| T2 | `SetPassed` transitions kRunning → kPassed, stores frame | Correct state + frame value |
| T3 | `SetTimeout` transitions kRunning → kTimeout | Correct state |
| T4 | `GetResult` returns nullptr for unknown stage | No false hits |
| T5 | Multiple stages stored independently | No cross-stage contamination |
| T6 | Thread safety: concurrent SetPassed + GetResult on two threads | No data race (run under TSan or via two std::threads) |

### TestStageHUDModule (unit tests — GoogleTest)

| # | Test | What it verifies |
|---|------|-----------------|
| T7 | HUD renders nothing when `IsHeartbeatActive()` returns true | Orchestrator suppression (AC-VF7, AC-VF16) |
| T8 | HUD renders bar when `IsHeartbeatActive()` returns false | Normal display path |
| T9 | ✕ button click fires `dia.automation.navigate_to Boot` command | Exit command dispatched (AC-VF14) |

Test helpers live in `Cluiche/CluicheTest/Modules/TestStages/Testing/` per project convention.

## Observability

| Pillar | Signal | Where |
|--------|--------|-------|
| **Logs** | `DIA_LOG_INFO` when stage transitions to kPassed or kTimeout | `TestResultsRegistry::SetPassed`, `::SetTimeout` |
| **Logs** | `DIA_LOG_INFO` when user clicks ✕ exit button (stage name + final state) | `TestStageHUDModule` button handler |
| **Traces** | `DIA_TRACE_ZONE` on `TestStageHUDModule::DoUpdate` | Per-frame HUD render cost visible in trace viewer |
| **Profiling** | None — HUD render is trivial ImGui; no hot loop | — |
| **Metrics** | None — `TestResultsRegistry` IS the metric store; downstream consumers emit from it | — |
| **Health** | None — HUD is a passive display; no failure mode with observable health state | — |

## Dependencies

- **Infrastructure spec Tasks 1-6** must be complete (Boot menu, manifest loader)
- **At least one test stage module** must exist to test against
- **ImGui or debug draw system** must be available for bottom bar rendering. If ImGui isn't integrated yet, this becomes a blocker — fall back to DiaGraphics debug text rendering.

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | Registry keyed by StringCRC stage names. HUD module has StringCRC kTypeId. |
| PD-004 | No STL in public APIs | Registry uses DynamicArrayC for results storage. No STL in public interface. |
| PD-006 | VS project files source of truth | Task 11 adds files to CluicheTest.vcxproj. |
| PD-007 | C++20 required | Standard features. |
| AD-001 (CT) | Three PUs | HUD module on MainPU (rendering access). Registry is thread-safe singleton (read from MainPU, written from SimPU stage modules). |
| AD-004 (CT) | Test levels included | Visual feedback supports the test level workflow. |
| AD-005 (CT) | App is testbed not product | HUD is developer tooling, not gameplay UI. |
| SD-TS-001 | One manifest stage per feature | Not a stage itself — cross-cutting module added to existing stages. |
| SD-TS-002 | Checkpoints in DoStart, auto-clear on stop | HUD reads checkpoints registered by stage modules (doesn't register its own). |
| SD-TS-003 | Metrics for threshold assertions | Registry stores frame counts which could be emitted as metrics. HUD itself doesn't emit metrics. |
| SD-TS-004 | All stages return to Boot | HUD module stops when stage stops, registry persists for Boot to read. |
| SD-TS-005 | Individual stages are feature specs | This is infrastructure, not a stage — correct placement as a cross-cutting feature. |

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Thread Safety | Stage modules run on SimPU but HUD reads on MainPU. Is the registry thread-safe? | Yes — writes are infrequent (SetRunning once, SetPassed once per stage). Use a mutex or atomic state enum. The HUD reads once per frame — minimal contention. No lock-free complexity needed. |
| 2 | ImGui Availability | Is ImGui integrated into CluicheTest today? | If not, this is a dependency. Fallback: use DiaGraphics debug text rendering (DrawText at screen coords). Less pretty but functional. The spec prefers ImGui but doesn't hard-require it. |
| 3 | HUD Module Placement | HUD is on MainPU but stage module may be on SimPU. Can HUD read stage's checkpoints? | Yes — AutomationService is accessible cross-PU (it's a singleton service, not PU-bound). HUD queries checkpoint state via AutomationService API, same as the orchestrator does. |
| 4 | Multiple Checkpoints | Stages with 2 checkpoints (SM+Anim, Animation2D) — does the bottom bar get too wide? | For 2-3 checkpoints it's fine. If a future stage has 5+, truncate with "..." and show count. Current max is 2 checkpoints per stage — no issue. |
| 5 | Budget Source | Where does the HUD get the frame budget for a stage? | From `TestResultsRegistry::SetRunning(name, budget)` — each stage passes its budget at DoStart. The HUD reads it from the registry's active stage data. |
| 6 | Singleton Lifetime | TestResultsRegistry is a singleton — when is it created/destroyed? | Created on first access (lazy init). Lives for the entire application session. Destroyed at app shutdown. Session-scoped by nature — no persistence needed. |
| 7 | Boot Menu Timing | BootUIPageModule queries the registry at DoStart. What if the user returns to Boot before the stage fully completes? | The stage's DoStop fires before Boot's DoStart. If DoStop doesn't call SetPassed (because checkpoints didn't pass), the state remains kRunning → HUD module's timeout detection would have called SetTimeout, or state stays kRunning which Boot shows as "—" (incomplete). Edge case is acceptable. |
| 8 | HUD stub / thread concern | TestStageHUDModule::DoUpdate() is currently an empty stub with a comment claiming ImGui is unsafe cross-thread. Is this concern valid? | Not valid — DebugUIModule already calls Dia::ImGui::NewFrame() on MainPU and BootMenuModule renders ImGui on MainPU without issue. The HUD runs on MainPU and should follow the same pattern. The stub just needs implementing. |
| 9 | Exit button command | `dia.automation.navigate_to` requires a target argument. Is this command available and callable from ImGui button code on MainPU? | Yes — Dia::API::ExecuteCommandJson is a free function callable from any thread. BootMenuModule already uses it. Pattern: if (ImGui::Button("X")) { Dia::API::ExecuteCommandJson("dia.automation.navigate_to", {{"target","Boot"}}); } |

## Status

`In Progress` — plan at @docs/specs/applications/cluichetest/systems/teststages/visual-feedback.plan.md
