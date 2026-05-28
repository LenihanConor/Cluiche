# Feature Spec: EntityTestStage

## Parent System
@docs/specs/systems/cluichetest/teststages.md

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-004, PD-006, PD-007, PD-009, PD-010 |
| Application | @docs/specs/applications/cluichetest.md | AD-001, AD-002, AD-003, AD-005 |
| System | @docs/specs/systems/cluichetest/teststages.md | SD-TS-001, SD-TS-002, SD-TS-003, SD-TS-004, SD-TS-005 |
| Sibling | @docs/specs/features/cluichetest/teststages/test-stage-infrastructure.md | Inherits Shared ACs 1–9; depends on Boot menu + manifest loader |
| Depends on | @docs/specs/systems/dia/diaentity.md | SD-ENT-001 through SD-ENT-020 |
| Depends on | @docs/specs/features/cluichetest/applicationflow/entity-module.md | EntityModule wiring pattern |

---

## Problem Statement

DiaEntity is fully implemented (94 unit tests pass) but has never been exercised under real PU timing in a stage lifecycle. No E2E path exists to validate: entity spawn/destroy across `EndOfFrame` boundaries, hierarchy cascade under real sim timing, query cache correctness after mutations, or mailbox message delivery across frames. This stage provides that validation contract.

---

## Template Answers

| # | Question | Answer |
|---|----------|--------|
| T1 | Engine feature under test | `DiaEntity` — `Domain`, component lifecycle (`OnAttach`/`OnDetach`), hierarchy (`ParentComponent`/`ChildBufferComponent`), query system, mailbox routing (`EntityRouter`), `DoUpdate` loop |
| T2 | Scene layout | Three entity groups: (1) a hierarchy tree (1 parent + 3 children, all with `TransformComponent` + `VisualTestRenderComponent`); (2) 4 standalone query-target entities with both components; (3) 1 "doomed" entity destroyed in `DoUpdate` on frame 1 to validate destroy+cascade. Total: 9 entities at DoStart, 8 after first EndOfFrame. |
| T3 | Acceptance criteria | Shared ACs 1–9 + stage-specific ACs AC-E1 through AC-E7 (see below) |
| T4 | Metrics | `entity.spawn_count` (total alive after DoStart), `entity.query_result_count` (entities matching full query), `entity.mailbox_messages_received` (running total of mailbox messages delivered to entities) |
| T5 | PU assignment | `EntityTestModule` on SimPU (entity simulation runs there; consistent with other test stage modules) |
| T6 | Assets | `entity_test_stage.blueprint` JSON defining 9 entities (hierarchy group, query targets, doomed entity) loaded via `EntityModule`. `entity_test_stage.diastage` + `entity_test_stage.diaapp` manifest files. |
| T7 | Unit test gap | Unit tests call `EndOfFrame()` manually in isolation. This stage validates: real TimeServer dt through `Domain::Update(dt)`; query cache invalidation after live destroy; mailbox delivery sequencing across frames under scheduler jitter. |
| T8 | Determinism | Fully deterministic — no randomness, fixed entity layout, destroy triggered at frame 1 unconditionally |
| T9 | Frame budget | Static after frame 1; negligible CPU cost per frame |
| T10 | Dependencies | `EntityModule` (owns Domain + blueprint load), `AutomationModule` (checkpoints + metrics), `DiaGeometry2DVisualDebugger::ShapeDrawer` (render circles) |

---

## Acceptance Criteria

### Shared ACs (inherited from test-stage-infrastructure.md)

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-S1 | Stage has a manifest entry with `transitions: ["Boot"]` | Manifest review |
| AC-S2 | Stage module registers at least one checkpoint in DoStart via AutomationService | Code review + checkpoint trigger returns success |
| AC-S3 | Stage module emits at least one metric to MetricRegistry | Metric query from orchestrator returns value |
| AC-S4 | Stage module logs at DoStart entry, DoStop entry (`DIA_LOG_INFO`) | Session log review |
| AC-S5 | Stage module returns `StartResult::kReady` only after scene setup is complete | Code review |
| AC-S6 | Stage module cleans up all resources in DoStop, returns `StopResult::kDone` | No leaks; Domain destroyed |
| AC-S7 | Navigating Boot → Stage → Boot → Stage produces identical checkpoint results | Orchestrator scenario runs twice in sequence |
| AC-S8 | Checkpoint names follow convention: `entity.<checkpoint_name>` | Code review |
| AC-S9 | Stage has a matching pytest scenario file | File exists at `Tools/orchestrator/scenarios/cluichetest/entity_test_stage/smoke.py` |

