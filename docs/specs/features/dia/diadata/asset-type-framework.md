# Feature Spec: Asset Type Framework

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaData | @docs/specs/systems/dia/diadata.md |
| Feature | Asset Type Framework | (this document) |

## Summary

A registration-based framework for defining asset types as pure metadata. Each asset type gets a descriptor declaring its identity (StringCRC type ID), human-readable name, file extension pattern, and schema constraints (required fields, value ranges, reference-typed fields). A central registry stores these descriptors and supports lookup by type ID or file pattern matching.

DiaData owns the descriptors — what asset types exist and what their data shape looks like. Downstream systems (DiaAssetPipeline via DiaPipeline `build-assets`) own what to do with them: validation execution, build step registration, and deployment.

## Problem

Without a central definition of "what asset types exist and what's their schema," every system that touches assets (pipeline, registry, editor, runtime loader) must independently know how to identify and interpret each type. Adding a new asset type becomes a cross-cutting code change. A single registry of type descriptors lets all downstream systems handle new types generically.

## Acceptance Criteria

1. An `AssetTypeDescriptor` defines an asset type: StringCRC type ID, human-readable name, file extension pattern (e.g. `"*.config.json"`), and a reference to a TypeDefinition for schema constraints
2. Schema constraints are expressed via TypeSystem reflection on the descriptor's associated TypeDefinition — required fields (via `TypeVariableAttributeRequired` from Feature 1), value ranges, reference-typed fields (fields that hold StringCRC IDs pointing to other assets)
3. A `TypeVariableAttributeAssetReference` attribute marks fields as references to other asset types, storing the expected target type ID (e.g. "this StringCRC field references a `texture`")
4. An `AssetTypeRegistry` stores descriptors in a HashTableC, keyed by StringCRC type ID
5. Query by type ID returns the descriptor
6. Query by file path matches against registered file patterns and returns the matching descriptor (or none)
7. Registration rejects duplicate type IDs with a clear error
8. Ships with descriptors for all 9 settled taxonomy types: Texture, Mesh/Sprite, Audio, Config, Entity Definition, Stage, UI Definition, Bundle, UI Bundle
9. Registering a new game-specific asset type = constructing and registering one descriptor. No changes needed in DiaData or downstream systems.
10. Pure metadata — no validation execution, no build steps, no file I/O. Descriptors describe; they don't act.

## API Design

### Core Types

```cpp
namespace Dia::Data
{
    struct AssetTypeDescriptor
    {
        Dia::Core::StringCRC mTypeId;           // e.g. "texture", "config", "entity"
        Dia::Core::Containers::String64 mName;  // e.g. "Texture", "Config", "Entity Definition"
        Dia::Core::Containers::String64 mFilePattern; // e.g. "*.config.json"
        const Dia::Core::Type::TypeDefinition* mTypeDefinition; // schema via TypeSystem reflection
    };

    class AssetTypeRegistry
    {
    public:
        bool Register(const AssetTypeDescriptor& descriptor);
        const AssetTypeDescriptor* FindByTypeId(const Dia::Core::StringCRC& typeId) const;
        const AssetTypeDescriptor* FindByFilePath(const Dia::Core::FilePath& path) const;
        unsigned int GetCount() const;
    };
}
```

### TypeVariableAttribute for References

```cpp
namespace Dia::Core::Type
{
    class TypeVariableAttributeAssetReference : public TypeVariableAttribute
    {
    public:
        TypeVariableAttributeAssetReference(const StringCRC& targetTypeId);
        const StringCRC& GetTargetTypeId() const;
    };
}
```

### Usage Pattern

```cpp
// Registering a custom game asset type
AssetTypeDescriptor weaponDesc;
weaponDesc.mTypeId = StringCRC("weapon");
weaponDesc.mName = "Weapon Definition";
weaponDesc.mFilePattern = "*.weapon.json";
weaponDesc.mTypeDefinition = &TypeDefinitionFor<WeaponConfig>();

assetTypeRegistry.Register(weaponDesc);

// Identifying a file's asset type
const AssetTypeDescriptor* desc = assetTypeRegistry.FindByFilePath(someFilePath);
if (desc)
{
    // desc->mTypeId tells you what type, desc->mTypeDefinition gives you the schema
}
```

### Built-in Taxonomy Descriptors

```cpp
// Called once at startup to register the 9 settled types
void RegisterBuiltInAssetTypes(AssetTypeRegistry& registry);
```

| Type ID | Name | File Pattern | TypeDefinition |
|---------|------|-------------|----------------|
| `texture` | Texture | `*.texture.png` | TextureAsset |
| `sprite` | Mesh/Sprite | `*.sprite.json` | SpriteAsset |
| `audio` | Audio | `*.audio.wav` | AudioAsset |
| `config` | Config | `*.config.json` | ConfigAsset |
| `entity` | Entity Definition | `*.entity.json` | EntityAsset |
| `stage` | Stage | `*.stage.json` | StageAsset |
| `ui` | UI Definition | `*.ui.json` | UIAsset |
| `bundle` | Bundle | `*.bundle.json` | BundleAsset |
| `uibundle` | UI Bundle | `*.uibundle.json` | UIBundleAsset |

