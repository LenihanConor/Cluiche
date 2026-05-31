# Feature Spec: UIUltralightTestStage

## Parent System
@docs/specs/systems/cluichetest/teststages.md

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-004, PD-006, PD-007, PD-009, PD-010 |
| Application | @docs/specs/applications/cluichetest.md | AD-001, AD-003, AD-005 |
| System | @docs/specs/systems/cluichetest/teststages.md | SD-TS-001, SD-TS-002, SD-TS-003, SD-TS-004, SD-TS-005 |
| Sibling | @docs/specs/features/cluichetest/teststages/test-stage-infrastructure.md | Inherits Shared ACs 1–9; depends on Boot menu + manifest loader |

---

## Problem Statement

The UIUltralight system has no E2E coverage — there is no automated way to verify that page loading, JavaScript binding, pixel buffer output, mouse input injection, and the JS↔C++ round-trip all work correctly under real app timing.

---

## Template Answers

| # | Question | Answer |
|---|----------|--------|
| T1 | Engine feature under test | `DiaUIUltralight` — `UISystem::Initialize`, `LoadPage`, `FetchUIDataBuffer`, `InjectMouseClick`, JS→C++ `BoundMethod` callbacks, C++→JS `GetTestValue` return value |
| T2 | Scene layout | No world-space geometry. Viewport shows: (1) the Ultralight pixel buffer overlay (Alpine.js panel composited over the game scene), (2) ImGui debug console with UIUltralight domain tab, (3) checkpoint panel. See mockup. |
| T3 | Acceptance criteria | Shared ACs 1–9 + stage-specific ACs AC-U1 through AC-U9 (see below) |
| T4 | Metrics | `cluichetest.ui.frames_until_loaded` (frames from DoStart to IsPageLoaded), `cluichetest.ui.round_trip_count` (number of successful round-trips) |
| T5 | PU assignment | `UIUltralightTestStageModule` on MainPU (UIModule lives on MainPU; test module depends on it) |
| T6 | Assets | `ui_ultralight_test_stage.diastage` + `ui_ultralight_test_stage.diaapp` manifests; `ui_ultralight_test.html` Alpine.js panel |
| T7 | Unit test gap | UIUltralight has no unit tests that exercise the full JS↔C++ bridge under real Ultralight SDK; all existing coverage is at the C++ interface level only |
| T8 | Determinism | Deterministic — no simulation, no randomness; page load frame count may vary by ±1 frame but checkpoints are convergence-based not frame-exact |
| T9 | Frame budget | 300 frames (10s at 30Hz); page expected to load within 10 frames |
| T10 | UI framework | Alpine.js single-file panel (no build step). React is out of scope for this stage — testing React+Ultralight coupling is a separate feature spec. |

---

## Acceptance Criteria

### Shared ACs (inherited from test-stage-infrastructure.md)

| # | Criterion | Manual Verification | Automated Verification |
|---|-----------|--------------------|-----------------------|
| AC-S1 | Stage has a manifest entry with `transitions: ["Boot"]` | Manifest review | — |
| AC-S2 | Stage module registers at least one checkpoint in DoStart via AutomationService | Code review | Checkpoint query returns success |
| AC-S3 | Stage module emits at least one metric to MetricRegistry | ImGui console shows metric value | Metric query from orchestrator returns value |
| AC-S4 | Stage module logs at DoStart entry, DoStop entry (`DIA_LOG_INFO`) | Session log review | Log review in pytest teardown |
| AC-S5 | Stage module returns `StartResult::kReady` only after scene setup is complete | Code review | — |
| AC-S6 | Stage module cleans up all resources in DoStop, returns `StopResult::kDone` | Code review | No ERROR logs after stop |
| AC-S7 | Navigating Boot → Stage → Boot → Stage produces identical checkpoint results | HUD badge shows PASS both times | Orchestrator runs stage twice; checkpoint results match |
| AC-S8 | Checkpoint names follow convention: `<feature>.<checkpoint_name>` | Code review | — |
| AC-S9 | Stage has a matching pytest scenario file | File exists | CI run |

### Stage-Specific ACs

