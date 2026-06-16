# Implementation Plan: DiaScene2D

**Spec:** [diascene2d.md](diascene2d.md)
**Status:** Done
**Research:** [design-decisions.md](../../../../../research/diascene2d/design-decisions.md)

---

## Context

DiaScene2D is the engine library for 2D scene management. It owns the `.diascene` file format (reflected struct), the layer table, and the SceneLoader that populates camera/light registries and spawns entities from scene files.

**Dependencies (all shipped):**
- DiaCamera2D — `Dia/DiaCamera2D/` (CameraRegistry2D, Camera2D, 8 behaviours, CameraBehaviourRegistry)
- DiaLighting2D — `Dia/DiaLighting2D/` (LightRegistry2D, PointLight2D)
- diaentitytemplate — `Dia/diaentitytemplate/` (Domain, blueprint loader, ComponentMacros, reflection)
- DiaCore/Reflect — `Dia/DiaCore/Reflect/` (JsonArchive, DIA_SERIALIZE, JsonDefinitionLoader)
- DiaGeometry2D — `Dia/DiaGeometry2D/` (AARect for world_bounds)

**Serialization note:** The spec references "DiaReflect" as a dependency. This is `Dia/DiaCore/Reflect/` (JsonReadArchive, JsonWriteArchive, DIA_SERIALIZE macros, SerializeResult). It is production-ready and used by DiaGeometry2D, diaentitytemplate, DiaAnimation2D etc. No separate DiaReflect module is needed.

---

## Phase 0: Spec Approval

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| T-00 | Approve system spec — mark `diascene2d.md` status `Draft` → `Approved`; resolve remaining open design questions (DiaGraphics dep direction, default layer, camera validation) | Spec file updated | Done | haiku | Open Q answers from research: (1) DiaGraphics takes ViewportTransform from DiaCamera2D; (2) default layer is implicit — auto-injected if not declared; (3) camera validation = hard error at load, recoverable at runtime via fallback |
| T-01 | Write feature spec `scene2d-format` — Scene2D struct, LayerDef, CameraEntry, LightEntry, EntityInstance, LayerTable, DIA_SERIALIZE roundtrip, `.diascene` schema | Spec file exists at `docs/specs/features/dia/diascene2d/scene2d-format.md`, status = Approved | Done | sonnet | Interview may be minimal — design decisions already locked in research |
| T-02 | Write feature spec `scene2d-loader` — SceneLoader2D, SceneLoadContext, Load/Unload, camera hydration, light hydration, entity spawning, instance_data patching, layer mask resolution, validation rules | Spec file exists at `docs/specs/features/dia/diascene2d/scene2d-loader.md`, status = Approved | Done | sonnet | Depends on T-01 (references format types) |

---

## Phase 1: Project Scaffold

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| T-03 | Create `Dia/DiaScene2D/` directory + `DiaScene2D.vcxproj` + `.vcxproj.filters`; add to `Cluiche.sln`; add project reference from downstream consumers | `msbuild Dia/DiaScene2D/DiaScene2D.vcxproj` succeeds (empty lib) | Done | sonnet | Static lib. References: DiaCamera2D, DiaLighting2D, diaentitytemplate, DiaCore, DiaMaths, DiaGeometry2D. Use DiaLighting2D.vcxproj as template. |
| T-04 | Create `dia.scene2d.architecture.module.md` YAML module doc | File exists, valid YAML frontmatter | Done | haiku | Namespace: `Dia::Scene2D::`. Layer: TBD (pending DiaArchitecture C7). |

---

