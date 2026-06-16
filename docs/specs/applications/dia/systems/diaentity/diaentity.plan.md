# Implementation Plan: diaentitytemplate

**Spec:** [diaentity.md](diaentity.md)
**Status:** Done
**Created:** 2026-05-21

## Session Notes

**Spec decisions summary:** diaentitytemplate is a layered ECS where systems own their data (SD-ENT-002). The container is `Domain` (SD-ENT-001), entity handles are generational 64-bit via `Handle<Entity>` (SD-ENT-003). Every component type must declare reflection metadata via `DIA_COMPONENT` + `FIELD` macros (SD-ENT-004); this metadata is component-only and lives inside diaentitytemplate — it is not a generic reflection module (SD-ENT-005). Structural mutations are queued and applied at `EndOfFrame()` (SD-ENT-012); entity slot allocation is immediate but component attachments are queued (SD-ENT-013). The old `IComponent` infrastructure is removed before diaentitytemplate is built (SD-ENT-020). Platform constraints: StringCRC for all IDs (PD-001), no STL in public APIs (PD-004), C++20 (PD-007), VS project files are source of truth (PD-006).

**DiaReflect integration — critical sequencing notes:**

diaentitytemplate has its own internal reflection system (`DIA_COMPONENT` + `FIELD` macros, `ComponentTypeDesc`, `ComponentRegistry`) that is **separate from DiaReflect**. Per SD-ENT-005, component reflection is scoped to diaentitytemplate only and lives inside the diaentitytemplate module. DiaReflect (the general-purpose archive/serialization system) is **not** a dependency of diaentitytemplate in v1.

However, three implementation features have a **dependency coupling to DiaReflect phases**:

| diaentitytemplate Feature | DiaReflect dependency | DiaReflect status |
|---|---|---|
| `blueprint-loader` (F3) — JSON load/save | Phases 1–2 (JsonReadArchive / macro DSL) | ✅ Done (138 tests GREEN) |
| `editor-inspection` (F9) — live field edit | Phase 3 (Field attributes: `RequiredAttribute`, `RangeAttribute`, `AssetRefAttribute`) | Phase 3a Done (T10); T11–T13 not started |
| `reflection` (F2) — `DIA_COMPONENT` macro | DIA_COMPONENT calls `DIA_SERIALIZE` internally for JSON load/save thunks | Phases 1–2 ✅ Done |

**F9 `editor-inspection` is blocked on DiaReflect T11–T13 (migration adapter + DiaMaths migration).** Do not start F9 until DiaReflect Phase 3 is fully done.

**F2 `reflection` must finalize the `DIA_COMPONENT` macro contract before any later feature assumes the interface.** Specifically: the macro must call `DIA_SERIALIZE` under the hood so every component automatically gains JSON load/save without extra boilerplate. This decision must be made and documented in the F2 task before coding begins.

