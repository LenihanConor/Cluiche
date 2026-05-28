# Implementation Plan: EntityTestStage

**Spec:** [entity-test-stage.md](entity-test-stage.md)
**Status:** In Progress
**Created:** 2026-05-27

---

## Session Notes

**Spec decisions summary:**
EntityTestStage exercises DiaEntity (94 unit tests, plan marked Done) under real PU timing for the first time. Two-module split: `EntityModule` (reusable, owns Domain, drives Update/EndOfFrame) + `EntityTestModule` (test-only thin layer, `ModuleRef<EntityModule>`, registers checkpoints). Scene is **code-based** (AD-002) — `EntityTestModule::DoStart` calls `GetDomain().CreateEntity()` directly. No blueprint JSON needed.

**Critical PU constraint:**
AutomationModule lives on **MainPU**. `ModuleRef<AutomationModule>` cannot resolve cross-PU. Therefore `EntityTestModule` MUST be on **MainPU** (not SimPU). `EntityModule` also on MainPU (same as existing placement). The `VisualDebuggerModule` goes on SimPU as the mandatory `SimToRender` writer — without it RenderPU starves and the app freezes.

**Binding constraints:**
- PD-001: all IDs StringCRC — kTypeId, checkpoint names, mailbox message key
- PD-004: no STL in public APIs — component interfaces use FIELD macros only
- PD-006: VS project files source of truth — all new .h/.cpp added to CluicheTest.vcxproj
- SD-ENT-012: structural changes (destroy, add component) queued — applied at EntityModule's EndOfFrame
- SD-ENT-017: Domain non-copyable/non-movable — direct member in EntityModule, never moved
- SD-ENT-018: single-threaded per realm — Domain accessed only from MainPU thread
- SD-TS-002: checkpoints registered in DoStart, auto-clear via UnregisterCheckpoints in DoStop
- SD-TS-004: `transitions: ["Boot"]` added to `cluiche_main.diaapp` stages array (not in `.diastage`)

**Architecture note — EntityModule current state:**
EntityModule exists at `Cluiche/CluicheTest/Modules/EntityModule.h/.cpp` but is minimal:
- Has `GetInspectable()` but no `GetDomain()` or `IsReady()`
- DoStart registers ParentComponent + ChildBufferComponent pools only and returns kReady immediately
- No blueprint loading (correct for AD-002)
- Task T-01 adds `GetDomain()` + `IsReady()` to complete the adapter contract

**Architecture note — manifest structure:**
`.diastage` points to a `.diaapp`; it does NOT contain `transitions`. Transitions live in the `stages[]` array of `cluiche_main.diaapp`. Stage files live at `Cluiche/Assets/Stages/<StageName>/`.

**Architecture note — scene setup (9 entities):**
EntityTestModule creates 9 entities via `GetDomain().CreateEntity(debugName)`, queues components via `GetDomain().QueueAddComponent<T>(entity, config)`, and calls `GetDomain().EndOfFrame()` to apply. Entity handles are held directly — no resolve-by-name needed since handles are returned from `CreateEntity()`. Component pools for TransformComponent and VisualTestRenderComponent are registered in EntityTestModule::DoStart before any QueueAddComponent calls.

Entity layout:
- **Hierarchy group** (4): Parent + ChildA + ChildB + ChildC — blue circles, dashed hierarchy lines
- **Query targets** (4): Query0-3 — green circles, all carry Transform+Visual
- **Doomed** (1): destroyed via QueueDestroy on frame 0, faded ghost with red X

