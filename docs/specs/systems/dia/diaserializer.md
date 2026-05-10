# System Spec: DiaSerializer

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaSerializer is the shared serialization primitives library for the Dia engine. It consolidates types and interfaces that every domain serializer needs — typed metadata, structured error results, and a base contract for versioned file I/O — so that each system's JSON (or future XML/binary) serializer builds on a common, consistent foundation rather than duplicating them.

The immediate motivation is that `MetadataValue`/`MetadataArray` and the metadata JSON loop are already duplicated between `DiaRig2D/Bone.h` and `DiaStateMachine/StateMachineMetadata.h`. DiaSerializer provides the single authoritative copy and the pattern that all future serializers follow.

DiaSerializer is infrastructure, not a feature system. It owns no domain types, no runtime game state, and no asset identity. It provides the building blocks that domain serializers (`JsonSkeletonSerializer`, `JsonStateMachineSerializer`, etc.) are built from.

**Dependency chain:**
`DiaSerializer → DiaCore (StringCRC, DynamicArrayC, Json, DIA_ASSERT, DIA_LOG_WARNING)`

## Responsibilities

- Provide `MetadataValue` (typed discriminated union: bool/int/float/string) as the canonical value type for per-object metadata in engine definitions
- Provide `MetadataEntry` (key–value pair, key is `StringCRC`) and `MetadataArray` (fixed-cap 16)
- Provide `SetMetadata` and `FindMetadata` inline helpers — insert/overwrite by key; lookup returning nullable pointer
- Provide `SerializeResult` structured error type replacing bare `bool` in all domain serializer interfaces; `operator bool()` for drop-in backward compatibility
- Provide `ISerializer` abstract base — version string, migration capability query, and a protected `ReadFileToBuffer` utility; defines the contract that all domain serializers extend
- Provide `JsonMetadataHelpers` header-only inline utilities for reading and writing `MetadataArray` to/from jsoncpp `Json::Value` objects — the shared getMemberNames loop + type-switch that `JsonSkeletonSerializer` and `JsonStateMachineSerializer` currently duplicate
- Ship with `DiaSerializer.vcxproj` static library registered in `Cluiche.sln`
- Provide `dia.serializer.architecture.module.md` YAML module documentation

## Non-Responsibilities

- JSON parsing or jsoncpp itself — jsoncpp stays in `DiaCore/Json/`
- Domain types (skeleton definitions, state machine definitions) — those stay in their modules
- Asset identity, type framework, or cross-file relationships — that is DiaData
- Binary serialization or wire protocols — out of scope v1
- Schema migration execution — `ISerializer` provides the hook; execution logic belongs in the concrete domain serializer
- Thread safety of serialization calls — caller's responsibility

## Public Interfaces

### MetadataValue, MetadataEntry, MetadataArray

```cpp
namespace Dia::Serializer {
    struct MetadataValue {
        enum Type : unsigned char { kBool, kInt, kFloat, kString };
        Type type = kBool;
        union { bool boolVal; int intVal; float floatVal; };
        Dia::Core::StringCRC stringVal;  // used when type == kString

        static MetadataValue FromBool(bool v);
        static MetadataValue FromInt(int v);
        static MetadataValue FromFloat(float v);
        static MetadataValue FromString(const char* v);
    };

    struct MetadataEntry {
        Dia::Core::StringCRC key;
        MetadataValue value;
    };

    static constexpr unsigned int kMaxMetadataEntries = 16;
    using MetadataArray = Dia::Core::Containers::DynamicArrayC<MetadataEntry, kMaxMetadataEntries>;

    // Insert or overwrite key in arr. DIA_ASSERT if arr is full and key not present.
    void SetMetadata(MetadataArray& arr,
                     Dia::Core::StringCRC key,
                     const MetadataValue& value);

    // Returns pointer to matching entry's value, or nullptr if not found.
    const MetadataValue* FindMetadata(const MetadataArray& arr,
                                      Dia::Core::StringCRC key);
}
```

### SerializeResult

```cpp
namespace Dia::Serializer {
    struct SerializeResult {
        bool ok;
        const char* error;  // null on success; must be a static string literal on failure

        explicit operator bool() const { return ok; }

        static SerializeResult Success();
        static SerializeResult Failure(const char* error);
    };
}
```

