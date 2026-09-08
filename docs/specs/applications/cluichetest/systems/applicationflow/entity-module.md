# Feature Spec: entity-module

**System:** CluicheTest Application Flow
**App:** CluicheTest
**Status:** Draft

## Summary

Add `EntityModule` to SimPU — a `Dia::ApplicationFlow::Module` that owns a `Domain`, loads a blueprint from the asset catalogue on `DoStart`, drives `Domain::Update` + `EndOfFrame` each sim tick, and exposes `IEntityInspectable` for future editor integration.

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | [Cluiche.md](../../../../platform/Cluiche.md) |
| Application | CluicheTest | [cluichetest.md](../../cluichetest.md) |
| System | CluicheTest Application Flow | [applicationflow.md](applicationflow.md) |
| Depends on system | diaentitytemplate | diaentitytemplate |
| Depends on system | DiaApplicationFlow | (platform infrastructure) |

## Goals

- Wire `Domain` into the SimPU stage lifecycle cleanly — one module per stage that needs entities
- Blueprint is loaded via `AssetService` — `DoStart` returns `kLoading` until the blueprint asset resolves
- `Domain::Update(dt)` + `Domain::EndOfFrame()` driven each sim tick
- `IEntityInspectable` accessible on the module for future editor wiring

## Acceptance Criteria

- `EntityModule` derives from `Dia::ApplicationFlow::Module`
- `DoStart` requests the blueprint asset via `AssetService`; returns `kLoading` until the asset resolves, then calls `JsonBlueprintLoader::Load`, calls `Domain::EndOfFrame()`, returns `kReady`
- `DoUpdate(dt)` calls `Domain::Update(dt)` then `Domain::EndOfFrame()` each tick
- `DoStop` destroys the `Domain` (or resets it); returns `kDone`
- `EntityModule` exposes `IEntityInspectable& GetInspectable()` — returns reference to the owned `Domain`
- `EntityModule` is registered in the DummyStage manifest under SimPU with `stages: ["DummyStage"]`
- `EntityModule` declares a `ModuleRef` dependency on `AssetServiceModule`
- Build passes; DummyStage loads without assert

## Data Model

### EntityModule

```cpp
class EntityModule : public Dia::ApplicationFlow::Module {
public:
    static constexpr Dia::Core::StringCRC kTypeId{"EntityModule"};

    explicit EntityModule(Dia::Core::StringCRC blueprintAssetId);

    // IEntityInspectable access for editor tooling
    Dia::Entity::IEntityInspectable& GetInspectable();

protected:
    StartResult DoStart() override;     // request blueprint asset → kLoading
    void        DoUpdate(float dt) override; // Domain::Update + EndOfFrame
    StopResult  DoStop() override;      // reset Domain → kDone

private:
    Dia::Core::StringCRC          mBlueprintAssetId;
    Dia::Entity::Domain           mDomain;
    Dia::Entity::JsonBlueprintLoader mLoader;
    bool                          mBlueprintLoaded = false;
};
```

### Asset loading handshake

```
DoStart:
  1. AssetService::Request(mBlueprintAssetId) → registers callback
  2. return kLoading

OnAssetLoaded callback (fires when blueprint JSON asset resolves):
  3. mLoader.Load(mDomain, blueprintJson)
  4. mDomain.EndOfFrame()
  5. mBlueprintLoaded = true

DoStart poll (called each tick while kLoading):
  6. if mBlueprintLoaded → return kReady
  7. else → return kLoading
```

### Manifest entry (dummy_stage.diastage)

```json
{
  "instance_id": "Entity",
  "type_id": "EntityModule",
  "stages": ["DummyStage"],
  "dependencies": ["AssetService"],
  "config": {
    "blueprint_asset_id": "stages/dummy/entities.blueprint"
  }
}
```

## Files Touched

