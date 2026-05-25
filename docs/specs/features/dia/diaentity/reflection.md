# Feature Spec: reflection

**System:** DiaEntity
**App:** Dia
**Status:** Draft

## Summary

Add compile-time reflection to DiaEntity components via `DIA_COMPONENT` + `FIELD` macros. Every component type declares its serialisable fields and dependency requirements through these macros; the resulting `ComponentTypeDesc` drives JSON load/save (via DiaReflect) and editor inspection. `ComponentRegistry` provides process-global lookup and iteration over all registered component types.

## Traceability

| Level | Spec |
|---|---|
| Platform | [platform.md](../../../../platform/PLATFORM.md) |
| Application | [dia.md](../../../applications/dia.md) |
| System | [diaentity.md](../../systems/dia/diaentity.md) |
| Depends on feature | [foundation.md](foundation.md) |
| Depends on system | [diareflect.md](../../systems/dia/diareflect.md) |

## Goals

- Every component type self-describes via `DIA_COMPONENT` + `FIELD` macros in the class body
- JSON load/save is automatic — delegated to DiaReflect `JsonReadArchive`/`JsonWriteArchive`; no per-type hand-written serialization
- `FIELD` accepts any type `T` that has a `DIA_SERIALIZE` block (open set, not a closed enum)
- `REQUIRES(OtherComponent)` is declared alongside fields; missing dependencies are fatal at attach
- `ComponentRegistry` supports editor/debug iteration over all registered types

## Acceptance Criteria

- `DIA_COMPONENT(ClassName, "string-name")` emits a static `StringCRC` type ID and a static `ComponentTypeDesc` with correct name, size, alignment, schema version, and field list
- `FIELD(type, name, default)` appends a `FieldDesc` to the component's field array
- `FIELD` works for any `T` with a `DIA_SERIALIZE` block — primitives, math types, `StringCRC`, nested structs, containers, asset handles, entity refs
- `DIA_COMPONENT_REGISTER(ClassName)` placed in a `.cpp` registers the desc with `ComponentRegistry` at static init; no ODR violations
- `ComponentRegistry::Find(typeId)` returns the registered desc; returns nullptr for unknown types
- `ComponentRegistry::GetCount()` / `GetByIndex(i)` allow full iteration
- JSON load thunk reads all declared fields via `JsonReadArchive`; missing fields keep their C++ defaults
- JSON save thunk writes all declared fields via `JsonWriteArchive`
- `REQUIRES(OtherComponent)` appends the required type CRC to the desc's dependency list
- `Domain::QueueAddComponent` asserts (debug) / logs + bails (release) if a required component is absent on the entity at attach time
- Schema version field on `ComponentTypeDesc` is set from the version arg in `DIA_COMPONENT`; stored in saved JSON for future migration

## Data Model

### FieldKind — category hint for editor widget rendering

```cpp
namespace Dia::Entity {
    enum class FieldKind : uint8_t {
        Primitive,   // bool, int, uint, float
        StringCRC,
        Math,        // Vec2, Vec3, Quat, Mat44
        AssetHandle,
        EntityRef,
        Nested,      // any struct with DIA_SERIALIZE — editor renders as collapsible sub-object
        Container,   // DynamicArrayC / T[N] — editor renders as list
    };
}
```

### FieldDesc

```cpp
namespace Dia::Entity {
    struct FieldDesc {
        const char*  name;       // field name as declared in FIELD macro
        uint32_t     nameCrc;    // CRC of name — for WriteField lookup
        uint16_t     offset;     // byte offset from component base pointer
        FieldKind    kind;       // editor hint
        bool         required;   // maps to DIA_FIELD_REQUIRED in DiaReflect
    };
}
```

### ComponentTypeDesc

