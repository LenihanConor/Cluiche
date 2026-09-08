# Feature Spec: blueprint-loader

**System:** diaentitytemplate
**App:** Dia
**Status:** Draft

## Summary

Provide `IBlueprintLoader` and `JsonBlueprintLoader` to instantiate entity graphs in a `Domain` from versioned JSON. A three-pass resolution strategy handles forward references between entities. Asset-trigger fields are discovered via `AssetRefAttribute` (DiaReflect Phase 3a) and fire `OnAssetLoaded` callbacks as assets resolve.

## Traceability

| Level | Spec |
|---|---|
| Platform | [platform.md](../../../../platform/Cluiche.md) |
| Application | [dia.md](../../dia.md) |
| System | diaentitytemplate |
| Depends on feature | [foundation.md](foundation.md) |
| Depends on feature | [reflection.md](reflection.md) |
| Depends on system | [diareflect.md](../diareflect/diareflect.md) |

## Goals

- Load a versioned JSON blueprint into a `Domain` without hand-written per-type deserialization
- Handle forward entity references cleanly via a three-pass strategy
- Discover asset-trigger fields automatically via `AssetRefAttribute` — no per-type loader code
- Keep the loader interface-based so a future `UsdBlueprintLoader` can replace it without diaentitytemplate API churn

## Acceptance Criteria

- `IBlueprintLoader::Load(Domain&, const Json::Value&)` is the single entry point
- `JsonBlueprintLoader` reads a v1 blueprint JSON, instantiates all entities, and queues component attachments
- Components are queued in `REQUIRES` dependency order so validation in `QueueAddComponent` always passes
- Pass 1: instantiate all entities + queue component attachments with `EntityRef` fields set to `Entity::Invalid()`
- Pass 2: walk the `references` block and patch each named `EntityRef` field on the correct component
- Pass 3: validate every `EntityRef` field that is required (carries `DIA_FIELD_REQUIRED`) is non-null — missing reference is a fatal load error via `DIA_ASSERT`
- `EndOfFrame` is called by the caller after `Load`; the loader does not call it
- Fields carrying `AssetRefAttribute` are enumerated after JSON load; the loader calls `AssetService::Request(assetId)` for each and the domain tracks pending load count
- `OnAssetLoaded` is called on the component as each asset resolves; domain reports ready when pending count reaches zero
- Schema version mismatch with no registered migration function → `DIA_ASSERT` + return false
- Unknown component type name in blueprint → `DIA_ASSERT` + skip entity (logged via `DIA_LOG_WARNING`)

## Blueprint JSON Schema (v1)

```json
{
  "version": 1,
  "entities": {
    "player": {
      "transform": { "position": [0, 0] },
      "health":    { "max": 100 }
    },
    "sword": {
      "transform": { "position": [1, 0] },
      "blade":     { "damage": 25 }
    }
  },
  "references": [
    { "entity": "player", "component": "weapon-slot", "field": "target", "ref": "sword" }
  ]
}
```

