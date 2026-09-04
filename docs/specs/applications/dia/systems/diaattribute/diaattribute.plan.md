**Spec:** @docs/specs/applications/dia/systems/diaattribute/diaattribute.md
**Status:** In Progress

> New system — data-driven gameplay attribute/stat framework, sibling to DiaEconomy (same idiom: StringCRC, JSON schema, DiaCondition-gated modifiers, Observer events, DiaCore containers — no shared code). Build order: Core (Phase 1) is the load-bearing prerequisite for everything else. Conditional Modifiers, Change Notifications, and the AI Accessor Bridge (Phase 2) are independent increments on top of Core — order between them is flexible. Visual Debugger and Save/Serialization (Phase 3) build on Phase 2's modifier-stack and change-event shapes.
>
> Each task = one feature spec. Feature specs live alongside this file.
>
> **Deferred/parked** (not tasks in this plan): Archetype/Instance Layering — revisit if a large shared-population use case appears; Dependency-Graph Derived Attributes and Non-Numeric Typed Properties — parked, no current driving use case. See bottom.

## Implementation Patterns

**Phase 1 — Core**
- Storage: `Dia::Core::Containers::HashTable<StringCRC, AttributeSlot>` inside `AttributeSet`, keyed by attribute name; each `AttributeSlot` holds `base_value` + `DynamicArrayC<ModifierEntry, kMaxModifiersPerAttribute>`.
- `ModifierHandle` issued from a `Dia::Core::HandlePool<ModifierTag>` — one pool per `AttributeSet` instance, same generational-handle pattern as `Dia::Entity::Entity`.
- Resolution pipeline is fixed and order-independent within a stage: `base → sum(Add) → *product(Multiply) → single Override (if present) → clamp(min,max)`. Override is an exclusive slot (`DIA_ASSERT` on a second Override for the same attribute), not a stack — this is the concrete rule that resolves "same-category modifier ordering."
- **Before writing `AttributeSetComponent`, resolve core.md's Open Design Question #1** — `ParentComponent`-style full `DIA_COMPONENT`/`FIELD` reflection (with a resolved `schema_name` field, for blueprint-JSON spawning) vs. `BlackboardComponent`-style bare wrapper (programmatic construction only). Recommended: the former. Confirm against `diaentitytemplate`'s current blueprint-loading code path first.

