# Research: Explore — Engine Data Architecture Foundation

**Session date:** 2026-04-27
**Folder:** docs/research/engine_data_arch/
**Reference input:** engine_data_architecture_foundation.md (external design document)

## Problem Space Overview

A game engine that aspires to support content-heavy development needs more than code-level abstractions — it needs a data architecture. This means stable identity for every meaningful object (assets, definitions, configs, scenes), a central registry to discover and inspect them, schema-driven validation, a build pipeline that transforms raw authored data into runtime-ready formats, and tooling that lets developers navigate the whole graph.

The reference document describes this in full production scope: 12 subsystems spanning identity, registry, schema, import, metadata, relationship graphs, dependency analysis, build/cook, caching, packaging, explorer UI, and validation. That architecture is proven (Unreal's Asset Registry, Unity's Addressables, Bungie's Tag system) but took large teams years to build.

The constraint for Cluiche is different: a small AI-developed game, small scope, but with room to grow. The question is not "how do we build all 12 systems?" but "what is the smallest vertical slice that unlocks real value and can be extended later without rework?"

## Existing Approaches

- **Unreal Engine Asset Registry**: Full-featured catalog with stable FNames/FPaths, asset type metadata, cook pipeline, reference graph, streaming support. Took years and hundreds of engineers.
- **Unity Addressables**: Layered on top of AssetBundles — stable string addresses, async loading, group-based packaging, build pipeline with content update support.
- **Godot Resources**: Simple ResourceLoader/ResourceSaver with `.tres`/`.res` files, path-based identity, no separate build step (resources loaded directly). Very accessible, limited scalability.
- **Custom indie approaches**: Many small engines use a flat manifest (JSON/TOML) mapping string IDs to file paths, with a simple loader and no build pipeline. Works for small games, breaks at scale.
- **ECS-adjacent data stores**: Some engines treat game definitions as ECS archetypes serialized to disk, unifying runtime and authored formats.

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| **Identity model** | Path-based / CRC-hashed / UUID / Composite (type+name) | Dia already uses StringCRC everywhere; natural fit for CRC-hashed IDs |
| **Registry scope** | Assets only / Assets + game defs / All meaningful objects | Broader scope = more value but more work |
| **Raw vs built separation** | None (load raw) / Soft (optional cook) / Hard (mandatory cook) | Hard separation is ideal but expensive to implement early |
| **Schema approach** | Code-only (C++ structs) / Data-driven (JSON schema) / Hybrid | Dia's TypeSystem already provides C++ reflection; extending to data-driven schemas is possible |
| **Build pipeline** | None / Simple copy+validate / Full cook with transforms | DiaAPI command framework could host build steps |
| **Storage format** | JSON / Binary / Mixed (JSON authored, binary runtime) | JSON for authoring (human-readable), binary for runtime |
| **Tooling surface** | CLI only / CLI + editor panels / Full explorer UI | CluicheEditor exists with CEF; could host explorer panels |
| **Reference model** | String paths / Typed CRC refs / Handle<T> with generation | Dia has Handle<T> already for safe references |
| **Incremental support** | Full rebuild / Hash-based dirty detection / Dependency-graph invalidation | Hash-based is the pragmatic middle ground |

## Known Tradeoffs

- **Stable IDs vs simplicity**: CRC/UUID identity survives renames but adds indirection; path-based identity is simpler but fragile
- **Mandatory cook vs raw loading**: A cook step ensures runtime consistency but slows iteration; raw loading is fast for small projects but doesn't scale
- **Schema strictness vs authoring freedom**: Strict schemas catch errors early but increase authoring friction; loose schemas are flexible but accumulate silent bugs
- **Central registry vs distributed discovery**: A single catalog enables powerful queries but becomes a bottleneck and single point of failure; distributed discovery is resilient but hard to query
- **Build complexity vs project size**: For a small AI-developed game, a full cook pipeline may be overengineering; but retrofitting one later is painful
- **Engine-generic vs game-specific**: Building too generically wastes time on abstractions nobody uses; building too specifically creates technical debt when scope grows

## Known Pitfalls (C++ / game engine context)

- **Compile-time registration overhead**: C++ lacks runtime reflection natively; Dia's TypeSystem solves this but registration must be explicit
- **Serialization versioning**: Schema evolution in C++ is painful without a migration strategy; Dia's TypeJsonSerializer already has v2 versioning which helps
- **Memory ownership in registries**: Who owns the canonical record? The registry? The loader? Need clear ownership semantics
- **Thread safety for asset access**: If ProcessingUnits run on separate threads, asset loading and registry queries must be thread-safe
- **Build tool portability**: MSBuild-only (PD-006) means build tools must be Windows-native or DiaAPI plugins
- **Hash collisions**: CRC32 has non-trivial collision probability at scale; acceptable for a small game but worth monitoring
- **Over-abstracting early**: The reference document describes 12 subsystems; implementing all of them before having real content to manage is a classic trap

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| **DiaCore TypeSystem** | Production-ready runtime RTTI, field reflection, JSON serialization with versioning. Natural foundation for schema validation and canonical record representation |
| **DiaCore CRC/StringCRC** | Already the engine-wide identity primitive (PD-001). Asset IDs would naturally be StringCRC values |
| **DiaCore Containers** | HashTableC, DynamicArrayC, Handle<T> — building blocks for registries and safe references |
| **DiaCore FilePath/PathStore** | Path aliasing with deferred resolution. Decouples asset references from physical locations — exactly what the identity system needs |
| **DiaCore JSON** | jsoncpp wrapper provides read/write. Sufficient for authored data format |
| **DiaAPI CommandRegistry** | Plugin-based command framework with categories. Natural host for import/build/validate commands |
| **DiaApplicationFlow ProcessingUnit** | Multi-phase lifecycle orchestration. Could model the import->validate->build pipeline as phases |
| **DiaApplicationFlow Module** | Hot-reload support, dependency graphs, JSON config. Asset systems could be Modules within a build ProcessingUnit |
| **CluicheEditor / DiaEditor** | Plugin-based editor with CEF UI. Natural host for explorer/inspection tooling |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-001 StringCRC | Asset IDs must use StringCRC, not raw strings. This aligns perfectly with the reference doc's "stable identity" requirement |
| PD-002 ProcessingUnit/Phase/Module | Build pipeline should be structured as a ProcessingUnit with phases (Import, Validate, Build, Stage), not an ad-hoc script |
| PD-003 Component system | Game definitions that reference assets should use IComponent/IComponentObject patterns |
| PD-004 No STL in public APIs | Registry, catalog, and all public interfaces must use DiaCore containers (HashTableC, DynamicArrayC) |
| PD-005 x64 Windows only | No cross-platform packaging concerns for now; simplifies the build/stage step significantly |
| PD-007 C++20 required | Can use concepts for schema constraints, std::span for zero-copy views into asset data, constexpr for compile-time validation |
| PD-008 Directory.Build.props | Build output paths are centralized; asset build output should follow the same `Cluiche/bin/` or `Cluiche/out/` convention |
| PD-009 Generated output under Cluiche/out/ | Built/cooked asset data belongs under `Cluiche/out/<AppName>/` alongside other generated output |

## Open Questions for Ideation

- **What's the minimum useful registry?** Is it enough to just catalog assets (meshes, textures, audio), or do game definitions (weapon stats, enemy configs) need to be in there from day one?
- **Is a cook step needed for a small game?** Could we start with raw-loading (JSON) and add cooking later, or does that create too much rework?
- **Should the schema system extend TypeSystem or be a separate data-driven layer?** Dia's TypeSystem is C++-defined; data-driven schemas (JSON Schema-like) would let AI authors define types without recompiling
- **Where does the registry live at runtime?** Is it a Module in the main ProcessingUnit? A standalone singleton? A separate ProcessingUnit?
- **How does this interact with DiaEnv?** The DiaEnv system (currently in spec) manages environment/dependency configuration — asset paths and build configuration may overlap
- **What's the right first game-data domain to target?** Textures? Config files? Scene definitions? The choice determines which importer and build step to implement first
- **Should the explorer be a DiaAPI CLI tool, a CluicheEditor panel, or both?** CLI is faster to build; editor panel is more discoverable
- **How much of the reference document's relationship graph is needed for a small game?** Full forward+reverse dependency tracking, or just "this asset references these other assets"?
