# Feature Spec: editor-inspection

**System:** diaentitytemplate
**App:** Dia
**Status:** Draft

## Summary

Implement `IEntityInspectable` on `Domain`, providing the editor with read-only inspection (Tier a) and live field editing (Tier b) of entity state at runtime. All field access is driven by `ComponentTypeDesc` reflection metadata — no per-type editor code. Tier (c) live structural edit is deferred but the mutation pipeline remains capable of routing editor-originated commands when it is built.

## Traceability

| Level | Spec |
|---|---|
| Platform | [platform.md](../../../../platform/PLATFORM.md) |
| Application | [dia.md](../../../applications/dia.md) |
| System | [diaentitytemplate.md](../../systems/dia/diaentitytemplate.md) |
| Depends on feature | [foundation.md](foundation.md) |
| Depends on feature | [reflection.md](reflection.md) |
| Depends on feature | [mailbox-router.md](mailbox-router.md) |
| Depends on system | [diareflect.md](../../systems/dia/diareflect.md) |

## Goals

- Editor can enumerate all entities and their components without per-type code
- Editor can read and write individual component fields safely — type mismatches are recoverable errors, never crashes
- Mailbox log is accessible for debug tooling
- Tier (c) live structural edit remains unblocked for future addition

## Acceptance Criteria

- `Domain` implements `IEntityInspectable`
- **Tier (a) — Read-only:**
  - `GetEntityCount()` returns the current live entity count
  - `GetAllEntities(out)` fills a `DynamicArrayC<Entity, 1024>` with all live entity handles
  - `GetComponentTypeIds(entity, out)` fills a `DynamicArrayC<StringCRC, 32>` with all component type IDs attached to the entity
  - `ReadField(entity, componentTypeId, fieldName, out)` writes the field value as a `Json::Value` — returns false if entity/component/field not found
- **Tier (b) — Live field edit:**
  - `WriteField(entity, componentTypeId, fieldName, value)` updates the field in-place via `JsonReadArchive` on the single field
  - Type mismatch between `value` and `FieldDesc::kind` → `DIA_LOG_WARNING` + return false; never asserts
  - `WriteField` on a non-existent entity, component, or field → return false + `DIA_LOG_WARNING`
  - `WriteField` bypasses the mutation queue — applied immediately (editor writes are synchronous, not gameplay mutations)
- `GetMailbox()` returns a const reference to the domain's `Mailbox`
- Tier (c) live structural edit (`QueueAddComponent`, `QueueRemoveComponent`, `QueueDestroy` via inspector) is not exposed in v1 but the existing `QueueX` methods on `Domain` remain callable from any source when tier (c) is built

## Data Model

### IEntityInspectable

```cpp
namespace Dia::Entity {
    class IEntityInspectable {
    public:
        virtual ~IEntityInspectable() = default;

        // Tier (a) — Read-only
        virtual uint32_t GetEntityCount() const = 0;
        virtual void     GetAllEntities(Dia::Core::Containers::DynamicArrayC<Entity, kMaxEntitiesPerDomain>& out) const = 0;
        virtual void     GetComponentTypeIds(Entity entity,
                             Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>& out) const = 0;

        virtual bool ReadField(Entity entity,
                               Dia::Core::StringCRC componentTypeId,
                               const char* fieldName,
                               Json::Value& out) const = 0;

        // Tier (b) — Live field edit
        virtual bool WriteField(Entity entity,
                                Dia::Core::StringCRC componentTypeId,
                                const char* fieldName,
                                const Json::Value& value) = 0;

        // Mailbox access
        virtual const Dia::Mailbox::Mailbox& GetMailbox() const = 0;
    };
}
```

### ReadField / WriteField implementation notes

- Field lookup: linear scan of `ComponentTypeDesc::fields` by `strcmp(fieldName, desc.name)`. Field counts are <20; no hash needed.
- `ReadField`: constructs a temporary `JsonWriteArchive` scoped to the single field, serializes via the component's `saveToJson` thunk filtered to the named field, writes result to `out`.
- `WriteField`: constructs a temporary `JsonReadArchive` from a single-field JSON object, deserializes via `loadFromJson` thunk filtered to the named field. Type mismatch detected by comparing `FieldDesc::kind` against the `Json::Value::type()`.