## Phase 2: scene2d-format (data types + serialization)

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| T-05 | Implement `Scene2D.h` — all reflected structs: `LayerDef`, `CameraEntry`, `LightEntry`, `EntityInstance`, `Scene2D` | Builds | Done | sonnet | All types in `Dia::Scene2D::` namespace. Use `DynamicArrayC` with caps from spec (layers=32, cameras=4, lights=16, entities=256). `world_bounds` = `Dia::Geometry2D::AARect`. |
| T-06 | Implement `Scene2DSerializers.h/.cpp` — `DIA_SERIALIZE` free functions for all 5 structs | Builds | Done | sonnet | Follow `DiaGeometry2DSerializers.h` pattern exactly. `instance_data` is a `Json::Value` (opaque map, resolved at load time by SceneLoader2D, not at deserialization). |
| T-07 | Implement `LayerTable.h/.cpp` — `Build()`, `GetBitIndex()`, `ResolveMask()`, `GetCount()`, `GetByIndex()`, `GetById()`, `Has()`, default layer injection | Builds | Done | sonnet | Max 32 layers (uint32 bitmask). Default layer auto-injected at bit 0 if not declared. HashTable<StringCRC, unsigned int> for name→bit lookup. |
| T-08 | GoogleTest: `Scene2DFormatTests` — roundtrip serialize/deserialize all structs; LayerTable build + mask resolution; default layer injection; edge cases (empty layers, max entities) | `dia run googletest --filter="Scene2DFormat*"` passes | Done | sonnet | Test file: `Cluiche/Tests/GoogleTests/DiaScene2D/Scene2DFormatTests.cpp`. Add to GoogleTests.vcxproj. |

---

## Phase 3: scene2d-loader (hydration + validation)

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| T-09 | Implement `SceneLoadContext.h` — struct holding refs to CameraRegistry2D, LightRegistry2D, Domain | Builds | Done | haiku | Thin struct, no logic. |
| T-10 | Implement `SceneLoader2D.h/.cpp` — `Load()`: parse JSON via JsonReadArchive, build LayerTable, hydrate cameras into CameraRegistry2D, hydrate lights (resolve affects_layers → bitmask) into LightRegistry2D, spawn entities into Domain via blueprint + instance_data | `dia run googletest --filter="Scene2DLoader*"` passes | Done | opus | Core complexity: camera behaviour factory resolution, instance_data field patching via diaentitytemplate reflection, error accumulation. This is the most architecturally complex task. |
| T-11 | Implement `SceneLoader2D::Unload()` — unregister cameras, unregister lights, destroy spawned entities | Test: load then unload leaves registries empty | Done | sonnet | Track registered IDs in a `DynamicArrayC<StringCRC>` per category for cleanup. |
| T-12 | Implement validation in `Load()` — exactly one active camera (hard error), layer name resolution warnings, blueprint-not-found errors | Test: invalid scenes produce expected errors | Done | sonnet | Use `SerializeResult`-style error accumulation. Return bool success + error list. |
| T-13 | GoogleTest: `Scene2DLoaderTests` — load valid .diascene, verify camera registry populated, lights have correct bitmasks, entities spawned with patched fields; unload clears all; validation rejects bad scenes | `dia run googletest --filter="Scene2DLoader*"` passes | Done | sonnet | Needs test `.diascene` fixture files in test data directory. Mock/stub Domain if needed, or use real Domain with test components. |

---

## Phase 4: Integration + Prove

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| T-14 | Create sample `.diascene` file for CluicheTest (e.g., `Cluiche/Assets/Stages/DummyStage/dummy.diascene`) demonstrating layers + camera + light + entities | File parses cleanly via loader | Done | haiku | Demonstrates the full format; useful for manual integration testing later. |
| T-15 | Full build: `msbuild Cluiche/Cluiche.sln /p:Configuration=Debug /p:Platform=x64`; run all tests: `dia run googletest` | Zero new failures | Done | haiku | |
| T-16 | Update system spec status → `Done`; update feature spec statuses → `Done`; update backlog | Specs + backlog updated | Done | haiku | |

---

## Dependencies

```
T-00 (approve system spec) ── T-01 (format spec) ── T-02 (loader spec)
                                     │
                                     ▼
                              T-03 (vcxproj) ── T-04 (module doc)
                                     │
                                     ▼
                    ┌─── T-05 (structs) ── T-06 (serializers) ── T-07 (LayerTable)
                    │                                                    │
                    │                                                    ▼
                    │                                             T-08 (format tests)
                    │
                    └─── T-09 (context) ─────┐
                                             ▼
                              T-10 (Load) ── T-11 (Unload) ── T-12 (validation)
                                                                      │
                                                                      ▼
                                                              T-13 (loader tests)
                                                                      │
                                                                      ▼
                                                    T-14 (sample) ── T-15 (full build) ── T-16 (done)

Parallelism:
- T-05 and T-09 can run in parallel (structs vs load context — independent headers)
- T-06 depends on T-05 (serializes the structs)
- T-07 depends on T-05 (uses LayerDef)
- T-10 depends on T-07 + T-09 (loader uses LayerTable + context)
- T-08 can run after T-07 (tests format layer)
- T-13 can run after T-12 (tests all loader paths)
```