| # | Criterion | Manual Verification | Automated Verification |
|---|-----------|--------------------|-----------------------|
| AC-U1 | `UISystem::LoadPage` completes without error; `IsPageLoaded()` returns true within budget | ImGui console: IsPageLoaded = true; no ERROR log entries | `ui.page_loaded` checkpoint passes within 10-frame timeout |
| AC-U2 | JS `window.onload` calls `app.OnPageReady()`; C++ callback fires | ImGui console: OnPageReady = fired | `ui.js_to_cpp_callback_fired` checkpoint passes |
| AC-U3 | Clicking the HTML button calls `app.OnButtonClicked()`; C++ callback fires | Alpine panel shows button in clicked state; ImGui console: OnButtonClicked = fired | `ui.js_to_cpp_callback_fired` checkpoint also covers this (same flag) |
| AC-U4 | C++ exposes `GetTestValue()` via BoundMethod returning `"dia_test_value_42"`; JS calls it and echoes back via `app.ReportReceivedValue(val)`; C++ verifies the value matches | ImGui console: round-trip match = ✓ | `ui.round_trip_value_correct` checkpoint passes |
| AC-U5 | After page loads, `FetchUIDataBuffer` returns a buffer whose size > 0 and contains at least one non-zero byte | Pixel buffer indicator shows non-empty | `ui.pixel_buffer_non_empty` checkpoint passes |
| AC-U6 | `InjectMouseClick(kLeft, x, y)` at the button's screen coordinates triggers `OnButtonClicked` in C++ | ImGui console: InjectMouseClick = handled | `ui.mouse_click_handled` checkpoint passes |
| AC-U7 | Navigating Boot → UIUltralightTestStage → Boot → UIUltralightTestStage produces identical `frames_until_loaded` metric (±1 frame tolerance) | HUD badge shows PASS on both runs | `ui.deterministic_reload` checkpoint passes; orchestrator asserts metric values within tolerance |
| AC-U8 | `cluichetest.ui.frames_until_loaded` metric emitted; value ≤ 10 | ImGui Metrics section shows value | pytest `assert_metric("cluichetest.ui.frames_until_loaded", "<=", 10)` |
| AC-U9 | No ERROR-level log entries emitted during the full scenario | Session log clean | pytest implicit log-error assertion (fixture teardown) |

---

## Design

**Visual mockup:** [@docs/specs/features/cluichetest/teststages/ui-ultralight-stage.mockup.html](ui-ultralight-stage.mockup.html) — open in a browser for visual acceptance gate reference.

### Viewport Layout

```
+----------------------------------------------+
|  [ImGui Debug Console - top-left, ~310px]    |
|  Title: "Debug Console"                      |
|  Domain tabs: UIUltralight* | RigidBody2D |..|
|    ▼ Bridge Status                           |
|      IsPageLoaded: true                      |
|      OnPageReady:  fired                     |
|      OnButtonClicked: fired (3×)             |
|      GetTestValue calls: 1                   |
|      ReportReceivedValue: match              |
|      InjectMouseClick: handled               |
|    ▼ Pixel Buffer                            |
|      Width: 1280  Height: 720  BGRA32        |
|      Size: 3.5 MB  Non-zero: true            |
|    ▼ Metrics                                 |
|      frames_until_loaded: 4                  |
|      round_trip_count: 1                     |
|    ▼ Determinism                             |
|      Run 1 loaded: 4  Run 2 loaded: 4  ✓    |
|  > [command input]                           |
|  Output | Warnings                           |
|                                              |
|  [Checkpoint panel - right of console]       |
|  ✓ ui.page_loaded             PASS           |
|  ✓ ui.js_to_cpp_callback_fired PASS          |
|  ✓ ui.round_trip_value_correct PASS          |
|  ✓ ui.pixel_buffer_non_empty  PASS           |
|  ⏳ ui.mouse_click_handled    PENDING         |
|  ⏳ ui.deterministic_reload   PENDING         |
|                                              |
|  [Ultralight pixel buffer overlay - right]   |
|  ┌─────────────────────────────────────┐    |
|  │ UIUltralight Test Panel (Alpine)    │    |
|  │ ● page loaded — OnPageReady fired   │    |
|  │ [Click me ✓ OnButtonClicked fired]  │    |
|  │ Round-trip:                         │    |
|  │   C++ sent: "dia_test_value_42"     │    |
|  │   JS echoed:"dia_test_value_42" ✓   │    |
|  │ [mouse click target zone ↖]         │    |
|  └─────────────────────────────────────┘    |
|                                              |
|  HUD: ui_ultralight_stage | ✓✓✓✓⏳⏳ | 018  RUNNING  ✕ |
+----------------------------------------------+
```

