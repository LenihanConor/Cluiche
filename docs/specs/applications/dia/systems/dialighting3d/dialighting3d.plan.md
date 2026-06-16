**Spec:** @docs/specs/applications/dia/systems/dialighting3d/dialighting3d.md
**Status:** Done

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Move `RGBA` to DiaCore (`Dia/DiaCore/Colour/RGBA.h+cpp`); add `using Dia::Graphics::RGBA = Dia::Core::RGBA;` alias in DiaGraphics so all existing callers compile unchanged; update DiaGraphics vcxproj to include the alias header; update spec design question 7 | `dia run googletest` — all existing tests still pass | Done | sonnet | 6107 tests pass |
| 2 | Create `Dia/DiaLighting3D/` library: vcxproj (static lib, refs DiaCore+DiaMaths+DiaObservation), vcxproj.filters, `dia.lighting3d.architecture.module.md`; register in `Cluiche.sln` | Project builds (no source files yet) | Done | haiku | Builds clean; registered in sln |
| 3 | Implement four light value types: `PointLight3D.h`, `DirectionalLight3D.h`, `SpotLight3D.h`, `AmbientLight3D.h` — all in `Dia/DiaLighting3D/`; add to vcxproj | Struct defaults and copy semantics | Done | sonnet | All tests pass |
| 4 | Implement `LightRegistry3D` (`Registry/LightRegistry3D.h+cpp`) with per-type fixed-slot storage, CRUD, indexed iteration, `UpdateAll(float dt)`, behaviour attach/detach | `TestLightRegistry3D.cpp` — CRUD, indexed iteration, UpdateAll dispatch | Done | sonnet | Behaviour methods stubbed pending Task 5 |
| 5 | Implement `ILightBehaviour3D` interface and `LightBehaviourRegistry3D` (self-registering factory, kMaxBehaviourTypes=16) | `TestLightBehaviourRegistry3D.cpp` — register, create, IsRegistered | Done | sonnet | 6107 tests pass; registry stubs fully implemented |
| 6 | Implement 3 engine behaviours: `FlickerBehaviour3D`, `PulseBehaviour3D`, `ColorCycleBehaviour3D` — each in `Behaviours/`; self-register on startup | `TestBehaviours3D.cpp` — Update mutates intensity/colour as expected | Done | sonnet | 6107 tests pass |
| 7 | Implement `LightBuilder3D` test helper in `Dia/DiaLighting3D/Testing/LightBuilder3D.h` | Used inline in test assertions | Done | haiku | 6107 tests pass |
| 8 | Write GoogleTest suite `Cluiche/Tests/GoogleTests/DiaLighting3D/TestLightRegistry3D.cpp`, `TestBehaviours3D.cpp`; add to GoogleTests.vcxproj; add `DiaLighting3D.lib` to GoogleTests linker deps and ProjectReference | `dia run googletest --filter="DiaLighting3D*"` all pass | Done | sonnet | 35 new tests; 6142 total pass |
| 9 | Run `dia docs registry` to regenerate module-registry.md; run `dia docs precommit` to verify all invariants | No violations reported | Done | haiku | All checks pass |