`SerializeResult::error` must be a static string literal (no allocation, no ownership transfer). The receiver may log it but must not store or free it.

### ISerializer

```cpp
namespace Dia::Serializer {
    class ISerializer {
    public:
        virtual ~ISerializer() = default;

        // Schema version string this serializer produces (e.g. "1.0").
        virtual const char* GetVersion() const = 0;

        // Returns true if this serializer can migrate data originally written
        // by fromVersion. Default: false (no migration supported).
        virtual bool CanMigrate(const char* fromVersion) const;

    protected:
        // Utility: read a file into caller-provided buffer. Returns false and
        // emits DIA_LOG_WARNING on I/O error. outBuffer is null-terminated.
        static bool ReadFileToBuffer(const char* path,
                                     char* outBuffer,
                                     unsigned int bufferSize);

        // Utility: write null-terminated data to a file. Returns false and
        // emits DIA_LOG_WARNING on I/O error.
        static bool WriteBufferToFile(const char* path,
                                      const char* data,
                                      unsigned int dataSize);
    };
}
```

Domain serializer interfaces (`IStateMachineSerializer`, `ISkeletonSerializer`) inherit from `ISerializer` and add typed `Load`/`Save` methods. `LoadFromFile`/`SaveToFile` convenience overloads on those interfaces call `ReadFileToBuffer` then the in-memory `Load`.

### JsonMetadataHelpers

