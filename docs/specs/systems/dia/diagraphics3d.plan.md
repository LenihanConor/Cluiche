**Spec:** @docs/specs/systems/dia/diagraphics3d.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `Dia/DiaGraphics3D/` directory with `Camera3D.h/.cpp`, `Light.h/.cpp`, `Mesh3DDrawCommand.h/.cpp` | Inspect headers compile; used by tasks 2+ | Pending | sonnet | Namespace `Dia::Graphics3D::`. Uses `Dia::Maths::Matrix44`, `Angle`, `Vector3D`, `Dia::Graphics::RGBA`, `Dia::Core::StringCRC`. No Light.cpp needed (header-only structs). |
| 2 | Create `Dia/DiaGraphics3D/Mesh3DFrameData.h/.cpp` | Unit test coverage in task 6 | Pending | sonnet | kMaxMeshDraws=4096, kMaxLights=32; drop-on-overflow; `DynamicArrayC` storage; `Clear`/`Copy`/accessors/drop-counters |
| 3 | Create `Dia/DiaGraphics3D/FrameData3D.h/.cpp` | Unit test in task 6 | Pending | sonnet | `FrameData3D : Dia::Graphics::FrameData, Mesh3DFrameData`; `Clear()`/`Copy()`/`operator=` delegate to both bases |
| 4 | Create `Dia/DiaGraphics3D/Testing/MockMesh3DFrameData.h` | Used in unit tests task 6 | Pending | haiku | Thin subclass exposing internals for tests; mirrors `MockVisitors.h` pattern |
| 5 | Create `DiaGraphics3D.vcxproj` + `DiaGraphics3D.vcxproj.filters` and register in `Cluiche.sln` | Build target appears; `dia run googletest` links | Pending | sonnet | StaticLib, all 4 configs (Debug/Release/Debug-Asan/Debug-Ubsan); AdditionalIncludeDirectories `./;./../`; deps: DiaMaths, DiaGraphics, DiaCore; new GUID `{C5D6E7F8-A9B0-1234-EF01-234567890123}` |
| 6 | Create `Cluiche/Tests/GoogleTests/Graphics3D/TestMesh3DFrameData.cpp` and wire into GoogleTests.vcxproj | `dia run googletest --filter="DiaGraphics3D*"` passes | Pending | sonnet | Covers: request/clear, capacity overflow drops, camera storage, light storage, FrameData3D copy, Camera3D helpers round-trip |
| 7 | Create `Dia/DiaGraphics3D/dia.graphics3d.architecture.module.md` | `dia validate manifest` passes | Pending | haiku | Schema `dia.module.v1`; layer `domain/visual/core`; deps: `dia.graphics`, `dia.maths`, `dia.core`; all 5 public types listed |
| 8 | Update plan+spec status to Done; commit | All tests still pass | Pending | haiku | Mark `graphics-3d-types` feature `Done`; update diagraphics3d.md system status |