**Phase 2 — Conditional Modifiers, Change Notifications, AI Accessor Bridge**
- Conditional Modifiers: parse `when_condition` via `Dia::Condition::ConditionExpr::LoadFromJson` **once**, at `AddModifier` time; immediately call `Validate(registry, outErrors)` and reject the `AddModifier` call on any unresolvable leaf or parse failure. Do **not** rely on runtime "fails closed on missing accessor" — `ConditionRegistry::GetFloat`/`GetBool` assert in Debug on a missing key (SD-009); eager `Validate()` at registration is what avoids ever hitting that path.
- Change Notifications: every mutating `AttributeSet` method (`SetBaseValue`, `AddModifier`, `RemoveModifier`) computes `GetValue()` before and after; fire `OnAttributeChanged` only if they differ (mirrors `EconomySystem::Earn`/`Spend`'s delta-gated `OnPoolChanged`). Boundary events (`OnAttributeReachedMaximum`/`Minimum`) fire on the false→true edge of "value is at clamp boundary," not on every mutation while already there. **Known, documented limitation:** condition-driven value changes (Feature 2, when a gating condition flips externally) fire no event — no code path in this system currently closes that gap; do not attempt to close it as part of this task.
- AI Accessor Bridge: `DiaCondition::FloatAccessorFn` is a non-capturing plain function pointer (`float(*)(void* data)`) with one `data` pointer shared by the whole `ConditionRegistry` instance — there is no per-registration parameter and no unregister method. Use the compile-time trampoline table pattern from accessor-bridge.md (`template<unsigned int Index> float BridgedAttributeAccessor(void* data)`, instantiated up to `kMaxBridgedAttributesPerSet` via `std::integer_sequence`) plus a new `AttributeSet::GetValueByIndex(unsigned int)`. Register one dedicated `ConditionRegistry` per bridged `AttributeSet` (`data = the AttributeSet*`) — do not assume it can be merged into some other system's existing shared registry.

**Phase 3 — Visual Debugger, Save/Serialization**
- Visual Debugger: `IDebugDomain` registered with `DiaDebugDomainRegistry`, matching the existing 14-domain migration pattern. Dual update path — push via `IAttributeObserver` subscription for explicit mutations, plus a bounded poll (only while the panel is open, only for the selected entity's conditional modifiers) to cover Change Notifications' documented condition-driven-change gap. Do not make the poll a global always-on tick.
- Save/Serialization: implement `Dia::SaveGame::ISaveable` (`Serialize(SaveContext&)`/`Deserialize(LoadContext&)`/`GetVersion()`) directly on `AttributeSet` — **not** `DiaSerializer`'s `MetadataValue`/`MetadataArray` (hard-capped at 8 entries, unsuitable for bulk attribute/modifier data). Write base values as one `BeginObject`/`Write` block, modifiers as one `BeginArray` of per-modifier objects. On load, reconstruct each modifier through the existing `AddModifier` path (reusing Core's + Conditional Modifiers' validation) rather than writing into internal storage directly; drop-and-warn on any `attribute_name` no longer in the current schema. Never serialize `ModifierHandle` values — they're runtime-only.

## Phase 1: Foundation

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | [core](core.md) — `AttributeDefinition`/`AttributeSchema`/`AttributeSet`, fixed modifier pipeline, `ModifierHandle`, `AttributeSetComponent` entity integration | `TestDiaAttributeCore*` | Done | sonnet | No prereq. TDD — prove RED first. Resolve ODQ #1 (component attachment mechanism) before writing `AttributeSetComponent`.; 24/24 TestDiaAttributeCore* pass (AC-1..AC-15). ODQ#1 resolved: ParentComponent-style schema_name FIELD; schema-asset lookup in OnAttach is a documented no-op stub pending a registry (not blocking - InitializeFromSchema covers programmatic use). |

## Phase 2: Independent increments (order flexible)

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 2 | [conditional-modifiers](conditional-modifiers.md) — `when_condition` evaluation, eager `Validate()`-at-registration rejection, symmetric add/remove lifecycle preserved | `TestDiaAttributeConditionalModifiers*` | Done | sonnet | Prereq: 1; 35/35 DiaAttribute* pass (Task1 24 + Task2 11, AC-1..AC-7). ConditionExpr is move-only -> heap-owned raw pointer in ModifierEntry, freed in RemoveModifier + AttributeSet dtor. New required dep: dia.condition. |
| 3 | [change-notifications](change-notifications.md) — `IAttributeObserver`/`AttributeObserverSubject`, diff-and-notify on all three mutating methods, edge-triggered boundary events | `TestDiaAttributeChangeNotifications*` | Done | sonnet | Prereq: 1; 48/48 DiaAttribute* pass (24 Core + 11 Cond + 13 ChangeNotif, AC-1..5). Corrected spec Design snippet: AttributeObserverSubject does NOT inherit Dia::Core::ObserverSubject (generic int-message base unrelated to typed events) - mirrors DiaEconomy's self-contained EconomyObserverSubject instead, per spec's own stated intent. |
| 4 | [accessor-bridge](accessor-bridge.md) — `GetValueByIndex`, compile-time trampoline table, `AttributeAccessorBridge::RegisterAccessors` | `TestDiaAttributeAccessorBridge*` | Not Started | opus | Prereq: 1. Highest design uncertainty in this system (ODQ #1: whether `ConditionRegistry` should gain an unregister method upstream) — flag to whoever owns DiaCondition before implementing the workaround. |

## Phase 3: Tooling & persistence

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 5 | [visual-debugger](visual-debugger.md) — `DiaAttributeVisualDebugger` `IDebugDomain`, push+poll dual update path | AC-1..AC-5 (GoogleTest) + panel visible | Not Started | sonnet | Prereq: 2, 3 |
| 6 | [save-serialization](save-serialization.md) — `AttributeSet : ISaveable`, `SaveContext`/`LoadContext` tree serialization | `TestDiaAttributeSaveSerialization*` | Not Started | sonnet | Prereq: 1. Partially blocked on system spec ODQ #2 (time-limited modifier duration) — do not add a speculative duration field if that question is still unresolved when this task starts. |

---

## Deferred / Parked — not tasks in this plan

| Item | Status | Notes |
|------|--------|-------|
| Archetype/Instance Layering | Deferred | Cheap to build right after Phase 1, expensive to retrofit after Phase 2/3 land against an instance-only API. Revisit if a large shared-population use case (many entities sharing one archetype's base values) appears before Phase 2 starts. See [choose.md](../../../../../research/gameplay_attribut_stat_system/choose.md). |
| Dependency-Graph Derived Attributes | Parked | Lowest-scoring research candidate (2.85) — no concrete stat-interdependency use case. See [evaluate.md](../../../../../research/gameplay_attribut_stat_system/evaluate.md). |
| Non-Numeric Typed Properties | Parked | Overlaps `DiaBlackboard`'s existing typed-slot storage; no driving use case. |
