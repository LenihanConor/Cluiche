# Implementation Plan: UIUltralightTestStage

**Spec:** [ui-ultralight-stage.md](ui-ultralight-stage.md)
**Status:** Done

---

## Naming Alignment

The spec was written before the `*TestStage` + `*TestStageModule` conventions were established. This plan uses the aligned names:

| Spec says | Plan uses | Reason |
|-----------|-----------|--------|
| `UIUltralightStage` | `UIUltralightTestStage` | Matches `*TestStage` convention |
| `UIUltralightTestModule` | `UIUltralightTestStageModule` | Matches `Geometry2DTestStageModule` pattern |
| `Modules/TestStages/UI/` | `Modules/TestStages/UIUltralight/` | Subdirectory named after domain (like `Entity/`) |
| `Assets/Stages/UIUltralightStage/` | `Assets/Stages/UIUltralightTestStage/` | Matches stage name |
| `ui_ultralight_stage.diastage` | `ui_ultralight_test_stage.diastage` | Matches stage name snake_case |
| `ui_ultralight_stage.diaapp` | `ui_ultralight_test_stage.diaapp` | Matches stage name snake_case |

The existing `Assets/Stages/UIUltralightStage/` folder (Alpine component library) must be renamed to `UIUltralightTestStage/`.

---

## Key Constraints

- Module inherits `TestStageModuleBase` — handles frame counting, timeout, checkpoint lifecycle, HUD integration
- Module has `kTypeId`, `kAllowedPUs = kMain`, `kDescription` (module-metadata pattern)
- Module lives on **MainPU** (UIModule is on MainPU; `ModuleRef` requires same PU)
- All IDs use `StringCRC` — stage name, module type, checkpoint names, metric names
- No STL in public APIs; Dia types throughout
- `PersistsAcrossEntries()` returns `true` — determinism check needs state across Boot→Stage→Boot→Stage
- Scaffold via `dia scaffold stage UIUltralightTest --budget 300` (or manual setup matching Geometry2D pattern)
- `VisualDebuggerModule` on SimPU (writes `SimToRender`) — required for rendering
- Boot transitions list must include `UIUltralightTestStage`
- 6 checkpoints, 2 metrics, 300-frame budget (10s at 30Hz)

---

## Implementation Patterns

### Module Structure (TestStageModuleBase)

```cpp
// UIUltralightTestStageModule.h
class UIUltralightTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;  // "UIUltralightTestStageModule"
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kMain;
    static constexpr const char* kDescription = "E2E test stage for DiaUIUltralight system";
    explicit UIUltralightTestStageModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::Core::StringCRC GetStageName() const override;       // "UIUltralightTestStage"
    unsigned int GetBudgetFrames() const override;             // 300
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    bool AreDependenciesReady() override;                      // UIModule started
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;
    bool PersistsAcrossEntries() const override { return true; }

private:
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::UIModule> mUI{this};
    UIUltralightTestPage mPage;

    bool mPageLoaded = false;
    bool mPageReadyFired = false;
    bool mButtonClickedFired = false;
    bool mRoundTripCorrect = false;
    bool mPixelBufferNonEmpty = false;
    bool mMouseClickHandled = false;
    bool mMouseInjected = false;

    unsigned int mFramesUntilLoaded = 0;
    unsigned int mRoundTripCount = 0;

    // Determinism — persists across entries
    unsigned int mRun1FramesUntilLoaded = 0;
    bool mDeterminismReady = false;
};
```

### Page Shell (UIUltralightTestPage)

```cpp
// UIUltralight/UIUltralightTestPage.h
class IUIUltralightTestCallbacks
{
public:
    virtual ~IUIUltralightTestCallbacks() = default;
    virtual void OnPageReady() = 0;
    virtual void OnButtonClicked() = 0;
    virtual void ReportReceivedValue(const Dia::UI::BoundMethodArgs&) = 0;
};

class UIUltralightTestPage : public Dia::UI::Page
{
public:
    explicit UIUltralightTestPage(IUIUltralightTestCallbacks* callbacks);
    void InitializePage();
    static constexpr const char* kTestValue = "dia_test_value_42";
    Dia::UI::BoundMethodValue GetTestValue(const Dia::UI::BoundMethodArgs& args);
private:
    // JS→C++ bound method handlers
    IUIUltralightTestCallbacks* mCallbacks;
};
```

### Manifest (ui_ultralight_test_stage.diaapp)

```json
{
    "version": 3,
    "processing_units": [
        {
            "instance_id": "MainPU",
            "frequency_hz": 30,
            "dedicated_thread": false,
            "modules": [
                {
                    "instance_id": "UIUltralightTestStageModule",
                    "type_id": "UIUltralightTestStageModule",
                    "stages": ["UIUltralightTestStage"],
                    "dependencies": ["AutomationModule", "UIModule"],
                    "channels": []
                }
            ]
        },
        {
            "instance_id": "SimPU",
            "frequency_hz": 30,
            "dedicated_thread": true,
            "modules": []
        }
    ]
}
```

### cluiche_main.diaapp changes

