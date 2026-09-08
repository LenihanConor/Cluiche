# Research: Explore — Reflection & Serialization

**Session date:** 2026-05-20
**Folder:** docs/research/reflect_serial/

## Problem Space Overview

Cluiche needs a serialization system that can write and reload runtime game-object state — rigid bodies, entities, component graphs, animation states — not just config files and asset definitions. The current DiaCore Type system was designed primarily for the asset pipeline (loading JSON definitions at startup) and has gaps that make it unsuitable for runtime state: it cannot follow pointers, has no binary format, no versioning migration strategy, and no test coverage.

The distinction between "config" (authored once, read at load) and "state" (produced at runtime, round-tripped) is critical. Config serialization tolerates verbosity and slow paths; state serialization must be fast enough to checkpoint, network-sync, or undo/redo without hitching. A single system can serve both if it is designed carefully, but conflating them has been the source of the current system's awkward shape.

The goal is to evaluate the full design space — from fixing the existing system incrementally to replacing it with a modern approach — and pick an architecture that (a) handles the runtime-state use cases (RigidBody2D, Entity, component graphs), (b) stays human-readable in its text form, (c) optionally compiles to binary for performance, and (d) fits the Cluiche module structure and C++20 platform constraint.

## Existing Approaches

**Macro-registration (current DiaCore approach)**
- Fields declared via `DIA_TYPE_ADD_VARIABLE` macros at global-init time
- Pros: no external tooling, familiar, already in codebase
- Cons: fragile, no pointer support, verbose, hard to test in isolation

**Template-based / CRTP introspection**
- Pfr (Boost.PFR), Glaze, refl-cpp — use C++ template metaprogramming to reflect aggregate fields without macros
- Pros: zero macro noise, works with plain structs, leverages C++20 concepts/requires
- Cons: requires `std::tuple`-style layout for pfr, complex error messages, some libraries are header-only but heavy

**Descriptor struct ("describe" pattern)**
- User writes a `describe()` function returning a tuple of field pointers + names
- Pros: explicit, composable, easy to test, pointer-safe, no macros, no external deps
- Cons: manual (user must keep in sync), slightly more boilerplate than codegen

**Codegen (protobuf / flatbuffers / custom)**
- Schema file → generated C++ struct + serializers
- Pros: fast binary, schema-as-documentation, strong versioning story
- Cons: build pipeline complexity, generated code is a black box, harder to integrate with existing Dia types

**Visitor pattern (manual)**
- Each type implements `Accept(Visitor&)` that visits each field
- Pros: simple, zero deps, pointer-safe (visitor can follow them), composable
- Cons: verbose per-type, no automatic field discovery

**JSON-native structs (Glaze / nlohmann with reflect)**
- Map C++ structs to JSON via template specialisation or `glaze::meta`
- Pros: human-readable by default, Glaze supports binary BEVE format from same registration
- Cons: external dep, STL-centric (conflicts with PD-004 for public APIs)

**YAML / TOML as text format**
- Alternative to JSON for human-readable layer
- Pros: more readable for nested state, comments allowed
- Cons: more complex parsers, less common in game engines, jsoncpp already in tree

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| **Registration style** | Macros / Descriptor fn / CRTP template / Codegen | Open — no macro lock-in |
| **Text format** | JSON / YAML / TOML / custom text | JSON preferred (jsoncpp already in tree) |
| **Binary format** | None / hand-rolled / MessagePack / FlatBuffers / custom | Needs to be zero-external-dep if hand-rolled |
| **Pointer handling** | Skip / flatten / ID-based reference / ownership graph | Critical gap in current system |
| **Versioning** | None / version int + migration fns / field-hash schema | Must support at least forward-compat |
| **Inheritance / polymorphism** | Flat (no inheritance) / base+derived tag / virtual dispatch | Needed for component graphs |
| **Collection support** | Fixed arrays / DiaCore containers / both | PD-004 forbids STL in public API |
| **Dependency** | Zero new deps / header-only / new external lib | Prefer zero new deps |
| **Backward compat with existing macros** | Hard break / shim layer / full compat | Not required |

