# System Spec: DiaMesh3D

## Parent Application
@docs/specs/applications/dia/dia.md

**Status:** `Done`

---

## Purpose

DiaMesh3D is the engine library for 3D mesh assets. It owns the canonical `Vertex3D` vertex layout, `Submesh` (index range + material binding), `Mesh3DAsset` (loadable mesh data: vertices, indices, submeshes, AABB bounds), and `Mesh3DAssetHandler` (runtime `IAssetTypeHandler` that reads cooked `.mesh3d` binaries through DiaAssetRuntime).

DiaMesh3D is **pure geometry** — no skeletons, no skinning attributes, no rendering backend types. Skin binding data (joint indices, blend weights) lives in DiaRig3D, following the same separation as DiaRig2D / DiaAnimation2D in 2D. The glTF import step belongs to DiaAssetPipeline (build-time); the runtime module loads a lightweight cooked binary and has no glTF dependency.

**Dependency chain:**
`DiaMesh3D → DiaAssetRuntime → DiaGeometry3D → DiaMaths → DiaCore`

---

## Responsibilities

- Own `Dia::Mesh3D::Vertex3D` — canonical static mesh vertex layout (position, normal, tangent, uv0, colour)
- Own `Dia::Mesh3D::Submesh` — index range + material id (`StringCRC`) within a mesh
- Own `Dia::Mesh3D::Mesh3DAsset` — async-loadable mesh asset (vertex/index/submesh storage, AABB bounds, atomic Ready/Failed state)
- Own `Dia::Mesh3D::Mesh3DAssetHandler` implementing `Dia::AssetRuntime::IAssetTypeHandler` — loads cooked `.mesh3d` binaries; mirrors `TextureHandler` two-phase pattern
- Provide `MeshBuilder3D` test helper in `Dia/DiaMesh3D/Testing/`
- Provide `DiaMesh3D.vcxproj` static library project registered in `Cluiche.sln`
- Provide `dia.mesh3d.architecture.module.md` YAML module documentation

## Non-Responsibilities

- glTF parsing — DiaAssetPipeline (build-time pipeline handler converts `.gltf`/`.glb` → `.mesh3d`)
- Skinning vertex attributes (joint indices, blend weights) — DiaRig3D owns skin binding data
- GPU buffer upload — DiaBgfx3D (`MeshRenderer` observes asset `Ready` state and calls `bgfx::createVertexBuffer`)
- Material resolution — DiaBgfx3D `MaterialRegistry`
- Skeleton / rig hierarchy — DiaRig3D
- Animation clip data — DiaAnimation3D
- Rendering — DiaBgfx3D
- Mesh LOD, procedural mesh generation, mesh instancing data

---

## Public Interfaces

### `Dia::Mesh3D::Vertex3D`

```cpp
// Dia/DiaMesh3D/Vertex3D.h
namespace Dia { namespace Mesh3D {

// Canonical static mesh vertex. 52 bytes.
// Skinning attributes (joint indices/weights) live in DiaRig3D's skin binding data.
struct Vertex3D
{
    Dia::Maths::Vector3D position;  // 12 bytes
    Dia::Maths::Vector3D normal;    // 12 bytes
    Dia::Maths::Vector4D tangent;   // 16 bytes — xyz=tangent, w=bitangent sign
    Dia::Maths::Vector2D uv0;       //  8 bytes — primary UV channel
    uint32_t             colour;    //  4 bytes — packed RGBA8 (0xFFFFFFFF default)
};

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
    Dia::Core::StringCRC materialId;  // hash of source material name; resolved by DiaBgfx3D MaterialRegistry
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

    // Called by handler on the asset-runtime owner thread after the worker decodes.
    void Populate(const Vertex3D* vertices, uint32_t vertexCount,
                  const uint16_t* indices,  uint32_t indexCount,
                  const Submesh*  submeshes, uint32_t submeshCount,
                  const Dia::Geometry3D::AABB& bounds);
    void MarkFailed(const char* reason);
};

} }
```

