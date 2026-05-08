# System Spec: DiaGame

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaGame is the game project system that owns the `.diagame` and `.diastage` file formats, their data types, serialization (Load + Save), and composition/loading APIs. It represents the "game project" layer above DiaApplication — where DiaApplication defines *how* an app runs (PU/Phase/Module), DiaGame defines *what* a game project is (its manifest, stages, config, and imports).

The driving principle: **serialization lives beside its object**. Every data type owns its own serializer at the same module level.

## Responsibilities

- Define the `DiaGameManifest` and `DiaStageManifest` data types
- Own the `.diagame` JSON file format (name, version, typed imports, config)
- Own the `.diastage` JSON file format (name, manifest path)
- Provide round-trip `ISerializer` implementations for both formats (Load + Save)
- Preserve unknown config fields via `rawConfig` for forward compatibility
- Provide `GameFileComposer` — loads .diagame, delegates import resolution to DiaApplication's `ManifestComposer`
- Provide `GameLoader` — high-level API to load a full ProcessingUnit tree from a .diagame file
- Provide `DiaGameManifestLoader` — low-level file loading for .diagame and .diastage

## Non-Responsibilities

- Application structure (PU/Phase/Module) — owned by DiaApplication
- Manifest composition and merge logic (.diaapp resolution, stage merge, cycle detection) — owned by DiaApplication's `ManifestComposer`
- Editor UI for editing game files — owned by DiaApplicationEditor
- Runtime asset loading — owned by DiaAssetRuntime
- Game-specific logic (save slots, player progress) — owned by game applications

## Public Interfaces

### Data Types

```cpp
namespace Dia::Game {
    struct DiaGameConfig {
        Dia::Core::Containers::String256 assetRoot;
    };

    struct DiaGameManifest {
        Dia::Core::Containers::String256 name;
        Dia::Core::Containers::String256 version;
        Dia::Core::Containers::DynamicArrayC<Dia::Application::TypedImport, 16> imports;
        DiaGameConfig config;
        Json::Value* rawConfig;  // Full config JSON for round-tripping unknown fields
    };

    struct DiaStageManifest {
        Dia::Core::Containers::String256 name;
        Dia::Core::Containers::String256 manifestPath;
    };
}
```

### Serializers (ISerializer implementations)

```cpp
namespace Dia::Game {
    class JsonDiaGameSerializer : public Dia::Serializer::ISerializer {
    public:
        const char* GetVersion() const override;
        SerializeResult Load(const char* data, DiaGameManifest& outManifest) const;
        SerializeResult Save(const DiaGameManifest& manifest, char* outBuffer, unsigned int bufferSize) const;
        SerializeResult LoadFromFile(const char* path, DiaGameManifest& outManifest) const;
        SerializeResult SaveToFile(const char* path, const DiaGameManifest& manifest) const;
    };

    class JsonDiaStageSerializer : public Dia::Serializer::ISerializer {
        // Same interface pattern for DiaStageManifest
    };
}
```

### Composition

```cpp
namespace Dia::Game {
    class GameFileComposer {
    public:
        // Loads .diagame, resolves typed imports via ManifestComposer
        ManifestValidationResult ComposeFromGameFile(
            const char* diagamePath,
            Dia::Application::ApplicationManifest& outComposedManifest,
            DiaGameManifest& outGameManifest);
    };
}
```

### Loading

```cpp
namespace Dia::Game {
    class GameLoader {
    public:
        // Full load: .diagame -> composed manifest -> instantiated PU tree
        static ProcessingUnit* LoadFromGameFile(
            ApplicationTypeRegistry& registry,
            const char* diagamePath,
            ManifestValidationResult& outResult);

        // Parse only (no PU construction)
        static ManifestValidationResult LoadGameManifest(
            const char* diagamePath, DiaGameManifest& outManifest);

        static ManifestValidationResult LoadStageManifest(
            const char* diastagePath, DiaStageManifest& outStage);
    };
}
```

### Low-Level Loader

```cpp
namespace Dia::Game {
    class DiaGameManifestLoader {
    public:
        static ManifestValidationResult LoadGameFile(const char* path, DiaGameManifest& outManifest);
        static ManifestValidationResult LoadStageFile(const char* path, DiaStageManifest& outManifest);
    };
}
```

## Dependencies

