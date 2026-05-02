# Feature Spec: JSON Definition Loader

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaData | @docs/specs/systems/dia/diadata.md |
| Feature | JSON Definition Loader | (this document) |

## Summary

A single-call API that loads a JSON file from disk, deserializes it into a typed C++ object via DiaCore's TypeJsonSerializer, and validates that required fields are present and correctly typed. Returns a structured result with either the loaded object or detailed error information.

This is the bootstrap utility for the entire DiaData system — the Asset Type Framework and Asset Registry both depend on it for reading JSON-authored content. It wraps existing infrastructure (TypeJsonSerializer, TypeDefinition, TypeRegistry) into a clean, ergonomic API that downstream systems call without needing to manage the deserialization ceremony themselves.

## Problem

Loading a typed C++ object from a JSON file currently requires: reading the file into a buffer, initializing TypeJsonSerializer with the TypeRegistry, calling Deserialize with a TypeInstance wrapper, and manually checking whether the result is valid. There's no validation of required fields, no structured error reporting, and each caller must repeat the same boilerplate. For AI-authored content where structural mistakes are common, this lack of validation means bad data reaches runtime silently.

## Acceptance Criteria

1. Given a file path and a registered type `T`, returns a deserialized C++ object of type `T`
2. Validates that all required fields (as marked by TypeVariableAttributes) are present in the JSON — fails with a per-field error if not
3. Validates that field types in the JSON match the TypeSystem reflection — fails with a per-field error on type mismatch
4. Returns a structured `LoadResult<T>` containing either the loaded object or a list of `LoadError` entries (field path, error kind, human-readable message)
5. Uses TypeJsonSerializer under the hood — does not implement a parallel serialization path
6. Works with any type registered in the TypeSystem — not specific to any asset type
7. Reads JSON from disk via DiaCore file I/O — caller provides a `FilePath`, not a raw buffer
8. Also supports loading from a raw JSON string/buffer for testing and pipeline use (overload)
9. No file watching, no caching, no registry interaction — pure load-and-validate
10. Thread-safe for concurrent reads (no shared mutable state)

## API Design

### Core Types

```cpp
namespace Dia::Data
{
    enum class LoadErrorKind
    {
        FileNotFound,
        JsonParseError,
        TypeNotRegistered,
        MissingRequiredField,
        TypeMismatch,
        DeserializationError
    };

    struct LoadError
    {
        LoadErrorKind mKind;
        Dia::Core::StringCRC mFieldPath;    // e.g. "mHealth", "mPosition.mX"
        Dia::Core::Containers::String64 mMessage;
    };

    template<typename T>
    struct LoadResult
    {
        bool mSuccess;
        T mValue;
        Dia::Core::Containers::DynamicArrayC<LoadError, 16> mErrors;

        bool HasErrors() const;
        const LoadError& GetFirstError() const;
    };

    class JsonDefinitionLoader
    {
    public:
        JsonDefinitionLoader(const Dia::Core::Type::TypeRegistry& registry);

        template<typename T>
        LoadResult<T> Load(const Dia::Core::FilePath& path) const;

        template<typename T>
        LoadResult<T> LoadFromBuffer(const Dia::Core::StringReader& buffer) const;
    };
}
```

### Usage Pattern