### Stage-Specific ACs

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-E1 | `entity.spawn_complete` checkpoint passes: all 9 entities alive after DoStart + initial `EndOfFrame` | Orchestrator polls checkpoint → `passed: true` within 5 frames |
| AC-E2 | `entity.query_correct` checkpoint passes: `Query<TransformComponent, VisualTestRenderComponent>()` returns exactly 8 entities after the doomed entity is destroyed (frame 1) | Checkpoint triggered after frame 2 minimum |
| AC-E3 | `entity.hierarchy_valid` checkpoint passes: parent entity has 3 children in `ChildBufferComponent`; each child's `ParentComponent` points to the parent | Code + runtime validation in checkpoint callback |
| AC-E4 | `entity.destroy_cascade` checkpoint passes: destroying the doomed entity (which has 0 children) on frame 1 leaves it `!IsAlive()` after `EndOfFrame` | Checkpoint callable after frame 1 |
| AC-E5 | `entity.mailbox_received` checkpoint passes: a `StringCRC`-keyed message broadcast via `MakeAllAddress()` is received by at least one `TransformComponent::DoUpdate` subscriber within 2 frames | Orchestrator triggers broadcast command via DiaAPI, then polls checkpoint |
| AC-E6 | `entity.lifecycle_complete` checkpoint passes: `OnAttach` fired for `TransformComponent` and `VisualTestRenderComponent` on all 9 entities; `OnDetach` fired on the doomed entity's components after destroy | Attachment counter tracked in a test-local counter; checkpoint verifies expected totals |
| AC-E7 | Circles and name labels render at correct world-space positions for all alive entities each frame | Visual inspection against mockup |

---

## Design

**Visual mockup:** [@docs/specs/features/cluichetest/teststages/entity-test-stage.mockup.html](entity-test-stage.mockup.html) — open in a browser for visual acceptance gate reference.

### Scene Layout

```
+----------------------------------------------+
|  [Debug Console - top-left ImGui panel]       |
|  Title: "Debug Console"                       |
|  Domain tabs: Entity* | RigidBody2D | ...      |
|    ▼ Domain Stats                             |
|      Entities:8  Alive:8  Destroyed:1         |
|      Components: TransformComponent:8         |
|                  VisualTestRenderComponent:8  |
|    ▼ Query Results                            |
|      Query<Transform,Visual>: 8 results       |
|    ▼ Mailbox                                  |
|      Messages received: 8                     |
|      Last: entity.ping → all                  |
|    ▼ Lifecycle Counters                       |
|      OnAttach fired: 18 (9×2 components)      |
|      OnDetach fired: 2 (1×2 on doomed entity) |
|  > [command input]                            |
|  Output | Warnings                            |
|                                               |
|  [Checkpoints panel - right of console]       |
|  ✓ entity.spawn_complete       PASS           |
|  ✓ entity.query_correct        PASS           |
|  ✓ entity.hierarchy_valid      PASS           |
|  ✓ entity.destroy_cascade      PASS           |
|  ○ entity.mailbox_received     PENDING        |
|  ✓ entity.lifecycle_complete   PASS           |
|                                               |
|  WORLD SPACE                                  |
|                                               |
|  — Hierarchy Group (left) ————————————————   |
|  [Parent]  ●  "Parent"                        |
|     ├─  ●  "Child A"                          |
|     ├─  ●  "Child B"                          |
|     └─  ●  "Child C"                          |
|  (lines drawn between parent and children)    |
|                                               |
|  — Query Targets (centre) ————————————————   |
|  ● "Query0"  ● "Query1"                       |
|  ● "Query2"  ● "Query3"                       |
|                                               |
|  — Doomed (right, greyed-out after frame 1)—  |
|  [✕]  "Doomed" (shown briefly then fades)     |
|                                               |
|  ——————————————————————————————————————————  |
|  HUD: entity_test_stage | ✓ 5/6 | 042   PASS ✕|
+----------------------------------------------+
```

