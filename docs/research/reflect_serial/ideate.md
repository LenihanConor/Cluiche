# Research: Ideate — Reflection & Serialization

**Input:** docs/research/reflect_serial/explore.md

## Candidates

### Candidate 1: Patch the current macro system
**Home module/system:** DiaCore/Type/ (existing)
**Size:** S
**Description:**
Keep `DIA_TYPE_DECLARATION` / `DIA_TYPE_DEFINITION` macros but fill the critical gaps: add pointer serialization via the existing `TypeVariableAttributesPointerAsObject` path (it was stubbed out), fix the static-init registration order with an explicit `TypeRegistry::Register()` call pattern, add binary output alongside JSON using the same field traversal, and add a proper version + field-presence scheme for forward-compat. No new design needed — just finish what was started.

The result would be a more complete version of what already exists. All 50 existing DIA_TYPE_DEFINITION callsites continue working unchanged.

**Primary value:** Lowest friction path; existing asset pipeline keeps working day one.

---

### Candidate 2: Descriptor function — `describe()` free function specialisation
**Home module/system:** DiaCore/Type/ (redesigned, replace internals)
**Size:** M
**Description:**
Each type opts in by providing a free function template specialisation:

```cpp
template<> auto Dia::Core::Type::describe<RigidBody2D>() {
    return fields(
        field("position",  &RigidBody2D::mPosition),
        field("velocity",  &RigidBody2D::mVelocity),
        field("mass",      &RigidBody2D::mMass)
    );
}
```

The `describe()` return value is a `std::tuple` of typed field descriptors. A generic `Serializer<Format>` walks the tuple at compile time using `if constexpr` and C++20 fold expressions. Format policies (JsonPolicy, BinaryPolicy) are swapped in as template parameters — same descriptor drives both. No macros. No global registry. Pointer fields are handled explicitly: `ref_field` marks a non-owning reference (serialized as an ID), `own_field` marks an owning pointer (serialized inline).

Third-party types (DiaMaths, DiaCore containers) get descriptors in a separate `Type/Descriptors/` file, keeping their headers untouched.

**Primary value:** No macros, no global init fragility, pointer-safe, single descriptor drives text + binary, C++20 idiomatic.

---

### Candidate 3: CRTP base mixin — `Serializable<T>`
**Home module/system:** DiaCore/Type/ (redesigned)
**Size:** M
**Description:**
Types opt in by inheriting `Dia::Core::Serializable<T>` and implementing a `Reflect(Visitor&)` method:

```cpp
class RigidBody2D : public Dia::Core::Serializable<RigidBody2D> {
public:
    void Reflect(Dia::Core::IVisitor& v) {
        v.Field("position", mPosition);
        v.Field("velocity", mVelocity);
        v.Field("mass",     mMass);
    }
};
```

`IVisitor` is a pure interface; `JsonWriter`, `JsonReader`, `BinaryWriter`, `BinaryReader` implement it. Pointer fields use `v.Ref("shape", mShapePtr)` which lets the visitor decide how to handle references (embed, ID, skip). Inheritance is supported by calling `Base::Reflect(v)` at the top of the method. The visitor approach means the serialization logic lives in the visitor, not the type — new formats (e.g., a debug-print visitor) can be added without touching any type.

Downside: types must inherit from `Serializable<T>`, which is invasive for third-party or already-defined types (DiaMaths vectors). Those would need wrapper descriptors.

**Primary value:** Visitor pattern is familiar, pointer-safe, extensible to new formats without modifying types, inheritance support is natural.

---

### Candidate 4: Glaze-style `glaze::meta` template specialisation (zero-dep clone)
**Home module/system:** DiaCore/Type/ (redesigned, new header)
**Size:** M
**Description:**
Inspired by the open-source Glaze library but written from scratch with zero external dependency and DiaCore containers instead of STL. Each type registers by specialising a `Dia::Core::Meta<T>` struct:

```cpp
template<> struct Dia::Core::Meta<RigidBody2D> {
    static constexpr auto value = Dia::Core::object(
        "position", &RigidBody2D::mPosition,
        "velocity", &RigidBody2D::mVelocity,
        "mass",     &RigidBody2D::mMass
    );
};
```

`object(...)` returns a compile-time tuple of `(StringCRC, member-pointer)` pairs. The serializer iterates the tuple with `std::apply` + fold expressions. JSON and binary are separate policy types passed to the same `serialize<Policy>(obj)` / `deserialize<Policy>(obj, data)` free function pair. This is the most "modern C++20" approach — constexpr everything, no runtime overhead for the descriptor itself.

The binary policy writes a tight layout: 4-byte field CRC + raw value bytes per field, with a 2-byte per-type version header. The JSON policy writes `{ "field": value }` objects identical to the current format (easy migration). Schema export (dump all registered Meta<T> to a JSON Schema document) is a free function over the same tuples.

**Primary value:** Most compile-time-safe, zero runtime overhead for descriptors, text and binary from same registration, schema export for editor tooling is trivial to add.

---

### Candidate 5: Two-layer system — config layer (existing) + state layer (new)
**Home module/system:** DiaCore/Type/ (keep) + new DiaCore/State/ module
**Size:** L
**Description:**
Accept that the current macro system is good enough for its primary job (asset/config loading from JSON) and do not touch it. Build a separate, purpose-designed state serialization module for runtime objects. The state layer uses the descriptor-function approach (Candidate 2 or 4) but is explicitly scoped to game state: binary-first (fast checkpoint/undo/network-sync), JSON as a debug dump format, reference IDs for non-owning pointers, and a clear ownership model aligned with the component system (IComponent/IComponentObject).

