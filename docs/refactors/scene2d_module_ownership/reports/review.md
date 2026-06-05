# Refactor Review — Scene2D Module Ownership

**Input:** Phases 1–4 changes

## Findings

### High Severity

- **`registeredtypes.diaschema` still lists `CameraModule`**
  Evidence: `Cluiche/Assets/CluicheTest/registeredtypes.diaschema:223` — `"type_id": "CameraModule"`
  Fix: Rename entry to `"type_id": "Camera2DModule"` and update description. Also add `Light2DModule` and `Scene2DModule` entries.

- **`ValidateCameras` count check removed without replacement**
  Evidence: `Scene2DTestStageModule.cpp` — old code checked `reg.GetCount() == 1`; new code only checks `Has("test_camera")`. Now that Camera2DModule's registry contains the `default` camera + `test_camera`, the count is 2, not 1. The check should be `GetCount() == 2` (or at minimum assert the default camera is still present).
  Fix: Change `ValidateCameras()` to `reg.GetCount() == 2 && reg.Has("test_camera")`.

### Medium Severity

- **`diacamera2d.md` spec and `dia.camera2d.architecture.module.md` reference `CameraModule` by name**
  Evidence: `docs/specs/systems/dia/diacamera2d.md:16,25,49` and `Dia/DiaCamera2D/dia.camera2d.architecture.module.md:33` — describe application-side integration as `CameraModule`.
  Fix: Update both to reference `Camera2DModule`.

- **`geometry2d-picking.md` spec references `CameraModule` throughout**
  Evidence: `docs/specs/features/dia/diapicking/geometry2d-picking.md:32,54,185,217,221` — AC11, task 6, design note all use the old name.
  Fix: Replace `CameraModule` with `Camera2DModule` in the spec body. These are historical — the feature is done — but stale names in specs cause confusion.

- **`geometry2d-picking.plan.md` and `diacamera2d.plan.md` likely also reference the old name**
  Evidence: Both plan files appeared in the grep results; not yet read.
  Fix: Check and update.

### Low Severity

- **`Scene2DTestStageModule` dependency list in `.diaapp` does not include `EntityModule`, `Camera2DModule`, `Light2DModule`**
  Evidence: `scene2d_test_stage.diaapp:30–34` — `Scene2DTestStageModule` only declares `["VisualDebuggerModule", "Scene2DModule"]`. Since `Scene2DModule` depends on all three, this is transitively safe, but explicit deps on the modules it directly reads would be cleaner and more defensible.
  Fix: Optional — add `"EntityModule"`, `"Camera2DModule"`, `"Light2DModule"` to `Scene2DTestStageModule` dependencies. Not a correctness issue.

## Summary

The structural refactor is correct — single ownership of each registry/domain is achieved and no code calls the removed `GetCameraRegistry`/`GetLightRegistry`/`GetEntityDomain` accessors. Two correctness issues need fixing before the prove step: the schema file still names `CameraModule` (runtime type resolution could fail), and the camera count invariant in `ValidateCameras` is now wrong (registry has 2 cameras, check was silently weakened). Three medium-severity spec/doc updates are needed to keep the reference material consistent.
