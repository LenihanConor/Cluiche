# Feature Spec: component-deps-and-refs

**System:** diaentitytemplate
**App:** Dia
**Status:** Draft

## Summary

Define `EntityRef<TComponent>` — the typed cross-entity reference slot used in component fields. Provides a struct wrapping a plain `Entity` with a C++20 concept constraint, a resolve-time validation method, and serialization support via DiaReflect so it works naturally inside `FIELD` declarations.

## Traceability

| Level | Spec |
|---|---|
| Platform | [platform.md](../../../../platform/PLATFORM.md) |
| Application | [dia.md](../../../applications/dia.md) |
| System | [diaentitytemplate.md](../../systems/dia/diaentitytemplate.md) |
| Depends on feature | [foundation.md](foundation.md) |
| Depends on feature | [reflection.md](reflection.md) |
| Depends on feature | [blueprint-loader.md](blueprint-loader.md) |

## Goals

- Provide a type-safe cross-entity reference that is validated once at resolve time, not at every dereference
- Work transparently inside `FIELD` declarations via DiaReflect serialization
- Catch cross-domain misuse in debug builds without runtime cost in release

## Acceptance Criteria

- `EntityRef<TComponent>` is a struct wrapping a plain `Entity`, constrained by a C++20 concept requiring `TComponent` derives from `IComponent`
- Default-constructed `EntityRef` holds `Entity::Invalid()`
- `EntityRef::Resolve(Domain&)` calls `domain.HasComponent<TComponent>(entity)` — returns true if valid, false otherwise; never asserts on an invalid handle
- `EntityRef::Resolve` fires a `DIA_ASSERT` in debug if the held entity handle was allocated from a different domain instance (cross-domain misuse); no-op check in release
- After a successful `Resolve`, callers use `domain.GetComponent<TComponent>(ref.entity)` directly — no extra indirection layer
- `FIELD(EntityRef<TransformComponent>, target, {})` compiles and round-trips through `JsonReadArchive`/`JsonWriteArchive` — serialized as a string (blueprint-local entity name); resolved to an `Entity` handle during blueprint Pass 2
- An unresolved `EntityRef` (still `Entity::Invalid()` after Pass 2) is caught by blueprint Pass 3 if the field carries `DIA_FIELD_REQUIRED`

## Data Model

### Concept

```cpp
namespace Dia::Entity {
    template<class T>
    concept ComponentType = std::is_base_of_v<IComponent, T>;
}
```

### EntityRef

```cpp
namespace Dia::Entity {
    template<ComponentType TComponent>
    struct EntityRef {
        Entity entity = Entity::Invalid();

        // Validates the held entity has TComponent attached in the given domain.
        // Debug: asserts if entity was not allocated from this domain.
        // Returns false (not assert) if entity is Invalid or component is absent.
        bool Resolve(const Domain& domain) const;

        bool IsValid() const { return entity != Entity::Invalid(); }
    };
}
```

### Serialization

`EntityRef<T>` is serialized as a plain `StringCRC` (the blueprint-local entity name). During `JsonWriteArchive` it writes the entity's debug name. During `JsonReadArchive` it reads the name string and stores it temporarily; the blueprint loader's Pass 2 replaces it with the resolved `Entity` handle.

A `DIA_SERIALIZE` block is provided for `EntityRef<T>` using a partial specialization pattern so `FIELD(EntityRef<T>, name, {})` compiles without any per-type registration.

## Files Touched

| File | Change |
|---|---|
| `Dia/diaentitytemplate/EntityRef.h` | New — `ComponentType` concept + `EntityRef<TComponent>` |
| `Dia/diaentitytemplate/EntityRef.inl` | New — `Resolve` implementation |
| `diaentitytemplate.vcxproj` / `.filters` | Add new files |
| `Tests/GoogleTests/Entity/EntityRefTests.cpp` | New — resolve, invalid handle, cross-domain assert, FIELD round-trip |

## Binding Decisions Compliance

| Decision | Summary | Compliance |
|---|---|---|
| PD-004 | No STL in public APIs | `EntityRef` wraps a plain `Entity` (alias for `Handle<EntityTag>`). No STL types. Compliant. |
| PD-007 | C++20 | Uses C++20 `concept` for `ComponentType` constraint. Compliant. |
| SD-ENT-008 | `EntityRef<TComponent>` validated at resolve | `Resolve` validates once via `HasComponent`. Not validated at every dereference. Compliant. |
| SD-ENT-018 | Single-threaded per domain | `Resolve` takes a `const Domain&` — no thread concerns in v1. Compliant. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Cross-domain detection | How does `Resolve` detect a cross-domain entity handle in debug? | `Domain` assigns itself a unique ID (a `uint32_t` counter incremented at construction, stored as a member). `Entity` handles encode this domain ID in a debug-only field (present only in `#ifndef DIA_RELEASE`). `Resolve` compares them and `DIA_ASSERT`s on mismatch. Zero cost in release — the domain ID field is compiled out. |
| 2 | Serialization of `EntityRef` during `JsonWriteArchive` | Write path needs the entity's blueprint name, but at runtime the domain only stores a debug name (debug builds only). What does the write thunk emit in release? | In release, write the entity handle's index as a string (e.g. `"entity:42"`). This is sufficient for debug tooling but not human-readable. If human-readable blueprints are needed in release, the caller must maintain a name→handle map externally. V1 does not require this. |
| 3 | `EntityRef` default in `FIELD` | `FIELD(EntityRef<T>, name, {})` — brace-init default. Does this work with the macro expansion? | Yes — `EntityRef<T>{}` default-constructs to `Entity::Invalid()`. The `FIELD` macro stores the default via C++ member initializer syntax, which handles brace-init correctly. Compliant. |
| 4 | Partial specialization for DIA_SERIALIZE | `EntityRef<T>` is a template. How does DiaReflect handle its serialization without per-type registration? | A single `DIA_SERIALIZE` free function template on `EntityRef<T>` covers all instantiations. The archive concept's `operator&` dispatches to this template. No per-component-type registration needed. |

## Open Questions

None.

## Status

`Approved`