### Module Structure

Two C++ files, both in `Cluiche/CluicheTest/Modules/TestStages/`:

**UIUltralightTestStageModule** (test stage module, MainPU):
```cpp
class UIUltralightTestStageModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;  // "UIUltralightTestStageModule"
    explicit UIUltralightTestStageModule(const Dia::Core::StringCRC& instanceId);

protected:
    StartResult DoStart() override;   // Wait for UIModule → load page → register checkpoints
    void DoUpdate(float dt) override; // Emit metrics; run mouse injection after first frame
    StopResult DoStop() override;     // Unregister checkpoints; reset state

private:
    void RegisterCheckpoints();

    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::UIModule>         mUI{this};
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::AutomationModule> mAutomation{this};

    UIUltralightTestPage mPage{this};

    bool mPageLoaded            = false;
    bool mPageReadyFired        = false;
    bool mButtonClickedFired    = false;
    bool mRoundTripCorrect      = false;
    bool mPixelBufferNonEmpty   = false;
    bool mMouseClickHandled     = false;
    bool mMouseInjected         = false;

    unsigned int mFrameCount         = 0;
    unsigned int mFramesUntilLoaded  = 0;
    unsigned int mRoundTripCount     = 0;

    // Run-1 metrics saved for determinism check
    unsigned int mRun1FramesUntilLoaded = 0;
    bool mDeterminismReady = false;

    static constexpr unsigned int kBudgetFrames = 300;
};
```

**UIUltralightTestPage** (Page shell + callback interface, `UI/` subdirectory):
```cpp
// Interface for JS→C++ callbacks
class IUIUltralightTestCallbacks
{
public:
    virtual void OnPageReady()                                        = 0;
    virtual void OnButtonClicked()                                    = 0;
    virtual void ReportReceivedValue(const Dia::UI::BoundMethodArgs&) = 0;
};

// BoundMethodValue GetTestValue(): returns "dia_test_value_42" as a string

class UIUltralightTestPage : public Dia::UI::Page
{
public:
    explicit UIUltralightTestPage(IUIUltralightTestCallbacks* callbacks);
    void InitializePage();

    // Returns the known test value (for round-trip verification)
    static constexpr const char* kTestValue = "dia_test_value_42";

    // GetTestValue is a BoundMethod WITH return value (MethodPtrWithRetVal)
    Dia::UI::BoundMethodValue GetTestValue(const Dia::UI::BoundMethodArgs& args);

private:
    void OnPageReady_JS(const Dia::UI::BoundMethodArgs& args);
    void OnButtonClicked_JS(const Dia::UI::BoundMethodArgs& args);
    void ReportReceivedValue_JS(const Dia::UI::BoundMethodArgs& args);

    IUIUltralightTestCallbacks* mCallbacks;
};
```

`UIUltralightTestStageModule` implements `IUIUltralightTestCallbacks` and owns the page.

### Alpine Panel

`ui_ultralight_test.html` — single Alpine.js file, no build step.

Bridge contract:
- `app.OnPageReady()` — called from `document.addEventListener('DOMContentLoaded', ...)`
- `app.OnButtonClicked()` — called from the button's `@click` handler
- `app.GetTestValue()` — called on DOMContentLoaded; return value echoed back via `app.ReportReceivedValue(val)`
- `app.ReportReceivedValue(val)` — called with the value returned by `GetTestValue()`

Mouse injection target: the button element, positioned at a known fixed coordinate (e.g. centre of panel at ~810, 120 in screen space, matching a 290×340 panel offset 16px from right edge of a 960px viewport).

### Checkpoint Logic

All 6 checkpoints registered in `DoStart()` via `AutomationService`:

