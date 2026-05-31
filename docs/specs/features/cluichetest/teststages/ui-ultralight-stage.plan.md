# Implementation Plan: UIUltralightTestStage

**Spec:** [ui-ultralight-stage.md](ui-ultralight-stage.md)
**Status:** Todo

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
| 2 | Rename `Assets/Stages/UIUltralightStage/` → `UIUltralightTestStage/`; create `ui_ultralight_test_stage.diastage` + `misc/ApplicationFlow/ui_ultralight_test_stage.diaapp` | Files exist at correct paths; diastage points to diaapp | Todo | haiku | Move existing Alpine samples into the renamed folder |
| 3 | Create `UIUltralightTestPage.h/.cpp` in `Modules/TestStages/UIUltralight/` — `IUIUltralightTestCallbacks` interface, `InitializePage()`, 4 bound methods (OnPageReady, OnButtonClicked, GetTestValue, ReportReceivedValue) | Build passes; page compiles | Todo | sonnet | |
| 4 | Create `UIUltralightTestStageModule.h/.cpp` — inherits `TestStageModuleBase`, implements `IUIUltralightTestCallbacks`; DoStart loads page + registers 6 checkpoints; DoUpdate polls page state + injects mouse + emits metrics; `PersistsAcrossEntries()` returns true | Stage loads; all 6 checkpoints reachable | Todo | sonnet | Depends on T-03 |
| 5 | Create `ui_ultralight_test.html` Alpine panel — `DOMContentLoaded` calls `app.OnPageReady()` + `app.GetTestValue()` → echoes via `app.ReportReceivedValue(val)`; button `@click` calls `app.OnButtonClicked()` | Open in browser; 4 bridge calls visible in console | Todo | sonnet | Depends on T-03 (bound method names) |
| 6 | Register stage in `cluiche_main.diaapp` (Boot transitions, stage entry, UIModule stages, VisualDebugger stages, HUD stages, Console stages); register in `cluichetest.diagame` (import entry); register in `assets.catalogue.json` (stage + manifest + ui entries) | Stage visible in Boot menu; `dia pipeline` passes | Todo | haiku | Depends on T-02 |
| 7 | Update `CluicheTest.vcxproj` + `.vcxproj.filters` — add UIUltralightTestStageModule.h/.cpp + UIUltralightTestPage.h/.cpp | Clean build | Todo | haiku | Depends on T-03, T-04 |
| 8 | Write pytest scenario `test_ui_ultralight.py` (6 tests); register in `default.json` | Scenario file valid; plan updated | Todo | sonnet | Depends on T-04 (checkpoint names) |
| 9 | `dia run cluichetest` — visual verify against mockup; all 6 checkpoints PASS; no ERROR logs | Manual visual gate | Todo | sonnet | Depends on T-04, T-05, T-06, T-07 |
| 10 | Commit + update spec status → Done | — | Todo | haiku | Depends on T-08, T-09 |

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