```cpp
Dia::Data::JsonDefinitionLoader loader(typeRegistry);

auto result = loader.Load<WeaponConfig>(weaponFilePath);
if (result.mSuccess)
{
    // Use result.mValue
}
else
{
    for (const auto& error : result.mErrors)
    {
        // Log: error.mFieldPath, error.mKind, error.mMessage
    }
}
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Create DiaData vcxproj and module structure | New `Dia/DiaData/` directory, `.vcxproj`, `.vcxproj.filters`, `dia.data.architecture.module.md`, add to solution |
| 2 | Implement LoadError and LoadResult types | Error enum, error struct, templated result struct with DiaCore containers |
| 3 | Implement JsonDefinitionLoader core | Constructor taking TypeRegistry, `Load<T>` from FilePath, `LoadFromBuffer<T>` from StringReader. Wires up file read → TypeJsonSerializer::Deserialize |
| 4 | Implement required-field validation | Walk TypeDefinition fields after deserialization, check required fields (via TypeVariableAttributes) are present in the JSON. Emit MissingRequiredField errors. |
| 5 | Implement type-mismatch validation | During deserialization, detect JSON value types that don't match TypeVariable expectations. Emit TypeMismatch errors with field path. |
| 6 | Add GoogleTest coverage | Tests for: successful load, missing file, malformed JSON, missing required field, type mismatch, load from buffer. Use test fixture types registered with TypeSystem. |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaCore TypeSystem | TypeDefinition, TypeVariable, TypeVariableAttributes, TypeInstance, TypeRegistry |
| DiaCore TypeJsonSerializer | Deserialize function — the core serialization engine this feature wraps |
| DiaCore CRC/StringCRC | Field path identification in errors |
| DiaCore FilePath | File path abstraction for reading JSON from disk |
| DiaCore JSON | jsoncpp for low-level JSON parsing (used indirectly via TypeJsonSerializer) |
| DiaCore Containers | DynamicArrayC for error lists, String64 for error messages |

## Files

| File | Action |
|------|--------|
| `Dia/DiaData/DiaData.vcxproj` | Create — new project |
| `Dia/DiaData/DiaData.vcxproj.filters` | Create — IDE organization |
| `Dia/DiaData/dia.data.architecture.module.md` | Create — module YAML frontmatter |
| `Dia/DiaData/JsonDefinitionLoader.h` | Create — public header |
| `Dia/DiaData/JsonDefinitionLoader.cpp` | Create — implementation |
| `Dia/DiaData/LoadResult.h` | Create — result and error types |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | **Compliant.** LoadError uses StringCRC for field paths. JsonDefinitionLoader is CRC-aware through TypeSystem integration. |
| PD-002 | ProcessingUnit/Phase/Module | **Not applicable.** JsonDefinitionLoader is a stateless utility, not an application-lifecycle component. |
| PD-003 | Component-based entities | **Not applicable.** This feature loads typed objects generically — it doesn't know about components. Entity Definition loading will use this feature but component awareness belongs in the Asset Type Framework. |
| PD-004 | No STL in public APIs | **Compliant.** LoadResult uses DynamicArrayC for errors, String64 for messages. No std::vector, std::string, or std::optional in public interface. |
| PD-005 | x64 Windows only | **Compliant.** No platform-specific code. |
| PD-007 | C++20 required | **Compliant.** Can use concepts to constrain `T` in Load<T> to types registered with TypeSystem. |
| PD-008 | Directory.Build.props | **Compliant.** DiaData.vcxproj inherits from Directory.Build.props — no overrides for OutDir, IntDir, PlatformToolset, LanguageStandard. |
| PD-009 | Generated output under Cluiche/out/ | **Not applicable.** This feature doesn't generate output files. |
| AD-001 | YAML frontmatter module docs | **Compliant.** `dia.data.architecture.module.md` created as Task 1. |
| AD-002 | No STL in public APIs | **Compliant.** Same as PD-004. |
| AD-003 | Namespace Dia::\<Module\>:: | **Compliant.** All code under `Dia::Data::` namespace. |
| AD-005 | Component-based entities | **Not applicable.** Same as PD-003 — component awareness is the Asset Type Framework's concern. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | API | Should `Load<T>` require `T` to be registered with TypeSystem at compile time (via concept), or fail at runtime if unregistered? | Runtime check with `TypeNotRegistered` error. Compile-time concepts could be added later but require TypeSystem to expose constexpr registration checks, which it doesn't currently support. |
| 2 | Validation | TypeVariableAttributes currently has `PointerAsObject` and `CustomJsonSerializer/Deserializer` but no `Required` attribute. Does a `Required` attribute need to be added to DiaCore? | Yes — a `TypeVariableAttributeRequired` attribute must be added to DiaCore as part of Task 4. This is a small, non-breaking addition to the existing attribute system (follows the same pattern as existing attributes). |
| 3 | Errors | Should LoadResult hold a fixed-capacity DynamicArrayC (e.g. max 16 errors) or a heap-allocated dynamic array? | Fixed capacity DynamicArrayC<LoadError, 16>. For a validation result, 16 errors is generous — if a file has more than 16 problems, the first 16 are enough to diagnose. Avoids heap allocation in the common path. |
| 4 | API | Should the loader be a class instance (constructed with TypeRegistry) or a namespace of free functions? | Class instance. Taking TypeRegistry in the constructor avoids passing it on every call and makes the dependency explicit. Also allows future extension (e.g., caching, custom attribute handlers) without API changes. |
| 5 | Scope | Task 1 creates the DiaData vcxproj. Should this feature also add DiaData to the solution file (Cluiche.sln)? | Yes — Task 1 includes adding DiaData.vcxproj to Cluiche.sln and setting up the project reference to DiaCore. |
| 6 | Validation | How should nested object validation work? If a field is a class type with its own required fields, should the loader validate recursively? | Yes — validation walks the TypeDefinition tree recursively. Field paths use dot notation (e.g., `"mPosition.mX"`) in LoadError to identify nested fields. This matches how TypeJsonSerializer already recurses through class-typed fields. |

## Status

`Approved`