- `version` — integer; checked against a loader-side constant; mismatch asserts if no migration registered
- `entities` — named map; key is the entity's local name (used only for reference resolution, not stored at runtime)
- Each entity value is a flat map of component type name → component config object
- `references` — array of wiring entries; each entry names the target entity, component, field, and referenced entity name
- Entity names are blueprint-local only; not persisted to `Domain` debug names (though the loader does set the entity's debug name to the blueprint key as a convenience)

## Data Model

### IBlueprintLoader

```cpp
namespace Dia::Entity {
    class IBlueprintLoader {
    public:
        virtual ~IBlueprintLoader() = default;

        // Returns true if all entities instantiated and all required refs resolved.
        // Caller must call Domain::EndOfFrame() after Load to apply queued mutations.
        virtual bool Load(Domain& domain, const Json::Value& blueprint) = 0;
    };
}
```

### JsonBlueprintLoader

```cpp
namespace Dia::Entity {
    class JsonBlueprintLoader final : public IBlueprintLoader {
    public:
        bool Load(Domain& domain, const Json::Value& blueprint) override;

    private:
        // Pass 1: instantiate entities + queue component attachments
        bool InstantiateEntities(Domain& domain, const Json::Value& entities,
                                 Dia::Core::Containers::DynamicArrayC<EntityNameHandle, kMaxEntitiesPerDomain>& outHandles);

        // Pass 2: patch EntityRef fields from references block
        bool PatchReferences(Domain& domain, const Json::Value& references,
                             const Dia::Core::Containers::DynamicArrayC<EntityNameHandle, kMaxEntitiesPerDomain>& handles);

        // Pass 3: validate required EntityRef fields are non-null
        bool ValidateReferences(Domain& domain,
                                const Dia::Core::Containers::DynamicArrayC<EntityNameHandle, kMaxEntitiesPerDomain>& handles);
    };

    // Internal — blueprint-local name → Entity handle mapping
    struct EntityNameHandle {
        Dia::Core::StringCRC name;
        Entity               entity;
    };
}
```

### Asset trigger flow

After Pass 1, the loader walks each component's `ComponentTypeDesc::fields` array for entries where `FieldDesc::kind == FieldKind::AssetHandle` and the field carries an `AssetRefAttribute`. For each such field the loader calls `AssetService::Request(assetId)` and increments the domain's pending-asset counter. When an asset resolves, `AssetService` calls back into the domain which calls `IComponent::OnAssetLoaded` on the owning component and decrements the counter.

## Files Touched

| File | Change |
|---|---|
| `Dia/diaentitytemplate/IBlueprintLoader.h` | New — `IBlueprintLoader` interface |
| `Dia/diaentitytemplate/JsonBlueprintLoader.h` | New — `JsonBlueprintLoader` declaration |
| `Dia/diaentitytemplate/JsonBlueprintLoader.cpp` | New — three-pass implementation |
| `Dia/diaentitytemplate/Domain.h` / `.cpp` | Modified — pending-asset counter, `OnAssetLoaded` dispatch |
| `diaentitytemplate.vcxproj` / `.filters` | Add new files |
| `Tests/GoogleTests/Entity/BlueprintLoaderTests.cpp` | New — round-trip tests, ref resolution, version mismatch, unknown type |

## Binding Decisions Compliance

| Decision | Summary | Compliance |
|---|---|---|
| PD-001 | StringCRC for all IDs | Entity names resolved to `StringCRC` internally. Component type lookup via `ComponentRegistry::Find(StringCRC)`. Compliant. |
| PD-004 | No STL in public APIs | `IBlueprintLoader::Load` takes `Json::Value` (blessed jsoncpp type). Internal `EntityNameHandle` array uses `DynamicArrayC`. No STL. Compliant. |
| PD-007 | C++20 | No C++20-specific features required here beyond what foundation/reflection already use. Compliant. |
| SD-ENT-004 | Reflection metadata required | Loader uses `ComponentTypeDesc::loadFromJson` thunk (set up in reflection feature) — no per-type loader code. Compliant. |
| SD-ENT-006 | Blueprint format versioned JSON; loader interface-based | `IBlueprintLoader` is the interface. `JsonBlueprintLoader` is the concrete impl. Schema v1 defined here. Version field checked on load. Compliant. |
| SD-ENT-007 | `REQUIRES` hard-validated at attach | Loader queues components in dependency order to ensure required components are queued before dependents. Compliant. |
| SD-ENT-008 | `EntityRef<TComponent>` validated at resolve | Pass 2 patches refs; Pass 3 validates required refs non-null. Compliant. |
| SD-ENT-012 | Structural changes queued at EndOfFrame | Loader calls `QueueAddComponent`; caller calls `EndOfFrame`. Loader never calls `EndOfFrame` itself. Compliant. |
| SD-ENT-013 | Entity slot allocation immediate | `CreateEntity` called in Pass 1; handle available immediately for reference wiring. Compliant. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Dependency ordering in Pass 1 | Components must be queued in `REQUIRES` dependency order. How does the loader determine this order from the blueprint? | The loader does a topological sort of the component set per entity using `ComponentTypeDesc::requires_`. If a cycle is detected (impossible by SD-ENT-007 design but defensive), it asserts. For v1 with small component counts (<20 per entity) this is a simple iterative pass. |
| 2 | Unknown component type | Blueprint references a component type name not in `ComponentRegistry`. | `DIA_ASSERT` in debug. In release: `DIA_LOG_WARNING`, skip that component, continue loading remaining components. Entity is created but incomplete — this mirrors the schema-mismatch policy. |
| 3 | `EntityNameHandle` array size | `DynamicArrayC<EntityNameHandle, kMaxEntitiesPerDomain>` — 1024 entries on the stack during Load. Is that acceptable? | At ~12 bytes per entry (4 CRC + 8 handle), 1024 entries = ~12KB stack. Acceptable for a load-time call (not per-frame). If this becomes a concern, move to a heap-backed array — but not a v1 concern. |
| 4 | References block entity/component not found | A `references` entry names an entity or component that doesn't exist in the blueprint. | `DIA_ASSERT` in debug. In release: `DIA_LOG_WARNING` + skip the reference entry. Pass 3 will then catch any resulting null required refs and assert. |
| 5 | Asset service coupling | Loader calls `AssetService::Request(assetId)`. Is `AssetService` a diaentitytemplate dependency or passed in? | Passed in as a pointer/interface to `JsonBlueprintLoader`'s constructor — keeps diaentitytemplate free of a hard `AssetService` dependency. If null (e.g. in unit tests), asset trigger fields are enumerated but no requests are made; `OnAssetLoaded` is never called. |
| 6 | Blueprint local names vs debug names | Entity blueprint keys (e.g. `"player"`) are set as the entity's debug name for convenience. Does this create a runtime dependency on the blueprint key string? | Debug names are debug-build only (`#ifdef`) and stored as a copy in the domain's internal buffer. No runtime dependency. In release the name is discarded entirely. |
| 7 | Multiple blueprints into one domain | Can `Load` be called multiple times on the same domain? | Yes — each call appends entities. Entity names are blueprint-local and not stored at runtime, so there is no collision risk between blueprints. Callers are responsible for calling `EndOfFrame` between loads if they need prior entities live before the next load. |

## Open Questions

None.

## Status

`Approved`
