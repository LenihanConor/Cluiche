# Feature Spec: Attribute Change Notifications

## Parent System
@docs/specs/applications/dia/systems/diaattribute/diaattribute.md

## Problem Statement

Without change events, every consumer (UI health bars, AI, the visual debugger, save-on-change triggers) must poll `GetValue()` every frame for every attribute it cares about. `DiaBlackboard` already establishes that poll-only is workable for simple slot storage, but `DiaEconomy`'s `IEconomyObserver`/`OnPoolChanged` precedent shows event-based notification is the better fit once modifiers and clamping are involved — the exact same shape applies here. This feature adds that Observer layer directly on top of Core's mutation API.

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-1 | `OnAttributeChanged` fires exactly once per mutation (`SetBaseValue`, `AddModifier`, `RemoveModifier`) that changes an attribute's resolved value, with correct `old_value`/`new_value` | Call each mutating method with a subscribed observer; assert one event per call with correct before/after values |
| AC-2 | A mutation that does not change the resolved value (e.g. `SetBaseValue` to the current value, or adding a modifier whose contribution is clamped away to nothing) fires **no** event — no spurious notifications | `SetBaseValue(attr, GetValue(attr))`; assert zero events |
| AC-3 | `OnAttributeReachedMaximum`/`OnAttributeReachedMinimum` fire exactly once on the transition into the clamp boundary, and do not re-fire on subsequent mutations that leave the value still at the boundary (edge-triggered, not level-triggered) | Push a value to max, mutate again while still clamped at max; assert only the first mutation fired the boundary event |
| AC-4 | Multiple observers can `Subscribe` to the same `AttributeSet`; `Unsubscribe` stops delivery; an `AttributeSet` going out of scope with live subscribers does not crash | Subscribe two observers, unsubscribe one, mutate; assert only the remaining observer received the event |
| AC-5 | Events are same-thread, synchronous — delivered inline during the mutating call, matching `DiaEconomy`'s contract; this feature does not relay across PUs | Mutate from a test on the calling thread; assert the observer callback ran before the mutating call returned |

## Design

### Event shape

```cpp
namespace Dia::Attribute {

    struct AttributeChangedEvent {
        Entity      entity;
        StringCRC   attribute_name;
        float       old_value;
        float       new_value;
    };

    class IAttributeObserver {
    public:
        virtual void OnAttributeChanged        (const AttributeChangedEvent&) {}
        virtual void OnAttributeReachedMaximum (Entity entity, StringCRC attribute_name) {}
        virtual void OnAttributeReachedMinimum (Entity entity, StringCRC attribute_name) {}
    };

    class AttributeObserverSubject : public Dia::Core::ObserverSubject {
    public:
        void Subscribe  (IAttributeObserver* observer);
        void Unsubscribe(IAttributeObserver* observer);
    };
}
```

### Diff-and-notify

Every mutating method on `AttributeSet` (`SetBaseValue`, `AddModifier`, `RemoveModifier`) computes `GetValue(attribute_name)` immediately before and after applying the change. If the two differ, `OnAttributeChanged` fires with both values (AC-1/AC-2) — this mirrors exactly how `EconomySystem::Earn`/`Spend` compute a delta and only fire `OnPoolChanged` when `delta != 0`. Boundary events are computed the same way, comparing the post-mutation value against `minimum_value`/`maximum_value` and firing only on the false→true edge of "is at boundary" (AC-3).

### Known limitation — conditional-modifier-driven changes are not covered

Feature 2 (Conditional Modifiers) allows a modifier's contribution to switch on/off because some *external* condition changed (a blackboard tag flips elsewhere) — with **zero calls into `AttributeSet`**. Since this feature's diff-and-notify only runs inside `AttributeSet`'s own mutating methods, a condition-driven value change fires **no** `OnAttributeChanged` event at all. This is a real gap, not an oversight to paper over:

- Consumers that need to react to condition-driven attribute drift should hook the condition's own source-of-truth change event (e.g. whatever fires when the blackboard tag changes), not `AttributeObserverSubject`.
- The visual debugger (Feature 5) cannot rely on this feature's events alone for conditional-modifier-driven changes and must poll that specific case (see Feature 5's AC-5).

This limitation must be documented in the module's public API comments, not just this spec, so future consumers don't assume push-only coverage they don't get.

## Open Design Questions

1. **Should this gap be closed instead of documented?** A more complete design would have `AttributeSet` itself poll/diff resolved values once per tick (independent of any mutating call) to catch condition-driven drift, turning this into a level-triggered check rather than purely mutation-triggered. That requires an `IModule`/tick hook, which Core's Non-Responsibilities explicitly excludes. Recommend: ship the documented limitation for v1; revisit only if a concrete consumer (e.g. UI health bars reacting to poison ticks) proves polling is unacceptable.

## Status

**Status:** `Approved`