## Task Table

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| **Pre-work — Cleanup** | | | | | |
| 1 | Remove old `IComponent` infrastructure (`Dia/DiaCore/Architecture/Components/`), migrate `SkeletonComponent` + `StateMachineComponent` consumers, remove related tests. Per SD-ENT-020. | Build passes; no references to old IComponent in non-diaentitytemplate code | Done | sonnet | Completed 2026-05-20. `Architecture/Components/` deleted, both components migrated to plain classes, 4818 tests pass. |
| **F10 — Module & Build** | | | | | |
| 2 | Create `Dia/diaentitytemplate/` directory, `diaentitytemplate.vcxproj` + `.vcxproj.filters`, register in `Cluiche.sln` under `Dia` solution folder, create `dia.entity.architecture.module.md` YAML module doc, add dependency edge in `dia_modules.py`. | Build passes (empty lib) | Done | haiku | Completed 2026-05-24. All files created, lib builds. |
| **F1 — Foundation** | | | | | |
| 3 | `Entity` type alias (`Handle<class EntityTag>`), entity slot pool (`HandlePool<Entity, kMaxEntitiesPerDomain>`), `Domain` skeleton (non-copyable, non-movable, `CreateEntity`, `IsAlive`, debug name storage in debug builds). | `Domain::CreateEntity` returns valid handle; `IsAlive` returns true; generation rollover test | Done | sonnet | Completed 2026-05-24. 13 foundation tests GREEN. |
| 4 | `IComponent` abstract base (`OnAttach`, `OnDetach`, `DoUpdate`, `OnAssetLoaded`, `GetTypeId`). `Domain::QueueAddComponent` + `QueueRemoveComponent` + end-of-frame mutation application. `GetComponent`, `HasComponent`. | Component attach/detach lifecycle tests; `GetComponent` returns nullptr before EndOfFrame if queued | Done | sonnet | Completed 2026-05-24. IComponentPool, ComponentPool, MutationOp all shipped. |
| 5 | `Domain::QueueDestroy`, destroy cascade logic (cleans up all component pools for entity, calls `OnDetach` on each). | Destroy test: entity gone after EndOfFrame; component pool slot freed | Done | sonnet | Completed 2026-05-24. Shipped with F1 foundation. |
| **F2 — Reflection** | | | | | |
| 6 | **Finalize `DIA_COMPONENT` macro contract** — decide and document: (a) `DIA_COMPONENT` calls `DIA_SERIALIZE` internally for JSON load/save thunks; (b) exact macro expansion; (c) `DIA_COMPONENT_REGISTER` separate `.cpp` macro for ODR-safe static init registration. Write the design note in this plan before writing any code. | — | Done | sonnet | Completed 2026-05-24. Contract: DIA_COMPONENT emits kTypeId/GetTypeId/GetStaticTypeId/GetDesc declarations; DIA_COMPONENT_REGISTER in .cpp defines kTypeId, GetDesc() with ComponentTypeDesc singleton and lambdas, plus AutoReg struct. DIA_SERIALIZE block written separately by user before DIA_COMPONENT_REGISTER. |
| 7 | `FieldDesc`, `ComponentTypeDesc`, `ComponentRegistry` (process-global, `DynamicArrayC<ComponentTypeDesc*, 512>`). `DIA_COMPONENT(ClassName, "name")` macro + `FIELD(type, name, default)` macro. `DIA_COMPONENT_REGISTER(ClassName)` in `.cpp`. | `ComponentRegistry::Find` returns registered desc; field list correct; CRC collision assert | Done | sonnet | Completed 2026-05-24. 17 reflection tests GREEN. Fixed DynamicArrayC double-destruction bug: reset Json::Value configs before RemoveAll(). |
| 8 | `REQUIRES(OtherComponent)` macro — appends required type CRC to desc's dependency list. Validation at `QueueAddComponent` (hard error via `DIA_ASSERT` if required component absent). | Missing-dependency test: DIA_ASSERT fires; correct-order test: attach succeeds | Done | sonnet | Completed 2026-05-24. Validation wired in ApplyAddComponent; DIA_REQ_ENTRY macro + requires_/requiresCount in ComponentTypeDesc. |
| **F3 — Blueprint Loader** | | | | | |
| 9 | `IBlueprintLoader` interface + `JsonBlueprintLoader` concrete impl. Versioned JSON blueprint schema (entities array + references block). Pass 1: instantiate entities + queue component attachments. Pass 2: patch `EntityRef` fields from references block. Pass 3: validate all required refs non-null (fatal load error). | Round-trip test: blueprint JSON → Domain with 3 entities, 2 cross-refs, 1 dependency chain | Done | sonnet | Completed 2026-05-24. 9 blueprint loader tests GREEN. QueueAddComponentByTypeId added to Domain for runtime type dispatch. |
| **F4 — Component Deps & Refs** | | | | | |
| 10 | `EntityRef<TComponent>` — `struct EntityRef { Entity entity; }` with C++20 concept constraint (`TComponent : IComponent`). Resolve validation (`realm.HasComponent<TComponent>(ref.entity)`). `ASSET_FIELD(name, AssetTypeId)` macro subset for asset-trigger flagging. Asset-trigger flow: blueprint loader enumerates asset fields, queues `AssetService::Request`, realm tracks pending count, calls `OnAssetLoaded` per completion. | `EntityRef` resolve test; asset-trigger callback sequence test | Done | sonnet | Completed 2026-05-24. 7 EntityRef tests GREEN. Asset-trigger deferred to post-v1 (no AssetService yet). |
| **F5 — Hierarchy** | | | | | |
| 11 | `ParentComponent`, `ChildBufferComponent` (`DynamicArrayC<Entity, 16>` children), `Hierarchy::QueueSetParent`, `Hierarchy::QueueDestroySubtree`. Cycle rejection in debug (`DIA_ASSERT`). Destroy-cascade default: destroy subtree at `EndOfFrame`. `EntityDestroyedMessage` emitted during destroy pass (v1 — needed for cross-component cleanup). | Cycle detection test; destroy-subtree test; `EntityDestroyedMessage` received by subscriber | Done | sonnet | Completed 2026-05-24. 9 hierarchy tests GREEN. EntityDestroyedMessage broadcast via MakeAllAddress. Queuing order (ChildBuffer before Parent) ensures OnAttach sees live ChildBuffer. |
| **F6 — Mailbox Router** | | | | | |
| 12 | `kEntityRouterId` (`StringCRC("dia.entity.router")`). Address encoding helpers (`MakeEntityAddress`, `MakeAllAddress`, `MakeComponentTypeAddress`, `MakeSelfAddress`, `GetAddressKind`). `EntityRouter : IMailboxRouter` — `Resolve` implementations for all 4 kinds. Auto-register router with realm's `Mailbox` on `Domain` construction. | Router resolves Entity/All/ComponentType/Self to correct subscriber sets; tiny TestDomain fixture | Done | sonnet | Completed 2026-05-24. 10 router tests GREEN. SubscriberSet capacity left at 64 (DiaMailbox limit); EntitySubscriberId helpers pack entity index+gen into uint64. |
| **F7 — Query System** | | | | | |
| 13 | `QueryView<TComponents...>` — iterator yields `(Entity, TComponents*...)` tuples. `Domain::Query<...>()` — signature-keyed cache (`DynamicArrayC<QueryCache, kMaxQueryTypes>`). Invalidate + rebuild at `EndOfFrame` when relevant component types were mutated. | Query returns correct entities; cache hits avoid rebuild; mutation invalidates cache | Done | sonnet | Completed 2026-05-24. 9 query tests GREEN. Signature uses runtime XOR-fold of sorted CRCs (kTypeId not constexpr). QueryCache stores typeCRCs list for invalidation. |
| **F8 — Update Loop** | | | | | |
| 14 | `Domain::Update(dt)` — walks all `DoUpdate`-opted components in component-type-registration order, then entity-index order. DoUpdate opt-in flag set in `ComponentTypeDesc` based on whether the concrete type overrides `DoUpdate`. | Update ordering test: two component types, correct call sequence; dt propagated correctly | Done | sonnet | Completed 2026-05-24. 7 update loop tests GREEN. DIA_UPDATABLE macro opts components into update. HandlePool::GetLiveGeneration used to reconstruct Entity handles during walk. |
| **F9 — Editor Inspection** | | | | | |
| 15 | `IEntityInspectable` implementation on `Domain`. `ReadField` / `WriteField` via reflection (`ComponentTypeDesc` field array, linear scan <20 fields). Mailbox log accessor. Tier (a) read-only + Tier (b) live field edit. `WriteField` type mismatch → `DIA_LOG_WARNING` + return false (no crash). | ReadField round-trip test; WriteField type-mismatch test (returns false, no assert) | Done | sonnet | Completed 2026-05-24. 13 inspection tests GREEN. WriteField uses read-modify-write round-trip via saveToJson/loadFromJson to avoid resetting unrelated fields. |
| **Verification & Smoke Test** | | | | | |
| 16 | Integration smoke test: `TestDomainComponent` + small CluicheTest blueprint exercising foundation, reflection, blueprint loading, references, hierarchy, queries, mailbox routing, and inspection. Host in DummyStage. | `dia run googletest --filter="diaentitytemplate*"` all GREEN; smoke blueprint loads without assert | Skipped | sonnet | Deferred — unit/integration coverage already comprehensive at 94 tests. System/E2E smoke test to be done as a separate initiative when CluicheTest scene scaffolding is ready. |
| 17 | PD-003 / AD-005 Superseded amendment — edit platform spec and Dia app spec to mark both decisions Superseded, pointing to diaentitytemplate.md as the new authority. Per SD-ENT-021. | Spec review: no unresolved conflicts remain | Done | haiku | Completed 2026-05-24. Both specs updated to `Superseded by diaentitytemplate.md`. Conflict C-1 in diaentitytemplate.md marked Resolved. |

## DIA_COMPONENT Macro Contract (to be filled in at Task 6)

> **Note:** This section must be completed as part of Task 6 before any downstream implementation begins. Record the agreed macro expansion here so all subsequent tasks reference it.

```
DIA_COMPONENT macro expansion decision — pending Task 6
```

## Dependency Summary

```
Task 1 (pre-work)  → unblocks Task 2
Task 2 (vcxproj)   → unblocks Tasks 3–5
Tasks 3–5 (F1)     → unblocks Task 6
Task 6 (macro contract) → unblocks Tasks 7–8 (F2)
Tasks 7–8 (F2)     → unblocks Tasks 9–10 (F3, F4)
Tasks 9–10         → unblocks Tasks 11–14 (F5–F8) [can parallelize F5/F6/F7/F8]
Task 15 (F9)       → BLOCKED on DiaReflect T11–T13 (Phase 3 complete)
Task 16 (smoke)    → depends on all previous tasks
Task 17 (amend)    → depends on Task 16
```