| Checkpoint name | Passes when |
|-----------------|-------------|
| `ui.page_loaded` | `mPageLoaded == true` |
| `ui.js_to_cpp_callback_fired` | `mPageReadyFired && mButtonClickedFired` |
| `ui.round_trip_value_correct` | `mRoundTripCorrect == true` |
| `ui.pixel_buffer_non_empty` | `mPixelBufferNonEmpty == true` |
| `ui.mouse_click_handled` | `mMouseClickHandled == true` |
| `ui.deterministic_reload` | `mDeterminismReady && abs(mRun1FramesUntilLoaded - mFramesUntilLoaded) <= 1` |

`ui.deterministic_reload` is designed to be polled across two navigation cycles by the pytest scenario — it passes on the second run once both `mRun1FramesUntilLoaded` and `mFramesUntilLoaded` are populated.

### DoUpdate Behaviour

```
Frame 0:      DoStart called → wait for UIModule.HasStarted() → load page → register checkpoints
Frame 1–N:    Poll IsPageLoaded() → once true, record mFramesUntilLoaded, set mPageLoaded
Frame N+1:    If pixel buffer size > 0 and non-zero bytes exist → set mPixelBufferNonEmpty
Frame N+2:    Inject mouse click at button coordinates (once only) → mMouseInjected = true
              mMouseClickHandled set via OnButtonClicked callback firing after injection
Ongoing:      Emit metrics each frame once loaded
```

### Pytest Scenario

```python
# Cluiche/Tests/E2E/scenarios/cluichetest/uiultralight/test_ui_ultralight.py

def test_ui_page_loads(dia_client):
    dia_client.navigate_to("UIUltralightTestStage")
    result = dia_client.poll_checkpoint("ui.page_loaded", timeout_s=5.0)
    assert result["passed"], f"Page did not load: {result['message']}"
    dia_client.navigate_to("Boot")

def test_ui_js_callbacks(dia_client):
    dia_client.navigate_to("UIUltralightTestStage")
    result = dia_client.poll_checkpoint("ui.js_to_cpp_callback_fired", timeout_s=5.0)
    assert result["passed"], f"JS callbacks did not fire: {result['message']}"
    dia_client.navigate_to("Boot")

def test_ui_round_trip(dia_client):
    dia_client.navigate_to("UIUltralightTestStage")
    result = dia_client.poll_checkpoint("ui.round_trip_value_correct", timeout_s=5.0)
    assert result["passed"], f"Round-trip value mismatch: {result['message']}"
    dia_client.navigate_to("Boot")

def test_ui_pixel_buffer(dia_client):
    dia_client.navigate_to("UIUltralightTestStage")
    result = dia_client.poll_checkpoint("ui.pixel_buffer_non_empty", timeout_s=5.0)
    assert result["passed"], f"Pixel buffer empty: {result['message']}"
    dia_client.navigate_to("Boot")

def test_ui_mouse_injection(dia_client):
    dia_client.navigate_to("UIUltralightTestStage")
    result = dia_client.poll_checkpoint("ui.mouse_click_handled", timeout_s=5.0)
    assert result["passed"], f"Mouse click not handled: {result['message']}"
    dia_client.navigate_to("Boot")

def test_ui_deterministic_reload(dia_client, assert_metric):
    # Run 1
    dia_client.navigate_to("UIUltralightTestStage")
    dia_client.poll_checkpoint("ui.page_loaded", timeout_s=5.0)
    dia_client.navigate_to("Boot")

    # Run 2 — determinism checkpoint passes once module has seen both runs
    dia_client.navigate_to("UIUltralightTestStage")
    result = dia_client.poll_checkpoint("ui.deterministic_reload", timeout_s=10.0)
    assert result["passed"], f"Determinism check failed: {result['message']}"

    assert_metric("cluichetest.ui.frames_until_loaded", "<=", 10)
    dia_client.navigate_to("Boot")
```

---

## Files Touched

