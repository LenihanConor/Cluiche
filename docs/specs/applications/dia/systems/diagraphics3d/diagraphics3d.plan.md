**Spec:** @docs/specs/applications/dia/systems/diagraphics3d/diagraphics3d.md
**Status:** Done

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `Dia/DiaGraphics3D/` directory with `Camera3D.h/.cpp`, `Light.h/.cpp`, `Mesh3DDrawCommand.h/.cpp` | Inspect headers compile; used by tasks 2+ | Done | sonnet | Namespace `Dia::Graphics3D::`. Uses `Dia::Maths::Matrix44`, `Angle`, `Vector3D`, `Dia::Graphics::RGBA`, `Dia::Core::StringCRC`. No Light.cpp needed (header-only structs). |
| 2 | Create `Dia/DiaGraphics3D/Mesh3DFrameData.h/.cpp` | Unit test coverage in task 6 | Done | sonnet | kMaxMeshDraws=4096, kMaxLights=32; drop-on-overflow; `DynamicArrayC` storage; `Clear`/`Copy`/accessors/drop-counters. Used IsFull/Add/RemoveAll/Assign (confirmed DynamicArrayC API). |
| 3 | Create `Dia/DiaGraphics3D/FrameData3D.h/.cpp` | Unit test in task 6 | Done | sonnet | `FrameData3D : Dia::Graphics::FrameData, Mesh3DFrameData`; `Clear()`/`Copy()`/`operator=` delegate to both bases |
| 4 | Create `Dia/DiaGraphics3D/Testing/MockMesh3DFrameData.h` | Used in unit tests task 6 | Done | haiku | Thin subclass exposing internals for tests; mirrors `MockVisitors.h` pattern |
| 5 | Create `DiaGraphics3D.vcxproj` + `DiaGraphics3D.vcxproj.filters` and register in `Cluiche.sln` | Build target appears; `dia run googletest` links | Done | sonnet | Also required: ProjectReference in GoogleTests.vcxproj (CLI builds vcxproj directly, not via SLN) |
| 6 | Create `Cluiche/Tests/GoogleTests/Graphics3D/TestMesh3DFrameData.cpp` and wire into GoogleTests.vcxproj | `dia run googletest --filter="DiaGraphics3D*"` passes | Done | sonnet | 19/19 tests pass. Angle::FromDegrees() (not float ctor). |
| 7 | Create `Dia/DiaGraphics3D/dia.graphics3d.architecture.module.md` | `dia validate manifest` passes | Done | haiku | Schema `dia.module.v1`; layer `domain/visual/core`; deps: `dia.graphics`, `dia.maths.matrix`, `dia.maths.vector`, `dia.core`; all 5 public types listed |
| 8 | Update plan+spec status to Done; commit | All tests still pass | Done | haiku | 6021 tests pass; 3 pre-existing failures unrelated to this feature |
