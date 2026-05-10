# Feature Spec: Manifest Configuration

## Parent System
@docs/specs/systems/cluichetest/applicationflow.md

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | CluicheTest | @docs/specs/applications/cluichetest.md |
| System | CluicheTest Application Flow | @docs/specs/systems/cluichetest/applicationflow.md |
| Feature | Manifest Configuration | (this file) |

## Problem Statement

CluicheTest needs its concrete manifest files — the `.diagame`, base `.diaapp`, and stage `.diastage`/`.diaapp` files — written and integrated so the application can boot from config.

## Acceptance Criteria

1. `cluichetest.diagame` — project root file with typed imports (one manifest, one+ stages)
2. `cluiche.diaapp` — base manifest: 3 PUs, 2 streams (+SimToUI), infrastructure modules, stages list
3. `dummy_stage.diastage` — declares DummyStage name and points to its manifest
4. `dummy_stage.diaapp` — DummyLevelModule added to SimPU for DummyStage
5. All module instance_ids, type_ids, dependencies, reads/writes match the module implementations
6. PU startup order: MainPU → SimPU → RenderPU
7. Boot is auto-stage, DummyStage is auto-stage (auto-advance from Boot)
8. Manifest validates cleanly (no errors from ManifestValidator)
9. Main.cpp loads .diagame, composes manifest, creates Application, runs Start/Update loop

## File Contents

### cluichetest.diagame

```json
{
    "name": "CluicheTest",
    "version": "2.0.0",
    "imports": [
        { "path": "cluiche.diaapp", "type": "manifest" },
        { "path": "dummy_stage.diastage", "type": "stage" }
    ],
    "config": {}
}
```

### cluiche.diaapp

As specified in the system spec — 3 PUs, 3 streams (InputToSim, SimToRender, SimToUI), all infrastructure modules with correct dependencies and stream handles.

### dummy_stage.diastage

```json
{
    "name": "DummyStage",
    "manifest": "dummy_stage.diaapp"
}
```

### dummy_stage.diaapp

DummyLevelModule merged into SimPU with stage=DummyStage, correct deps/streams/timeouts.

### Main.cpp Entry Point

```cpp
int main() {
    // Create bootstrap resources (window, canvas)
    auto bootstrap = CreateBootstrapResources();

    // Load and compose manifest
    auto manifest = Dia::ApplicationFlow::ManifestComposer::Compose("cluichetest.diagame");

    // Validate
    Dia::ApplicationFlow::ManifestValidator validator(Dia::ApplicationFlow::GetGlobalTypeRegistry());
    validator.Validate(manifest);
    if (validator.HasErrors()) { /* log and exit */ }

    // Create and run application
    Dia::ApplicationFlow::Application app(manifest, Dia::ApplicationFlow::GetGlobalTypeRegistry(), bootstrap);
    app.Start();
    while (app.Update()) {}

    return 0;
}
```

## Files Touched

| File | Action |
|------|--------|
| `Cluiche/CluicheTest/Data/cluichetest.diagame` | New |
| `Cluiche/CluicheTest/Data/cluiche.diaapp` | New |
| `Cluiche/CluicheTest/Data/dummy_stage.diastage` | New |
| `Cluiche/CluicheTest/Data/dummy_stage.diaapp` | New |
| `Cluiche/CluicheTest/Main.cpp` | Rewrite — v2 entry point |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` | Update (data files, Main.cpp) |

## Dependencies

- **DiaApplicationFlow** — Application, ManifestComposer, ManifestValidator, TypeRegistry
- **DiaSerializer** — JSON file loading
- **Config Format v2** (DiaAppFlow feature) — manifest schema
- **All CluicheTest modules** — registered via DIA_MODULE, referenced by type_id in manifest

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-010 | Platform | .diagame is root, .diastage for stages | Compliant — exact pattern used |
| SD-001 (DiaAppFlow) | System | Config is sole source of truth | Compliant — manifest is the complete app description |
| SD-005 (CT AppFlow) | System | Stage-specific from .diastage | Compliant — DummyStage comes from dummy_stage.diastage |
| SD-010 (DiaAppFlow) | System | PU order = array order | Compliant — MainPU first, SimPU second, RenderPU third |
| AD-003 (CT) | App | Entry point in Main.cpp | Compliant — Main.cpp creates and runs Application |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Location | Where do .diaapp/.diastage files live relative to the binary? | In a `Data/` directory deployed alongside the binary by the asset pipeline. Paths in .diagame are relative to .diagame location. |
| 2 | Bootstrap | What does CreateBootstrapResources() return? | A struct with window handle and canvas pointer. Implementation creates SFML window (title, resolution from config or hardcoded for testbed). |
| 3 | Deployment | Should manifest files be copied to output by the build or asset pipeline? | Asset pipeline deploys them (per-app-bin-layout feature). For development, a post-build xcopy or DiaCLI handles it. |
| 4 | Config | Should .diagame contain app config (window title, resolution) or is that separate? | .diagame has a `"config"` field for app-level settings. Bootstrap reads it for window creation params. Keeps everything in one hierarchy. |

## Open Questions

None.

## Status

`Approved` — 2026-05-09
