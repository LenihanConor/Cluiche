# Research: Choose — Reflection & Serialization

**Input:** docs/research/reflect_serial/evaluate.md

## Decision

**Winner: C7 — Archive pattern (`serialize(Archive&, T&)` free function)**

Score: 3.95 (highest), with clear separation from runner-up C2 (3.60).

## Why C7

The archive pattern is the right fit for Cluiche's stated use cases:

1. **Versioning is native** — the `unsigned version` parameter is part of the function signature. Migration logic is just `if (version < N)` guards in the function body. No bolted-on schema layer needed.

2. **Binary and text from one registration** — write one `serialize()` function per type, swap the archive at the call site. JsonWriteArchive for debug dumps, BinaryWriteArchive for checkpoints/replays/network sync.

3. **Pointer handling is explicit** — `ar & owned("shape", ptr)` for owning pointers, `ar & ref_id("owner", handle)` for non-owning references. No magic — the author decides ownership semantics per field.

4. **Zero MSVC risk** — no constexpr member-pointer tricks, no non-type template parameters with member pointers. Just free functions, operator overloads, and C++20 concepts for the Archive constraint.

5. **Familiar pattern** — anyone who has seen cereal or Boost.Serialize can read and extend this immediately. Reduces onboarding cost.

6. **Incremental adoption** — types opt in one at a time. The existing DiaAssetCatalogue/TypeDefinition system continues working for config loading; new `serialize()` functions are added for types that need state serialization.

## Why not the runner-ups

| Candidate | Reason to pass |
|-----------|----------------|
| C2 (describe free fn) | Strong design but versioning is not first-class — would need a separate migration layer. The `auto` return type makes documentation and extension harder. Good as a complementary lightweight path for plain-data types. |
| C4 (Meta<T> specialisation) | MSVC constexpr member-pointer limitations are a real productivity risk on this single-compiler platform. Would be rank 1 on GCC. |

## Registration syntax: Macros as DSL over C7

The user-facing registration uses **macros as syntactic sugar** over the archive free function. The macro is the API; the archive pattern is the engine.

```cpp
// 80% case — simple types, no migration
DIA_SERIALIZE(RigidBody2D, 1)
    DIA_FIELD(mPosition)
    DIA_FIELD(mVelocity)
    DIA_FIELD(mMass)
    DIA_FIELD_OWNED_PTR(mShape)
DIA_SERIALIZE_END
```

Expands to:

```cpp
template<class Archive>
void serialize(Archive& ar, RigidBody2D& obj, unsigned version) {
    ar & named("mPosition", obj.mPosition);
    ar & named("mVelocity", obj.mVelocity);
    ar & named("mMass", obj.mMass);
    ar & owned("mShape", obj.mShape);
}
```

**Rules:**
- `DIA_FIELD(member)` — stringifies the member name automatically, no repetition
- `DIA_FIELD_OWNED_PTR(member)` — owning pointer (serialized inline)
- `DIA_FIELD_REF_ID(member)` — non-owning reference (serialized as ID)
- `DIA_SERIALIZE(Type, version)` — declares the function with version number
- **Escape hatch** — for types that need custom version migration (`if (version < 2)` guards), write the raw `serialize()` free function directly instead of using the macro

This gives clean call sites for the common case without sacrificing any C7 capability. The macros are purely syntactic — no global registration, no static-init, no runtime side effects. The archive infrastructure underneath is identical whether the function was written by hand or expanded from a macro.

## Attributes and defaults

The existing type system provides per-field attributes (Required, CustomSerializer, CustomDeserializer, PointerAsObject, AssetReference) but has **no default value support** — missing fields retain uninitialized memory. The new system improves on every axis:

### How C7 handles existing capabilities

| Current capability | C7 equivalent |
|---|---|
| `TypeVariableAttributeRequired` | `DIA_FIELD_REQUIRED(member)` — archive tracks field presence, errors on missing |
| `TypeVariableAttributesCustomJsonSerializer` | Escape hatch: write raw `serialize()` free function with any custom logic |
| `TypeVariableAttributesCustomJsonDeserializer` | Same escape hatch — full control over read path |
| `TypeVariableAttributesPointerAsObject` | `DIA_FIELD_OWNED_PTR(member)` — owning pointer serialized inline |
| `TypeVariableAttributeAssetReference` | `DIA_FIELD_ATTR(AssetRef, "TargetType")` — metadata tag for tooling |

### Default values (new — fills existing gap)

Objects are constructed with C++ member initializers before deserialization begins. The archive only overwrites fields present in the source data. Missing fields retain their constructor defaults — a well-defined, testable behavior rather than uninitialized memory.

```cpp
struct RigidBody2D {
    Vector2D mPosition{0.0f, 0.0f};  // default used if field missing
    Vector2D mVelocity{0.0f, 0.0f};
    float mMass = 1.0f;              // default used if field missing
};
```

### Field attributes (extension point)

Attributes use variant macros for common cases and a general `DIA_FIELD_ATTR` for rarer metadata:

```cpp
DIA_SERIALIZE(RigidBody2D, 1)
    DIA_FIELD_REQUIRED(mPosition)           // must be present in source
    DIA_FIELD(mVelocity)                    // optional, keeps default if missing
    DIA_FIELD(mMass)
    DIA_FIELD_ATTR(Range, 0.0f, 100.0f)    // validation: clamp or error on out-of-range
    DIA_FIELD_OWNED_PTR(mShape)             // owning pointer, serialized inline
    DIA_FIELD_REF_ID(mOwner)               // non-owning reference, serialized as ID
    DIA_FIELD_ATTR(AssetRef, "DiaTexture") // metadata tag for tooling
DIA_SERIALIZE_END
```

