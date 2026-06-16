# Implementation Plan: DiaLighting2D

**Spec:** @docs/specs/applications/dia/systems/dialighting2d/dialighting2d.md
**Status:** Done

---

## Summary

DiaLighting2D is a lightweight engine library (static lib) providing the `PointLight2D` value type and `LightRegistry2D`. Mirrors the DiaCamera2D pattern exactly: value type + registry + testing builder + module doc + vcxproj. No PU/Module coupling, no rendering, no behaviours (v2).

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|---|---|---|---|---|
| 1 | Create `Dia/DiaLighting2D/` directory structure | — | Done | haiku | Directories: root, Registry/, Testing/ |
| 2 | Create `PointLight2D.h` value type | Unit tests (T2) | Done | sonnet | Struct: position Vec2D, radius float, colour[4] RGBA, intensity float, layerMask uint32_t, enabled bool. Trivially copyable. |
| 3 | Create `LightRegistry2D.h/.cpp` | Unit tests (T3) | Done | sonnet | Register/Unregister/Get/Has/GetCount/GetByIndex/GetLightsForLayer. Fixed-slot array like CameraRegistry2D (kMaxLights=16). StringCRC keys. |
| 4 | Create `Testing/LightBuilder.h` | Used by T2,T3 | Done | haiku | Fluent builder: `WithLight(id, posX, posY, radius, ...)`, `OnLayers(mask)`, `Disabled()`. Returns `LightRegistry2D&`. |
| 5 | Create `DiaLighting2D.vcxproj` + `.vcxproj.filters` | `dia run googletest` builds | Done | sonnet | Static library. Include dirs: `./;./../`. Deps: DiaMaths, DiaCore. 4 configs (Debug, Release, Debug-Asan, Debug-Ubsan). Copy DiaCamera2D.vcxproj as template. |
| 6 | Register project in `Cluiche.sln` | Solution loads | Done | haiku | Add project entry + config mappings. Place in Dia solution folder. New GUID. |
| 7 | Add `ProjectReference` in GoogleTests.vcxproj | Tests link | Done | haiku | Reference DiaLighting2D.vcxproj. Add include path. |
| 8 | Write GoogleTest files | `dia run googletest --filter="LightRegistry2D*"` | Done | sonnet | `Tests/GoogleTests/DiaLighting2D/TestLightRegistry.cpp` — Register/Unregister/Get/Has, GetByIndex iteration, GetLightsForLayer filtering, duplicate-id assert, max-capacity. |
| 9 | Create `dia.lighting2d.architecture.module.md` | — | Done | haiku | YAML frontmatter: module_id `dia.lighting2d`, deps [dia.core, dia.maths], forbidden [dia.graphics, dia.entity, dia.scene2d, dia.applicationflow, dia.bgfx]. |
| 10 | Build + run tests green | `dia run googletest --filter="*Light*"` | Done | sonnet | Full pipeline compile; all tests pass. |

---

## Dependency Graph

```
T1 (dirs)
 ├── T2 (PointLight2D.h)
 ├── T4 (LightBuilder.h) — needs T2
 ├── T3 (LightRegistry2D) — needs T2
 ├── T5 (vcxproj) — needs T2, T3
 │    ├── T6 (sln registration) — needs T5
 │    └── T7 (GoogleTests ref) — needs T5
 │         └── T8 (test files) — needs T3, T4, T7
 ├── T9 (module doc) — independent
 └── T10 (build+verify) — needs all above
```

**Parallelizable pairs:** T2+T9, T4+T5 (once T2 done), T6+T7 (once T5 done).

---

## Key Decisions (from spec)

- **No DiaGeometry2D dependency** — no spatial queries in v1 (unlike DiaCamera2D which uses AARect for BoundsClamp)
- **Layer mask is raw uint32_t** — name→bit resolution is caller's responsibility (DiaScene2D or application code)
- **Fixed-slot storage** — `kMaxLights = 16` array (no heap). Can bump later if needed.
- **No observability in v1** — unlike CameraRegistry2D which has Gauge/Counter. Add when renderer consumes the registry.
- **No behaviours** — no flicker/pulse/etc. v2 will follow ICameraBehaviour factory pattern.

---

## File Inventory (what will be created)

```
Dia/DiaLighting2D/
├── PointLight2D.h
├── Registry/
│   ├── LightRegistry2D.h
│   └── LightRegistry2D.cpp
├── Testing/
│   └── LightBuilder.h
├── DiaLighting2D.vcxproj
├── DiaLighting2D.vcxproj.filters
└── dia.lighting2d.architecture.module.md

Cluiche/Tests/GoogleTests/DiaLighting2D/
└── TestLightRegistry.cpp
```

---

## Risk / Open Items

1. **GUID collision** — generate fresh GUID for vcxproj (not copy DiaCamera2D's)
2. **Directory.Build.props** — verify `bin/sharedlibs/` output path picks up new project automatically (it should via convention)
3. **GoogleTests link order** — may need DiaLighting2D added to AdditionalDependencies if not automatic via ProjectReference