The two layers never merge: `TypeJsonSerializer` stays for asset loading; `StateSerializer` handles runtime game objects. Clear naming prevents confusion about which to use.

**Primary value:** No risk to existing asset pipeline; state layer can be designed cleanly without backward-compat constraints; right tool for each job.

---

### Candidate 6: Annotated struct with `#pragma`-free reflection via C++20 concepts
**Home module/system:** DiaCore/Type/ (redesigned)
**Size:** M
**Description:**
Use C++20 concepts to define a `Reflectable` concept, then provide a default reflection path for plain aggregates via a thin adaptation of the "magic_get / pfr" approach — but implemented from scratch without Boost. The key insight: for a standard-layout aggregate with N members, you can construct a `T` from N brace-init args and use structured bindings to extract member pointers, all without any macros or user-written descriptors. This works for simple flat structs (e.g., `Vector2D`, `RGBA`, `RigidBodyState`).

For types that do not qualify (non-aggregate, pointer members, inheritance), the user falls back to providing an explicit `describe<T>()` specialisation (Candidate 2). The result is a two-tier system: zero-boilerplate for simple structs, explicit opt-in for complex ones.

The limitation: the aggregate approach relies on the number of members being known at compile time (requires a `member_count<T>` specialisation or structured-binding trick), is fragile with compilers, and does NOT work with inheritance. MSVC support for this pattern is limited compared to GCC/Clang.

**Primary value:** Minimal boilerplate for simple types; auto-reflects plain data structs without any registration code.

---

### Candidate 7: Serialization archive pattern (Boost.Serialize style, zero-dep)
**Home module/system:** DiaCore/Type/ (redesigned)
**Size:** M
**Description:**
A classic archive-based approach where each type implements a single `template<class Archive> void serialize(Archive& ar, unsigned version)` free function:

```cpp
template<class Archive>
void serialize(Archive& ar, RigidBody2D& body, unsigned version) {
    ar & named("position", body.mPosition);
    ar & named("velocity", body.mVelocity);
    ar & named("mass",     body.mMass);
}
```

`Archive` is a concept (C++20): types satisfying it can be `JsonWriteArchive`, `JsonReadArchive`, `BinaryWriteArchive`, `BinaryReadArchive`. The `&` operator is overloaded per archive type. Versioning is the function's `version` parameter — migration logic lives inside the function body with `if (version < 2)` guards. Pointer handling: `ar & owned_ptr("shape", body.mShape)` vs `ar & ref_id("owner", body.mOwner)`.

This is the most familiar pattern for anyone who has used Boost.Serialize or cereal — no macros, no inheritance, no global registry. The archive types are the only non-trivial implementation work.

**Primary value:** Familiar pattern, zero external deps, pointer-safe, versioning built in to the function signature, easy to test each archive independently.

---

### Candidate 8: Hybrid — keep registry, replace macro with static descriptor member
**Home module/system:** DiaCore/Type/ (incremental redesign)
**Size:** S–M
**Description:**
A middle path: keep the TypeRegistry and the runtime TypeDefinition concept, but replace the macro-based registration with a static constexpr descriptor member on each type:

```cpp
struct RigidBody2D {
    static constexpr auto kDescriptor = Dia::Core::Descriptor<RigidBody2D>{
        field("position", &RigidBody2D::mPosition),
        field("velocity", &RigidBody2D::mVelocity)
    };
};
```

At startup, a single `TypeRegistry::RegisterAll()` call walks all known descriptors (registered via a lightweight linked list of static objects — no global-init order issue). Text and binary serializers both consume `kDescriptor` via the same template path. The existing `TypeDefinition` runtime structure is retained as the registry value, but it is now populated from the constexpr descriptor rather than macro side-effects.

This gives the no-macro, compile-time-safe benefits of Candidate 4 while keeping the runtime registry that `JsonDefinitionLoader` and `DiaAssetCatalogue` depend on. Migration path: existing macro callsites are replaced one file at a time with the `kDescriptor` pattern.

**Primary value:** Evolutionary path from current system; retains runtime registry for asset pipeline; no macros; migration can be done incrementally.

---

## Coverage Map

The candidates span the full range of the design axes from explore.md:

- **Registration style**: Macros (C1), Descriptor fn (C2), CRTP (C3), Meta<T> specialisation (C4), Dual-layer (C5), Auto-aggregate (C6), Archive fn (C7), Static member descriptor (C8)
- **Scope**: S candidates (C1, C8) fix or evolve the current system; M candidates (C2–C4, C6–C7) are clean redesigns; L candidate (C5) is the most conservative two-layer split
- **Pointer handling**: C1 patches it; C2/C4/C7/C8 make it explicit in the descriptor/archive; C3 delegates to the visitor; C6 avoids it (aggregate-only)
- **Binary format**: All candidates except C1 (as described) include a binary path natively
- **Versioning**: C7 has the strongest versioning story (in-function migration); C4/C8 use a type-version header; C1/C3 have weak versioning
- **Dependency**: All candidates are zero new external deps (C6 is the only one that flirts with a pfr-style trick which could be fragile on MSVC)
