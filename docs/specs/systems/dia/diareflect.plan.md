# Implementation Plan: DiaReflect

**Spec:** [diareflect.md](diareflect.md)
**Status:** Not Started
**Created:** 2026-05-21

## Session Notes

**Spec decisions summary:** DiaReflect uses the C7 archive pattern with macro-DSL registration (SD-REFLECT-001). No global registry for value types (SD-REFLECT-002). Circular references out of scope (SD-REFLECT-003). Defaults from C++ member initializers (SD-REFLECT-004). Binary format: 4-byte field CRC + raw bytes, 2-byte version header (SD-REFLECT-005). Eventual replacement for DiaCore/Type (SD-REFLECT-006). Platform constraints: StringCRC for field IDs (PD-001), no STL in public API (PD-004), x64 little-endian only (PD-005), VS project files (PD-006), C++20 (PD-007). Fixed-capacity containers in public API (SD-CORE-001). Header-only `.h`+`.inl` pattern (SD-CORE-002).

## Implementation Patterns

### Phase 1: Foundation (F1–F4)

**Archive Concept (`Dia::Reflect::Archive`):**
- C++20 `concept` requiring `operator&(NamedField<T>&)` — direction (read/write) is part of the archive type, not the concept
- `NamedField<T>` wraps field name (StringCRC) + reference to value
- `OwnedPtrField<T>` and `RefIdField<T>` for pointer semantics
- `SerializeResult` — DynamicArrayC of errors, success if empty

**Macro DSL pattern:**
```cpp
#define DIA_SERIALIZE(Type, Version) \
    template<class Archive> \
    void serialize(Archive& ar, Type& obj, unsigned version = Version) {

#define DIA_FIELD(member) \
    ar & Dia::Reflect::named(#member, obj.member);

#define DIA_FIELD_REQUIRED(member) \
    ar & Dia::Reflect::named(#member, obj.member).required();

#define DIA_FIELD_OWNED_PTR(member) \
    ar & Dia::Reflect::owned(#member, obj.member);

#define DIA_FIELD_REF_ID(member) \
    ar & Dia::Reflect::refId(#member, obj.member);

#define DIA_BASE(BaseType) \
    serialize(ar, static_cast<BaseType&>(obj), BaseType::kVersion);

#define DIA_SERIALIZE_END }
```

**JSON archives:**
- `JsonWriteArchive` — builds `Json::Value` tree; `operator&` appends to current object node
- `JsonReadArchive` — walks existing `Json::Value`; missing fields → no-op (keeps default)
- Nested objects push/pop a node stack

**Binary archives:**
- `BinaryWriteArchive` — appends to `DynamicArrayC<uint8_t, kBufferSize>`; per-field: 4-byte CRC + value bytes
- `BinaryReadArchive` — scans buffer by field CRC; tolerates unknown CRCs (skip by size prefix)
- 2-byte version header at start of each type blob

### Phase 2: Containers & Inheritance (F5–F6)

**Container specializations:**
- `T[N]` — detect via `std::is_bounded_array_v<T>`, iterate N elements
- `DynamicArrayC<T,N>` — write count + elements; read count, resize, fill
- `HashTableC<K,V,N>` — write as array of {key, value} pairs; read and insert

**Inheritance:**
- `DIA_BASE(BaseType)` simply calls `serialize(ar, static_cast<Base&>(obj), baseVersion)`
- Base and derived have independent version numbers

**Polymorphic registry:**
- `PolymorphicRegistry` singleton — `DynamicArrayC<PolyEntry, 256>` mapping CRC → factory + serialize fn
- `DIA_SERIALIZE_POLYMORPHIC(Concrete, Base, Version)` registers at first use (function-local static)
- JSON: `{"_type": "ConcreteType", ...fields...}`
- Binary: 4-byte type CRC before field data

### Phase 3: Attributes & Migration (F7–F8)