| File | Change |
|---|---|
| `CluicheTest/Modules/EntityModule.h` | New |
| `CluicheTest/Modules/EntityModule.cpp` | New |
| `CluicheTest/CluicheTest.vcxproj` / `.filters` | Add new files |
| `Assets/CluicheTest/stages/dummy/entities.blueprint` | New — minimal test blueprint |
| `dummy_stage.diastage` | Modified — add EntityModule entry |

## Binding Decisions Compliance

| Decision | Summary | Compliance |
|---|---|---|
| PD-001 | StringCRC for all IDs | `kTypeId`, `mBlueprintAssetId`, and component IDs all use `StringCRC`. Compliant. |
| PD-002 | PU/Phase/Module architecture | `EntityModule` is a `Dia::ApplicationFlow::Module` in SimPU. Compliant. |
| PD-004 | No STL in public APIs | `EntityModule` public API uses `StringCRC`, `IEntityInspectable&`. No STL. Compliant. |
| PD-006 | VS project files are source of truth | `CluicheTest.vcxproj` updated manually. Compliant. |
| PD-007 | C++20 | No additional C++20 features beyond what diaentitytemplate already uses. Compliant. |
| PD-010 | `.diastage` declares stage metadata | EntityModule registered in `dummy_stage.diastage`. Compliant. |
| SD-ENT-002 | Systems own data; components are typed adapters | `EntityModule` owns the `Domain`. System data (physics, rendering) lives in their respective systems. Compliant. |
| SD-ENT-017 | Domain is non-copyable, non-movable | `mDomain` is a direct member — not copied or moved. Compliant. |
| SD-ENT-018 | Single-threaded per domain | `Domain` accessed only from SimPU thread. Compliant. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | DoStart polling | DiaApplicationFlow calls `DoStart` once and polls its result each tick while `kLoading`. Is that the correct model, or does `DoStart` return once and `DoUpdate` drives the loading check? | Per the DiaApplicationFlow pattern: `DoStart` is called once per stage entry and may return `kLoading`; the framework polls it each tick until `kReady` or `kFailed`. `DoUpdate` is not called until `DoStart` returns `kReady`. So the asset-loaded flag check lives in `DoStart`, not `DoUpdate`. |
| 2 | Domain reset vs destroy on DoStop | `DoStop` resets the Domain. Does `Domain` have a `Reset()` method, or is the pattern to destroy and reconstruct? | Destroy and reconstruct — `Domain` is non-movable so reset-in-place requires a destructor + placement-new, which is fragile. Preferred pattern: `mDomain` is a `std::optional<Dia::Entity::Domain>` (or a manually managed buffer) so `DoStop` can call the destructor and `DoStart` re-constructs. Decision to make at implementation start. |
| 3 | Blueprint asset type | What asset type does `AssetService` use for `.blueprint` JSON files? | A `BlueprintAssetHandler` registered with `AssetService` that reads the JSON file and returns a `Json::Value`. This handler needs to be registered at application startup — `EntityModule::DoStart` or a one-time registration in the app's boot sequence. Flag for implementation. |
| 4 | Editor exposure timing | `GetInspectable()` returns a reference to `mDomain`. Is it safe to call before `DoStart` returns `kReady`? | Returns a valid reference always (Domain exists as a member from construction). Before blueprint loads, the Domain is empty — editor sees zero entities, which is correct and safe. |
| 5 | Multiple EntityModules | Could a future stage need two Domains (e.g. a game world + a UI entity layer)? | Yes — each stage can have its own `EntityModule` instance with a different `blueprintAssetId`. They are independent Domains. No multi-domain coordination needed in v1. |

## Open Questions

- **Domain lifecycle pattern** (AI Q2) — `std::optional` vs placement-new to be decided at implementation start.
- **BlueprintAssetHandler registration** (AI Q3) — needs a home in the boot sequence; flag at implementation time.
- **Editor IEntityInspectable wiring** — deferred to next spec (real editor example pending).

## Status

`Approved`