**Design rule:** Common field kinds (`REQUIRED`, `OWNED_PTR`, `REF_ID`) get dedicated macros. Rarer attributes (`Range`, `AssetRef`) use the general `DIA_FIELD_ATTR` extension point. This keeps the 80% case clean and the 20% case possible.

## Containers and arrays

The archive must handle arrays and DiaCore containers natively — no per-container registration needed.

### Static arrays (C-style / std::array)

```cpp
struct Polygon {
    Vector2D mVertices[8];
    int mVertexCount = 0;
};

DIA_SERIALIZE(Polygon, 1)
    DIA_FIELD(mVertices)       // archive detects array type, writes/reads N elements
    DIA_FIELD(mVertexCount)
DIA_SERIALIZE_END
```

The archive detects `T[N]` via `std::is_bounded_array_v` and iterates. JSON writes a JSON array; binary writes element count + N packed values.

### Dynamic containers (DynamicArrayC, HashTable)

```cpp
struct Inventory {
    Dia::Core::Containers::DynamicArrayC<Item, 64> mItems;
};

DIA_SERIALIZE(Inventory, 1)
    DIA_FIELD(mItems)
DIA_SERIALIZE_END
```

The archive handles DiaCore containers via template specializations of the archive `operator&`. Each container type (DynamicArrayC, HashTable, LinkList) gets one specialization in the archive header — users don't write anything special. JSON writes `[...]`; binary writes count + packed elements. Element type must itself be serializable (has a `serialize()` function or is arithmetic).

### Design:

- **Static arrays**: detected automatically via type traits
- **Dynamic containers**: one archive specialization per container type (written once in the archive library)
- **Nested serializable types**: recursive — if the element type has `serialize()`, the archive calls it per element
- **Arithmetic elements**: written directly (no `serialize()` needed for int, float, etc.)

## Inheritance

Derived types call the base serialization explicitly. Two patterns:

### Macro pattern (common case)

```cpp
DIA_SERIALIZE(DerivedBody, 2)
    DIA_BASE(RigidBody2D)      // serialize base fields first
    DIA_FIELD(mDerivedField)
DIA_SERIALIZE_END
```

`DIA_BASE(BaseType)` expands to `serialize(ar, static_cast<BaseType&>(obj), baseVersion)` — the base type's serialize function handles its own fields and versioning independently.

### Raw pattern (custom migration)

```cpp
template<class Archive>
void serialize(Archive& ar, DerivedBody& obj, unsigned version) {
    serialize(ar, static_cast<RigidBody2D&>(obj), 1);  // base at version 1
    ar & named("derivedField", obj.mDerivedField);
}
```

### Polymorphism dispatch (owned pointers to base)

When deserializing `DIA_FIELD_OWNED_PTR(mShape)` where `mShape` is `ICollisionShape*`, the archive needs to know which concrete type to construct. This requires:

1. **Type tag in the data** — JSON: `{ "_type": "CircleShape", ... }`; binary: 4-byte type CRC before the object data
2. **Factory registration** — a lightweight registry mapping type CRC → factory function. Each concrete type registers once:

```cpp
DIA_SERIALIZE_POLYMORPHIC(CircleShape, ICollisionShape, 1)
    DIA_FIELD(mRadius)
DIA_SERIALIZE_END
```

`DIA_SERIALIZE_POLYMORPHIC` registers the type CRC + factory with the archive's polymorphic dispatch table. This is the *only* place a registry exists — it's opt-in, scoped to polymorphic pointers only, and uses explicit registration (no static-init order issues).

## Explicitly out of scope

- **Circular references** — the serializer does not detect or handle cycles. Object graphs must be acyclic. If a cycle exists, the user must break it with `DIA_FIELD_REF_ID` (serialize as ID, not inline).
- **Partial deserialization / streaming** — the archive reads/writes complete objects. Partial field reads are a future extension.
- **STL containers** — per PD-004, the archive public API does not support std::vector/std::map. Internal use is fine.

## Composability note

Two additional overlays carry forward:

- **C6 auto-aggregate** — for trivial structs (`Vector2D`, `RGBA`, `Colour`) where all fields are arithmetic and there are no pointers, a `std::is_aggregate` + field-count check can generate the `serialize()` function automatically. Eliminates boilerplate for the simplest ~20 types.
- **C2 describe-style** — for types that only need field enumeration (no versioning, no pointer fields), a lighter `describe<T>()` path can coexist and feed into the same archive infrastructure.

These overlays are spec-phase decisions, not research-phase. The core architecture is C7 with macro-DSL registration.

## Constraints carried forward to spec

| Constraint | Source | Implication |
|------------|--------|-------------|
| PD-001 StringCRC | Platform | Field names in binary format are 4-byte CRC, not strings |
| PD-004 No STL public API | Platform | Archive API accepts DiaCore containers, not std::vector |
| PD-005 x64 Windows only | Platform | Binary format is little-endian, no byte-swap layer |
| PD-006 VS project files | Platform | No codegen step; all registration is in-source |
| PD-007 C++20 | Platform | Archive concept uses C++20 `requires` clause |
| Existing asset pipeline untouched | Architecture | DiaAssetCatalogue + TypeDefinition + JsonDefinitionLoader continue working; new system is additive |

## Next step

Write the system spec for DiaReflect (or DiaSerialize — naming TBD in spec interview). The spec will define the module boundary, public API, archive concept, and the phased implementation plan.