- **Debug Console** (ImGui, top-left, ~310px wide): domain tabs (Entity active), four collapsible sections — Domain Stats, Query Results, Mailbox, Lifecycle Counters.
- **Checkpoints panel** (ImGui, right of console, ~220px wide): 6 checkpoint rows with name + PASS/PENDING/FAIL badge.
- **World space**: three groups — hierarchy tree (left), query targets (centre), doomed entity slot (right, fades after frame 1).
- **HUD bar** (28px, bottom): stage name | checkpoint badge count | frame counter | PASS status | exit button.

### Component Types

Both components are CluicheTest-local (not engine Dia modules — AD-002 code-based, test-only).

```cpp
// CluicheTest/Modules/TestStages/Entity/TransformComponent.h
class TransformComponent : public Dia::Entity::IComponent {
public:
    DIA_COMPONENT(TransformComponent, "transform");
    FIELD(float, x, 0.0f);
    FIELD(float, y, 0.0f);
    DIA_UPDATABLE; // opts into DoUpdate for mailbox subscription
    void OnAttach(Dia::Entity::Domain& realm, Dia::Entity::Entity self) override;
    void OnDetach(Dia::Entity::Domain& realm, Dia::Entity::Entity self) override;
    void DoUpdate(Dia::Entity::Domain& realm, Dia::Entity::Entity self, float dt) override;
};

// CluicheTest/Modules/TestStages/Entity/VisualTestRenderComponent.h
class VisualTestRenderComponent : public Dia::Entity::IComponent {
public:
    DIA_COMPONENT(VisualTestRenderComponent, "visual-test-render");
    FIELD(float,            radius,  12.0f);
    FIELD(Dia::Core::StringCRC, label, StringCRC{""});
    void OnAttach(Dia::Entity::Domain& realm, Dia::Entity::Entity self) override;
    void OnDetach(Dia::Entity::Domain& realm, Dia::Entity::Entity self) override;
};
```

Both components live in `CluicheTest/Modules/TestStages/Entity/` — not in any Dia module (escalation rule: test-only data, no generic engine value).

### Module Structure

The stage uses two modules on SimPU. `EntityModule` is the reusable adapter (owned by the system); `EntityTestModule` is the test-only layer that sits on top.

```
SimPU
  ├─ EntityModule          ← reusable; owns Domain + blueprint load + Update/EndOfFrame
  └─ EntityTestModule      ← test-only; ModuleRef<EntityModule>; registers checkpoints, drives destroy/mailbox, renders visuals
```

```cpp
// CluicheTest/Modules/TestStages/EntityTestModule.h
namespace CluicheTest {

class EntityTestModule : public Dia::ApplicationFlow::Module
{
public:
    static constexpr Dia::Core::StringCRC kTypeId{"EntityTestModule"};
    explicit EntityTestModule(const Dia::Core::StringCRC& instanceId);

protected:
    StartResult DoStart() override;   // waits for EntityModule kReady, then registers checkpoints
    void        DoUpdate(float deltaTime) override;
    StopResult  DoStop() override;

private:
    void RegisterCheckpoints(Dia::Automation::AutomationService& automation);
    void RegisterMetrics(Dia::Automation::AutomationService& automation);
    void SubmitVisuals(Dia::Geometry2DVisualDebugger::ShapeDrawer& drawer);

    // Resolve named entities from the Domain after blueprint load
    void ResolveEntityHandles(Dia::Entity::Domain& domain);

    Dia::ApplicationFlow::ModuleRef<EntityModule>            mEntityModule{this};
    Dia::ApplicationFlow::ModuleRef<AutomationModule>        mAutomation{this};
    Dia::ApplicationFlow::ModuleRef<Geometry2DDebugModule>   mShapeDrawer{this};

    // Entity handles resolved from Domain after blueprint loads
    Dia::Entity::Entity  mParentEntity;
    Dia::Entity::Entity  mChildEntities[3];
    Dia::Entity::Entity  mQueryEntities[4];
    Dia::Entity::Entity  mDoomedEntity;

    int   mFrameCount              = 0;
    int   mOnAttachCount           = 0;
    int   mOnDetachCount           = 0;
    int   mMailboxMessagesReceived = 0;
    bool  mDoomedDestroyed         = false;
    bool  mHandlesResolved         = false;
};

} // namespace CluicheTest
DIA_MODULE(EntityTestModule);
```

