# Research: Evaluate — Reflection & Serialization

**Input:** docs/research/reflect_serial/ideate.md (restructured)

## Framing

The candidates are evaluated as **descriptor styles** only. All candidates share the same format capability:
- **Text**: clean JSON (no `_class_name`/`_crc_validation_array` noise; metadata optional, pushed to outer envelope only when needed for polymorphic dispatch)
- **Binary**: hand-rolled, zero new deps (4-byte field CRC + raw value bytes, 2-byte version header per type)
- **Format selection**: policy/archive type passed to the same serializer — adding a new format never touches type registrations

C3 (CRTP visitor) and C6 (auto-aggregate for plain structs) are noted as structural overlays that can be layered on top of any descriptor style chosen.

## Scoring Criteria

| Axis | Weight | Meaning |
|------|--------|---------|
| **Engine Value** | 0.25 | Improves Dia module reusability, capability, or removes existing liability |
| **Game Value** | 0.20 | Enables target use cases: RigidBody2D, Entity/component serialization, save/replay |
| **Implementation Cost** | 0.25 | Inverse of effort — 5 = very cheap, 1 = very expensive |
| **Risk** | 0.15 | Inverse of uncertainty — 5 = well-understood, 1 = highly uncertain (MSVC quirks count) |
| **Cluiche Fit** | 0.15 | Aligns with PD-001 through PD-007, module structure, C++20 direction |

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | **Total** |
|-----------|:---:|:---:|:---:|:---:|:---:|:---:|
| C1: Patch macros | 2 | 2 | 4 | 4 | 3 | **2.95** |
| C2: `describe<T>()` free fn | 4 | 4 | 3 | 3 | 4 | **3.60** |
| C4: `Meta<T>` struct specialisation | 5 | 4 | 2 | 2 | 4 | **3.45** |
| C7: `serialize(Archive&, T&)` free fn | 4 | 5 | 3 | 4 | 4 | **3.95** |
| C8: `kDescriptor` static member + registry | 3 | 3 | 3 | 3 | 4 | **3.15** |

### Scoring rationale (key differentiators)

**C1 — Patch macros (2.95)**
Cheap but doesn't remove the liability. Macros are still static-init fragile, hard to test, and don't naturally express versioning. Pointer serialization would require finishing a stub that was abandoned for a reason.

**C2 — `describe<T>()` free function (3.60)**
Clean and composable. Specialising a free function template means third-party types (DiaMaths, DiaCore containers) can be described without touching their headers — exactly what's needed. The field list is a `std::tuple` of member pointers, walked at compile time with `std::apply` + fold expressions. Moderate implementation work; no tricky MSVC edge cases because member pointers in tuples are a well-exercised pattern. Risk point deducted because `std::tuple` fold expressions over heterogeneous types can produce hard-to-read errors when something goes wrong.

**C4 — `Meta<T>` struct specialisation (3.45)**
The most compile-time-elegant design — the descriptor is `constexpr`, zero runtime overhead, schema export is trivial. Loses points on Cost and Risk because constexpr member pointer tuples have real MSVC pain: non-type template parameters containing member pointers hit MSVC limitations that Clang/GCC handle cleanly. Would be rank 1 on GCC; on MSVC it's a real productivity risk.

**C7 — `serialize(Archive&, T&)` free function (3.95)**
Highest score. The archive pattern (cereal / Boost.Serialize heritage) is the most natural fit for the use cases stated: rigid body state, entity graphs, game saves, replay. Versioning is a first-class concept — the `unsigned version` parameter is passed to the function body, migration logic is just `if (version < 2)` guards. Pointer handling is explicit: `ar & owned("shape", ptr)` vs `ar & ref_id("owner", handle)`. Implementation risk is low — archive types are straightforward to write one at a time (JsonWriteArchive first, BinaryWriteArchive second). No constexpr tricks, no MSVC edge cases. Scores 5 on Game Value because it's the only style where versioning is native rather than bolted on.

**C8 — `kDescriptor` static member + registry (3.15)**
The evolutionary path — least disruption to DiaAssetCatalogue. But it inherits the static-init concerns (the registry still needs populating), and a `static constexpr` member with member pointers hits the same MSVC edge cases as C4. The retained runtime registry is the right call for the asset pipeline but complicates the design for the new use cases.

---

## Top 3 Candidates

### Rank 1: C7 — Archive pattern (score: 3.95)
**Why:** Versioning is native and first-class, which is the most commonly underestimated requirement in any serialization system. The free function pattern means any type can opt in without inheritance or struct membership. JsonWriteArchive and BinaryWriteArchive are independent implementations of the same `Archive` concept — adding or replacing formats is a new file, not a change to existing types. The pattern is immediately familiar to anyone who has seen cereal or Boost.Serialize, reducing onboarding cost. Aligns with PD-007 (C++20 concepts for the Archive constraint) and PD-001 (StringCRC naturally used as field names in binary format).

**Watch out for:** The `&` operator overload for archives can produce confusing error messages when a field type is not yet supported. Need a clear `static_assert` + concept guard on the Archive concept so failures are readable.

---

### Rank 2: C2 — `describe<T>()` free function (score: 3.60)
**Why:** The cleanest separation between "what fields does this type have" (the descriptor) and "how are they serialized" (the format policy). Third-party type support without header changes is a concrete win for DiaMaths and DiaCore containers. The compile-time tuple walk means the serializer has zero branching overhead in release builds.

**Watch out for:** The serializer that walks the tuple needs careful design to handle nested types, arrays, and pointer fields. The `describe<T>()` return type (`auto`) means the field list is a deduced type — the implementation is slightly harder to document and extend than the archive approach.

---

### Rank 3: C4 — `Meta<T>` struct specialisation (score: 3.45)
**Why:** If MSVC constexpr member pointer support were reliable, this would be rank 1. The compile-time descriptor with zero runtime overhead is genuinely attractive, and schema export for editor tooling falls out for free.

**Watch out for:** MSVC (the only compiler on this platform, PD-005) has known limitations with constexpr non-type template parameters containing member pointers. This needs a proof-of-concept compile test before committing to this path.

---

## Recommendation

**C7 (Archive pattern)** is the right choice. The stated use cases — rigid body serialization, entity serialization, anything that is not config — are exactly the use cases the archive pattern was designed for. Save files need versioning. Replay systems need binary. Debug dumps need human-readable JSON. All three come from writing one `serialize()` function per type and swapping the archive type at the call site. The implementation risk is the lowest of the clean designs, the pattern is well-understood, and it produces no MSVC surprises. 

C2 is a strong runner-up and could be adopted for types that are pure data (no versioning needed, no pointer fields) as a lighter registration path — the two approaches are composable rather than exclusive.

One structural overlay worth carrying into the spec regardless of descriptor style: C6-style auto-reflection for plain aggregates (e.g., `Vector2D`, `RGBA`) where all fields are arithmetic and there are no pointers. This eliminates boilerplate for the simplest types and can be detected automatically via a `std::is_aggregate` + field-count check.
