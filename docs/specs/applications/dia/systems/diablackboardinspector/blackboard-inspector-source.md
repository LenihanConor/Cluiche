# Feature Spec: BlackboardInspectorSource

## Parent System
@docs/specs/applications/dia/systems/diablackboardinspector/diablackboardinspector.md

**Status:** `Approved`

---

## Problem Statement

The `BlackboardRegistry` (Feature 1) holds all registered boards, but nothing serialises that data and pushes it to the editor. A game-side debug data source is needed that reads the registry each tick, detects changes, and emits a JSON payload over the `"blackboard.state"` WebSocket topic whenever board structure changes (slots added/removed, observers attached/detached).

Because the registry is owned by a SimPU Module, the source cannot write directly to the `DebugServer` (which lives on MainPU). It must use the existing `EventStream` push pattern established by the entity inspector.

---

## Solution Overview

Two parts: the data source itself and the `EventStream` wiring in `DebugServerHostModule`.

### BlackboardInspectorSource

```cpp
// CluicheGameBaseline/Modules/InspectorSources/BlackboardInspectorSource.h

struct BlackboardInspectEvent {
    Json::Value payload;
};

class BlackboardInspectorSource final
    : public Dia::DebugServer::ChangeDetectedSourceBase
{
public:
    explicit BlackboardInspectorSource(
        const Dia::Blackboard::BlackboardRegistry& registry);

    Dia::Core::StringCRC        GetTopic()  const override;
    Dia::DebugServer::SourcePolicy GetPolicy() const override;

protected:
    unsigned int CollectAndHash(Json::Value& payload) override;

private:
    const Dia::Blackboard::BlackboardRegistry& mRegistry;
};
```

`ChangeDetectedSourceBase::Tick()` calls `CollectAndHash` on every tick; it only calls `Push` (which calls `NotifySubscribers` via the EventStream path) when the hash changes from the last tick or when subscriber count changes.

`CollectAndHash` iterates `mRegistry.GetAll()`, accesses each board's slot array and observer array, and builds the JSON payload. Hash is derived by XOR-accumulating `(board.id.Value() ^ slot_count ^ observer_count)` for every board — changes in any board's structure change the hash and trigger a push.

### Wire payload

```json
{
  "boards": [
    {
      "id":    "player",
      "label": "PlayerModule",
      "slots": [
        { "key": "health",   "type": "HealthBoard",   "value": { "current": 80.0, "max": 100.0 } },
        { "key": "movement", "type": "MovementBoard",  "value": "[no serializer]" }
      ],
      "observers": ["HealthUI", "DamageSystem"]
    }
  ]
}
```

Field value serialization uses `BlackboardRegistry::HasSerializer` / `Serialize`. Slot key strings are emitted via `StringCRC::AsChar()`. Observer names come from `IBlackboardObserver::GetId().AsChar()`.

### DebugServerHostModule wiring

`BlackboardInspectorSource` runs on SimPU and cannot call `mServer.NotifySubscribers` directly (MainPU). Pattern follows entity inspector:

1. `BlackboardInspectorSource` calls `Push(payload)` (inherited from `EventDrivenSourceBase` — actually `ChangeDetectedSourceBase` inherits from `InspectorDataSourceBase` which has access to the stream). The source writes a `BlackboardInspectEvent` to a named `EventStream`.
2. `DebugServerHostModule` (MainPU) holds an `EventStreamReader<BlackboardInspectEvent>`; in `DoUpdate()` it drains the stream and calls `mServer.NotifySubscribers(evt.payload["topic"], evt.payload)`.

`DebugServerHostModule::kSourceCount` is bumped by 1; the source is instantiated in `DoStart()` and passed the registry reference obtained via `ModuleRef<BlackboardRegistryModule>`.

### Files

