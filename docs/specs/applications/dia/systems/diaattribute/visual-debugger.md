# Feature Spec: DiaAttributeVisualDebugger

## Parent System
@docs/specs/applications/dia/systems/diaattribute/diaattribute.md

## Problem Statement

Stat bugs — wrong modifier evaluation order, a dangling modifier left behind after an equip/unequip mismatch, a conditional modifier stuck inactive — are otherwise invisible without log spelunking. This feature is a separate module (`DiaAttributeVisualDebugger`), following the same `IDebugDomain` migration pattern already used by 14 existing domains (per `DiaDebugDomain`'s system spec), giving an in-game overlay for live per-entity attribute and modifier-stack inspection.

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-1 | `DiaAttributeVisualDebugger` registers as an `IDebugDomain` and appears in the `DiaDebugPanel` domain list | Open the debug panel; confirm the domain is listed |
| AC-2 | Selecting an entity with an `AttributeSetComponent` shows every attribute's resolved value, base value, and `[minimum_value, maximum_value]` range | Select an entity; assert all three fields render per attribute |
| AC-3 | The active modifier stack for a selected attribute lists `modifier_name`, `operation`, `value`, and — if Feature 2 (Conditional Modifiers) is present — whether its `when_condition` currently evaluates true/false | Add a mix of unconditional and conditional modifiers; assert the panel lists all of them with correct condition state |
| AC-4 | The overlay updates when `OnAttributeChanged` fires (Feature 3), rather than polling every frame for every displayed attribute | Mutate an attribute via `SetBaseValue`; assert the panel updates without a fixed-interval poll driving it |
| AC-5 | For attributes carrying a conditional modifier, the panel polls that modifier's `when_condition` state at a bounded cadence *specific to conditional modifiers on the currently-selected entity* — because Feature 3 explicitly does not fire change events for condition-driven value changes (its documented limitation), a push-only overlay would be blind to exactly the bug class this feature exists to surface | With a conditional modifier whose backing condition flips without any `AttributeSet` mutation, assert the panel's displayed condition-true/false state updates within the polling cadence |

## Design

### Scaffold

Follows the same shape as prior `IDebugDomain` migrations (e.g. `DiaEntityVisualDebugger`, `DiaGridVisibilityVisualDebugger`) — a domain class registering with `DiaDebugDomainRegistry`, contributing a panel section to the Ultralight-based `DiaDebugPanel`, driven by entity selection state shared with other domains.

### Two update paths, matching the two ways a value can change

- **Push path (AC-4):** subscribe an `IAttributeObserver` (Feature 3) to the selected entity's `AttributeSetComponent`; on `OnAttributeChanged`, mark the corresponding row dirty for re-render. This covers every explicit `SetBaseValue`/`AddModifier`/`RemoveModifier` call.
- **Poll path (AC-5):** once per N frames (bounded, only while the panel is open and an entity with conditional modifiers is selected — not a background always-on poll), re-evaluate each visible conditional modifier's `when_condition` and diff against last-displayed state. This exists *specifically* to cover Feature 3's documented gap (condition-driven value changes fire no event) — it is not a general-purpose polling fallback for everything this feature could otherwise get via push.

## Open Design Questions

1. **Poll cadence for conditional modifiers (AC-5)** — every frame is safest but costliest; tying it to the panel's own visible-refresh rate (only while open, only for the selected entity) bounds the cost to "one entity's conditional modifiers, N times a second," not a global background poll. Confirm the exact cadence against `DiaDebugPanel`'s existing refresh conventions during implementation rather than inventing a new one.

## Status

**Status:** `Done`
