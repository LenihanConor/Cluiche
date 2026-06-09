# Feature Spec: mesh-asset-and-loader

## Parent System
@docs/specs/systems/dia/diamesh3d.md

**Status:** `Approved`

---

## Summary

Create the `Dia/DiaMesh3D/` module. Defines the canonical 3D mesh asset types (`Vertex3D`, `Submesh`, `Mesh3DAsset`) and a runtime handler (`Mesh3DAssetHandler`) that loads cooked `.mesh3d` binary files through the `DiaAssetRuntime` `IAssetTypeHandler` pattern.

`Vertex3D` is pure geometry — no skinning attributes. Skin binding data (joint indices + weights) lives in DiaRig3D's `Rig3DAsset`. The glTF import step belongs to DiaAssetPipeline (build-time); the runtime module has no glTF dependency.

This feature does **not** render anything. It produces `Mesh3DAsset` instances available to downstream systems via `DiaAssetRuntime`. GPU upload happens in `DiaBgfx3D`.

---

## Problem

Phase 2 needs static and skinned meshes. Today the engine has no mesh asset type; `DiaAssetRuntime` handles textures only. Adding a `Mesh3DAsset` type requires:

- A renderer-agnostic data structure holding vertex/index/submesh data without leaking bgfx types
- An `IAssetTypeHandler` that reads cooked `.mesh3d` binaries on a worker thread and delivers the populated asset on the owner thread (matching the `TextureHandler` two-phase pattern)
- A cooked binary format — custom flat binary (header + raw arrays) so load is a `memcpy` with zero parse cost
- A submesh model supporting per-submesh material ID binding (`StringCRC`)
- AABB bounds baked into the cooked file for scene culling

Skinned-mesh attributes (joint indices, blend weights) are baked into DiaRig3D's `Rig3DAsset` at the pipeline stage. `Vertex3D` is static-mesh-only.

---

## Goals

- Create `Dia/DiaMesh3D/` module with `dia.mesh3d.architecture.module.md` YAML
- Define `Dia::Mesh3D::Vertex3D` — 52-byte canonical vertex layout (position, normal, tangent, uv0, colour; no joint data)
- Define `Dia::Mesh3D::Submesh` — index range + material ID within a mesh
- Define `Dia::Mesh3D::Mesh3DAsset` — loadable mesh with atomic State, vertex/index/submesh storage, AABB bounds
- Define `Dia::Mesh3D::Mesh3DAssetHandler` implementing `IAssetTypeHandler` — reads `.mesh3d` cooked binary; registered under type prefix `"mesh3d."`
- Define the `.mesh3d` binary format: fixed-size header (`MESH` magic, version byte, vertex count, index count, submesh count, AABB as 6 floats) followed by raw `Vertex3D[]`, `uint16_t[]`, `Submesh[]` arrays
- Two-phase async loading: worker reads file + `memcpy` into host-side buffers; `Tick()` drains results on owner thread and fires `OnLoadComplete`
- All public APIs PD-004 compliant (`DynamicArrayC` throughout; no STL in headers)
- Test helper `MeshBuilder3D` in `Dia/DiaMesh3D/Testing/` for constructing synthetic assets in tests
- Tests under `Cluiche/Tests/GoogleTests/` covering binary round-trip load, malformed file rejection, capacity overflow

---

## Non-Goals

- **glTF parsing at runtime** — DiaAssetPipeline owns the `cgltf`-backed pipeline handler that emits `.mesh3d` binaries
- **Skinning vertex attributes** — `jointIndices`/`jointWeights` live in DiaRig3D's `Rig3DAsset` (parallel `SkinBinding` array index-matched to vertices)
- **Rendering** — `DiaBgfx3D` consumes `Mesh3DAsset`; GPU upload is `MeshRenderer`'s responsibility
- **Mesh editing / authoring** — assets are read-only after load
- **Mesh LOD** — single-resolution mesh per asset; LOD is a future optimisation
- **Procedural mesh generation** — meshes come from cooked files only
- **Mesh reuse / instancing data** — instancing is a render-side optimisation in `DiaBgfx3D`
- **Material data** — only `materialId` (`StringCRC`) is carried per submesh; resolving to shader/textures is `DiaBgfx3D`'s concern
- **meshoptimizer encoding** — deferred; can be layered onto the same format in Phase 3 if file size or GPU cache performance demands it

---

## Public Interfaces

### `Dia::Mesh3D::Vertex3D`

