**Spec:** @docs/specs/applications/dia/systems/diamesh3dvisualdebugger/mesh3d-bounds-and-origins.md
**Status:** Done

---

## Implementation Patterns

- **Draw classes** wrap `IVisualDebugger` exactly like `PhysicsShapesDrawer`: `#ifdef DIA_DEBUG` guards entire file, `Dia::Mesh3D::` namespace, const refs stored at construction, no STL in public API.
- **RequestDrawLine3D / RequestDrawRay3D** are called on the `FrameData&` cast to `DebugFrameData&` — `FrameData` inherits `DebugFrameData`.
- **MeshBoundsDrawer** expands AABB to 12 edges via `RequestDrawLine3D`; translation extracted with `cmd.transform.GetTranslation()` (SD-MVD-002: translation only).
- **MeshOriginDrawer** emits 3 rays via `RequestDrawRay3D`; arm length = `kCrossArmLen * mManager.GetDebugScale()`; colour cycles `{kActive,kGoal,kWarning,kCapped,kHealthy}[abs(cmd.layer) % 5]`.
- **vcxproj** mirrors `DiaRigidBody2DVisualDebugger.vcxproj` structure; GUID `{E3F4A5B6-C7D8-9012-EF01-234567890ABC}`; project references: DiaCore, DiaMaths, DiaGraphics, DiaGraphics3D, DiaMesh3D, DiaVisualDebugger; include dirs `./;./../;$(ProjectDir)../../External/imgui/`.
- **sln** registration: `Project` entry + 6 config entries in GlobalSection(ProjectConfigurationPlatforms) + `NestedProjects` entry under `{693FA062-085F-522C-A3C0-99B21FE0C8C0}` (3.1-Visual-Tools folder).
- **GoogleTests** must add `DiaMesh3DVisualDebugger.lib` to GoogleTests `AdditionalDependencies` and a `ProjectReference`; test files in `GoogleTests/DiaMesh3D/`.

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `kMesh3DBounds`, `kMesh3DOrigins`, `kMesh3DStats` to `DebugLayerNames.h` | Build passes | Done | haiku | Added kMesh3DBounds, kMesh3DOrigins, kMesh3DStats to DebugLayerNames.h |
| 2 | Create `DiaMesh3DVisualDebugger.vcxproj` + `.vcxproj.filters`; register in `Cluiche.sln` | Build passes | Done | sonnet | vcxproj + filters created; registered in Cluiche.sln under 3.1-Visual-Tools |
| 3 | Create `dia.mesh3dvisualdebugger.architecture.module.md` | `dia docs registry` passes | Done | haiku | dia.mesh3dvisualdebugger.architecture.module.md created |
| 4 | Implement `MeshBoundsDrawer.h/.cpp`; add to vcxproj | Build passes | Done | sonnet | MeshBoundsDrawer.h/.cpp created; 12-edge AABB wireframe, colour by asset state; DebugColourPalette used |
| 5 | Implement `MeshOriginDrawer.h/.cpp`; add to vcxproj | Build passes | Done | sonnet | MeshOriginDrawer.h/.cpp created; 3 rays + ImGui mHighlightSkinned checkbox; palette cycle by layer%5 |
| 6 | Write `TestMeshBoundsDrawer.cpp` and `TestMeshOriginDrawer.cpp`; add to `GoogleTests.vcxproj` | `dia run googletest --filter="MeshBoundsDrawer*:MeshOriginDrawer*"` all pass | Done | sonnet | 16/16 tests pass: 9 MeshBoundsDrawer + 7 MeshOriginDrawer; fix: FrameData.h required for static_cast to compile; tests use FrameData not DebugFrameData |
