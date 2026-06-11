# Plan: DiaMesh3D

**Spec:** @docs/specs/systems/dia/diamesh3d.md
**Status:** Done

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `Dia/DiaMesh3D/` module — vcxproj, vcxproj.filters, module YAML, add to Cluiche.sln | `dia check deps` + `dia validate manifest` | Not Started | haiku | Scaffold only; no implementation files yet |
| 2 | Define `Vertex3D`, `Submesh` structs — headers only, `static_assert(sizeof(Vertex3D) == 52)` | Compile clean | Not Started | haiku | Pure data; no logic |
| 3 | Define `Mesh3DAsset` — header + cpp; atomic State, vertex/index/submesh storage via `DynamicArrayC`, AABB bounds, `Populate()` / `MarkFailed()` | Unit tests: construct, populate, state transitions | Not Started | sonnet | No threading here; two-phase pattern is in the handler |
| 4 | Define `.mesh3d` binary format + `Mesh3DBinaryReader` — reads header, validates magic/version/counts, memcpy into staging buffers | Unit tests: valid load, bad magic, bad version, count overflow | Not Started | sonnet | Internal to the module; no public surface |
| 5 | Define `Mesh3DAssetHandler` — implements `IAssetTypeHandler`; worker-thread read, owner-thread Tick drain, `LookupMesh()` | Integration test: load `unit_cube.mesh3d`, verify AABB | Not Started | sonnet | Mirrors TextureHandler pattern |
| 6 | Create `MeshBuilder3D` test helper in `Dia/DiaMesh3D/Testing/` and bake `unit_cube.mesh3d` fixture | Used by handler integration test (task 5) | Not Started | haiku | Fixture must match expected AABB min/max |
| 7 | Write GoogleTests — `TestMesh3DAsset.cpp`, `TestMesh3DBinaryReader.cpp` — all ACs covered | `dia run googletest` green | Not Started | sonnet | ACs 1–11 in feature spec |
| 8 | DiaBgfx3D: define `MaterialDescriptor` JSON schema and `MaterialRegistry` — parse JSON, kick async texture loads, mark Ready when all deps land | Unit test: parse valid descriptor, missing texture → Failed | Not Started | sonnet | No bgfx types in descriptor struct; only in resolved material |
| 9 | DiaBgfx3D: implement `MeshRenderer` component — holds mesh asset ID, per-frame Tick polling (mesh Ready → materials Ready → textures Ready → submit draw calls) | Smoke test: entity with MeshRenderer renders a cube | Not Started | sonnet | Three states: Pending / Ready / Failed |
| 10 | DiaBgfx3D: populate thin surface properties (`isTransparent`, `surfaceTag`) on `MeshRenderer` from JSON descriptor at material load time | — | Not Started | haiku | No new module; fields on MeshRenderer component |
| 11 | `dia docs registry` — regenerate module registry with DiaMesh3D entry | Registry valid | Not Started | haiku | Run after task 1 |

---

## Dependency Order

```
1 (scaffold)
  └─ 2 (structs)
       └─ 3 (Mesh3DAsset)
            └─ 4 (binary reader)
                 └─ 6 (fixture + test helper)
                      └─ 5 (handler)
                           └─ 7 (GoogleTests)
                                └─ 8 (MaterialRegistry)
                                     └─ 9 (MeshRenderer)
                                          └─ 10 (surface properties)
11 can run after 1.
```

Tasks 1–7 are entirely within DiaMesh3D and can be completed before touching DiaBgfx3D. Tasks 8–10 require DiaMesh3D to be building clean.
