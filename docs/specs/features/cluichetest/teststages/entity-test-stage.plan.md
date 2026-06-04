# Implementation Plan: EntityTestStage

**Spec:** [entity-test-stage.md](entity-test-stage.md)
**Status:** In Progress
**Created:** 2026-05-27
**Revised:** 2026-06-01 — collapsed redundancy against TestStageModuleBase, corrected paths, removed dead T-06 console tab

---

## Session Notes

**What already exists (verified 2026-06-01):**

1. **TestStageModuleBase** (`Cluiche/CluicheTest/Modules/TestStages/TestStageModuleBase.h/.cpp`) — provides:
   - Frame counting, budget/timeout, TestResultsRegistry integration
   - AutomationService stream resolution + polling (`AreDependenciesReady()` gate)
   - Checkpoint auto-registration + auto-unregister in DoStop
   - ReportPassed/ReportFailed with deferred capture (RenderFence wait)
   - Entry counting for determinism (`GetEntryCount()`)

2. **EntityModule** (`Cluiche/CluicheGameBaseline/Modules/EntityModule.h/.cpp`) — owns `Dia::Entity::Domain`, registers ParentComponent + ChildBufferComponent pools, drives `Update(dt)` + `EndOfFrame()` each frame. **Missing:** `GetDomain()` and `IsReady()` accessors (only exposes `GetInspectable()`).

3. **VisualDebuggerModule** (`Cluiche/CluicheGameBaseline/Modules/VisualDebuggerModule.h`) — SimPU module; accessed via `ModuleRef<VisualDebuggerModule>` (NOT a static). Exposes `GetLayerManager()`.

4. **IVisualDebugger / Drawer pattern** (`Cluiche/CluicheTest/Modules/TestStages/Drawers/`) — implement `GetLayerName()`, `Draw(FrameData&)`, `DrawImGui()`; register with `DebugLayerManager` using priority + stage tag. Lazy-init in first `OnUpdate` (see Geometry2DTestStageModule pattern).

5. **DiaVisualDebuggerConsole** — has stage tabs (one per registered stage tag). Layers with `DrawImGui()` get their inspector UI rendered under the stage tab automatically. **No custom "Entity" tab needed** — just register drawers with the `"Entity"` stage tag and implement `DrawImGui()` on the drawer for counter display.

6. **VisualDebuggerConsoleModule** — RenderPU module. Reads `ServiceStream<DebugLayerManager>`. No modification needed by this plan.

**Critical PU constraint:**
EntityModule is on SimPU (it accesses Domain which must be single-threaded, SD-ENT-018). EntityTestStageModule must also be on SimPU so `ModuleRef<EntityModule>` resolves. AutomationService is reached via `ServiceStreamReader` (cross-PU), which `TestStageModuleBase` already handles.

**Spec deviations addressed:**
- Spec says SimPU for EntityTestModule — confirmed correct; TestStageModuleBase uses ServiceStreamReader for AutomationService (cross-PU safe).
- Spec mentions `EntityModule` in `CluicheTest/Modules/` — actual location is `CluicheGameBaseline/Modules/`. No move needed; just reference it there.
- Spec T-06 "add Entity tab to VisualDebuggerConsoleModule" — unnecessary. DrawImGui() on the drawer achieves the same result automatically under the stage's tab.
- Spec references `VisualDebuggerModule::GetStaticLayerManager()` — that static is eliminated. Use `ModuleRef<VisualDebuggerModule>` + lazy init.

---

## Implementation Patterns

### EntityModule additions (T-01)

```cpp
// EntityModule.h — add to public interface:
bool                  IsReady()   const { return mReady; }
Dia::Entity::Domain&  GetDomain()       { return mDomain; }

// EntityModule.h — private:
bool mReady = false;

// EntityModule.cpp — DoStart (after existing pool registration):
mReady = true;
return StartResult::kReady;

// EntityModule.cpp — DoStop (before existing log):
mReady = false;
```

### TestStageModuleBase subclass (T-03)

```cpp
// EntityTestStageModule.h
class EntityTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Validates diaentitytemplate: spawn/destroy/hierarchy/query/mailbox/lifecycle";
    explicit EntityTestStageModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override { return 120; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    bool AreDependenciesReady() override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;

private:
    // ... entity handles, counters, drawer
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::EntityModule> mEntityModule{this};
#ifdef DIA_DEBUG
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebuggerRef{this};
    std::unique_ptr<EntityTestDrawer> mDrawer;
#endif
};
```

