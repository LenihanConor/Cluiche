# Refactor Summary — Scene2D Module Ownership

**Session folder:** docs/refactors/scene2d_module_ownership/
**Date:** 2026-06-04

## One-Line Outcome

Scene2DModule is now a pure loader — cameras, lights, and entities live in dedicated single-owner modules (Camera2DModule, Light2DModule, EntityModule) and are visible to all consumers.

## Loop Completed

1. **Audited:** Scene2DModule owned parallel copies of CameraRegistry2D, LightRegistry2D, and Entity::Domain, creating three isolated worlds invisible to the modules meant to update and expose them. No Light2DModule existed. CameraModule name mismatched the engine namespace.
2. **Planned:** 4 phases — rename CameraModule→Camera2DModule, create Light2DModule, rewire Scene2DModule to use ModuleRefs, update consumers. Target: single ownership per registry/domain.
3. **Executed:** All 4 phases applied across 18 files. Scene2DModule stripped to LayerTable + SceneLoader2D + 3 ModuleRefs. Camera2DModule and Light2DModule created. Scene2DTestStageModule updated with 4 ModuleRefs.
4. **Reviewed:** 2 high (schema stale, camera count check weakened), 3 medium (doc/spec stale names), 1 low (optional manifest deps). All 6 findings fixed before prove.
5. **Proved:** A3–A8 fully supported by static analysis. A1–A2 partially supported (code structure correct; live run output pending).

## Specs Updated

| Spec | Sections Changed |
|------|-----------------|
| `docs/specs/features/cluichegamebaseline/scene2d-module.md` | Design: interface, ownership table, .diaapp wiring |
| `docs/specs/systems/dia/diacamera2d.md` | CameraModule → Camera2DModule throughout |
| `docs/specs/systems/dia/diacamera2d.plan.md` | CameraModule → Camera2DModule throughout |
| `Dia/DiaCamera2D/dia.camera2d.architecture.module.md` | non_responsibilities: CameraModule → Camera2DModule |
| `docs/specs/features/dia/diapicking/geometry2d-picking.md` | CameraModule → Camera2DModule throughout |
| `docs/specs/features/dia/diapicking/geometry2d-picking.plan.md` | CameraModule → Camera2DModule throughout |

## Remaining Work

- **A1/A2 live verification:** Run `dia run cluichetest` and `dia run googletest` to confirm Scene2DTestStage passes all 5 checkpoints and no GoogleTests regress
- **Low finding (optional):** Add `EntityModule`, `Camera2DModule`, `Light2DModule` as explicit deps on `Scene2DTestStageModule` in `scene2d_test_stage.diaapp` for clarity
