# Refactor Prove — Scene2D Module Ownership

**Input:** docs/refactors/scene2d_module_ownership/outputs/plan.json acceptance criteria

## Claims

### Claim A1: dia run cluichetest — Scene2DTestStage passes all 5 checkpoints
**Assessment:** partial
**Evidence for:**
- `Scene2DTestStageModule` compiles against correct modules; all 5 checkpoint lambdas present
- `ValidateCameras` now checks `GetCount() == 2 && Has("test_camera")` — correct for Camera2DModule's default camera + scene camera
- `ValidateLights` reads from `mLightRef` (Light2DModule)
- `ValidateEntities` reads from `mEntityRef` (EntityModule)
- `ValidateLayers` reads from `mSceneRef` (Scene2DModule, owner of LayerTable)
- Scene2DModule's `DoStart` null-checks all three ModuleRefs and returns `kFailed` cleanly if any are absent
- Manifest dependency order: EntityModule → Camera2DModule → Light2DModule start before Scene2DModule
**Evidence against:** n/a (code-structural; runtime verification requires `dia run cluichetest`)
**Missing evidence:** Live run output confirming all 5 checkpoints pass
**Next checks:** `dia run cluichetest` — confirm Scene2DTestStage passes

---

### Claim A2: dia run googletest — all existing tests pass
**Assessment:** partial
**Evidence for:**
- No GoogleTest files include `CameraModule.h` — only `TestCameraViewport.cpp` referenced it, and only in comments
- `TestCameraViewport.cpp` uses `Dia::Graphics::Camera2D` / `ViewportTransform` directly, not via module — unaffected by rename
- `registeredtypes.diaschema` now has `Camera2DModule`, `Light2DModule`, `Scene2DModule` — schema-validated tests will pass
**Evidence against:** n/a
**Missing evidence:** Live run output confirming all GoogleTests pass
**Next checks:** `dia run googletest`

---

### Claim A3: Only one Entity::Domain instance exists at runtime in SimPU
**Assessment:** supported
**Evidence for:**
- `Entity::Domain` declared as member only in `EntityModule.h` (grep returns 1 file in CluicheGameBaseline)
- `Scene2DModule.h` has no `Entity::Domain` member — confirmed by grep returning no matches
- `Scene2DModule.cpp` has no `#include <DiaEntity/Domain.h>` — confirmed by grep
**Evidence against:** none
**Missing evidence:** none

---

### Claim A4: Only one CameraRegistry2D instance exists at runtime in SimPU
**Assessment:** supported
**Evidence for:**
- `CameraRegistry2D` declared as member only in `Camera2DModule.h` and `Camera2DModule.cpp` (grep returns exactly those 2 files in CluicheGameBaseline)
- `Scene2DModule.h` and `.cpp` contain no `CameraRegistry2D` include or member — grep confirms
**Evidence against:** none
**Missing evidence:** none

---

### Claim A5: Only one LightRegistry2D instance exists at runtime in SimPU
**Assessment:** supported
**Evidence for:**
- `LightRegistry2D` declared as member only in `Light2DModule.h` and `Light2DModule.cpp` (grep returns exactly those 2 files in CluicheGameBaseline)
- `Scene2DModule.h` and `.cpp` contain no `LightRegistry2D` include or member — grep confirms
**Evidence against:** none
**Missing evidence:** none

---

### Claim A6: Scene-loaded cameras visible to PickingModule/VisualDebuggerModule via Camera2DModule
**Assessment:** supported
**Evidence for:**
- `VisualDebuggerModule.h:55` — `ModuleRef<Camera2DModule> mCameraRef{this}`
- `PickingModule.h:55` — `ModuleRef<Camera2DModule> mCameraRef{this}`
- `VisualDebuggerModule.cpp:50–51` — reads `GetActiveCamera()` and `GetWindowSize()` from `mCameraRef`
- `PickingModule.cpp:32` — reads `GetViewportTransform()` from `mCameraRef`
- Scene2DModule now loads into `Camera2DModule::GetRegistry()` — same registry these modules read
- Therefore scene-loaded cameras (including `test_camera`) are accessible to both modules
**Evidence against:** none
**Missing evidence:** none

---

### Claim A7: Scene2DModule has no #include for Domain.h, CameraRegistry2D.h, or LightRegistry2D.h
**Assessment:** supported
**Evidence for:**
- Grep of `DiaEntity/Domain|CameraRegistry2D|LightRegistry2D` against `Scene2DModule.h` → no matches
- Grep of same pattern against `Scene2DModule.cpp` → no matches
- The linter-added `#include <DiaApplicationFlow/ProcessingUnit.h>` in Scene2DModule.cpp is unrelated — does not reintroduce any of the removed includes
**Evidence against:** none
**Missing evidence:** none

---

### Claim A8: CameraModule name does not appear anywhere in code or manifests
**Assessment:** supported
**Evidence for:**
- Grep of `CameraModule` across all `*.{h,cpp,json,diaapp,diastage,diaschema,vcxproj,filters}` → returns only refactor history files (audit.json, plan.json, execute.json, review.json) — no live source files
- Grep across `*.md` → returns only refactor history docs and the updated specs (which now all say `Camera2DModule`)
**Evidence against:** none
**Missing evidence:** none