| File | Change |
|------|--------|
| `docs/specs/features/cluichetest/teststages/ui-ultralight-stage.md` | This spec |
| `docs/specs/features/cluichetest/teststages/ui-ultralight-stage.mockup.html` | Visual acceptance gate |
| `docs/specs/features/cluichetest/teststages/ui-ultralight-stage.plan.md` | Implementation plan (created at implementation start) |
| `docs/specs/systems/cluichetest/teststages.md` | Register feature in Features table |
| `Cluiche/CluicheTest/Modules/TestStages/UIUltralightTestStageModule.h/.cpp` | New test stage module |
| `Cluiche/CluicheTest/Modules/TestStages/UIUltralight/UIUltralightTestPage.h/.cpp` | Page shell + callback interface |
| `Cluiche/Assets/Stages/UIUltralightTestStage/ui_ultralight_test_stage.diastage` | Stage metadata |
| `Cluiche/Assets/Stages/UIUltralightTestStage/misc/ApplicationFlow/ui_ultralight_test_stage.diaapp` | Module wiring |
| `Cluiche/Assets/Stages/UIUltralightTestStage/Presentation/UI/ui_ultralight_test.html` | Alpine.js test panel |
| `Cluiche/Assets/CluicheTest/assets.catalogue.json` | Register new stage + assets |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` | Add new module source files |
| `Cluiche/CluicheTest/CluicheTest.vcxproj.filters` | Add new module filters |
| `Cluiche/Tests/E2E/scenarios/cluichetest/uiultralight/test_ui_ultralight.py` | Pytest scenario (6 tests) |
| `Cluiche/Tests/E2E/plans/cluichetest/default.json` | Register scenario in default plan |

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `ui-ultralight-stage.mockup.html` | Visual sign-off | Done | sonnet | Approved 2026-05-27 |
| 2 | Create `UIUltralightTestPage.h/.cpp` in `Modules/TestStages/UIUltralight/` — page shell, `InitializePage`, all 4 bound methods | Unit test: page binds methods without crash | Todo | sonnet | |
| 3 | Create `UIUltralightTestStageModule.h/.cpp` — DoStart/DoUpdate/DoStop, all 6 checkpoints, metrics | Stage loads; all checkpoints reachable | Todo | sonnet | Depends on Task 2 |
| 4 | Create `ui_ultralight_test.html` Alpine panel — OnPageReady, OnButtonClicked, GetTestValue round-trip | Open in browser; all 4 bridge calls visible | Todo | sonnet | Depends on Task 2 (bound method names) |
| 5 | Create `ui_ultralight_test_stage.diastage` + `ui_ultralight_test_stage.diaapp` | Manifest validation passes | Todo | haiku | Depends on Task 3 |
| 6 | Register stage in `assets.catalogue.json` | Stage visible in Boot menu | Todo | haiku | Depends on Task 5 |
| 7 | Update `CluicheTest.vcxproj` + `.vcxproj.filters` | Clean build | Todo | haiku | Depends on Tasks 2, 3 |
| 8 | Write pytest scenario `test_ui_ultralight.py` (6 tests); register in `default.json` | Scenario file valid; plan updated | Todo | sonnet | Depends on Task 3 (checkpoint names) |
| 9 | `dia run cluichetest` — visual verify against mockup; all 6 checkpoints PASS | Manual visual gate + no ERROR logs | Todo | sonnet | Depends on Tasks 3–7 |
| 10 | Commit + update spec status → Done | — | Todo | haiku | Depends on Tasks 8, 9 |

---

## Dependencies

- Tasks 2 and 4 can run in parallel (page .cpp and HTML share bound method names but don't block each other).
- Task 3 depends on Task 2 (needs page type).
- Tasks 5, 6, 7 can run in parallel after Task 3.
- Task 8 can run in parallel with 5–7 (only needs checkpoint names from Task 3).
- Task 9 requires Tasks 3–7 all complete.
- Task 10 requires Tasks 8 and 9 complete.

---

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all entity/component IDs | `kTypeId`, checkpoint names (`"ui.page_loaded"` etc.), stage name `"UIUltralightTestStage"`, metric names all `StringCRC`. |
| PD-004 | No STL containers in public APIs | Module and page interfaces use Dia types. `BoundMethodArgs` is a Dia container. `mCallbacks` is a raw pointer to an interface, not an STL container. |
| PD-006 | VS project files are source of truth | Task 7 adds all new source files to `CluicheTest.vcxproj` and `.vcxproj.filters`. |
| PD-007 | C++20 required | `constexpr StringCRC`, `constexpr const char*` for `kTestValue`, standard C++ features throughout. |
| PD-009 | Generated output under `Cluiche/out/` | Session logs and metrics in `Cluiche/out/CluicheTest/sessions/`. |
| PD-010 | `.diagame` root; `.diastage` for stage metadata | `ui_ultralight_test_stage.diastage` added as a typed stage import; loader resolves manifest via the `.diastage` pointer. |
| AD-001 | Three PUs (Main/Render/Sim) | `UIUltralightTestStageModule` lives on MainPU alongside `UIModule`. No structural changes to PU topology. |
| AD-003 | Entry point in `Main.cpp` | No change; stage hooks into existing app lifecycle via manifest. |
| AD-005 | App is testbed, not shipped product | Stage exists purely for engine validation; no production constraints apply. |
| SD-TS-001 | One manifest stage per feature | Exactly one stage: `UIUltralightTestStage`. |
| SD-TS-002 | Checkpoints registered in DoStart, auto-cleared on stop | All 6 checkpoints registered in `DoStart` via `AutomationService`; auto-cleared via module ownership on stop. |
| SD-TS-003 | Metrics for threshold assertions | `cluichetest.ui.frames_until_loaded` and `cluichetest.ui.round_trip_count` emitted; pytest asserts `frames_until_loaded <= 10`. |
| SD-TS-004 | All stages return to Boot | `transitions: ["Boot"]` in `ui_ultralight_test_stage.diastage`. |
| SD-TS-005 | Individual stages are feature specs | This spec IS the feature spec for this stage. |

---

## Open Questions

None.

---

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| Q1 | Design — mouse coordinates | The button's screen coordinates for `InjectMouseClick` must be hardcoded. How should these be determined and kept in sync with the HTML layout? | Hardcode in `UIUltralightTestStageModule`; document in a comment referencing the Alpine panel layout. Accept ±10px tolerance since the button is large. | **Hardcode in module with a comment referencing the HTML button position. Button occupies the full panel width (~290px) so any x within the panel and y at button row (~120px from panel top) will hit it.** |
| Q2 | Design — GetTestValue return type | `GetTestValue` returns a string — does `BoundMethod::CreateBoundMethodWithRetVal` support `BoundMethodValue::kString`? | Yes — `BoundMethodValue` has a `String64` branch; string return is supported. | **Confirmed — `BoundMethodValue` supports `kString` via `String64`. `kTestValue = "dia_test_value_42"` fits within 64 chars.** |
| Q3 | Design — determinism checkpoint scope | `ui.deterministic_reload` requires state from two separate navigation cycles. Should the module persist run-1 data across stop/start, or should the pytest scenario own the comparison? | Module persists run-1 data (static member or singleton). Simpler for the checkpoint contract. | **Module uses a static member (`mRun1FramesUntilLoaded`) that survives stop/start. Reset only when the second run completes and the checkpoint fires. Pytest scenario navigates twice and polls the checkpoint on run 2.** |
| Q4 | Design — pixel buffer sampling | Checking all bytes of a 3.5 MB buffer for non-zero is expensive. What is the correct sampling strategy? | Sample a fixed stride (e.g. every 1024 bytes); if any sampled byte is non-zero, pass. | **Sample every 1024 bytes (≈3584 samples for a 1280×720 BGRA buffer). If any sample is non-zero, `mPixelBufferNonEmpty = true`. Cost is negligible at 30Hz.** |
| Q5 | Design — Alpine vs React | React pipeline exists; should this stage use Alpine or React? | Alpine — panel is self-contained, no build step, testing the bridge not the framework. | **Alpine confirmed. React integration testing is a separate future spec. Alpine keeps the failure signal clean: a bridge failure is always DiaUIUltralight, not a build pipeline issue.** |
| Q6 | Tasks — PU for test module | `UIUltralightTestStageModule` depends on `UIModule` which is on MainPU. Must the test module also be on MainPU? | Yes — `ModuleRef` requires same PU as the referenced module. | **Confirmed. Both on MainPU. No structural change to PU topology.** |

---

## Status

`Approved` — 2026-05-27