## Known Tradeoffs

- **Explicitness vs automation**: More automatic field discovery (pfr-style) reduces boilerplate but makes it harder to control what gets serialized (e.g. skip transient fields, handle pointers specially)
- **Text readability vs binary size**: JSON is ~3–5× larger than a tight binary; for checkpointing large scenes this matters
- **Single system vs two layers**: One system that does both text and binary is elegant but usually means the text format is less human-friendly (envelope headers, type tags) — two separate formats with a shared descriptor is often cleaner
- **Pointer ownership vs references**: Serializing owning pointers is straightforward (embed the object); serializing non-owning references requires an ID/handle scheme which is application-specific
- **Versioning granularity**: Per-field hash schemas catch renames and reorders; a single version int is simpler but requires discipline
- **Global registry vs per-type descriptors**: Global registry (current approach) is convenient but fragile at static-init time and hard to test; per-type descriptors with explicit registration are more predictable

## Known Pitfalls (C++ / game engine context)

- Static-init order fiasco: current system auto-registers types in constructors at global init — order is undefined across TUs, can produce silent failures
- Pointer serialization is genuinely hard: need to decide ownership model before designing it
- Version migration is always underestimated: "we'll add it later" means you never can without breaking files
- Text format round-trip precision: floats serialized as text lose precision unless hex-float or sufficient decimal places used
- Circular references in object graphs will stack-overflow a naive recursive serializer
- Binary format endianness: Windows-only (PD-005) reduces this risk but worth noting
- C++20 reflection (P2996) is not yet in MSVC — cannot rely on language-level reflection

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| DiaCore/Type/ | Existing reflection system — audit complete; reuse or replace |
| DiaCore/Json/ | jsoncpp wrapper already in tree — text format baseline |
| DiaCore/Containers/ | DynamicArrayC, HashTable, LinkList — must be serializable |
| DiaCore/Strings/ | String8–1024 already have TypeDefinition + custom JSON attrs |
| DiaMaths | Vector2D, Matrix, Quaternion already registered — any new system must handle them |
| DiaAssetCatalogue | Primary consumer of current type system; must not regress |
| DiaRigidBody2D | Target use case: serialize body state (position, velocity, mass, shape) |
| DiaApplicationFlow | Entity/component graphs are the deeper target use case |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-001 StringCRC | Field names should be identified by CRC in binary format; text format can use the string form |
| PD-004 No STL in public APIs | Serializer API must accept DiaCore containers, not std::vector/std::map |
| PD-005 x64 Windows only | Binary format endianness is fixed (little-endian); no need for portable byte-swap layer |
| PD-006 VS project files | No build-time codegen step unless it fits cleanly into MSBuild custom build rules |
| PD-007 C++20 required | Can use concepts, if constexpr, std::span, structured bindings — but NOT P2996 reflection |
| PD-008 Directory.Build.props | Any generated headers must land in a known output path under out/ |

## Open Questions for Ideation

- Should the descriptor/registration live **on the type** (member function) or **alongside the type** (free function / specialisation)? The latter allows registering third-party types without touching their headers.
- Can the **binary format** be derived automatically from the same descriptor that drives text, or do they need separate paths?
- How should **non-owning pointer fields** (e.g. a RigidBody pointing to a CollisionShape it does not own) be handled — skip, ID, or error?
- Should **DiaCore/Strings** and **DiaMaths** types migrate to the new system, or keep their current TypeDefinition registrations as a shim?
- Is a **schema export** (e.g., emit a JSON Schema or a human-readable field manifest) useful for editor tooling?
- What is the right **versioning granularity** — per-type version integer, per-field presence flags, or full content-hash schemas?
- Should the system support **partial deserialization** (only read fields that are present, leave rest at defaults) for forward-compat with files written by newer code?
