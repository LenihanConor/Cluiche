**Spec:** @docs/specs/features/dia/diamesh3d/mesh-asset-and-loader.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `Dia/DiaMesh3D/` module scaffold — vcxproj, vcxproj.filters, module YAML, add to Cluiche.sln | `dia check deps` clean | Pending | haiku | |
| 2 | Define `Vertex3D.h`, `Submesh.h` structs with `static_assert(sizeof(Vertex3D) == 52)` | Compile clean | Pending | haiku | |
| 3 | Implement `Mesh3DAsset.h` + `Mesh3DAsset.cpp` — atomic State, `DynamicArrayC` storage, `Populate()`, `MarkFailed()`, AABB | Unit tests: construct, populate, state transitions | Pending | sonnet | |
| 4 | Implement `Mesh3DBinaryReader.h` + `.cpp` — reads `.mesh3d` header, validates magic/version/counts, memcpy into staging buffers | Unit tests: valid read, bad magic, bad version, count overflow | Pending | sonnet | |
| 5 | Create `MeshBuilder3D.h` test helper + bake `unit_cube.mesh3d` fixture | Used by handler integration test (task 6) | Pending | haiku | |
| 6 | Implement `Mesh3DAssetHandler.h` + `.cpp` — `IAssetTypeHandler`, worker-thread load, owner-thread `Tick()`, `LookupMesh()` | Integration test: load `unit_cube.mesh3d`, verify AABB | Pending | sonnet | |
| 7 | Write GoogleTests — `TestMesh3DAsset.cpp` + `TestMesh3DBinaryReader.cpp` — all ACs covered, add to GoogleTests.vcxproj | `dia run googletest` green | Pending | sonnet | |
| 8 | `dia docs registry` — regenerate module registry with DiaMesh3D entry | Registry valid | Pending | haiku | |