```cpp
// Dia/DiaMesh3D/Vertex3D.h
namespace Dia { namespace Mesh3D {

// 52 bytes. Pure geometry — no skinning attributes.
struct Vertex3D
{
    Dia::Maths::Vector3D position;  // 12 bytes
    Dia::Maths::Vector3D normal;    // 12 bytes
    Dia::Maths::Vector4D tangent;   // 16 bytes — xyz=tangent, w=bitangent sign
    Dia::Maths::Vector2D uv0;       //  8 bytes — primary UV channel
    uint32_t             colour;    //  4 bytes — packed RGBA8 (0xFFFFFFFF default)
};

static_assert(sizeof(Vertex3D) == 52, "Vertex3D size mismatch");

} }
```

### `Dia::Mesh3D::Submesh`

```cpp
// Dia/DiaMesh3D/Submesh.h
namespace Dia { namespace Mesh3D {

struct Submesh
{
    uint32_t             indexStart;  // first index in the mesh's index buffer
    uint32_t             indexCount;  // number of indices for this submesh
    Dia::Core::StringCRC materialId;  // hash of source material name; resolved by DiaBgfx3D
};

} }
```

### `Dia::Mesh3D::Mesh3DAsset`

```cpp
// Dia/DiaMesh3D/Mesh3DAsset.h
namespace Dia { namespace Mesh3D {

class Mesh3DAsset
{
public:
    enum class State : unsigned char { Pending = 0, Ready = 1, Failed = 2 };

    static constexpr uint32_t kMaxVertices  = 65535;   // uint16_t index width
    static constexpr uint32_t kMaxIndices   = 196608;  // 65535 * 3
    static constexpr uint32_t kMaxSubmeshes = 32;

    explicit Mesh3DAsset(Dia::Core::StringCRC assetId);

    Dia::Core::StringCRC GetAssetId() const;
    State                GetState()   const;
    bool                 IsReady()    const;

    const Dia::Core::Containers::DynamicArrayC<Vertex3D, kMaxVertices>&  GetVertices()  const;
    const Dia::Core::Containers::DynamicArrayC<uint16_t, kMaxIndices>&   GetIndices()   const;
    const Dia::Core::Containers::DynamicArrayC<Submesh,  kMaxSubmeshes>& GetSubmeshes() const;
    const Dia::Geometry3D::AABB&                                         GetBounds()    const;

    // Called by handler on owner thread after worker decode.
    void Populate(const Vertex3D* vertices, uint32_t vertexCount,
                  const uint16_t* indices,  uint32_t indexCount,
                  const Submesh*  submeshes, uint32_t submeshCount,
                  const Dia::Geometry3D::AABB& bounds);
    void MarkFailed(const char* reason);
};

} }
```

### `.mesh3d` Binary Format

```
Offset  Size   Field
------  ----   -----
0       4      Magic: 'M','E','S','H'
4       1      Version: 1
5       4      vertexCount  (uint32_t)
9       4      indexCount   (uint32_t)
13      4      submeshCount (uint32_t)
17      4      aabbMinX     (float)
21      4      aabbMinY     (float)
25      4      aabbMinZ     (float)
29      4      aabbMaxX     (float)
33      4      aabbMaxY     (float)
37      4      aabbMaxZ     (float)
41      vertexCount  * sizeof(Vertex3D)   — raw Vertex3D array
+       indexCount   * sizeof(uint16_t)   — raw index array
+       submeshCount * sizeof(Submesh)    — raw Submesh array
```

Total header: 41 bytes. All values little-endian. Padding: none.

### `Dia::Mesh3D::Mesh3DAssetHandler`

```cpp
// Dia/DiaMesh3D/Mesh3DAssetHandler.h
namespace Dia { namespace Mesh3D {

class Mesh3DAssetHandler : public Dia::AssetRuntime::IAssetTypeHandler
{
public:
    // Lookup loaded mesh by asset id. Thread-safe (shared lock).
    Mesh3DAsset* LookupMesh(Dia::Core::StringCRC assetId) const;
    unsigned int GetLoadedCount() const;

    // IAssetTypeHandler
    void Load  (const Dia::Core::StringCRC& assetId,
                const Dia::Core::Containers::String512& resolvedPath,
                Dia::AssetRuntime::IAssetLoadCallback* callback) override;
    void Unload(const Dia::Core::StringCRC& assetId) override;

    // Drains worker results on owner thread; fires OnLoadComplete / OnLoadFailed.
    void Tick();
};

} }
```

---

## Implementation

### Files introduced

```
Dia/DiaMesh3D/
├── DiaMesh3D.vcxproj
├── DiaMesh3D.vcxproj.filters
├── dia.mesh3d.architecture.module.md
├── Vertex3D.h
├── Submesh.h
├── Mesh3DAsset.h
├── Mesh3DAsset.cpp
├── Mesh3DAssetHandler.h
├── Mesh3DAssetHandler.cpp
├── Mesh3DBinaryReader.h          (internal — reads .mesh3d header + arrays)
├── Mesh3DBinaryReader.cpp
└── Testing/
    ├── MeshBuilder3D.h           (test helper — builds synthetic Mesh3DAsset)
    └── Fixtures/
        └── unit_cube.mesh3d      (cooked test fixture — baked unit cube)
```