`EntityTestModule::DoStart` polls `EntityModule` readiness. Once the blueprint is loaded and `EntityModule` returns `kReady`, it calls `ResolveEntityHandles` (queries Domain by debug name) and then registers checkpoints. Until then it returns `kLoading`.

### Checkpoint Logic

```cpp
StartResult EntityTestModule::DoStart()
{
    // Poll EntityModule until blueprint is loaded
    if (!mEntityModule->IsReady())
        return StartResult::kLoading;

    if (!mHandlesResolved)
    {
        ResolveEntityHandles(mEntityModule->GetDomain());
        RegisterCheckpoints(...);
        RegisterMetrics(...);
        mHandlesResolved = true;
    }
    return StartResult::kReady;
}

// Checkpoint callbacks (registered in DoStart):

// entity.spawn_complete — passes once all 9 entities are alive
[this]() { return mDomain.GetEntityCount() == 9; }

// entity.query_correct — passes once doomed entity gone, query returns 8
[this]() {
    auto view = mDomain.Query<TransformComponent, VisualTestRenderComponent>();
    return mDoomedDestroyed && view.Count() == 8;
}

// entity.hierarchy_valid — checks parent/child linkage
[this]() {
    auto* cb = mDomain.GetComponent<ChildBufferComponent>(mParentEntity);
    if (!cb || cb->children.Size() != 3) return false;
    for (auto child : mChildEntities) {
        auto* p = mDomain.GetComponent<ParentComponent>(child);
        if (!p || p->value.entity != mParentEntity) return false;
    }
    return true;
}

// entity.destroy_cascade — passes once doomed entity is gone
[this]() { return mDoomedDestroyed && !mDomain.IsAlive(mDoomedEntity); }

// entity.mailbox_received — passes once at least one message received
[this]() { return mMailboxMessagesReceived > 0; }

// entity.lifecycle_complete — 18 attaches (9 entities × 2 components), 2 detaches (doomed × 2)
[this]() { return mOnAttachCount == 18 && mOnDetachCount == 2; }
```

### Update Loop

`EntityModule::DoUpdate` drives `Domain::Update(dt)` and `Domain::EndOfFrame()` — `EntityTestModule` does not call these directly.

```cpp
void EntityTestModule::DoUpdate(float deltaTime)
{
    auto& domain = mEntityModule->GetDomain();

    // Frame 1: destroy the doomed entity (queued; applied by EntityModule's EndOfFrame)
    if (mFrameCount == 0 && !mDoomedDestroyed) {
        domain.QueueDestroy(mDoomedEntity);
        mDoomedDestroyed = true;
    }

    // Every 10 frames: send a mailbox broadcast to all entities
    if (mFrameCount % 10 == 0) {
        domain.GetMailbox().Send(
            Dia::Entity::MakeAllAddress(), StringCRC("entity.ping"), {});
    }

    ++mFrameCount;

    // Submit circles + labels for all alive entities
    SubmitVisuals(domain);

    // Update metrics
    auto& automation = mAutomation->GetService();
    automation.EmitMetric(StringCRC("entity.spawn_count"),
        static_cast<float>(domain.GetEntityCount()));
    automation.EmitMetric(StringCRC("entity.query_result_count"),
        static_cast<float>(
            domain.Query<TransformComponent, VisualTestRenderComponent>().Count()));
    automation.EmitMetric(StringCRC("entity.mailbox_messages_received"),
        static_cast<float>(mMailboxMessagesReceived));
}
```