### Drawer pattern (T-05)

```cpp
// EntityTestDrawer.h — #ifdef DIA_DEBUG only
class EntityTestDrawer : public Dia::Debug::IVisualDebugger
{
public:
    EntityTestDrawer(/* refs to entity positions, hierarchy, counters */);
    Dia::Core::StringCRC GetLayerName() const override; // "entity.shapes"
    void Draw(Dia::Graphics::FrameData& frameData) override; // circles + hierarchy lines
    void DrawImGui() override; // Domain Stats, Query Results, Mailbox, Lifecycle Counters
};

// Registered in OnUpdate (lazy init):
// mgr->Register(mDrawer.get(), 20, Dia::Core::StringCRC("Entity"));
```

`DrawImGui()` replaces the old T-06 "Entity tab" — the console's stage tab system picks it up automatically.

---

## Task Table

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| T-00 | *(Mockup)* `entity-test-stage.mockup.html` | Visual sign-off | Done | sonnet | Approved 2026-05-27 |
| T-01 | Add `GetDomain()` + `IsReady()` + `mReady` to `CluicheGameBaseline/Modules/EntityModule.h/.cpp` | `dia run googletest` — build passes; existing usage unaffected | Todo | haiku | 3-line change |
| T-02 | Create `TransformComponent.h/.cpp` + `VisualTestRenderComponent.h/.cpp` under `Cluiche/CluicheTest/Modules/TestStages/Entity/` | Build passes (added to vcxproj in T-03) | Todo | sonnet | TransformComponent: x,y + DIA_UPDATABLE + OnAttach/OnDetach counters. VisualTestRenderComponent: radius, label, colour. Static counter pointer injection for lifecycle tracking. |
| T-03 | Run `/new-cluichetest-stage EntityTest` scaffold. Then: (a) replace skeleton with EntityTestStageModule inheriting TestStageModuleBase; (b) add EntityModule to `.diaapp` SimPU modules; (c) add T-02 component files to vcxproj+filters; (d) add VisualDebuggerModule on SimPU; (e) force-copy cluiche_main.diaapp to bin | `dia pipeline --target cluichetest` passes; EntityTestStage in Boot menu | Todo | sonnet | Skill handles: diastage, diaapp, vcxproj, cluiche_main, diagame, catalogue, pipeline.toml |
| T-04 | Implement `EntityTestStageModule` — `AreDependenciesReady` checks EntityModule; `OnStart` registers component pools, creates 9 entities (4 hierarchy + 4 query + 1 doomed), registers 6 checkpoints; `OnUpdate` destroys doomed on frame 1, broadcasts mailbox every 10 frames, checks pass/fail | `dia run cluichetest` — stage loads, all 6 checkpoints PASS within budget | Todo | sonnet | Depends T-01, T-02, T-03. All frame/timeout/registry logic inherited from TestStageModuleBase. |
| T-05 | Create `EntityTestDrawer` (`Drawers/EntityTestDrawer.h/.cpp`) — `IVisualDebugger` subclass; `Draw()` renders circles (blue=hierarchy, green=query, red-X=doomed) + dashed hierarchy lines; `DrawImGui()` renders Domain Stats, Query Results, Mailbox, Lifecycle sections. Lazy-init in OnUpdate via `ModuleRef<VisualDebuggerModule>`, register with stage tag `"Entity"`. | Visual matches mockup; console shows Entity tab with counters | Todo | sonnet | Follows Geometry2DShapesDrawer pattern exactly. Replaces old T-05 + T-06. |
| T-06 | Write `Tools/orchestrator/scenarios/cluichetest/entity_test_stage/smoke.py` — navigate_to EntityTestStage, wait_for all 6 checkpoints, assert metrics, navigate_to Boot | Scenario file valid; `dia orchestrate` passes | Todo | sonnet | Mailbox triggered automatically (broadcast every 10 frames) |
| T-07 | `dia run cluichetest` — visual verify against mockup | All 6 checkpoints PASS; circles + labels + hierarchy lines render; DrawImGui counters correct | Todo | sonnet | |
| T-08 | Update spec status → Done; commit | Spec status = Done | Todo | haiku | |

---

## Dependencies

