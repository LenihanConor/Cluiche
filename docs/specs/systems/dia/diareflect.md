# System Spec: DiaReflect

## Parent Application
@docs/specs/applications/dia.md

## Summary

DiaReflect is the engine's reflection and serialization system, replacing the existing DiaCore/Type macro-based registration. It uses the **archive pattern** (C7 from research): each type provides a `serialize(Archive&, T&, version)` free function — expressed via a macro DSL for the common case — and the archive implementation determines format (JSON text, binary). The system handles value types, containers (static and dynamic), inheritance, polymorphic pointers, field attributes, versioning, and migration.

DiaReflect is an **eventual replacement** for DiaCore/Type. New types use DiaReflect; existing types migrate incrementally. Once migration is complete, the old macro system and TypeDefinition infrastructure are removed.

**Research:** [docs/research/reflect_serial/summary.md](../../../research/reflect_serial/summary.md)

## Responsibilities

**Owns:**
- Archive concept (`Dia::Reflect::Archive`) — C++20 concept constraining read/write archive types
- Archive implementations: `JsonWriteArchive`, `JsonReadArchive`, `BinaryWriteArchive`, `BinaryReadArchive`
- Macro DSL: `DIA_SERIALIZE`, `DIA_FIELD`, `DIA_FIELD_REQUIRED`, `DIA_FIELD_OWNED_PTR`, `DIA_FIELD_REF_ID`, `DIA_FIELD_ATTR`, `DIA_BASE`, `DIA_SERIALIZE_POLYMORPHIC`
- Container archive specializations (DynamicArrayC, HashTableC, static arrays)
- Polymorphic type registry (type CRC → factory function, opt-in only)
- Field attributes (Required, Range, AssetRef — extensible)
- Version migration (per-type version integer, in-function `if (version < N)` guards)

**Does NOT own:**
- Asset pipeline loading (DiaAssetCatalogue — uses DiaReflect eventually, but owns its own workflow)
- JSON parsing library (DiaCore/Json/ wraps jsoncpp — DiaReflect consumes it)
- Domain-specific serialization formats (e.g., .diagame, .diastage — those are domain serializers built on DiaReflect)
- Object lifetime / ownership semantics beyond what the archive writes

## Features

| # | Feature | Size | Description | Spec |
|---|---------|------|-------------|------|
| 1 | Archive Concept & Core Types | M | C++20 `Archive` concept, `named()` / `owned()` / `ref_id()` field wrappers, version header, `SerializeResult` | TBD |
| 2 | JsonWriteArchive & JsonReadArchive | M | JSON text format archives — human-readable output, round-trip, missing-field tolerance (keeps defaults) | TBD |
| 3 | BinaryWriteArchive & BinaryReadArchive | M | Binary format archives — 4-byte field CRC + raw value bytes, 2-byte version header, little-endian | TBD |
| 4 | Macro DSL | S | `DIA_SERIALIZE` / `DIA_FIELD` / `DIA_BASE` / etc. — syntactic sugar expanding to free functions | TBD |
| 5 | Container Specializations | S | Archive `operator&` specializations for `T[N]`, `DynamicArrayC<T,N>`, `HashTableC<K,V,N>` | TBD |
| 6 | Inheritance & Polymorphism | M | `DIA_BASE(BaseType)`, `DIA_SERIALIZE_POLYMORPHIC(Concrete, Base, version)`, type tag dispatch, factory registry | TBD |
| 7 | Field Attributes | S | Required validation, Range clamp/error, AssetRef metadata tag, extensible attribute system | TBD |
| 8 | Migration from DiaCore/Type | L | Incremental migration of existing types; shim layer if needed; removal of old macros when complete | TBD |

## Public API

### Namespace
`Dia::Reflect::`

### Key types
- `concept Archive` — requires `operator&(NamedField<T>)` for read or write direction
- `JsonWriteArchive`, `JsonReadArchive` — text format, wraps jsoncpp `Json::Value`
- `BinaryWriteArchive`, `BinaryReadArchive` — compact binary format
- `PolymorphicRegistry` — singleton mapping type CRC → factory + serialize function
- `SerializeResult` — success/failure with optional error detail

### Entry points
```cpp
// Write to JSON
Dia::Reflect::JsonWriteArchive ar;
serialize(ar, myObject, MyType::kVersion);
Json::Value json = ar.GetRoot();

// Read from JSON
Dia::Reflect::JsonReadArchive ar(jsonValue);
serialize(ar, myObject, MyType::kVersion);

// Write to binary
Dia::Reflect::BinaryWriteArchive ar;
serialize(ar, myObject, MyType::kVersion);
auto bytes = ar.GetBuffer();  // DynamicArrayC<uint8_t, N>
```

### Macro DSL
```cpp
// Simple type (80% case)
DIA_SERIALIZE(RigidBody2D, 1)
    DIA_FIELD(mPosition)
    DIA_FIELD(mVelocity)
    DIA_FIELD(mMass)
    DIA_FIELD_OWNED_PTR(mShape)
DIA_SERIALIZE_END

// With inheritance
DIA_SERIALIZE(DerivedBody, 2)
    DIA_BASE(RigidBody2D)
    DIA_FIELD(mExtraField)
DIA_SERIALIZE_END

// Polymorphic concrete type
DIA_SERIALIZE_POLYMORPHIC(CircleShape, ICollisionShape, 1)
    DIA_FIELD(mRadius)
DIA_SERIALIZE_END

// Escape hatch — custom versioned migration
template<class Archive>
void serialize(Archive& ar, LegacyType& obj, unsigned version) {
    if (version < 2) {
        float oldField;
        ar & named("oldField", oldField);
        obj.mNewField = convert(oldField);
    } else {
        ar & named("newField", obj.mNewField);
    }
}
```