| Dependency | What DiaGame uses from it |
|-----------|--------------------------|
| DiaApplication | `TypedImport`, `ManifestComposer::ComposeFromTypedImports`, `ApplicationManifest`, `ManifestValidator`, `ApplicationManifestLoader`, `ApplicationTypeRegistry`, `ProcessingUnit` |
| DiaSerializer | `ISerializer`, `SerializeResult` |
| DiaCore | Containers (`DynamicArrayC`, `String256`, `String512`), `StringCRC`, JSON (jsoncpp) |
| DiaLogger | `DIA_LOG_*` macros |

**Dependency direction:** DiaGame depends on DiaApplication (not the reverse). DiaApplication has no knowledge of DiaGame types.

## File Formats

### .diagame (Game Project Root)

```json
{
    "name": "string",
    "version": "string",
    "imports": [
        { "path": "relative/path", "type": "manifest|stage" }
    ],
    "config": {
        "asset_root": "string (optional)",
        // ... extensible — unknown fields preserved via rawConfig
    }
}
```

### .diastage (Stage Descriptor)

```json
{
    "name": "string",
    "manifest": "relative/path/to/stage.diaapp"
}
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| DiaGame File Format | .diagame and .diastage file formats, parsing, typed imports | [diagame-file-format.md](../../features/dia/diaapplication/diagame-file-format.md) | Done (migrated from DiaApplication) |
| Game Serializers | JsonDiaGameSerializer + JsonDiaStageSerializer with round-trip support | — | Done |
| GameFileComposer | Orchestrates .diagame loading + import resolution | — | Done |
| GameLoader | High-level PU instantiation from .diagame | — | Done |
| Typed Game Config | Parse path_aliases, ultralight, and future config into typed DiaGameConfig fields | — | Not Started |
| Stage Config | Extend DiaStageManifest with per-stage config (path_aliases) | — | Not Started |

## Inherited Binding Decisions

| ID | Decision | How DiaGame honours it |
|----|----------|----------------------|
| PD-001 | Use StringCRC for all entity/component IDs | Stage IDs derived via StringCRC; import resolution uses StringCRC for path dedup |
| PD-004 | No STL containers in public APIs | All public types use Dia containers (DynamicArrayC, String256). STL limited to internal jsoncpp interop |
| PD-006 | Visual Studio project files are source of truth | DiaGame.vcxproj maintained manually; registered in Cluiche.sln |
| PD-007 | C++20 is the required language standard | Inherited from Directory.Build.props |
| PD-008 | Directory.Build.props owns output paths | DiaGame is a StaticLibrary; output to sharedlibs/ via Directory.Build.props |
| PD-010 | .diagame is the project root file | DiaGame IS the owner of this decision's implementation — defines, parses, serializes, and loads .diagame/.diastage |
| AD-001 | Module system with YAML frontmatter | DiaGame module doc to be created (dia.game.architecture.module.md) |
| AD-002 | No STL containers in public APIs | Same as PD-004 |
| AD-003 | Namespace convention: `Dia::<Module>::` | DiaGame uses `Dia::Game::` namespace |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Scope | Should DiaGame eventually own the stage merge logic (currently in ManifestComposer::MergeStageManifest)? | Possibly — stage merging is conceptually game-level, but deeply interleaved with ManifestComposer's internals. Revisit when stage logic grows beyond what ManifestComposer's inline .diastage parse handles. |
| 2 | rawConfig | Is Json::Value* on a public struct acceptable given AD-002 (no STL in APIs)? | Json::Value is from jsoncpp (external dependency), not STL. The pointer is nullable and owned. ApplicationManifest already uses this pattern for metadata. Acceptable. |
| 3 | Growth | Should game state persistence (save/load player progress) live in DiaGame? | No — game state is game-specific. DiaGame owns project structure, not runtime state. A future DiaGameState module could exist for shared save infrastructure. |
| 4 | Config | Should DiaGameConfig grow typed fields or stay minimal with rawConfig? | Grow typed fields for well-known config (path_aliases, ultralight). rawConfig provides forward compatibility for new fields before they get typed. Both coexist. |
| 5 | Dependencies | Could DiaApplication ever need to depend on DiaGame (circular)? | No. DiaApplication's ManifestComposer accepts TypedImports (its own type) and resolves stages via inline parse. The dependency is strictly one-way: DiaGame → DiaApplication. |

## Status

`Active` — Module extracted and operational. Serializers, composer, and loader all functional.