## Files Touched

| File | Change |
|---|---|
| `Dia/diaentitytemplate/IEntityInspectable.h` | New — `IEntityInspectable` interface |
| `Dia/diaentitytemplate/Domain.h` | Modified — inherit `IEntityInspectable` |
| `Dia/diaentitytemplate/Domain.cpp` | Modified — implement all `IEntityInspectable` methods |
| `diaentitytemplate.vcxproj` / `.filters` | Add new files |
| `Tests/GoogleTests/Entity/EditorInspectionTests.cpp` | New |

## Binding Decisions Compliance

| Decision | Summary | Compliance |
|---|---|---|
| PD-001 | StringCRC for all IDs | `GetComponentTypeIds` returns `StringCRC`. `ReadField`/`WriteField` take `StringCRC componentTypeId`. Compliant. |
| PD-004 | No STL in public APIs | `GetAllEntities` and `GetComponentTypeIds` use `DynamicArrayC`. `ReadField`/`WriteField` use `Json::Value` (blessed). No STL. Compliant. |
| SD-ENT-004 | Reflection metadata required | `ReadField`/`WriteField` driven entirely by `ComponentTypeDesc`. No per-type editor code. Compliant. |
| SD-ENT-016 | Editor inspection v1: read-only + live field edit; structural edit deferred | Tier (a) + (b) implemented. Tier (c) deferred. `QueueX` methods remain public on `Domain` for future tier (c) wiring. Compliant. |
| SD-ENT-020 | Live structural edit pipeline must remain capable | `WriteField` uses immediate apply (bypasses queue). Future tier (c) will call existing `QueueX` methods — no pipeline rework needed. Compliant. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | `WriteField` immediate vs queued | `WriteField` bypasses the mutation queue and applies immediately. Does this risk breaking query caches or iterator stability mid-frame? | Editor writes happen outside the gameplay update loop — the editor polls `IEntityInspectable` between frames, not during `Domain::Update`. Immediate apply is safe in that context. If an editor write occurs mid-frame (future tier c scenario), it should route through `QueueX`. Document this constraint in `IEntityInspectable.h`. |
| 2 | `ReadField` single-field serialization | Filtering `saveToJson` to a single named field requires either a custom archive or post-filtering the full JSON output. Which approach? | Post-filter: call `saveToJson` to get the full component JSON, then extract the named field. Slightly wasteful but simple and correct. A single-field archive is an optimisation deferred to if profiling shows it matters. |
| 3 | `GetAllEntities` with 1024-entity cap | `DynamicArrayC<Entity, kMaxEntitiesPerDomain>` is a 1024-entry array passed by reference. Stack size? | `Entity` is 8 bytes (64-bit handle). 1024 × 8 = 8KB. Passed by reference — lives on the caller's stack (editor code, not per-frame gameplay). Acceptable. |
| 4 | `WriteField` on `EntityRef` fields | Writing an `EntityRef` field via JSON — the value is an entity name string, but at runtime we need a handle. How does `WriteField` resolve it? | `WriteField` on an `EntityRef` field accepts a JSON string (entity debug name), looks up the entity by debug name in the domain (`GetDebugName` reverse lookup), and patches the `EntityRef::entity` field directly. If no matching debug name found → `DIA_LOG_WARNING` + return false. Debug-name reverse lookup is O(N) — acceptable for editor use. |
| 5 | Tier (c) future proofing | The spec says "QueueX methods remain callable from any source." Is there anything in this feature's implementation that would accidentally close that door? | No — `IEntityInspectable` is a pure interface. `Domain`'s `QueueX` methods are public and unaffected by this feature. When tier (c) is built, new virtual methods are added to `IEntityInspectable` that call through to the existing `QueueX` methods. No rework needed. |

## Open Questions

None.

## Status

`Approved`