- Add `"UIUltralightTestStage"` to Boot's `transitions` array
- Add `UIUltralightTestStage` stage entry (empty transitions, no auto_advance)
- Add `"UIUltralightTestStage"` to `UIModule`'s `stages` list (it currently only has `"DummyStage"`)
- Add `"UIUltralightTestStage"` to `VisualDebuggerModule`'s `stages` list on SimPU
- Add `"UIUltralightTestStage"` to `TestStageHUDModule`'s `stages` list on RenderPU
- Add `"UIUltralightTestStage"` to `VisualDebuggerConsoleModule`'s `stages` list on RenderPU

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | *(Mockup)* `ui-ultralight-stage.mockup.html` | Visual sign-off | Done | sonnet | Approved 2026-05-27 |
| 2 | Rename `Assets/Stages/UIUltralightStage/` → `UIUltralightTestStage/`; create `ui_ultralight_test_stage.diastage` + `misc/ApplicationFlow/ui_ultralight_test_stage.diaapp` | Files exist at correct paths; diastage points to diaapp | Done | haiku | New folder created (UIUltralightStage left unchanged as template); stage_ui alias set |
| 3 | Create `UIUltralightTestPage.h/.cpp` in `Modules/TestStages/UIUltralight/` — `IUIUltralightTestCallbacks` interface, `InitializePage()`, 4 bound methods (OnPageReady, OnButtonClicked, GetTestValue, ReportReceivedValue) | Build passes; page compiles | Done | sonnet | Uses window.onload (not DOMContentLoaded) — app object injected in OnDOMReady before window.onload |
| 4 | Create `UIUltralightTestStageModule.h/.cpp` — inherits `TestStageModuleBase`, implements `IUIUltralightTestCallbacks`; DoStart loads page + registers 6 checkpoints; DoUpdate polls page state + injects mouse + emits metrics; `PersistsAcrossEntries()` returns true | Stage loads; all 6 checkpoints reachable | Done | sonnet | Mouse click at (2591,78) for 2752-wide window (80% of 3440); LoadingScreenModule reused on SimPU for UI composite |
| 5 | Create `ui_ultralight_test.html` vanilla JS panel — `window.onload` calls `app.OnPageReady()` + `app.GetTestValue()` → echoes via `app.ReportReceivedValue(val)`; button `onclick` calls `app.OnButtonClicked()` | Visible in top-right panel; all 4 bridge calls fire | Done | sonnet | Alpine.js dropped (no CDN in Ultralight); plain HTML+CSS+JS |
| 6 | Register stage in `cluiche_main.diaapp` (Boot transitions, stage entry, UIModule stages, TestStageHUDModule stages, LoadingScreenModule stages); register in `cluichetest.diagame` (import entry); register in `assets.catalogue.json` (stage + manifest + ui entries) | Stage visible in Boot menu | Done | haiku | LoadingScreenModule added to UIUltralightTestStage stages for UI composite on SimPU |
| 7 | Update `CluicheTest.vcxproj` + `.vcxproj.filters` — add UIUltralightTestStageModule.h/.cpp + UIUltralightTestPage.h/.cpp | Clean build | Done | haiku | Filter group ApplicationFlow\Modules\TestStages\UIUltralight added |
| 8 | Write pytest scenario `test_ui_ultralight.py` (6 tests); register in `default.json` | Scenario file valid; plan updated | Done | sonnet | |
| 9 | `dia run cluichetest` — visual verify against mockup; all 6 checkpoints PASS; no ERROR logs | Manual visual gate | Done | sonnet | Visually verified by user — panel visible top-right, all 6 checkpoints green, PASS 1/300 |
| 10 | Commit + update spec status → Done | — | Done | haiku | Committed 9099b3cd |

---

## Dependencies

```
T-01 (mockup)     → DONE
T-02 (scaffold)   ─┐
T-03 (page)       ─┤ parallel
                   │
T-04 (module)    ← T-03
T-05 (html)      ← T-03  (parallel with T-04)
T-06 (manifests) ← T-02
T-07 (vcxproj)   ← T-03, T-04
T-08 (pytest)    ← T-04
T-09 (verify)    ← T-04, T-05, T-06, T-07
T-10 (commit)    ← T-08, T-09
```

---

## Files Touched

| File | Change | Task |
|------|--------|------|
| `Cluiche/Assets/Stages/UIUltralightTestStage/` | Renamed from `UIUltralightStage/` | T-02 |
| `Cluiche/Assets/Stages/UIUltralightTestStage/ui_ultralight_test_stage.diastage` | New stage pointer | T-02 |
| `Cluiche/Assets/Stages/UIUltralightTestStage/misc/ApplicationFlow/ui_ultralight_test_stage.diaapp` | Module wiring | T-02 |
| `Cluiche/CluicheTest/Modules/TestStages/UIUltralight/UIUltralightTestPage.h` | Page shell + callback interface | T-03 |
| `Cluiche/CluicheTest/Modules/TestStages/UIUltralight/UIUltralightTestPage.cpp` | Page implementation | T-03 |
| `Cluiche/CluicheTest/Modules/TestStages/UIUltralightTestStageModule.h` | Test stage module header | T-04 |
| `Cluiche/CluicheTest/Modules/TestStages/UIUltralightTestStageModule.cpp` | Test stage module implementation | T-04 |
| `Cluiche/Assets/Stages/UIUltralightTestStage/Presentation/UI/ui_ultralight_test.html` | Alpine.js test panel | T-05 |
| `Cluiche/Assets/CluicheTest/Global/misc/ApplicationFlow/cluiche_main.diaapp` | Stage entry, Boot transition, module stage lists | T-06 |
| `Cluiche/Assets/CluicheTest/cluichetest.diagame` | Import entry | T-06 |
| `Cluiche/Assets/CluicheTest/assets.catalogue.json` | Stage + manifest + UI entries | T-06 |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` | Add 4 source files | T-07 |
| `Cluiche/CluicheTest/CluicheTest.vcxproj.filters` | Add 4 source files to filter tree | T-07 |
| `Cluiche/Tests/E2E/scenarios/cluichetest/uiultralight/test_ui_ultralight.py` | 6 pytest tests | T-08 |
| `Cluiche/Tests/E2E/plans/cluichetest/default.json` | Register scenario | T-08 |
| `docs/specs/features/cluichetest/teststages/ui-ultralight-stage.md` | Status → Done; rename stage throughout | T-10 |