### Visual Rendering

`SubmitVisuals` queries all alive `TransformComponent` + `VisualTestRenderComponent` pairs and submits one circle to `ShapeDrawer` per entity. Hierarchy lines are drawn as `Line` primitives from parent position to each child position. Labels are submitted via ImGui background drawlist at projected screen coordinates.

### Manifest Entry

`entity_test_stage.diastage`:
```json
{
  "name": "entity_test_stage",
  "manifest": "Assets/CluicheTest/Stages/entity_test_stage.diaapp",
  "transitions": ["Boot"]
}
```

`entity_test_stage.diaapp`:
```json
{
  "modules": [
    { "type": "EntityModule",     "pu": "SimPU", "config": { "blueprint_asset_id": "stages/entity_test/entity_test_stage.blueprint" } },
    { "type": "EntityTestModule", "pu": "SimPU", "dependencies": ["EntityModule"] }
  ]
}
```

### Pytest Scenario

```python
# Tools/orchestrator/scenarios/cluichetest/entity_test_stage/smoke.py
def test_entity_test_stage_loads(cluichetest):
    cluichetest.navigate_to("entity_test_stage")

    cluichetest.wait_for_checkpoint("entity.spawn_complete", timeout_frames=10)
    cluichetest.wait_for_checkpoint("entity.hierarchy_valid", timeout_frames=10)
    cluichetest.wait_for_checkpoint("entity.destroy_cascade", timeout_frames=10)
    cluichetest.wait_for_checkpoint("entity.query_correct",   timeout_frames=10)
    cluichetest.wait_for_checkpoint("entity.lifecycle_complete", timeout_frames=10)

    # Trigger mailbox broadcast via DiaAPI, then validate
    cluichetest.send_command("entity.broadcast_ping")
    cluichetest.wait_for_checkpoint("entity.mailbox_received", timeout_frames=10)

    spawn_count = cluichetest.get_metric("entity.spawn_count")
    assert spawn_count == 8  # 9 spawned, 1 destroyed

    query_count = cluichetest.get_metric("entity.query_result_count")
    assert query_count == 8

    cluichetest.navigate_to("Boot")
```

---

## Files Touched