```
T-01 (EntityModule accessors) ──┐
T-02 (components)             ──┼── T-03 (scaffold + wiring) ── T-04 (module impl) ──┬── T-05 (drawer)
                                                                                      ├── T-06 (pytest)
                                                                                      └──┐
T-05 ──────────────────────────────────────────────────────────────────────────────────┬── T-07 (visual verify)
T-06 ──────────────────────────────────────────────────────────────────────────────────┤
                                                                                       └── T-08 (commit)

T-01 and T-02 can run in parallel.
T-03 depends on T-02 (needs files to add to vcxproj).
T-05 and T-06 can run in parallel after T-04.
T-07 requires T-04, T-05 complete.
T-08 requires T-06 and T-07 complete.
```

---

## Files Touched

| File | Change | Task |
|------|--------|------|
| `Cluiche/CluicheGameBaseline/Modules/EntityModule.h` | Add `GetDomain()`, `IsReady()`, `mReady` | T-01 |
| `Cluiche/CluicheGameBaseline/Modules/EntityModule.cpp` | Set `mReady` in DoStart/DoStop | T-01 |
| `Cluiche/CluicheTest/Modules/TestStages/Entity/TransformComponent.h/.cpp` | New test component | T-02 |
| `Cluiche/CluicheTest/Modules/TestStages/Entity/VisualTestRenderComponent.h/.cpp` | New test component | T-02 |
| `Cluiche/Assets/Stages/EntityTest/entity_test_stage.diastage` | New stage pointer | T-03 |
| `Cluiche/Assets/Stages/EntityTest/misc/ApplicationFlow/entity_test_stage.diaapp` | Module wiring (SimPU: EntityModule + EntityTestStageModule + VisualDebuggerModule) | T-03 |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` + `.vcxproj.filters` | Add all new .h/.cpp files | T-03 |
| `Cluiche/Assets/CluicheTest/Global/Misc/ApplicationFlow/cluiche_main.diaapp` | Add stage to stages[]; Boot transitions | T-03 |
| `Cluiche/Assets/CluicheTest/cluichetest.diagame` | Add stage import | T-03 |
| `Cluiche/Assets/CluicheTest/assets.catalogue.json` | Add stage + manifest entries | T-03 |
| `pipeline.toml` | Add asset_stages entry + deploy files | T-03 |
| `Cluiche/bin/CluicheTest/Debug/x64/assets/global/misc/ApplicationFlow/cluiche_main.diaapp` | Force-copy after edit | T-03 |
| `Cluiche/CluicheTest/Modules/TestStages/EntityTestStageModule.h/.cpp` | Full test stage module | T-04 |
| `Cluiche/CluicheTest/Modules/TestStages/Drawers/EntityTestDrawer.h/.cpp` | IVisualDebugger — circles, lines, DrawImGui counters | T-05 |
| `Tools/orchestrator/scenarios/cluichetest/entity_test_stage/smoke.py` | New E2E scenario | T-06 |
| `docs/specs/features/cluichetest/teststages/entity-test-stage.md` | Status → Done | T-08 |

---

## What Changed From Original Plan

| # | Issue | Resolution |
|---|-------|------------|
| 1 | Plan wrote frame counting, TestResultsRegistry, timeout logic manually in T-04 | Removed — all inherited from TestStageModuleBase |
| 2 | Plan referenced `VisualDebuggerModule::GetStaticLayerManager()` | Replaced with `ModuleRef<VisualDebuggerModule>` + lazy init (static was eliminated 2026-05-30) |
| 3 | Plan had EntityModule at `CluicheTest/Modules/EntityModule.h` | Corrected path: `CluicheGameBaseline/Modules/EntityModule.h` |
| 4 | T-06 "add Entity tab to VisualDebuggerConsoleModule" | Eliminated — `DrawImGui()` on the drawer surfaces under the stage tab automatically. Console has no custom tabs; it auto-discovers drawers by stage tag. |
| 5 | Plan had 10 tasks (T-00 through T-09) with manual boilerplate | Collapsed to 9 tasks (T-00 through T-08); T-04 is much thinner |
| 6 | Plan said MainPU for EntityTestModule (AutomationModule constraint) | Corrected: SimPU. TestStageModuleBase uses `ServiceStreamReader<AutomationService>` which is cross-PU safe. EntityModule + Domain require SimPU (SD-ENT-018). |
| 7 | Spec/plan referenced blueprint loading + `ResolveEntityHandles` | Removed — AD-002 (code-based scene). Entities created directly in OnStart; handles returned from CreateEntity(). |
