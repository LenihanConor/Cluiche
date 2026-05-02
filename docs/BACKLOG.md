# Cluiche Backlog

Derived from spec status across `docs/specs/`. Updated manually — when a spec moves to Done, remove or tick it here.

---

## In Progress

_Nothing currently in progress._

---

## Ready to Build (Approved → implement)

These specs are `Approved` with all features `Approved`. No spec work needed — go straight to implementation.

### Systems

| System | Spec | Features | Depends On |
|--------|------|----------|------------|
| DiaApplicationEditor | [diaapplicationeditor.md](specs/systems/dia/diaapplicationeditor.md) | 15 features, all Approved — **not yet implemented** | DiaEditor ✅, DiaWebSocket ✅, DiaUICEF ✅ |
| DiaData | [diadata.md](specs/systems/dia/diadata.md) | 3 features, all Approved — json-definition-loader, asset-type-framework, identity-relationship-backbone | DiaCore ✅ |
| DiaIK | [diaik.md](specs/systems/dia/diaik.md) | 6 features, all Approved — **not yet implemented. Blocked on DiaRig** | DiaRig ✅, DiaMaths ✅, DiaCore ✅, DiaLogger ✅ |

### Standalone Features (system Done, feature Approved)

| Feature | Spec | System |
|---------|------|--------|
| per-app-bin-layout | [per-app-bin-layout.md](specs/features/dia/diapipeline/per-app-bin-layout.md) | DiaPipeline ✅ |

---

## Spec Work Needed (Draft or unset — review/approve before building)

| Item | Spec | What's needed |
|------|------|---------------|
| DiaApplication — data-driven-application-system | [data-driven-application-system.md](specs/features/dia/diaapplication/data-driven-application-system.md) | status unset — needs review |
| DiaAssetPipeline system | TBD | Needs `/spec-system` — build-time asset pipeline (discover, validate, transform, deploy). Depends on DiaData, DiaAPI, DiaLogger |
| DiaStageLoader system | TBD | Needs `/spec-system` — runtime Stage/Bundle loading from built output. Depends on DiaData |
| DiaAssetBrowserEditor system | TBD | Needs `/spec-system` — editor UI for asset browsing, inspection, relationship graph. Depends on DiaData, DiaEditor |
| DiaStateMachineEditor system | TBD | Needs `/spec-system` — editor plugin for state machine visual debugging + design-time editing. Depends on DiaStateMachine ✅, DiaEditor |
| DiaRigidBody2D — Visual Debugger | [diarigidbody2dvisualdebugger.md](specs/systems/dia/diarigidbody2dvisualdebugger.md) | System spec exists, `Draft` — needs `/spec-review` to approve |
| DiaSoftBody2D — Visual Debugger | [visual-debugger.md](specs/features/dia/diasoftbody2d/visual-debugger.md) | Spec file exists, no status — needs `/spec-review` |

---

## Loose Ends (non-spec items)

| Item | Notes |
|------|-------|
| ~~DiaRig2D — Exhaustive tests~~ | Done 2026-05-02: 38 new tests (golden, invariant, stress, boundary, determinism, integration) in `Cluiche/Tests/GoogleTests/Rig2D/` |
| DiaApplication — Feature 6: Compile-Time Dependency Validation | Deferred by user ("let's come back and talk about 6") |
| HotReloadManager — `CollectDependentModules()` / `UpdateDependencyReferences()` | Placeholder stubs; needs real implementation |
| `Dia::Core::Blackboard` — general-purpose key-value store | Identified during DiaStateMachine research; useful for AI, animation, gameplay. Needs `/spec-feature` under DiaCore. |

---

## Deferred

| Item | Spec | Reason |
|------|------|--------|
| DiaEnv — env-export | [diaenv.md](specs/systems/dia/diaenv.md) | Covered by deps.json + mirrors + submodules |
| DiaEnv — docker continuous monitoring (`dia env doctor`) | [diaenv.md](specs/systems/dia/diaenv.md) | Premature until setup + verify stable (both now Done) |
