# Feature Spec: gpu-resources

**Parent:** [diabgfx3d.md](diabgfx3d.md)
**Status:** Done

## Summary

The GPU-side resource layer for `DiaBgfx3D`: `MaterialRegistry` maps `StringCRC` material IDs to shader programs and base colours; `MeshGpuCache` lazily uploads `Mesh3DAsset` vertex/index data to bgfx GPU buffers on first use and tears them down on shutdown. Both types live in `Dia/DiaBgfx3D/Resources/` with no bgfx handle types in their public surface.

## Goals

- `Dia::Bgfx3D::MaterialRegistry` — `Register`, `Resolve`, `GetDefault`; `DynamicArrayC` backing, max 256 materials
- `Dia::Bgfx3D::MeshGpuCache` — `GetOrUpload` uploads when asset is Ready, returns `nullptr` if not; `DestroyAll` frees all GPU handles on shutdown
- `GpuMesh` stores `unsigned short` handle idx fields, not `bgfx::VertexBufferHandle` structs — bgfx types stay in `.cpp` files only
- Unit tests cover lookup miss on `MaterialRegistry` and not-Ready behaviour on `MeshGpuCache`

## Binding Decisions

- **PD-001** — `StringCRC` for all IDs; `meshId` and `materialId` are `StringCRC`
- **PD-004** — No STL in public APIs; `MaterialRegistry` uses `DynamicArrayC`
- **BG3-005** — No bgfx types in public surface; `GpuMesh` uses `unsigned short` for handle indices
- **BG3-008** — `MaterialRegistry` is a flat `DynamicArrayC<MaterialDescriptor, 256>`; growth path is a `params` array per descriptor
- **BG3-009** — `MeshGpuCache` may use STL `unordered_map` internally; public API must be STL-free
- **BG3-011** — Namespace is `Dia::Bgfx3D::`

## Public Interfaces

```cpp
// Dia/DiaBgfx3D/Resources/MaterialRegistry.h
namespace Dia { namespace Bgfx3D {

class ShaderProgram;

struct MaterialDescriptor
{
    Dia::Core::StringCRC id;
    ShaderProgram*       program;           // not owned; lives in Canvas3D
    uint32_t             baseColourRGBA;    // 0xFFFFFFFF default
};

class MaterialRegistry
{
public:
    static constexpr unsigned int kMaxMaterials = 256;

    MaterialRegistry();

    void                        Register(const MaterialDescriptor& desc);
    const MaterialDescriptor*   Resolve(Dia::Core::StringCRC id) const;  // nullptr if not found
    const MaterialDescriptor&   GetDefault() const;

private:
    Dia::Core::Containers::DynamicArrayC<MaterialDescriptor, kMaxMaterials> mMaterials;
    MaterialDescriptor                                                       mDefault;
};

} }
```

```cpp
// Dia/DiaBgfx3D/Resources/MeshGpuCache.h
namespace Dia { namespace Bgfx3D {

struct GpuMesh
{
    unsigned short vertexBuffer;  // bgfx::VertexBufferHandle::idx
    unsigned short indexBuffer;   // bgfx::IndexBufferHandle::idx
    uint32_t       indexCount;
};

class MeshGpuCache
{
public:
    MeshGpuCache();
    ~MeshGpuCache();

    const GpuMesh* GetOrUpload(const Dia::Mesh3D::Mesh3DAsset& asset);  // nullptr if not Ready
    void           DestroyAll();

private:
    // StringCRC → GpuMesh; STL map permitted internally per BG3-009
};

} }
```

## Acceptance Criteria

1. `MaterialRegistry::Resolve` returns a valid pointer for a registered ID and `nullptr` for an unknown ID
2. `MaterialRegistry::GetDefault` always returns a valid descriptor (non-null program after `Canvas3D` init)
3. `MaterialRegistry` backing is `DynamicArrayC<MaterialDescriptor, 256>`; `MaterialRegistry.h` includes no STL headers
4. `MeshGpuCache::GetOrUpload` returns a non-null `GpuMesh*` with non-zero buffer idx fields when called with a Ready `Mesh3DAsset`
5. `GetOrUpload` returns `nullptr` for a not-Ready asset; the renderer skips the draw
6. `GpuMesh` holds only `unsigned short` and `uint32_t` fields — no bgfx types in `MeshGpuCache.h`
7. `DestroyAll` releases all GPU handles without crash; safe to call on an empty cache
8. `dia run googletest --filter="DiaBgfx3D_MaterialRegistry*"` passes; `--filter="DiaBgfx3D_MeshGpuCache*"` passes (Noop bgfx renderer)

## Tasks

| # | Task | Status |
|---|------|--------|
| 1 | `Resources/MaterialRegistry.h/.cpp` — Register/Resolve/GetDefault, DynamicArrayC backing, default descriptor | Todo |
| 2 | `Resources/MeshGpuCache.h/.cpp` — GetOrUpload (lazy upload when Ready), DestroyAll | Todo |
| 3 | GoogleTests: `DiaBgfx3D_MaterialRegistryTest`, `DiaBgfx3D_MeshGpuCacheTest` | Todo |
