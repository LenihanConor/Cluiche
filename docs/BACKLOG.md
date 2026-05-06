# Cluiche Backlog

Derived from spec status across `docs/specs/`. When a spec moves to Done, move it to [BACKLOG-HISTORY.md](BACKLOG-HISTORY.md).

---

## In Progress

| System | Spec | Progress | Notes |
|--------|------|----------|-------|
| DiaAssetCatalogueEditor | [diaassetcatalogueeditor.md](specs/systems/dia/diaassetcatalogueeditor.md) | 6/8 features Done | Tasks 9–13 remaining — graph node colors, expand no-op, rules list, manual override tracking. [Plan](specs/systems/dia/diaassetcatalogueeditor.plan.md) |

---

## Ready to Build (Approved → implement)

These specs are `Approved` with all features `Approved`. No spec work needed — go straight to implementation.

### Systems

| System | Spec | Features | Depends On |
|--------|------|----------|------------|
| DiaApplicationEditor | [diaapplicationeditor.md](specs/systems/dia/diaapplicationeditor.md) | 15 features, all Approved — **not yet implemented** | DiaEditor ✅, DiaWebSocket ✅, DiaUICEF ✅ |
| DiaApplication — Flow Tree | [diaapplication.md](specs/systems/dia/diaapplication.md) | 3 features, all Approved — manifest-imports (A), pu-parent-child-tree (B), stage-manifests (C). Build order: A→B→C. Phase D (editor graph view) unlocks inside DiaApplicationEditor after A/B/C ship. | DiaCore ✅ |

---

### Standalone Features (system Done, feature Approved)

| Feature | Spec | System |
|---------|------|--------|
| per-app-bin-layout | [per-app-bin-layout.md](specs/features/dia/diapipeline/per-app-bin-layout.md) | DiaPipeline ✅ |

---

## Spec Work Needed (Draft or unset — review/approve before building)

| Item | Spec | What's needed |
|------|------|---------------|
| E2E Testing system | TBD | Needs `/spec-system` + 2 child feature specs (Phase 1: Hybrid Harness, Phase 2: Multi-Stage Test Levels). Research complete — see `docs/research/e2e_testing/summary.md`. First targets: DiaRigidBody2D then CluicheEditor. |
| DiaStateMachineEditor system | TBD | Needs `/spec-system` — editor plugin for state machine visual debugging + design-time editing. Depends on DiaStateMachine ✅, DiaEditor |

---

## Loose Ends (non-spec items)

| Item | Notes |
|------|-------|
| DiaApplication — Feature 6: Compile-Time Dependency Validation | Deferred by user ("let's come back and talk about 6") |
| HotReloadManager — `CollectDependentModules()` / `UpdateDependencyReferences()` | Placeholder stubs; needs real implementation |
| `Dia::Core::Blackboard` — general-purpose key-value store | Identified during DiaStateMachine research; useful for AI, animation, gameplay. Needs `/spec-feature` under DiaCore. |
| DiaStateMachine — `MarkValid()` exposed on definitions | Added to support serializer load path; could be misused to bypass `Validate()`. Consider making package-internal if access control becomes a concern. |
| Phase 3d — Physics body serialization | DiaRigidBody2D / DiaSoftBody2D body definitions — DiaAssetCatalogue ✅ now unblocked |