**Architecture note — checkpoint/metric pattern (from RigidBody2DTestModule):**
```cpp
// DoStart — guard pattern
auto* automationModule = mAutomation.Get();
if (!automationModule || !automationModule->GetService())
    return StartResult::kLoading;

// TestResultsRegistry — mandatory for HUD
const Dia::Core::StringCRC checkpoints[] = { ... };
TestResultsRegistry::GetInstance().SetRunning(
    Dia::Core::StringCRC("EntityTestStage"), kBudgetFrames, checkpoints, 6);

// Register checkpoints
auto* service = mAutomation.Get()->GetService();
service->RegisterCheckpoint(this, StringCRC("entity.spawn_complete"),
    [this]() -> Dia::Automation::CheckpointResult {
        return { condition, "description", 0.0f };
    });

// DoUpdate — mandatory frame counter
++mFrameCount;
TestResultsRegistry::GetInstance().SetActiveFrameCount(mFrameCount);

// DoStop — cleanup
if (auto* s = mAutomation.Get()->GetService())
    s->UnregisterCheckpoints(this);
```

**Architecture note — visual rendering:**
`VisualDebuggerModule::GetStaticLayerManager()` provides the layer manager. Register a new layer `"entity.shapes"`, submit `Circle` primitives per entity each frame (same pattern as RigidBody2DTestModule's drawers). Hierarchy lines submitted as `Line` primitives. Layer created lazily in first `DoUpdate` call (same as RigidBody2D drawers).

**Architecture note — Debug Console Entity tab:**
The mockup shows a full ImGui debug console with 4 collapsible sections: Domain Stats (spawn/alive/destroyed counts + component type breakdown), Query Results (live query counts), Mailbox (message routing info), Lifecycle Counters (OnAttach/OnDetach totals). This is rendered via `VisualDebuggerConsoleModule` which already has a tab system — add an "Entity" tab that reads counters from EntityTestModule.

**Architecture note — `/new-cluichetest-stage` skill:**
The skill scaffolds all 8 touch points (diastage, diaapp, module skeleton, vcxproj+filters, cluiche_main.diaapp, diagame, assets.catalogue.json, pipeline.toml). It produces a MainPU module with the correct guard/poll pattern and TestResultsRegistry wiring. The generated skeleton is then customised for EntityTestStage's specific needs (two-module split, extra components, visual drawers, debug console tab).

---

## Implementation Patterns

### T-01 — EntityModule additions

```cpp
// EntityModule.h — add to public interface:
bool                   IsReady()   const { return mReady; }
Dia::Entity::Domain&   GetDomain()       { return mDomain; }

// EntityModule.h — private:
bool mReady = false;

// EntityModule.cpp — DoStart (after existing pool registration):
mReady = true;
return StartResult::kReady;

// EntityModule.cpp — DoStop (before existing log):
mReady = false;
return StopResult::kDone;
```

### T-02 — Component pattern (TransformComponent)

```cpp
// TransformComponent.h
class TransformComponent : public Dia::Entity::IComponent {
public:
    DIA_COMPONENT(TransformComponent, "transform", 1)
    FIELD(float, x, 0.0f)
    FIELD(float, y, 0.0f)
    DIA_UPDATABLE

    void OnAttach(Dia::Entity::Domain&, Dia::Entity::Entity) override;
    void OnDetach(Dia::Entity::Domain&, Dia::Entity::Entity) override;
    void DoUpdate(Dia::Entity::Domain&, Dia::Entity::Entity, float dt) override;

    // Injected at registration time by EntityTestModule before pool creation
    static int* sAttachCounter;
    static int* sDetachCounter;
};

// TransformComponent.cpp
DIA_SERIALIZE(TransformComponent, TransformComponent::kVersion)
    DIA_FIELD(x)
    DIA_FIELD(y)
DIA_SERIALIZE_END

static Dia::Entity::FieldDesc s_TransformComponent_fields[] = {
    DIA_FIELD_ENTRY(float, x, TransformComponent)
    DIA_FIELD_ENTRY(float, y, TransformComponent)
};
DIA_COMPONENT_REGISTER(TransformComponent, "transform", true,
    s_TransformComponent_fields, DIA_ARRAY_COUNT(s_TransformComponent_fields),
    nullptr, 0)
```

**VisualTestRenderComponent** — same pattern but with `FIELD(float, radius, 8.0f)`, `FIELD_STRING(label, "")`, `FIELD(unsigned int, colour, 0x4FC3F7FF)`. Not DIA_UPDATABLE. Used only for visual rendering layer read-back.

**Counter injection:** EntityTestModule sets static counter pointers before `RegisterPool` so OnAttach/OnDetach can update module-owned counters without a back-pointer.

### T-03 — Scaffold via `/new-cluichetest-stage EntityTest`

The skill generates:
1. `Cluiche/Assets/Stages/EntityTest/entity_test_stage.diastage`
2. `Cluiche/Assets/Stages/EntityTest/misc/ApplicationFlow/entity_test_stage.diaapp`
3. `Cluiche/CluicheTest/Modules/TestStages/EntityTestStageModule.h/.cpp` (skeleton)
4. `CluicheTest.vcxproj` + `.vcxproj.filters` entries
5. `cluiche_main.diaapp` — stage entry + Boot transition + HUD coverage
6. `cluichetest.diagame` — import entry
7. `assets.catalogue.json` — stage + manifest entries
8. `pipeline.toml` — asset_stages + deploy files

After scaffold: rename the generated skeleton module to `EntityTestModule` (since the two-module split means we customise it heavily) and add `EntityModule` dependency to the `.diaapp`. The `.diaapp` must also include `VisualDebuggerModule` on SimPU writing `SimToRender`.

### T-04 — EntityTestModule scene setup

```cpp
StartResult EntityTestModule::DoStart()
{
    DIA_LOG_INFO("CluicheTest", "EntityTestModule::DoStart entry");

    // Wait for EntityModule
    auto* em = mEntityModule.Get();
    if (!em || !em->IsReady())
        return StartResult::kLoading;

    // Wait for AutomationModule
    auto* am = mAutomation.Get();
    if (!am || !am->GetService())
        return StartResult::kLoading;

    if (!mSceneBuilt)
    {
        auto& domain = em->GetDomain();

        // Register component pools (TransformComponent + VisualTestRenderComponent)
        TransformComponent::sAttachCounter  = &mOnAttachCount;
        TransformComponent::sDetachCounter  = &mOnDetachCount;
        domain.RegisterPool(new ComponentPool<TransformComponent>(...));
        domain.RegisterPool(new ComponentPool<VisualTestRenderComponent>(...));

        // Hierarchy group (4 entities — blue, #4FC3F7)
        mParentEntity = domain.CreateEntity("Parent");
        domain.QueueAddComponent<TransformComponent>(mParentEntity, MakeTransformConfig(650.f, 70.f));
        domain.QueueAddComponent<VisualTestRenderComponent>(mParentEntity, MakeVisualConfig(18.f, "Parent", 0x4FC3F7FF));
        // ChildA, ChildB, ChildC at offsets below parent...
        // Set parent via ParentComponent + ChildBufferComponent

        // Query targets (4 entities — green, #66BB6A)
        // Query0-3 in 2×2 grid...

        // Doomed entity (1 — grey/red)
        mDoomedEntity = domain.CreateEntity("Doomed");
        domain.QueueAddComponent<TransformComponent>(mDoomedEntity, ...);
        domain.QueueAddComponent<VisualTestRenderComponent>(mDoomedEntity, ...);

        domain.EndOfFrame();
        mSceneBuilt = true;
    }

    RegisterCheckpoints(am->GetService());

    const Dia::Core::StringCRC checkpoints[] = {
        Dia::Core::StringCRC("entity.spawn_complete"),
        Dia::Core::StringCRC("entity.query_correct"),
        Dia::Core::StringCRC("entity.hierarchy_valid"),
        Dia::Core::StringCRC("entity.destroy_cascade"),
        Dia::Core::StringCRC("entity.mailbox_received"),
        Dia::Core::StringCRC("entity.lifecycle_complete")
    };
    TestResultsRegistry::GetInstance().SetRunning(
        Dia::Core::StringCRC("EntityTestStage"), kBudgetFrames, checkpoints, 6);

    return StartResult::kReady;
}
```

### T-04 — DoUpdate logic

```cpp
void EntityTestModule::DoUpdate(float /*deltaTime*/)
{
    ++mFrameCount;
    TestResultsRegistry::GetInstance().SetActiveFrameCount(mFrameCount);

    auto& domain = mEntityModule.Get()->GetDomain();

    // Frame 0: destroy doomed entity
    if (mFrameCount == 1 && !mDoomedDestroyed)
    {
        domain.QueueDestroy(mDoomedEntity);
        mDoomedDestroyed = true;
    }

    // Broadcast mailbox message every 10 frames
    if (mFrameCount % 10 == 0)
    {
        domain.GetMailbox().Broadcast(Dia::Core::StringCRC("entity.ping"), Json::Value("ping"));
        ++mMailboxMessagesSent;
    }

    // Check mailbox for received messages (via EntityRouter delivery)
    // Update mMailboxMessagesReceived counter

    // Submit visuals (circles + hierarchy lines)
    SubmitVisuals();

    // Check completion: all 6 pass within budget
    if (AllCheckpointsPassed() && !mCompleted)
    {
        mCompleted = true;
        TestResultsRegistry::GetInstance().SetPassed(
            Dia::Core::StringCRC("EntityTestStage"), mFrameCount);
    }
    else if (mFrameCount >= kBudgetFrames && !mCompleted)
    {
        mCompleted = true;
        TestResultsRegistry::GetInstance().SetTimeout(
            Dia::Core::StringCRC("EntityTestStage"));
    }
}
```

### T-05 — Visual rendering (SubmitVisuals)

```cpp
void EntityTestModule::SubmitVisuals()
{
#ifdef DIA_DEBUG
    if (!mEntityDrawer)
    {
        if (auto* mgr = VisualDebuggerModule::GetStaticLayerManager())
        {
            mEntityDrawer = std::make_unique<EntityTestDrawer>(*mgr);
            mgr->Register(mEntityDrawer.get(), 20);
        }
    }
    if (mEntityDrawer)
    {
        mEntityDrawer->Clear();
        // Hierarchy group — blue circles (#4FC3F7), dashed lines parent→child
        // Query targets — green circles (#66BB6A)
        // Doomed — faded grey circle with red X (only before destroy)
        mEntityDrawer->Submit();
    }
#endif
}
```

Alternatively: inline drawing into the layer manager directly each frame (simpler, no separate class needed — just submit primitives to the layer each DoUpdate).

### T-06 — Debug Console Entity Tab

Add to `VisualDebuggerConsoleModule` (which already renders tabs for RigidBody2D, Animation2D, etc.):
- New `"Entity"` tab showing 4 collapsible sections:
  1. **Domain Stats** — Spawned/Alive/Destroyed/Frame + component type breakdown with coloured dots
  2. **Query Results** — live Query<Transform,Visual>, Query<Parent>, Query<ChildBuffer> counts
  3. **Mailbox** — message key, direction, delivery count
  4. **Lifecycle Counters** — OnAttach/OnDetach totals with explanatory sub-text

Data source: EntityTestModule exposes public getters for its counters. VisualDebuggerConsoleModule gets a ModuleRef<EntityTestModule> or reads via a shared struct.

---

## Task Table

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| T-00 | *(Mockup)* `entity-test-stage.mockup.html` | Visual sign-off | Done | sonnet | Approved 2026-05-27 |
| T-01 | Add `GetDomain()` + `IsReady()` + `mReady` to `EntityModule.h/.cpp`; set `mReady = true` in DoStart, `mReady = false` in DoStop | Build passes; existing EntityModule usage unaffected | Todo | haiku | Prerequisite for T-04 |
| T-02 | Create `TransformComponent.h/.cpp` + `VisualTestRenderComponent.h/.cpp` under `Cluiche/CluicheTest/Modules/TestStages/Entity/` — `DIA_COMPONENT` + `FIELD` + `DIA_UPDATABLE` (Transform only); `DIA_SERIALIZE` + `DIA_COMPONENT_REGISTER` in `.cpp`; static counter pointer injection for OnAttach/OnDetach | Build passes; add to vcxproj in T-03 | Todo | sonnet | VisualTestRenderComponent: radius, label, colour fields. TransformComponent: x, y fields |
| T-03 | Run `/new-cluichetest-stage EntityTest` — scaffolds all 8 touch points (diastage, diaapp, module skeleton, vcxproj+filters, cluiche_main.diaapp, diagame, assets.catalogue.json, pipeline.toml). Then: (a) replace generated skeleton with EntityTestModule pointing at EntityModule; (b) add EntityModule to the `.diaapp` MainPU modules with dependency; (c) add T-02 component files to vcxproj; (d) ensure VisualDebuggerModule on SimPU writes SimToRender; (e) force-copy cluiche_main.diaapp to bin | `dia pipeline --target cluichetest` passes; EntityTestStage in Boot menu | Todo | sonnet | Collapses old T-04/T-05/T-06/T-07 into one task. Skill handles: diastage, diaapp, vcxproj, cluiche_main, diagame, catalogue, pipeline.toml |
| T-04 | Implement `EntityTestModule.h/.cpp` — DoStart guard/poll pattern, RegisterPool for both component types, create 9 entities (4 hierarchy + 4 query + 1 doomed), QueueAddComponent + EndOfFrame, register all 6 checkpoints, TestResultsRegistry::SetRunning with 6 checkpoint names; DoUpdate: frame-0 QueueDestroy, broadcast every 10 frames, mailbox check, SubmitVisuals, SetActiveFrameCount, completion check; DoStop: UnregisterCheckpoints + reset | `dia run cluichetest` — stage loads, 6 checkpoints register, frame-0 destroy applies | Todo | sonnet | Depends T-01, T-02, T-03 |
| T-05 | Implement visual rendering — circles colour-coded (blue=#4FC3F7 hierarchy, green=#66BB6A query, grey/red doomed), dashed hierarchy lines parent→children, entity name labels, doomed ghost disappears after frame 0. Legend overlay matching mockup | Visual matches mockup when stage runs | Todo | sonnet | Use VisualDebuggerModule::GetStaticLayerManager() pattern from RigidBody2DTestModule |
| T-06 | Implement Debug Console Entity tab — add "Entity" tab to VisualDebuggerConsoleModule with 4 collapsible sections: Domain Stats (spawned/alive/destroyed/frame + component breakdown), Query Results (3 queries), Mailbox (key/direction/count), Lifecycle Counters (attach/detach with explanatory text). Data from EntityTestModule public getters | Tab renders matching mockup layout when stage active | Todo | sonnet | Requires EntityTestModule to expose counters publicly; tab only active during EntityTestStage |
| T-07 | Write `Tools/orchestrator/scenarios/cluichetest/entity_test_stage/smoke.py` — navigate_to EntityTestStage, wait_for_checkpoint all 6, assert metrics (entity.spawn_count, entity.alive_count, entity.destroy_count), navigate_to Boot | Scenario file exists and is syntactically valid | Todo | sonnet | Mailbox checkpoint passes automatically (broadcast every 10 frames) |
| T-08 | `dia run cluichetest` — visual verify: circles + labels render, hierarchy lines connect parent to children, doomed entity disappears after frame 0, checkpoint panel shows 6/6 PASS, HUD shows stage name + frame counter, Debug Console Entity tab shows correct values | All 6 checkpoints PASS within budget frames | Todo | sonnet | Compare against mockup |
| T-09 | Update spec status → Done; commit | Spec status = Done | Todo | haiku | |

---

## Dependencies

```
T-01 (EntityModule additions) → unblocks T-04
T-02 (components)             → unblocks T-03 (vcxproj entries), T-04
T-03 (scaffold + wiring)      → unblocks T-04, T-05, T-06
T-04 (EntityTestModule impl)  → unblocks T-05, T-06, T-07, T-08
T-05 (visuals)                → unblocks T-08
T-06 (debug console tab)      → unblocks T-08
T-07 (pytest scenario)        → unblocks T-09

T-01 and T-02 can run in parallel.
T-03 depends on T-02 (needs files to add to vcxproj).
T-05, T-06, T-07 can run in parallel after T-04.
T-08 requires T-04, T-05, T-06 all complete.
T-09 requires T-07 and T-08 complete.
```

---

## Files Touched

| File | Change | Task |
|------|--------|------|
| `Cluiche/CluicheTest/Modules/EntityModule.h` | Add `GetDomain()`, `IsReady()`, `mReady` | T-01 |
| `Cluiche/CluicheTest/Modules/EntityModule.cpp` | Set `mReady` in DoStart/DoStop | T-01 |
| `Cluiche/CluicheTest/Modules/TestStages/Entity/TransformComponent.h/.cpp` | New test component | T-02 |
| `Cluiche/CluicheTest/Modules/TestStages/Entity/VisualTestRenderComponent.h/.cpp` | New test component | T-02 |
| `Cluiche/Assets/Stages/EntityTest/entity_test_stage.diastage` | New stage pointer | T-03 |
| `Cluiche/Assets/Stages/EntityTest/misc/ApplicationFlow/entity_test_stage.diaapp` | Module wiring (MainPU: EntityModule + EntityTestModule; SimPU: VisualDebuggerModule) | T-03 |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` + `.vcxproj.filters` | Add all new .h/.cpp files | T-03 |
| `Cluiche/Assets/CluicheTest/Global/Misc/ApplicationFlow/cluiche_main.diaapp` | Add stage to stages[]; Boot transitions; TestStageHUDModule + VisualDebuggerConsoleModule stages | T-03 |
| `Cluiche/Assets/CluicheTest/cluichetest.diagame` | Add stage import | T-03 |
| `Cluiche/Assets/CluicheTest/assets.catalogue.json` | Add stage + manifest entries | T-03 |
| `pipeline.toml` | Add asset_stages entry + deploy files | T-03 |
| `Cluiche/bin/CluicheTest/Debug/x64/assets/global/misc/ApplicationFlow/cluiche_main.diaapp` | Force-copy after edit | T-03 |
| `Cluiche/CluicheTest/Modules/TestStages/EntityTestModule.h/.cpp` | Full test module implementation | T-04 |
| `Cluiche/CluicheTest/Modules/TestStages/EntityTestModule.h` | Visual layer registration + SubmitVisuals | T-05 |
| `Cluiche/CluicheTest/Modules/TestStages/EntityTestModule.cpp` | Colour-coded circles, dashed lines, legend | T-05 |
| VisualDebuggerConsoleModule (header + cpp) | Add Entity tab with 4 collapsible sections | T-06 |
| `Tools/orchestrator/scenarios/cluichetest/entity_test_stage/smoke.py` | New E2E scenario | T-07 |
| `docs/specs/features/cluichetest/teststages/entity-test-stage.md` | Status → Done | T-09 |

---

## Audit Corrections Applied

| # | Issue | Fix |
|---|-------|-----|
| 1 | Plan said SimPU for EntityTestModule | Fixed: MainPU — AutomationModule ModuleRef cannot resolve cross-PU |
| 2 | Missing TestResultsRegistry integration | Added: SetRunning(6 checkpoints) in DoStart, SetActiveFrameCount each DoUpdate, SetPassed/SetTimeout on completion |
| 3 | Missing pipeline.toml | Added to T-03 via `/new-cluichetest-stage` skill |
| 4 | Missing force-copy of cluiche_main.diaapp to bin | Added to T-03 |
| 5 | Missing VisualDebuggerModule on SimPU (SimToRender writer) | Added to T-03 `.diaapp` wiring |
| 6 | Missing Debug Console Entity tab | Added as dedicated T-06 |
| 7 | 4 separate manifest/catalogue/vcxproj tasks | Collapsed into single T-03 using `/new-cluichetest-stage` skill |
| 8 | Mockup visual details (colour coding, dashed lines, doomed ghost, legend) | Explicit in T-05 description |
| 9 | Old plan referenced `entity_test_stage.stage.json` | Removed — skill doesn't produce this; not needed |
| 10 | No completion/timeout logic | Added to T-04 DoUpdate pattern (SetPassed/SetTimeout) |