### Files modified

```
Cluiche/Cluiche.sln               — add DiaMesh3D.vcxproj reference
Cluiche/Tests/GoogleTests/        — add TestMesh3DAsset.cpp, TestMesh3DBinaryReader.cpp
```

### Binary load flow (worker thread)

```
1. Open file via Dia::Core::FilePath::Resolve.
2. Read 41-byte header; validate magic ('MESH') and version (1).
3. Validate counts against kMaxVertices / kMaxIndices / kMaxSubmeshes.
4. Allocate staging buffers (stack or JobSystem scratch).
5. fread Vertex3D[] → staging vertex buffer.
6. fread uint16_t[] → staging index buffer.
7. fread Submesh[]  → staging submesh buffer.
8. Reconstruct Dia::Geometry3D::AABB from header floats.
9. Push PendingResult{assetId, buffers, bounds, callback} to result queue (mutex-guarded).
```

### Owner-thread Tick

Drains `PendingResult` queue. For each entry: calls `asset->Populate(...)`, fires `callback->OnLoadComplete(assetId)`. No GPU work — that is `DiaBgfx3D`'s responsibility.

---

## Files Introduced / Modified

| File | Change |
|------|--------|
| `Dia/DiaMesh3D/` (entire module) | NEW |
| `Dia/DiaMesh3D/DiaMesh3D.vcxproj{,.filters}` | NEW |
| `Dia/DiaMesh3D/dia.mesh3d.architecture.module.md` | NEW |
| `Cluiche/Cluiche.sln` | Add DiaMesh3D.vcxproj reference |
| `Cluiche/Tests/GoogleTests/` | Add TestMesh3DAsset.cpp, TestMesh3DBinaryReader.cpp |

---

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| DiaMaths (`Vector3D`, `Vector4D`, `Vector2D`) | Hard | Vertex3D fields |
| DiaGeometry3D (`AABB`) | Hard | Per-mesh bounds; `AABB` constructed from header floats |
| DiaAssetRuntime (`IAssetTypeHandler`, `IAssetLoadCallback`) | Hard | Handler integration |
| DiaCore (`StringCRC`, `DynamicArrayC`, `String512`) | Hard | IDs, storage, path strings |

---

## Acceptance Criteria

1. `Dia/DiaMesh3D/DiaMesh3D.vcxproj` builds clean under `/std:c++20`; links DiaCore, DiaMaths, DiaGeometry3D, DiaAssetRuntime
2. `static_assert(sizeof(Vertex3D) == 52)` passes
3. `Dia::Mesh3D::Mesh3DAsset` exists with atomic State, `IsReady()`, vertex/index/submesh accessors, `GetBounds()` returning `Dia::Geometry3D::AABB`
4. `Dia::Mesh3D::Mesh3DAssetHandler` implements `IAssetTypeHandler`; registered under type prefix `"mesh3d."`; `LookupMesh(StringCRC)` is the public accessor
5. Handler loads `unit_cube.mesh3d` fixture; verifies AABB min=(-0.5,-0.5,-0.5), max=(0.5,0.5,0.5)
6. Bad magic or unsupported version fires `OnLoadFailed` with a descriptive message
7. Vertex/index/submesh count exceeding `kMax*` constants fires `OnLoadFailed`
8. `dia run googletest` is green; `TestMesh3DBinaryReader` covers valid load, bad magic, bad version, count overflow
9. No bgfx types in any DiaMesh3D public header
10. PD-004 audit: no `std::vector`, `std::string`, `std::array` in any public header
11. `dia.mesh3d.architecture.module.md` validates via `dia check deps`

---

## Traceability

| Level | Spec |
|-------|------|
| Platform | @docs/specs/platform/Cluiche.md |
| Application | @docs/specs/applications/dia.md |
| System | @docs/specs/systems/dia/diamesh3d.md |

---

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | Compliant — asset IDs (`"mesh3d.*"`), `materialId` per submesh |
| PD-004 | No STL in public APIs | Compliant — `DynamicArrayC` throughout; no STL in headers |
| PD-005 | x64 only | Compliant |
| PD-006 | VS project files source of truth | Compliant — `.vcxproj{,.filters}` manually maintained |
| PD-007 | C++20 | Compliant |
| PD-008 | `Directory.Build.props` owns build paths | Compliant |
| AD-001 | Module YAML frontmatter | Compliant — `dia.mesh3d.architecture.module.md` created |
| AD-003 | Namespace `Dia::<Module>::` | Compliant — `Dia::Mesh3D::` |
