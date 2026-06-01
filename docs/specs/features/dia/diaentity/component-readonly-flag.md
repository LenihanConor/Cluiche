# Feature Spec: Component Readonly Flag

## Parent System
@docs/specs/systems/dia/diaentity.md

## Binding Decisions
- SD-ENT-022: components must not reach outside the Domain; readonly vs behaviour split

---

## Problem Statement

Components currently have no compile-time or runtime signal declaring whether they are pure data (read by the module) or active behaviour (lifecycle hooks, DoUpdate, mailbox). This leads to ambiguous patterns: components that hold a `static*` to reach external services, components that are data-only but still define empty `OnAttach` overrides, and no enforcement of the boundary rule.

`DIA_READONLY` codifies the contract: a readonly component is a data tag. The Domain skips its lifecycle hooks. The module is the system that reads it and drives external services.

---

## Summary

Add `DIA_READONLY` macro (mirrors `DIA_UPDATABLE`). Domain skips `OnAttach`/`OnDetach`/`DoUpdate` for readonly components. `ComponentTypeDesc` carries a `kFlagReadOnly` bit. Debug assertion if a readonly component overrides any lifecycle method.

---

## Goals

1. Distinguish data-tag components from behaviour components at the type level
2. Domain enforces the boundary — skips hooks for readonly pools
3. Inspector/editor can badge readonly components differently
4. No runtime cost for readonly components (no vtable dispatch for hooks)

---

## Tasks

| # | Task | Test |
|---|------|------|
| 1 | Add `DIA_READONLY` macro to `ComponentMacros.h` — emits `static constexpr bool kIsReadOnly = true;` | Compiles |
| 2 | Add `kFlagReadOnly` to `ComponentTypeDesc` flags enum; pass in `DIA_COMPONENT_REGISTER` | `GetDesc().flags & kFlagReadOnly` returns true for readonly components |
| 3 | `Domain::ApplyAddComponent` — skip `OnAttach` call if pool's type has `kFlagReadOnly` | Unit test: readonly component with OnAttach override — assert fires in debug |
| 4 | `Domain::ApplyDestroyEntity` / `ApplyRemoveComponent` — skip `OnDetach` for readonly | Unit test: detach counter stays 0 for readonly component |
| 5 | `Domain::Update` — skip DoUpdate iteration for readonly pools | Unit test: DoUpdate not called on readonly component |
| 6 | Debug-only static_assert or runtime assert: if component has `kIsReadOnly == true` and overrides OnAttach/OnDetach/DoUpdate, fire a diagnostic | Build-time or first-frame assertion in debug |
| 7 | Apply `DIA_READONLY` to `VisualTestRenderComponent` and `PickableCircleComponent` in EntityTestStage as canonical examples | Stage still passes (module drives, no hooks called) |

---

## Non-Goals

- No serialization difference — readonly components serialize identically
- No editor write-protect — both readonly and behaviour fields are editable in live-edit mode
- No `DIA_WRITEONLY` — if you need write-only intent, use mailbox signal

---

## Open Design Questions

1. Should `DIA_READONLY` imply the component's fields are `public` for direct module access, or keep the existing `FIELD` macro (which already exposes fields publicly via `public:` toggle)?
2. Should the vtable methods (`OnAttach`, etc.) be removed from readonly components entirely (preventing override), or kept as no-ops with a debug assert on override?

---

## Status

`Draft`