| File | Change |
|------|--------|
| `docs/specs/features/cluichetest/teststages/entity-test-stage.md` | This spec |
| `docs/specs/features/cluichetest/teststages/entity-test-stage.mockup.html` | Visual acceptance gate (Task 1) |
| `Cluiche/CluicheTest/Modules/TestStages/Entity/TransformComponent.h/.cpp` | New test-local component |
| `Cluiche/CluicheTest/Modules/TestStages/Entity/VisualTestRenderComponent.h/.cpp` | New test-local component |
| `Cluiche/CluicheTest/Modules/EntityTestModule.h/.cpp` | New test module (thin layer over EntityModule) |
| `Cluiche/CluicheTest/Modules/EntityModule.h/.cpp` | New reusable module (if not already implemented from entity-module.md) |
| `Cluiche/Assets/CluicheTest/Stages/entity_test_stage.blueprint` | Blueprint JSON defining 9 entities |
| `Cluiche/Assets/CluicheTest/Stages/entity_test_stage.diastage` | New stage manifest metadata |
| `Cluiche/Assets/CluicheTest/Stages/entity_test_stage.diaapp` | New stage module wiring (EntityModule + EntityTestModule) |
| `Cluiche/Assets/CluicheTest/assets.catalogue.json` | Register new stage + blueprint asset |
| `Cluiche/Assets/CluicheTest/cluichetest.diagame` | Import new stage |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` | Add new source files |
| `Tools/orchestrator/scenarios/cluichetest/entity_test_stage/smoke.py` | E2E scenario |

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `entity-test-stage.mockup.html` — hierarchy tree, query targets, doomed entity, debug console, checkpoints panel, HUD | Visual sign-off before C++ | Done | sonnet | Approved 2026-05-27 |
| 2 | Verify `EntityModule` is implemented (entity-module.md); if not, implement it — `DoStart` polls blueprint asset → kLoading/kReady, `DoUpdate` drives `Domain::Update` + `EndOfFrame`, `DoStop` destroys Domain, `GetDomain()` + `IsReady()` accessors | Build passes; blueprint loads; `GetDomain()` returns live Domain | Todo | sonnet | Prerequisite for Task 3; check entity-module.md status first |
| 3 | Create `TransformComponent.h/.cpp` + `VisualTestRenderComponent.h/.cpp` with `DIA_COMPONENT` + `FIELD` macros; `DIA_COMPONENT_REGISTER` in `.cpp`; `OnAttach`/`OnDetach` increment module-provided counters | GoogleTest: attach/detach counter round-trip | Todo | sonnet | Test-local in `CluicheTest/Modules/TestStages/Entity/`; depends on Task 2 |
| 4 | Create `entity_test_stage.blueprint` JSON — 9 entities with debug names, `TransformComponent` + `VisualTestRenderComponent` fields, hierarchy refs (Parent/ChildBuffer) | Blueprint loads via `JsonBlueprintLoader` without error; 9 entities alive | Todo | sonnet | Data-driven scene definition |
| 5 | Create `EntityTestModule.h/.cpp` — `DoStart` polls `EntityModule` readiness + `ResolveEntityHandles` + registers all 6 checkpoints; `DoUpdate` destroy-on-frame-1, mailbox broadcast, `SubmitVisuals`; metrics | Stage loads + all 6 checkpoints fire | Todo | sonnet | Depends on Tasks 2, 3, 4 |
| 6 | Add `entity_test_stage.diastage` + `entity_test_stage.diaapp` (two modules: EntityModule + EntityTestModule) | Manifest validation passes | Todo | haiku | |
| 7 | Register stage + blueprint in `assets.catalogue.json`; import in `cluichetest.diagame` | Stage visible in Boot menu | Todo | haiku | |
| 8 | Update `CluicheTest.vcxproj` (add all new source files) | Clean build | Todo | haiku | |
| 9 | Write pytest scenario `entity_test_stage/smoke.py` | Orchestrator scenario passes | Todo | sonnet | Mailbox checkpoint triggered by automatic broadcast (no DiaAPI command needed) |
| 10 | `dia run cluichetest` — visual verify against mockup; circles, labels, hierarchy lines, checkpoint panel | Manual visual gate | Todo | sonnet | |
| 11 | Commit + update spec status → Done | — | Todo | haiku | |

---

## Dependencies

- Tasks 1 and 2 can run in parallel.
- Tasks 3 and 4 can run in parallel after Task 2 (both depend on EntityModule being available).
- Task 5 depends on Tasks 3 and 4.
- Tasks 6, 7, 8 can run in parallel after Task 5.
- Task 9 can run in parallel with Tasks 6–8.
- Task 10 requires Tasks 5–8 complete (full build + visual run).
- Task 11 requires Tasks 9 and 10 complete.

---

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | `kTypeId`, component type names (`"transform"`, `"visual-test-render"`), checkpoint names (`"entity.spawn_complete"` etc.), metric names, mailbox message key — all `StringCRC`. |
| PD-004 | No STL containers in public APIs | Module and component interfaces use `DIA_COMPONENT`/`FIELD` macros, `Dia::Core::DynamicArrayC`. No STL in any signature. |
| PD-006 | VS project files source of truth | Task 6 adds all new source files to `CluicheTest.vcxproj`. |
| PD-007 | C++20 required | `constexpr StringCRC`, `DIA_COMPONENT` macro expansion, `if constexpr` in reflection. |
| PD-009 | Generated output under `Cluiche/out/` | Session logs and metrics in `Cluiche/out/CluicheTest/sessions/`. |
| PD-010 | `.diagame` root; `.diastage` for stage metadata | `entity_test_stage.diastage` added as a typed stage import in `cluichetest.diagame`. |
| AD-001 | Three PUs | `EntityTestModule` on SimPU. No structural changes to PU topology. |
| AD-002 | Levels are code-based | Scene setup is pure C++; no data-driven level loading. |
| AD-003 | Entry point in `Main.cpp` | No change; stage hooks into existing app lifecycle via manifest. |
| AD-005 | App is testbed, not shipped product | Stage exists purely for engine validation. |
| SD-TS-001 | One manifest stage per feature | Exactly one stage: `entity_test_stage`. |
| SD-TS-002 | Checkpoints in DoStart, auto-clear on stop | All 6 checkpoints registered in `DoStart`; auto-clear via module ownership. |
| SD-TS-003 | Metrics for threshold assertions | `entity.spawn_count`, `entity.query_result_count`, `entity.mailbox_messages_received` emitted each frame. |
| SD-TS-004 | All stages return to Boot | `transitions: ["Boot"]` in `entity_test_stage.diastage`. |
| SD-TS-005 | Individual stages are feature specs | This spec IS the feature spec for this stage. |
| SD-ENT-002 | Systems own their data; components are typed adapters | `EntityTestModule` owns the `Domain`. `TransformComponent` and `VisualTestRenderComponent` are typed config + lifecycle adapters; rendering is driven by the module via `SubmitVisuals`. |
| SD-ENT-012 | Structural changes queued at EndOfFrame | Destroy on frame 1 uses `QueueDestroy`; applied at next `EndOfFrame`. |
| SD-ENT-017 | Domain is non-copyable, non-movable | `mDomain` is a direct member; never copied or moved. |
| SD-ENT-018 | Single-threaded per realm | `Domain` accessed only from SimPU thread. |
| SD-ENT-019 | Namespace `Dia::Entity::` | All DiaEntity types used via `Dia::Entity::` namespace. |

---

## Open Questions

None.

---

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| Q1 | Design — EntityModule vs inline Domain | Should `EntityTestModule` use the existing `EntityModule` adapter spec (entity-module.md) as a base, or own its `Domain` directly as a member? | **Use `EntityModule` via `ModuleRef`.** `EntityModule` owns Domain + blueprint load + Update/EndOfFrame. `EntityTestModule` is a thin test layer that reads the Domain via `GetDomain()`. Keeps reusable system code separate from test-only validation logic. |
| Q2 | Design — mailbox broadcast trigger | Should the orchestrator trigger the broadcast via a DiaAPI command, or should the module broadcast automatically? | **Automatic broadcast** every 10 frames from `DoUpdate` — no new DiaAPI command needed. Orchestrator polls `entity.mailbox_received` checkpoint which passes once any message is received. |
| Q3 | Design — VisualTestRenderComponent escalation check | Is `VisualTestRenderComponent` test-only or generally useful enough for a Dia module? | **Test-only.** Lives in `CluicheTest/Modules/TestStages/Entity/`. A generic `DebugRenderComponent` would be a Dia-level decision; this is a throwaway test fixture. |
| Q4 | Design — hierarchy lines | Is `ShapeDrawer::Line` the right primitive for hierarchy relationships, or ImGui drawlist? | **`ShapeDrawer::Line`** — consistent with how other stages use ShapeDrawer for world-space geometry. |
| Q5 | Tasks — entity.broadcast_ping DiaAPI command | Is a `entity.broadcast_ping` DiaAPI command needed? | **No** — Q2 resolved as automatic broadcast. Orchestrator just polls `entity.mailbox_received`. |
| Q6 | Tasks — DiaEntity + EntityModule implementation status | Is `DiaEntity` fully built? Is `EntityModule` implemented? | DiaEntity plan is marked Done (T1–T15, T17). `entity-module.md` is `Approved` but implementation not confirmed. Task 2 covers this verification gate. |

---

## Status

`Approved` — 2026-05-27
**Plan:** [entity-test-stage.plan.md](entity-test-stage.plan.md)