The TypeDefinitions for these built-in types define the base schema (what fields a Stage, Bundle, etc. must have). Game-specific extensions add fields to their own types, not to these.

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Implement AssetTypeDescriptor | Struct with type ID, name, file pattern, TypeDefinition pointer |
| 2 | Implement TypeVariableAttributeAssetReference | New DiaCore TypeVariableAttribute marking fields as asset references with a target type ID |
| 3 | Implement AssetTypeRegistry | HashTableC-backed registry with Register, FindByTypeId, FindByFilePath. Duplicate rejection. |
| 4 | Implement file pattern matching | Logic for matching a FilePath against registered `*.ext.json` patterns in FindByFilePath |
| 5 | Define built-in taxonomy TypeDefinitions | C++ structs for the 9 asset types (TextureAsset, SpriteAsset, etc.) registered with TypeSystem. Fields use TypeVariableAttributeRequired and TypeVariableAttributeAssetReference where applicable. |
| 6 | Implement RegisterBuiltInAssetTypes | Single function registering all 9 descriptors |
| 7 | Add GoogleTest coverage | Tests for: registration, duplicate rejection, lookup by ID, lookup by file path, pattern matching, built-in types registered correctly |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaData — JSON Definition Loader (Feature 1) | LoadResult/LoadError types. TypeVariableAttributeRequired (added in Feature 1's Task 4). |
| DiaCore TypeSystem | TypeDefinition, TypeVariable, TypeVariableAttributes, TypeRegistry |
| DiaCore CRC/StringCRC | Type IDs, reference target IDs |
| DiaCore Containers | HashTableC (registry backing), String64 (names, patterns) |
| DiaCore FilePath | File path matching in FindByFilePath |

## Files

| File | Action |
|------|--------|
| `Dia/DiaData/AssetTypeDescriptor.h` | Create — descriptor struct |
| `Dia/DiaData/AssetTypeRegistry.h` | Create — registry class header |
| `Dia/DiaData/AssetTypeRegistry.cpp` | Create — registry implementation |
| `Dia/DiaData/BuiltInAssetTypes.h` | Create — built-in taxonomy TypeDefinitions and registration |
| `Dia/DiaData/BuiltInAssetTypes.cpp` | Create — implementation |
| `Dia/DiaCore/Type/TypeVariableAttributeAssetReference.h` | Create — new attribute in DiaCore |
| `Dia/DiaCore/Type/TypeVariableAttributeAssetReference.cpp` | Create — implementation |
| `Dia/DiaData/DiaData.vcxproj` | Modify — add new files |
| `Dia/DiaCore/DiaCore.vcxproj` | Modify — add new attribute files |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | **Compliant.** Asset type IDs are StringCRC. Reference target IDs are StringCRC. All registry lookups are CRC-keyed. |
| PD-002 | ProcessingUnit/Phase/Module | **Not applicable.** Pure metadata framework, no lifecycle involvement. |
| PD-003 | Component-based entities | **Compliant.** Entity Definition is a registered asset type. Its TypeDefinition schema can reference IComponent types by StringCRC via TypeVariableAttributeAssetReference. |
| PD-004 | No STL in public APIs | **Compliant.** Registry uses HashTableC. Names use String64. No STL in public interface. |
| PD-005 | x64 Windows only | **Compliant.** No platform-specific code. |
| PD-007 | C++20 required | **Compliant.** No specific C++20 features required but available. |
| PD-008 | Directory.Build.props | **Compliant.** DiaData.vcxproj and DiaCore.vcxproj inherit centralized settings. |
| PD-009 | Generated output under Cluiche/out/ | **Not applicable.** No generated output. |
| AD-001 | YAML frontmatter module docs | **Compliant.** DiaData module doc already created in Feature 1. |
| AD-002 | No STL in public APIs | **Compliant.** Same as PD-004. |
| AD-003 | Namespace Dia::\<Module\>:: | **Compliant.** All code under `Dia::Data::`. New DiaCore attribute under `Dia::Core::Type::`. |
| AD-005 | Component-based entities | **Compliant.** Same as PD-003. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | API | Should FindByFilePath support multiple pattern matches (e.g. a file matching two types), or return the first match? | First match. File patterns should be unambiguous — if two types match the same file, that's a registration error. FindByFilePath returns one descriptor or null. |
| 2 | Taxonomy | Should the built-in TypeDefinitions (TextureAsset, SpriteAsset, etc.) be minimal stubs or fully-fleshed schemas? | Minimal stubs defining the essential fields that the pipeline and runtime will need (e.g. StageAsset has a bundles array, BundleAsset has an assets array, EntityAsset has a components array). Game-specific fields go in game-specific types. |
| 3 | DiaCore | Adding TypeVariableAttributeAssetReference to DiaCore means this feature modifies DiaCore. Is that acceptable? | Yes — it follows the same pattern as existing attributes (TypeVariableAttributesPointerAsObject, etc.). It's a small, non-breaking addition. Feature 1 already sets the precedent by adding TypeVariableAttributeRequired to DiaCore. |
| 4 | Patterns | Should file patterns support wildcards beyond `*.ext.suffix` (e.g. directory-based patterns like `Textures/**/*.png`)? | No — keep patterns simple: `*.suffix` or `*.suffix.ext`. Directory-based discovery is the pipeline's job, not the type framework's. The type framework just answers "given this filename, what type is it?" |
| 5 | Registry | Should AssetTypeRegistry have a fixed capacity (like TypeRegistry's 128 limit) or grow dynamically? | Fixed capacity. 64 asset types is generous for any game on this engine. Use HashTableC with a compile-time size. Matches the pattern set by TypeRegistry. |
| 6 | Scope | The 9 built-in TypeDefinition structs (TextureAsset, etc.) are non-trivial to design — their fields determine what the pipeline validates and what the runtime expects. Should their exact field definitions be deferred to the pipeline/loader specs? | Yes — Feature 2 defines minimal stub structs with the obvious structural fields (e.g. StageAsset.mBundles, BundleAsset.mAssets). The pipeline and loader specs may extend these or add constraints. The stubs are the contract that "these fields will exist." |

## Status

`Approved`