## Dependencies

| Dependency | What DiaReflect uses from it |
|------------|------------------------------|
| DiaCore/Containers | DynamicArrayC, HashTableC — archive specializations; internal buffers |
| DiaCore/Strings | StringCRC — field name CRCs in binary format |
| DiaCore/Json | jsoncpp wrapper — JSON archive reads/writes Json::Value |
| DiaCore/Core | DIA_ASSERT, DIA_LOG_* |

No dependencies on any other Dia module. DiaReflect is a leaf library alongside DiaCore (or physically within it — placement TBD at implementation).

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-REFLECT-001 | Archive pattern (C7) with macro-DSL registration | Scored highest in research (3.95); native versioning, zero MSVC risk, familiar pattern, pointer-safe | All DiaReflect | Accepted | Yes |
| SD-REFLECT-002 | No global registry for value types | Macros are purely syntactic — no static-init registration. Only polymorphic types register (explicitly). | All DiaReflect | Accepted | Yes |
| SD-REFLECT-003 | Circular references explicitly out of scope | Users must break cycles with `DIA_FIELD_REF_ID`. Archive does not detect or handle cycles. | All DiaReflect | Accepted | Yes |
| SD-REFLECT-004 | Default values come from C++ member initializers | Archive only overwrites fields present in source data. Missing fields retain constructor defaults. | All DiaReflect | Accepted | Yes |
| SD-REFLECT-005 | Binary format: 4-byte field CRC + raw value bytes, 2-byte version header | Compact, CRC-identified fields enable forward-compat (skip unknown fields), aligns with PD-001 | Binary archives | Accepted | Yes |
| SD-REFLECT-006 | Eventual replacement of DiaCore/Type | New types use DiaReflect; existing types migrate incrementally; old system removed when done | DiaReflect + DiaCore/Type | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all child features · `No` = guidance only

## Inherited Binding Decisions

| Source | ID | Decision | How this system honours it |
|--------|-----|----------|---------------------------|
| Platform | PD-001 | StringCRC for all IDs | Field names stored as CRC in binary format; string form in JSON |
| Platform | PD-004 | No STL in public APIs | Archive API accepts DiaCore containers; no std::vector/std::map in signatures |
| Platform | PD-005 | x64 Windows only | Binary format is little-endian, no byte-swap layer |
| Platform | PD-006 | VS project files | No codegen step; all registration is in-source C++ |
| Platform | PD-007 | C++20 required | Archive concept uses `requires` clause; `if constexpr` for type dispatch |
| Platform | PD-008 | Directory.Build.props owns build paths | If DiaReflect is a separate .vcxproj, it follows shared output rules |
| Application | AD-001 | Module system with YAML frontmatter | DiaReflect module(s) will have `dia.*.architecture.module.md` files |
| Application | AD-002 | No STL in public APIs | Same as PD-004 — reinforced at application level |
| Application | AD-003 | Namespace `Dia::<Module>::` | Uses `Dia::Reflect::` namespace |
| DiaCore | SD-CORE-002 | Header-only via `.h` + `.inl` | Archive templates and macro expansions follow this pattern |
| DiaCore | SD-CORE-003 | StringCRC is canonical ID type | Field CRCs use StringCRC infrastructure |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Placement | Should DiaReflect be a separate .vcxproj or a module within DiaCore? | TBD — leaning toward within DiaCore (like Type/ today) since it has no deps beyond DiaCore internals. Separate .vcxproj only if other modules need DiaReflect without all of DiaCore. |
| 2 | Migration | How does migration coexist with DiaAssetCatalogue which depends on TypeDefinition? | Shim: DiaAssetCatalogue can consume DiaReflect-registered types via an adapter that exposes them as TypeDefinitions. Migrates last. |
| 3 | Containers | Should HashTableC serialization preserve insertion order or sort by key? | JSON: sort by key for deterministic output. Binary: write in iteration order (fastest). |
| 4 | Polymorphism | What happens if a type CRC is not in the registry during deserialization? | Return error in SerializeResult — do not assert. Caller decides whether to skip or fail. |
| 5 | Versioning | Can version numbers go backward (e.g., remove a field in v3, re-add differently in v4)? | Yes — version is just an integer; migration logic is arbitrary code in the function body. |
| 6 | Attributes | Are attributes compile-time only (macro metadata) or runtime queryable? | Runtime queryable — needed for editor tooling to discover field constraints without running serialization. |
| 7 | SD-CORE-001 | Fixed-capacity containers — does this apply to DiaReflect internal buffers? | Yes for public-facing buffers (SerializeResult, error lists). Binary archive write buffer may use a growable internal buffer with a configurable initial size. |
| 8 | Escape hatch | Can a type use BOTH the macro DSL and a raw serialize() function? | No — one or the other. The macro expands to the function; defining both is a linker error. Clear guidance in docs. |

## Out of Scope

- Circular reference detection or handling
- Partial/streaming deserialization (read subset of fields from a large blob)
- STL container support in public API
- Network serialization protocol (endianness negotiation, compression) — build on top of BinaryArchive
- Schema export for editor tooling (future feature, not in initial system)
- Codegen / build-time reflection tools

## Status

`Approved` — Plan: [diareflect.plan.md](diareflect.plan.md)