**Attributes:**
- `FieldAttribute` base — polymorphic chain per field (runtime queryable)
- `RequiredAttribute` — archive checks presence, adds to error list if missing
- `RangeAttribute<T>` — clamp or error on out-of-range (template on arithmetic type)
- `AssetRefAttribute` — metadata tag (type CRC), no serialization behavior
- `DIA_FIELD_ATTR(AttrType, ...)` macro stashes attribute on current field in a thread-local builder

**Migration shim (DiaCore/Type coexistence):**
- Adapter class: `ReflectTypeDefinition<T>` wraps a DiaReflect-registered type as a `TypeDefinition`
- DiaAssetCatalogue can load types through this adapter without code changes
- Old macros (`DIA_TYPE_DECLARATION` / `DIA_TYPE_DEFINITION`) remain functional until all types migrate
- Migration order: DiaMaths → DiaCore containers → DiaPhysics → DiaAssetCatalogue (last)

## Task Table

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| **Phase 1 — Foundation** | | | | | |
| 1 | Create `Dia/DiaCore/Reflect/` directory + module doc + vcxproj entries | Build passes | Not Started | haiku | New module within DiaCore |
| 2 | Archive concept + `NamedField<T>`, `OwnedPtrField<T>`, `RefIdField<T>`, `SerializeResult` | Unit tests: concept check, field wrapper construction | Not Started | sonnet | Core types — TDD |
| 3 | Macro DSL (`DIA_SERIALIZE`, `DIA_FIELD`, `DIA_FIELD_REQUIRED`, `DIA_FIELD_OWNED_PTR`, `DIA_FIELD_REF_ID`, `DIA_BASE`, `DIA_SERIALIZE_END`) | Unit tests: macro expansion compiles, produces correct function signature | Not Started | sonnet | Thin macros over #2 |
| 4 | `JsonWriteArchive` + `JsonReadArchive` | Unit tests: round-trip arithmetic, nested struct, missing field keeps default, required field errors | Not Started | sonnet | First usable archives — TDD |
| 5 | `BinaryWriteArchive` + `BinaryReadArchive` | Unit tests: round-trip, version header, unknown field skip, size correctness | Not Started | sonnet | Second format — TDD |
| **Phase 2 — Containers & Inheritance** | | | | | |
| 6 | Static array (`T[N]`) archive specialization | Unit tests: write/read int[4], struct[3], empty array | Not Started | sonnet | `std::is_bounded_array_v` |
| 7 | `DynamicArrayC<T,N>` archive specialization | Unit tests: write/read varying sizes, empty, full capacity | Not Started | sonnet | |
| 8 | `HashTableC<K,V,N>` archive specialization | Unit tests: write/read, key ordering in JSON | Not Started | sonnet | |
| 9 | Inheritance (`DIA_BASE`) integration | Unit tests: derived serializes base fields, independent versioning | Not Started | sonnet | |
| 10 | Polymorphic registry + `DIA_SERIALIZE_POLYMORPHIC` | Unit tests: register, deserialize by type tag, unknown type returns error | Not Started | opus | Most complex feature |
| **Phase 3 — Attributes & Migration** | | | | | |
| 11 | Field attributes system (`RequiredAttribute`, `RangeAttribute<T>`, `AssetRefAttribute`) | Unit tests: required fires error, range clamps, asset ref queryable | Not Started | sonnet | |
| 12 | `DIA_FIELD_ATTR` macro + attribute builder | Unit tests: macro compiles, attributes discoverable at runtime | Not Started | sonnet | |
| 13 | Migration adapter (`ReflectTypeDefinition<T>`) | Unit tests: adapter exposes DiaReflect type as TypeDefinition, JsonDefinitionLoader can load it | Not Started | opus | Bridge to old system |
| 14 | Migrate DiaMaths types (Vector2D, Matrix22, etc.) | Existing DiaMaths tests still pass + new round-trip tests | Not Started | sonnet | First real migration |
| 15 | Documentation + remove DiaCore/Type dead code (when all migrated) | Build passes, no references to old macros | Not Started | haiku | Final cleanup |
