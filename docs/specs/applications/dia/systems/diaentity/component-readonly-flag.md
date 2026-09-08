# Feature Spec: Component Data Flow Contracts

## Parent System
@docs/specs/systems/dia/diaentitytemplate.md

## Binding Decisions
- SD-ENT-022: components must not reach outside the Domain; readonly vs behaviour split

---

## Problem Statement

Components currently have no compile-time or runtime signal declaring:
1. Whether they are pure data (read by a module/component) or active behaviour (lifecycle hooks, DoUpdate, mailbox)
2. Which updatable component is responsible for writing to which data component

This leads to implicit multi-writer conflicts (two updatable components both writing Transform), ordering-dependent bugs that only manifest as frame glitches, and no enforcement of the boundary rule.

---

## Summary

Two complementary macros that together define a fully explicit data flow graph per entity:

- **`DIA_READONLY`** — marks a component as pure data. Domain skips `OnAttach`/`OnDetach`/`DoUpdate`. No behaviour hooks execute.
- **`DIA_WRITES(TargetComponent)`** — declares that this updatable component is the sole writer to a target component. Domain validates at entity creation that no two updatable components on the same entity declare writes to the same target.

Together: readonly is the consumer-side rule (I am data, don't tick me), writes is the producer-side rule (I am the one who writes to X). The Domain enforces both.

---

## Goals

1. Distinguish data-tag components from behaviour components at the type level
2. Domain enforces readonly — skips hooks for readonly pools
3. Domain enforces single-writer — asserts at spawn if two updatable components declare writes to the same target
4. Self-documenting entity composition — DIA_WRITES declarations reveal the data flow graph
5. No runtime cost for readonly components (no vtable dispatch for hooks)
6. Inspector/editor can badge readonly vs behaviour components differently

---

## Tasks

| # | Task | Test |
|---|------|------|
| 1 | Add `DIA_READONLY` macro to `ComponentMacros.h` — emits `static constexpr bool kIsReadOnly = true;` | Compiles |
| 2 | Add `kFlagReadOnly` to `ComponentTypeDesc` flags enum; set in `DIA_COMPONENT_REGISTER` when `kIsReadOnly` is true | `GetDesc().flags & kFlagReadOnly` returns true for readonly components |
| 3 | `Domain::ApplyAddComponent` — skip `OnAttach` call if pool's type has `kFlagReadOnly` | Unit test: readonly component's OnAttach never fires |
| 4 | `Domain::ApplyDestroyEntity` / `ApplyRemoveComponent` — skip `OnDetach` for readonly | Unit test: detach counter stays 0 for readonly component |
| 5 | `Domain::Update` — skip DoUpdate iteration for readonly pools | Unit test: DoUpdate not called on readonly component |
| 6 | Debug-only assertion: if a component has `kIsReadOnly == true` and also `kIsUpdatable == true`, static_assert or registration-time assert | Build-time or registration-time error |
| 7 | Add `DIA_WRITES(TargetComponent)` macro — emits a static `WritesTo()` method returning a span/array of target type IDs | Compiles; `MovementComponent::WritesTo()` returns `{TransformComponent::kTypeId}` |
| 8 | Extend `ComponentTypeDesc` with `writesTo` pointer + count (same pattern as fields/requires) | `GetDesc().writesTo[0] == TransformComponent::kTypeId` |
| 9 | `DIA_WRITES_ENTRY(TargetComponent)` macro for registration block (mirrors `DIA_REQ_ENTRY`) | Registration compiles with writes metadata |
| 10 | Domain validation on add/remove: when a component is added or removed, check all updatable components on that entity for duplicate write targets. Debug assert on conflict. | Unit test: adding two components that both DIA_WRITES(TransformComponent) fires assertion; removing one clears the conflict |
| 11 | Domain validation for writes target: assert that the target component type has `kFlagReadOnly` (you should only declare writes to data components, not to other behaviour components) | Unit test: DIA_WRITES targeting a non-readonly component fires assertion |
| 12 | Apply to canonical examples in EntityTestStage — `TransformComponent` (readonly), `MovementComponent` (updatable, writes transform) | Stage passes, data flow is explicit |

---

## Non-Goals

- No serialization difference — readonly components serialize identically
- No editor write-protect — both readonly and behaviour fields are editable in live-edit
- No runtime dispatch cost for the writes validation (debug-only)
- No ordering between updatable components — single-writer eliminates the ordering problem
- No `DIA_CLAIMS_FROM` ownership transfer — future feature if needed

---

## Design Decisions

1. **One macro per target** — `DIA_WRITES(A)` `DIA_WRITES(B)` as separate declarations, not a variadic list. Keeps the macro simple and grep-friendly.
2. **Validate on add/remove too** — not just spawn. Any time a component is added or removed the Domain re-checks for duplicate writers on that entity.
3. **Opt-in enforcement** — updatable components without `DIA_WRITES` are unconstrained (they can write to themselves freely). The single-writer rule only activates for components that declare cross-component write targets. Self-mutation (a behaviour component updating its own fields) never needs declaration.

---

## Status

`Done`