`kMaxVertices = 65535` keeps indices to `uint16_t` — consistent with bgfx's default index width. Larger meshes are authored as multi-prim glTF files (each submesh ≤ 65535 verts) at the asset pipeline level.

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

    // Drains worker results on the asset-runtime owner thread; fires OnLoadComplete.
    void Tick();
};

} }
```

---

## Dependencies

| Dependency | What's used |
|---|---|
| DiaAssetRuntime | `IAssetTypeHandler`, `IAssetLoadCallback` |
| DiaGeometry3D | `Dia::Geometry3D::AABB` (per-mesh bounds) |
| DiaMaths | `Vector3D`, `Vector4D`, `Vector2D` (Vertex3D fields) |
| DiaCore | `StringCRC`, `DynamicArrayC`, `String512` |
| DiaObservation | Logs (load errors, capacity overflow), metrics (loaded count, failure count) |

### Does NOT depend on

- DiaGraphics / DiaGraphics3D (no FrameData — renderer reads the asset directly)
- DiaBgfx / DiaBgfx3D
- DiaRig3D / DiaSkinning3D
- DiaAnimation3D
- DiaScene3D
- DiaAssetPipeline (runtime has no glTF dependency — build-time only)

---

## Relationship with DiaRig3D

DiaRig3D owns a `SkinBinding` asset — a parallel per-vertex array of joint indices and blend weights, loaded from the same `.glb` source but emitted as a separate `.rig3d` cooked binary. A skinned character has two asset IDs: `"mesh3d.dragon"` (geometry) and `"rig3d.dragon"` (skeleton + skin binding). DiaMesh3D has no dependency on DiaRig3D.

This mirrors the 2D pattern: `DiaRig2D` is completely independent of sprite/texture data; the rig is separate authored data loaded by a separate serializer.

---

## Relationship with DiaAssetPipeline

DiaAssetPipeline owns a `Mesh3DPipelineHandler` (Python) that:
1. Reads a `.gltf` / `.glb` source file
2. Parses geometry using `cgltf` (Python binding or subprocess)
3. Emits a cooked `.mesh3d` binary alongside a `.rig3d` binary (if skinned)
4. Records both in the asset manifest

The `cgltf` library lives only in DiaAssetPipeline — DiaMesh3D has no glTF dependency at runtime.

---

## Features

| Feature | Description | Spec |
|---|---|---|
| `mesh-asset-and-loader` | `Mesh3DAsset`, `Vertex3D`, `Submesh`, `Mesh3DAssetHandler`, cooked `.mesh3d` binary loader | [mesh-asset-and-loader.md](mesh-asset-and-loader.md) ✅ Done |

---

## Inherited Binding Decisions

| ID | Decision | How it applies |
|---|---|---|
| PD-001 | StringCRC for all IDs | Asset IDs (`"mesh3d.dragon"`), material IDs per submesh |
| PD-004 | No STL containers in public APIs | `DynamicArrayC` for vertex/index/submesh storage; no `std::vector` in any public header |
| PD-005 | x64 only | Single build target |
| PD-007 | C++20 required | Module compiles under `/std:c++20` |
| PD-008 | `Directory.Build.props` owns build paths | Library output to `bin/sharedlibs/<Config>/<Platform>/` |
| AD-001 | Module system with YAML frontmatter | `dia.mesh3d.architecture.module.md` required |
| AD-002 | No STL in public APIs | Same as PD-004 |
| AD-003 | Namespace convention `Dia::<Module>::` | `Dia::Mesh3D::` namespace |

---

## Resolved Design Questions

1. **Cooked binary format** — ✅ Resolved. Custom flat binary: fixed-size header (4-byte magic `MESH`, 1-byte version, vertex count, index count, submesh count, AABB min/max as 6 floats) followed by raw `Vertex3D[]`, `uint16_t[]`, and `Submesh[]` arrays. Load is a `memcpy` into `Mesh3DAsset` buffers — zero parse cost. Format is owned by DiaMesh3D; versioning is explicit in the header byte. `meshoptimizer` encoding is a Phase 3 option if file size or GPU cache performance becomes a concern.

2. **Skinning attributes** — ✅ Resolved. `Vertex3D` is pure geometry (52 bytes, no joint data). Skin binding data (per-vertex `uint32_t jointIndices` + `Vector4D jointWeights`) lives in DiaRig3D's `Rig3DAsset` as a parallel array index-matched to the mesh's vertex array. Skeleton hierarchy and skin binding are always authored together in the same glTF `skin` node and ship as one `Rig3DAsset` with a single asset ID (e.g. `"rig3d.dragon"`). The `mesh-asset-and-loader` feature spec must be revised to remove skinning attributes from `Vertex3D` before implementation starts.

3. **`.mesh3d` asset type prefix** — ✅ Resolved. `"mesh3d."` is the registered type prefix for `Mesh3DAssetHandler`. Asset IDs follow the convention `"mesh3d.<name>"` (e.g. `"mesh3d.character_dragon"`, `"mesh3d.terrain_cliff"`). Consistent with the module name and `Dia::Mesh3D::` namespace; avoids ambiguity with any future `DiaMesh2D`.

4. **Mesh-to-material binding** — ✅ Resolved. `Submesh.materialId` (`StringCRC`) is the binding point — set at cook time, resolved at render time. The mesh never holds shader handles or texture handles; it only claims a material by name. `DiaBgfx3D`'s `MaterialRegistry` resolves the name to GPU resources. This keeps DiaMesh3D free of any renderer dependency.

5. **Material descriptor format** — ✅ Resolved. Materials start as JSON descriptors owned by DiaBgfx3D — no cook step, no pipeline handler, human-authored. A descriptor lists shader ID and texture asset IDs per sampler slot. DiaBgfx3D parses the JSON at load, kicks off async texture loads, and marks the material Ready when all dependencies land. A cooked binary format is a future optimisation if JSON parse becomes a bottleneck.

6. **No DiaMaterials module** — ✅ Resolved. Material data (descriptor, resolved GPU handles, `MaterialRegistry`) lives entirely in DiaBgfx3D. No other system needs to read material data at runtime — only the renderer resolves `materialId` to GPU resources. The trigger to promote materials to their own module would be a second renderer backend or a non-rendering system needing to inspect material properties; neither applies now.

7. **Thin surface properties** — ✅ Resolved. Non-rendering systems (audio, scene culling, decals) that need surface characteristics (transparency flag, surface type tag, decal acceptance) receive a thin property extracted from the material — not a reference to the material itself. These properties live on the entity component (e.g. `MeshRenderer`) or a dedicated `SurfaceProperties` component. They are promoted to a shared module only if two or more non-rendering systems share the same data. `DiaBgfx3D` populates them when it loads the JSON descriptor.

8. **Load ordering** — ✅ Resolved. `MeshRenderer::Tick()` in DiaBgfx3D polls asset state each frame — mesh Ready, all submesh materials Ready, all material textures Ready — before submitting draw calls. No new coordination system. A `MeshRenderer` component has three states: `Pending` (waiting on any dep), `Ready` (all deps loaded), `Failed` (any dep failed).

---

## Status

`Approved`