```cpp
namespace Dia::Entity {
    using DefaultCtorFn  = void (*)(void* placement);
    using LoadFromJsonFn = void (*)(IComponent* dst, const Json::Value& config);
    using SaveToJsonFn   = void (*)(const IComponent* src, Json::Value& outConfig);

    struct ComponentTypeDesc {
        Dia::Core::StringCRC typeId;
        const char*          debugName;
        uint16_t             size;
        uint16_t             alignment;
        uint16_t             schemaVersion;
        uint16_t             flags;           // kFlagOverridesDoUpdate, etc.

        const FieldDesc*     fields;
        uint16_t             fieldCount;

        // Required component type CRCs (from REQUIRES macro)
        const Dia::Core::StringCRC* requires_;
        uint16_t                    requiresCount;

        DefaultCtorFn        defaultConstruct;
        LoadFromJsonFn       loadFromJson;    // delegates to JsonReadArchive
        SaveToJsonFn         saveToJson;      // delegates to JsonWriteArchive
    };
}
```

### ComponentRegistry

```cpp
namespace Dia::Entity {
    class ComponentRegistry {
    public:
        static ComponentRegistry& Get();

        bool Register(const ComponentTypeDesc& desc);        // called from DIA_COMPONENT_REGISTER
        const ComponentTypeDesc* Find(Dia::Core::StringCRC typeId) const;

        uint32_t             GetCount() const;
        const ComponentTypeDesc& GetByIndex(uint32_t i) const;

    private:
        Dia::Core::Containers::DynamicArrayC<const ComponentTypeDesc*, kMaxComponentTypesPerDomain> mDescs;
    };
}
```

### Macro DSL