| File | Change |
|------|--------|
| `CluicheGameBaseline/Modules/InspectorSources/BlackboardInspectorSource.h` | New |
| `CluicheGameBaseline/Modules/InspectorSources/BlackboardInspectorSource.cpp` | New |
| `CluicheGameBaseline/Types/BlackboardInspectEvent.h` | New — plain struct with `Json::Value payload` |
| `CluicheGameBaseline/Modules/DebugServerHostModule.cpp` | Amended — bump `kSourceCount`, add stream reader, drain in `DoUpdate` |
| `CluicheGameBaseline/CluicheGameBaseline.vcxproj` | Amended — add new .h/.cpp |
| `CluicheGameBaseline/CluicheGameBaseline.vcxproj.filters` | Amended |

---

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| 1 | `GetTopic()` returns `StringCRC{"blackboard.state"}` | Unit test |
| 2 | `CollectAndHash` emits a `"boards"` JSON array with one entry per registered board | Unit test with 2 boards registered |
| 3 | Each board entry contains `id`, `label`, `slots` array, and `observers` array | Unit test |
| 4 | Slot entries with a registered serializer include a populated `"value"` object | Unit test using `MockCostProvider`-style mock + serializer lambda |
| 5 | Slot entries without a registered serializer emit `"value": "[no serializer]"` | Unit test |
| 6 | `observers` array contains `GetId().AsChar()` strings for all attached observers | Unit test with `MockBlackboardObserver` attached |
| 7 | Hash changes when a slot is added to a registered board | Unit test: record hash, add slot, call CollectAndHash again, verify new hash ≠ old |
| 8 | Hash changes when an observer is attached/detached | Unit test |
| 9 | Hash does not change when boards and slots are stable across two consecutive calls | Unit test |
| 10 | Source produces no output (no `Push`) when no boards are registered | Unit test: empty registry, verify payload `"boards"` array is empty and no push fires |
| 11 | `DebugServerHostModule` drains `BlackboardInspectEvent` stream on MainPU each tick | Integration: run game, connect editor, verify `"blackboard.state"` topic arrives |
| 12 | Topic constant matches what `DiaBlackboardInspectorPlugin` subscribes to | Code review — both sides use same `StringCRC{"blackboard.state"}` |

---

## Tasks

| # | Task | Depends On | Notes |
|---|------|------------|-------|
| 1 | Create `BlackboardInspectEvent.h` in `CluicheGameBaseline/Types/` | [blackboard-registry](blackboard-registry.md) done | Plain struct, no .cpp |
| 2 | Implement `BlackboardInspectorSource.h/.cpp` | 1 | `CollectAndHash` loops registry, builds JSON, computes hash |
| 3 | Amend `DebugServerHostModule.cpp` — bump `kSourceCount`, add `EventStreamReader<BlackboardInspectEvent>`, drain in `DoUpdate` | 2 | Follow existing `EntityInspectEvent` pattern |
| 4 | Add new files to `CluicheGameBaseline.vcxproj` + `.filters` | 2 | Use `dia docs vcxproj-add` |
| 5 | Write unit tests covering ACs 1–10 | 2 | In `GoogleTests/CluicheGameBaseline/` or new `TestBlackboardInspectorSource.cpp` |
| 6 | `dia run googletest` — all tests pass | 5 | — |
| 7 | `dia run cluichetest` — game starts, no crash; `DIA_LOG_INFO` shows source activated | 3 | Manual smoke test |

---

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | Topic `StringCRC{"blackboard.state"}`; slot keys via `StringCRC::AsChar()`; observer ids via `GetId()` |
| PD-002 | PU/Phase/Module architecture | Source runs on SimPU; EventStream push to MainPU `DebugServerHostModule` |
| PD-004 | No STL in public APIs | `CollectAndHash` signature uses `Json::Value&`; no STL in the class's public header |
| AD-003 | Namespace `Dia::<Module>::` | Source lives in `CluicheGameBaseline` (game-side, not an engine module) — no namespace constraint applies here |
