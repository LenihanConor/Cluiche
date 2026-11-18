# Feature Spec: DiaBlackboard Scopes and Change Notifications

**Parent:** @docs/specs/applications/dia/systems/diablackboard/diablackboard.md
**Status:** `Approved` (2026-11-18, user)
**Research:** @docs/research/worldsim_data_driven/summary.md (§0: A13, A13b)

## Summary

Blackboards can be chained: agent → world → game. A read through the chain falls back to the parent when the key isn't local. Each board's slot cap is set in its constructor, and boards can carry a name. A new `Set<T>` notifies observers that a value changed. Every board is owned by a module and reached through `ModuleRef`; there are no new singletons.

## Problem

The world sim needs to publish world-scale summaries (season, climate phase, drought, local danger…) that gameplay reads, and more world-scale state is coming. Today `Blackboard` has these limits:
- It is flat: there is no fall-through to a wider scope.
- It is capped at 32 slots by a template constant.
- It notifies only when a slot is registered or unregistered, not when a value changes.

`GlobalBlackboard` is a singleton, which the project rule forbids without sign-off. Its only user today is the Arena test stage.

## Goals

1. Scoped lookup: an agent's board can read world and game state without each consumer holding several references.
2. Value-change notifications for writes that want them.
3. No behaviour change for any current caller.

## Non-Goals

- Thread safety. Boards are SimPU-only, and off-SimPU readers get FrameStream snapshots (PU-boundary rule).
- Propagating parent changes to a child's observers. To watch world summaries, attach to the world board.
- Removing `GlobalBlackboard`. It stays as debt, unused by new code.
- The CoW world and game boards and their summary slots. Those belong to the World Step / CoW amendment (world-sim spec 6).

## Design

```cpp
namespace Dia::Blackboard {
    class Blackboard {
    public:
        explicit Blackboard(uint32_t maxSlots = 32, Dia::Core::StringCRC name = {});
        // Unchanged, all local-only: Register, Unregister, Get, TryGet, Has, AddObserver, RemoveObserver, VisitSlots, VisitObservers.

        bool SetParent(const Blackboard* parent);            // false (and unchanged) if it would form a cycle or exceed depth 8
        const Blackboard* GetParent() const;
        Dia::Core::StringCRC GetName() const;

        template<typename T> const T* Lookup(Dia::Core::StringCRC key) const;   // local, then up the chain; type-checked
        bool HasInChain(Dia::Core::StringCRC key) const;
        const Blackboard* FindOwner(Dia::Core::StringCRC key) const;           // the board in the chain that holds the key

        template<typename T> void Set(Dia::Core::StringCRC key, const T& value);  // local slot only; notifies OnSlotChanged
    };
    class IBlackboardObserver {
        // existing: GetId, OnSlotRegistered, OnSlotUnregistered
        virtual void OnSlotChanged(Dia::Core::StringCRC /*key*/) {}          // new, defaulted
    };
}
```

- **Shadowing.** A local key hides the same key further up the chain. A type mismatch on `Lookup` asserts, like `Get`.
- **Writes are local only.** `Set` on a key that isn't registered locally asserts, even if a parent holds it. A write through the mutable `Get`/`TryGet` reference does **not** notify; this is documented in the header.
- **Lifetime.** A parent must outlive its children. A board's destructor asserts in Debug if it is still the parent of a live child, tracked with a child counter.
- **Storage.** Slots go from `DynamicArrayC<SlotEntry*, 32>` to a `DynamicArray` sized from `maxSlots`. Exceeding the cap still asserts.
- **Inspector.** `BlackboardInspectorSource` shows the board's name, its local slots, and then each ancestor's slots marked "inherited from <name>", with shadowed keys flagged.
- **Condition bridge.** Any existing blackboard → DiaCondition accessor bridge keeps its local semantics. A chain-aware variant (`Lookup`) is added only if a consumer needs it (see Open Design Question 1).

**CoW intended use** (built in spec 6, not here):
- An agent board's parent is the world board, owned by `WorldStepModule` and written with `Set` at each commit.
- The world board's parent is the game board, owned by the CoW game module.
- Gameplay modules on SimPU reach the boards through `ModuleRef`.

## Acceptance Criteria

1. **Compatibility.** Every existing DiaBlackboard test passes unchanged, and so do the AI test stages that use blackboards. `Get`, `TryGet` and `Has` stay local-only.
2. **Slot cap.** `Blackboard(maxSlots)` holds exactly `maxSlots` slots, and the next `Register` asserts. The default is 32.
3. **Chain lookup.**
   - `Lookup` and `HasInChain` find a key locally, in the parent, and in the grandparent.
   - A local key shadows a parent key.
   - A missing key returns null or false.
   - A type mismatch asserts.
   - `FindOwner` names the right board.
4. **`SetParent` safety.** `SetParent` refuses a cycle (A→B→A, and self-parenting) and a chain deeper than 8, leaving the board unchanged. Destroying a parent that still has a child asserts in Debug.
5. **`Set` and notifications.** `Set` writes the local slot and calls `OnSlotChanged(key)` once on each observer of that board, and on no other board. `Set` on a key held only by a parent asserts. A write through `Get`'s reference does not notify. Existing observers that don't override `OnSlotChanged` compile and behave as before.
6. **Inspector.** The inspector source reports the board's name, its local slots, and inherited slots by ancestor, with shadowing flagged. Its existing contract fixtures pass, and a new chain fixture is added.

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaBlackboard/Blackboard.h`, `Blackboard.inl`, `Blackboard.cpp` | Scopes, cap, name, `Set` |
| `IBlackboardObserver.h` | `OnSlotChanged` (defaulted) |
| `Testing/BlackboardTestHelpers.h` | Mock observer counts `OnSlotChanged` |
| `Cluiche/CluicheGameBaseline/Modules/InspectorSources/BlackboardInspectorSource.cpp` + contract fixtures | Chain view |
| `Cluiche/Tests/GoogleTests/DiaBlackboard/*` | New tests |
| `Dia/DiaBlackboard/Docs/*.module.md` | Responsibilities |
| `diablackboard.md` | **Fill in the stub system spec**: purpose, responsibilities, and a features table with this feature |

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| No singletons without sign-off (project rule) | Use `ModuleRef` or streams | No new singleton. Boards are module-owned and reached through `ModuleRef`. `GlobalBlackboard` is left unused by new code. |
| PU boundaries are streams (project rule) | No cross-PU mutex reads | SimPU-only. Off-SimPU readers use FrameStream snapshots. |
| PD-004 | No STL in public APIs | Dia containers; the `StringCRC` name. |
| AD-001 (CoW) | Ask before Dia edits | Signed off 2026-11-18 (research A13b). |

The DiaBlackboard system spec is a stub with no decisions. This feature fills in its body (see Files Touched).

## Test Plan

TDD:
1. Compatibility (existing suite).
2. The slot cap.
3. Chain lookup and shadowing.
4. `SetParent` safety.
5. `Set` and notifications.
6. The inspector chain.

Scoped runs use `--filter="*Blackboard*"` with `--shards 2`, plus the AI test stages' E2E. There is one full suite run at close-out.

## Open Design Questions

1. **A chain-aware condition bridge.** AI rules and utility scoring read blackboards through DiaCondition accessors. Should they see inherited keys? **Recommended:** only when a consumer asks. The first CoW gameplay rule that reads a world summary decides it, and the change would be a `Lookup`-based accessor variant added to the bridge.

## Status

**Status:** `Approved` (2026-11-18, user; explicit chain methods, `Set` + `OnSlotChanged`). No plan yet.