```cpp
// In ComponentFoo.h:
//
// class ComponentFoo : public Dia::Entity::IComponent {
// public:
//     DIA_COMPONENT(ComponentFoo, "foo", 1)   // ClassName, string-name, schema version
//
//     FIELD(float,               speed,     1.0f)
//     FIELD(Dia::Core::StringCRC, tag,      StringCRC{})
//     FIELD(Dia::Maths::Vector2D, offset,   Dia::Maths::Vector2D{})
//
//     REQUIRES(TransformComponent)
//
//     void DoUpdate(Domain&, Entity, float dt) override;
// };
//
// In ComponentFoo.cpp:
//   DIA_COMPONENT_REGISTER(ComponentFoo);
//
// DIA_COMPONENT expands to:
//   - static constexpr StringCRC kTypeId = StringCRC{"foo"};
//   - static constexpr uint16_t kVersion = 1;
//   - StringCRC GetTypeId() const override { return kTypeId; }
//   - static const ComponentTypeDesc& GetDesc();
//   - static FieldDesc sFields[];          (populated by FIELD macros)
//   - static StringCRC sRequires[];        (populated by REQUIRES macros)
//   - DIA_SERIALIZE block that calls DIA_FIELD for each FIELD entry
//
// DIA_COMPONENT_REGISTER expands to:
//   - A function-local static bool that calls ComponentRegistry::Get().Register(T::GetDesc())
//     on first use — ODR-safe, no global constructor ordering issues.
```

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaEntity/ComponentTypeDesc.h` | New — `FieldDesc`, `FieldKind`, `ComponentTypeDesc` |
| `Dia/DiaEntity/ComponentRegistry.h` | New — `ComponentRegistry` class |
| `Dia/DiaEntity/ComponentRegistry.cpp` | New — singleton implementation |
| `Dia/DiaEntity/ComponentMacros.h` | New — `DIA_COMPONENT`, `FIELD`, `REQUIRES`, `DIA_COMPONENT_REGISTER` |
| `Dia/DiaEntity/Domain.cpp` | Modified — `QueueAddComponent` validates `REQUIRES` at attach |
| `DiaEntity.vcxproj` / `.filters` | Add new files |
| `Tests/GoogleTests/Entity/ReflectionTests.cpp` | New — macro expansion, registry, JSON round-trip tests |

## Binding Decisions Compliance

| Decision | Summary | Compliance |
|---|---|---|
| PD-001 | StringCRC for all IDs | Component type IDs are `StringCRC` via `DIA_COMPONENT`. Field name CRCs stored in `FieldDesc::nameCrc`. `REQUIRES` stores `StringCRC` dependency list. Compliant. |
| PD-004 | No STL in public APIs | `ComponentRegistry` uses `DynamicArrayC`. `ComponentTypeDesc` uses raw arrays + counts. No STL. Compliant. |
| PD-007 | C++20 | `DIA_COMPONENT` emits a `DIA_SERIALIZE` block using DiaReflect's C++20 `Archive` concept. Compliant. |
| SD-ENT-004 | Reflection metadata required for every component | This feature implements the full reflection system. `DIA_COMPONENT` + `FIELD` are mandatory for any type registering with `ComponentRegistry`. Compliant. |
| SD-ENT-005 | Reflection is component-only, lives inside DiaEntity | `ComponentTypeDesc` and `ComponentRegistry` are in `Dia/DiaEntity/`. DiaReflect is used as a serialization engine only — its `Archive` concept and archives are a dependency, not ownership of the registry. Compliant. |
| SD-ENT-006 | Blueprint format versioned JSON; loader interface-based | `schemaVersion` on `ComponentTypeDesc` enables future migration. Compliant. |
| SD-ENT-007 | `REQUIRES` hard-validated at attach | `QueueAddComponent` checks `desc.requires_` array against `Domain::HasComponent` for each entry. Missing dep → `DIA_ASSERT` in debug, `DIA_LOG_WARNING` + bail in release. Compliant. |
| SD-ENT-012 | Structural changes queued at EndOfFrame | `REQUIRES` validation fires at queue time (inside `QueueAddComponent`), not at EndOfFrame — fail-fast before the op enters the queue. Compliant in spirit; failure is caught earlier. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | DIA_SERIALIZE integration | `DIA_COMPONENT` emits a `DIA_SERIALIZE` block. Does this mean every component header must include DiaReflect headers? | Yes — `ComponentMacros.h` includes DiaReflect's macro header. This is a compile-time dependency but not a link-time blocker. Acceptable given DiaReflect is a DiaCore-resident system. |
| 2 | FIELD default values | `FIELD(type, name, default)` — where are defaults stored? In the `DIA_SERIALIZE` block via C++ member initializers, or separately in `FieldDesc`? | In C++ member initializers on the component class, consistent with DiaReflect's SD-REFLECT-004. `FieldDesc` carries no default value. `JsonReadArchive` leaves fields untouched if absent, so the C++ default is preserved naturally. |
| 3 | `REQUIRES` validation timing | Validation fires inside `QueueAddComponent` before the op enters the mutation queue. If the required component is itself in the queue but not yet applied, validation fails. Is that correct? | Yes. Blueprint loader handles multi-component entities by queuing all attachments in dependency order (required components first). Callers that add components one-by-one must respect ordering. This is documented in the blueprint-loader feature. |
| 4 | `kMaxComponentTypesPerDomain` | `ComponentRegistry` uses `DynamicArrayC<const ComponentTypeDesc*, kMaxComponentTypesPerDomain>`. What's the cap? | `kMaxComponentTypesPerDomain = 64` (surfaced in foundation AI Q2). If exceeded, `Register` asserts. Sufficient for v1; revisit if real games push past it. |
| 5 | `DIA_COMPONENT_REGISTER` ODR safety | Function-local static bool pattern — safe across TUs? | Yes. Each `.cpp` that calls `DIA_COMPONENT_REGISTER(Foo)` gets its own function-local static. The registration itself is idempotent (registry checks for duplicate CRCs and asserts). No issues across TUs or shared libs in v1 (single process, static libs only). |
| 6 | `FieldKind::Nested` rendering | Editor gets `FieldKind::Nested` for structs with `DIA_SERIALIZE`. Does DiaEntity need to expose the nested type's field list for the editor to render sub-fields? | The editor can call `JsonReadArchive`/`JsonWriteArchive` on the nested field as a blob and render it opaquely, or DiaReflect's polymorphic registry can be consulted for richer rendering. V1: treat `Nested` as an opaque JSON sub-object in the editor. Richer sub-field rendering is a future editor feature. |
| 7 | `saveToJson` on const IComponent* | The thunk signature is `void (*)(const IComponent* src, Json::Value& outConfig)`. `JsonWriteArchive` requires a non-const reference. Does this require a `const_cast`? | Yes — `const_cast<IComponent*>(src)` inside the thunk. The write archive reads from the object; it does not mutate it. The const_cast is safe and isolated to the thunk. Document it with a comment in `ComponentMacros.h`. |

## Open Questions

None.

## Status

`Approved`