---

## Key Design Decisions (from spec + research)

1. **`instance_data` stored as `Json::Value`** in the deserialized struct — resolved lazily by SceneLoader2D at load time, not by the archive. This avoids coupling the reflected struct to diaentitytemplate's field patching API.

2. **Camera/light hydration uses blueprint + instance_data** — same format as entities but into registries, not ECS. SceneLoader2D resolves the blueprint (catalogue lookup), creates the runtime value (Camera2D / PointLight2D), patches fields via reflection, then registers.

3. **No DiaGraphics dependency** — SceneLoader2D populates registries and spawns entities. Rendering is the application's concern (wired in PU modules).

4. **LayerTable is scene-scoped** — built per-load, not global. Each scene defines its own layer set. The active LayerTable is passed to renderers by the application.

5. **Unload tracks ownership** — SceneLoader2D remembers what it registered (camera IDs, light IDs, entity handles) so Unload can cleanly reverse it without touching things registered by code.

---

## Files Created/Modified

| File | Change | Task |
|------|--------|------|
| `docs/specs/systems/dia/diascene2d.md` | Status Draft → Approved | T-00 |
| `docs/specs/features/dia/diascene2d/scene2d-format.md` | New feature spec | T-01 |
| `docs/specs/features/dia/diascene2d/scene2d-loader.md` | New feature spec | T-02 |
| `Dia/DiaScene2D/DiaScene2D.vcxproj` | New project file | T-03 |
| `Dia/DiaScene2D/DiaScene2D.vcxproj.filters` | New filters file | T-03 |
| `Cluiche/Cluiche.sln` | Add DiaScene2D project | T-03 |
| `Dia/DiaScene2D/dia.scene2d.architecture.module.md` | New module doc | T-04 |
| `Dia/DiaScene2D/Scene2D.h` | Reflected structs | T-05 |
| `Dia/DiaScene2D/Scene2DSerializers.h` | DIA_SERIALIZE declarations | T-06 |
| `Dia/DiaScene2D/Scene2DSerializers.cpp` | DIA_SERIALIZE implementations | T-06 |
| `Dia/DiaScene2D/LayerTable.h` | LayerTable class | T-07 |
| `Dia/DiaScene2D/LayerTable.cpp` | LayerTable implementation | T-07 |
| `Cluiche/Tests/GoogleTests/DiaScene2D/Scene2DFormatTests.cpp` | Unit tests | T-08 |
| `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj` | Add test files | T-08, T-13 |
| `Dia/DiaScene2D/SceneLoadContext.h` | Context struct | T-09 |
| `Dia/DiaScene2D/SceneLoader2D.h` | Loader class declaration | T-10 |
| `Dia/DiaScene2D/SceneLoader2D.cpp` | Load + Unload + validation | T-10, T-11, T-12 |
| `Cluiche/Tests/GoogleTests/DiaScene2D/Scene2DLoaderTests.cpp` | Loader unit tests | T-13 |
| `Cluiche/Assets/Stages/DummyStage/dummy.diascene` | Sample scene file | T-14 |
| `docs/BACKLOG.md` | Move DiaScene2D to Done | T-16 |

---

## Risks & Open Items

1. **`instance_data` patching** — diaentitytemplate's `JsonBlueprintLoader` handles `Component.Field: value` for entities. Camera/light patching needs equivalent logic but targets non-entity types (Camera2D, PointLight2D). May need a thin reflection adapter or manual field-by-field application in v1.

2. **Blueprint resolution** — SceneLoader2D needs to look up blueprints by StringCRC from the asset catalogue. The exact API path (catalogue → asset → parsed blueprint) should be confirmed when implementing T-10.

3. **CameraBehaviourRegistry wiring** — DiaCamera2D has the factory registry. Confirm it's accessible from a library context (no module/PU coupling needed — just needs the registry instance passed in SceneLoadContext or discovered via include).

4. **Test fixture approach for T-13** — Loader tests need a real or mocked Domain + registries. Real Domain is preferred (matches engine patterns) but requires component pool setup. Decide during T-13.