```cpp
// DiaSerializer/JsonMetadataHelpers.h — header-only, requires <DiaCore/Json/json.h>

namespace Dia::Serializer {
    // Write all entries in arr into a JSON object under key "metadata".
    // Serializes bool/int/float as JSON native types; string as JSON string.
    void WriteMetadataToJson(const MetadataArray& arr, Json::Value& outObject);

    // Read a "metadata" JSON object from root and populate arr.
    // Unknown type keys emit DIA_LOG_WARNING and are skipped.
    // Returns false if the metadata node exists but is malformed.
    bool ReadMetadataFromJson(const Json::Value& root, MetadataArray& outArr);
}
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| metadata-primitives | `MetadataValue`, `MetadataEntry`, `MetadataArray`, `SetMetadata`/`FindMetadata`, `JsonMetadataHelpers` — the shared metadata layer moved from DiaRig2D and DiaStateMachine | [metadata-primitives.md](../../features/dia/diaserializer/metadata-primitives.md) | Approved |
| serializer-contract | `SerializeResult` structured error type + `ISerializer` base interface (version string, migration query, file I/O helpers) | [serializer-contract.md](../../features/dia/diaserializer/serializer-contract.md) | Approved |
| diarig2d-migration | Rename `ISkeletonLoader` → `ISkeletonSerializer`, `JsonSkeletonLoader` → `JsonSkeletonSerializer`; remove local `MetadataValue` from `DiaRig2D/Bone.h`; adopt `SerializeResult`; add `LoadFromFile`/`SaveToFile` to the skeleton serializer | [diarig2d-migration.md](../../features/dia/diaserializer/diarig2d-migration.md) | Approved |
| diastatemachine-migration | Remove `DiaStateMachine/StateMachineMetadata.h`; adopt `Dia::Serializer::` types; make `MarkValid()` private with `friend class JsonStateMachineSerializer`; add `CallbackRegistry::Finalize()`/`IsFinalized()` + assert in all `Load()` paths; adopt `SerializeResult`; add `LoadFromFile`/`SaveToFile` to `IStateMachineSerializer` | [diastatemachine-migration.md](../../features/dia/diaserializer/diastatemachine-migration.md) | Approved |

## Dependencies on Other Systems

**Required:**
- **DiaCore** — `StringCRC` (metadata keys), `DynamicArrayC` (MetadataArray container), `DiaCore/Json/` (jsoncpp wrapper for JsonMetadataHelpers), `DIA_ASSERT`, `DIA_LOG_WARNING`, `FilePath`/file I/O utilities

**Explicitly excluded:**
- **DiaRig2D**, **DiaStateMachine**, **DiaData** — DiaSerializer is a foundation layer; it must not depend on any system that uses it (would be circular)
- **DiaLogger** — uses `DIA_LOG_WARNING` macro from DiaCore directly; no full logger channel registration

**Dependents (once DiaSerializer ships):**
- **DiaRig2D** — removes local MetadataValue; inherits ISerializer for JsonSkeletonSerializer
- **DiaStateMachine** — removes StateMachineMetadata.h; inherits ISerializer for JsonStateMachineSerializer
- **All future domain serializers** — DiaApplicationFlow data-driven topology, spatial grid config, DiaIK2D solver config, physics body definitions

## Out of Scope

- jsoncpp itself — stays in `DiaCore/Json/`; DiaSerializer includes it but does not own it
- Domain types (StateMachineDefinition, SkeletonDef, etc.)
- Asset identity, registries, or build pipeline — that is DiaData
- Binary, XML, or other non-JSON formats in v1 — extension point exists via `ISerializer` but not implemented
- Schema migration execution logic — `CanMigrate()` advertises capability; concrete serializers execute the migration
- Schema versioning for platform-level (vcxproj, Directory.Build.props) files

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-001 | `MetadataValue` uses a tagged union, not `std::variant` | `std::variant` is STL; PD-004 forbids STL in public APIs. Tagged union is zero-cost and self-contained. | Metadata primitives | Accepted | Yes |
| SD-002 | `MetadataArray` cap is 16 | Matches the existing implementations in DiaRig2D and DiaStateMachine. Sufficient for per-state/per-bone annotation; not a general-purpose property bag. | Metadata primitives | Accepted | Yes |
| SD-003 | `SerializeResult::error` is a static string literal (no allocation) | Keeps domain serializers allocation-free on the error path. Callers who need dynamic messages should log at the serializer and return a static category string (e.g. "metadata parse error"). | Serializer contract | Accepted | Yes |
| SD-004 | `ISerializer` provides version + migration query; `LoadFromFile`/`SaveToFile` live on domain interfaces | A single `ISerializer::Load(void*)` virtual would be type-erased and require casts at call sites. Typed domain interfaces are safer. Common file I/O is a protected utility (`ReadFileToBuffer`), not a virtual method. | Serializer contract | Accepted | Yes |
| SD-005 | `JsonMetadataHelpers` is header-only in `DiaSerializer/` | It wraps jsoncpp (DiaCore dependency); placing it in DiaSerializer keeps it reusable without introducing a DiaCore → DiaSerializer dep (which would be circular). Header-only avoids a link-time dep for consumers that only use the metadata helpers. | JsonMetadataHelpers | Accepted | Yes |
| SD-006 | Migration hook is a query (`CanMigrate`) + execution in the concrete serializer | The base interface cannot know domain-specific migration steps. A query lets callers detect unsupported versions early. Concrete serializers call their own chain of in-memory migrators. | Serializer contract | Accepted | Yes |
| SD-007 | `MarkValid()` on definition classes becomes private + `friend class JsonStateMachineSerializer` | `MarkValid()` exposed publicly is a smell — it can bypass `Validate()`. `friend` restricts it to the one legitimate non-Builder caller. | DiaStateMachine migration | Accepted | Yes |
| SD-008 | `CallbackRegistry::Finalize()` / `IsFinalized()` added; `DIA_ASSERT(registry.IsFinalized())` in all `Load()` paths | Boot-order mistakes (loading before registering all callbacks) silently produce machines with null function pointers. The assert converts a silent data error into an explicit programming error. | DiaStateMachine migration | Accepted | Yes |
| SD-009 | Namespace is `Dia::Serializer::` | Consistent with `Dia::<Module>::` convention (AD-003). Short enough not to clutter call sites. | All | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Metadata keys are `StringCRC`; no raw `const char*` keys in MetadataEntry. |
| PD-004 | Platform | No STL containers in public APIs | `MetadataArray` is `DynamicArrayC`, not `std::vector`. `MetadataValue` is a tagged union, not `std::variant`. |
| PD-005 | Platform | x64 only | `DiaSerializer.vcxproj` targets x64 exclusively. |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaSerializer.vcxproj` and `.vcxproj.filters` created and manually maintained. |
| PD-007 | Platform | C++20 required | Compiled under `/std:c++20`. |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaSerializer.vcxproj` must NOT override OutDir, IntDir, PlatformToolset, WindowsTargetPlatformVersion, or LanguageStandard. |
| AD-001 | Dia App | Module system with YAML frontmatter | Create `dia.serializer.architecture.module.md` with public API, responsibilities, and dependency declarations. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::Serializer::` namespace. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Namespace | Should `Dia::Serializer::` conflict with any existing namespace in the codebase? | No — the `Dia::` namespace exists; `Serializer` sub-namespace is new. `Dia::Core::` is taken. `Dia::Serializer::` is clean. |
| 2 | Metadata cap | Is 16 metadata entries per object sufficient for all anticipated use cases (animation, AI, physics, application)? | Yes for v1. Animation metadata (speed, blend weight, layer) typically <5 entries. AI metadata (priority, aggro radius, faction) typically <10. If a domain needs more, the cap is a compile-time constant (`kMaxMetadataEntries`) that can be raised. |
| 3 | MetadataValue string storage | `stringVal` is a `StringCRC` — it hashes the string, losing the original text. Is this acceptable for metadata string values? | Yes — string metadata values in definitions (e.g. "idle", "walking") are looked up by CRC at runtime. If human-readable strings are needed in a debug tool, the domain code must maintain its own string table. This is the existing DiaRig2D pattern. |
| 4 | ISerializer and domain interfaces | Does `ISerializer` need to be a direct base of domain interfaces, or can domain interfaces stand alone and `ISerializer` is only for file I/O utility? | `ISerializer` is a base: domain interfaces inherit it to get `GetVersion()`, `CanMigrate()`, and the protected file I/O helpers. This is a shallow hierarchy — domain interfaces add their typed `Load`/`Save` virtuals on top. |
| 5 | Migration design | What does a migration actually look like in practice? | Concrete serializer detects version mismatch on load. If `CanMigrate(fromVersion)` returns true, it runs a chain of in-memory transform functions (v1→v2, v2→v3) on the raw `Json::Value` before parsing. No migration is needed in v1 — the hook is designed in, not exercised. |
| 6 | DiaStateMachine friend pattern | All three definition classes (`StateMachineDefinition`, `HierarchicalStateMachineDefinition`, `PushdownAutomatonDefinition`) need `friend class JsonStateMachineSerializer`. Is this manageable? | Yes — three friend declarations in three headers. The alternative (leaving `MarkValid()` public) is the worse smell. If more serializer backends are added, each gets its own `friend` declaration. |
| 7 | CallbackRegistry Finalize | What happens if a second `Finalize()` is called? | Second call is a DIA_ASSERT (programmer error — finalize is a one-way gate). `IsFinalized()` returns true once set. |
| 8 | JsonMetadataHelpers opt-in | Consumers that don't use JSON should not be forced to include jsoncpp headers. Is header-only `JsonMetadataHelpers.h` safe? | Yes — it's a separate header; consumers only include it when writing JSON-backed serializers. Non-JSON consumers (future XML/binary) skip it entirely. The guard `#include <DiaCore/Json/json.h>` is inside `JsonMetadataHelpers.h`, not in `MetadataValue.h`. |
| 9 | SerializeResult error lifetime | Static string literals are immortal, but what about format-string-style messages with variable content? | Not supported — `SerializeResult` is not a logging system. Callers that need rich diagnostics should call `DIA_LOG_WARNING` before returning the `Failure(staticCategory)`. The static string in `SerializeResult` is a category label, not a full message. |
| 10 | vcxproj placement | Where does `DiaSerializer.vcxproj` live? | `Dia/DiaSerializer/DiaSerializer.vcxproj` — same pattern as every other Dia module. Added to `Cluiche.sln` under the `Dia` solution folder. |
| 11 | Breaking change scope | Migrating DiaRig2D and DiaStateMachine to DiaSerializer types is a breaking change to their internal headers. Is there a compatibility concern? | No external consumers directly include `DiaRig2D/Bone.h` or `DiaStateMachine/StateMachineMetadata.h` for the metadata types — they use the public skeleton/machine APIs. The migration is a header rename + namespace update; GoogleTest build will catch any breakage. |
| 12 | Test coverage | What tests does DiaSerializer itself need? | `SetMetadata`/`FindMetadata` round-trip for all four types, overwrite semantics, full-cap assert, `SerializeResult` bool operator, `JsonMetadataHelpers` round-trip for all types and malformed JSON. Migration/file I/O utilities tested via DiaRig2D and DiaStateMachine integration tests. |

## Status

`Approved` — System spec approved 2026-05-02. No implementation started.
