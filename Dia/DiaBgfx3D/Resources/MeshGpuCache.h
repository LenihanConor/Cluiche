////////////////////////////////////////////////////////////////////////////////
// Filename: MeshGpuCache.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <stdint.h>

namespace Dia { namespace Mesh3D { class Mesh3DAsset; } }

namespace Dia
{
    namespace Bgfx3D
    {
        // Holds bgfx buffer handle indices for a single mesh uploaded to the GPU.
        // unsigned short matches bgfx::VertexBufferHandle::idx / IndexBufferHandle::idx.
        // No bgfx types in this header (BG3-005, PD-004).
        struct GpuMesh
        {
            unsigned short vertexBuffer;   // bgfx::VertexBufferHandle::idx
            unsigned short indexBuffer;    // bgfx::IndexBufferHandle::idx
            uint32_t       indexCount;
        };

        // Lazy GPU-buffer cache keyed by Mesh3DAsset ID.
        // Thread-safety: single-threaded; must be driven from the render thread.
        // STL-free public API (PD-004, BG3-009). std::unordered_map used internally.
        class MeshGpuCache
        {
        public:
            MeshGpuCache();
            ~MeshGpuCache();

            // Returns a pointer to the cached GpuMesh if the asset is Ready,
            // uploading vertex/index data to GPU on first call.
            // Returns nullptr if the asset is not yet Ready (renderer should skip draw).
            const GpuMesh* GetOrUpload(const Dia::Mesh3D::Mesh3DAsset& asset);

            // Destroys all GPU handles and clears the cache.
            // Safe to call on an empty cache. Must be called before bgfx::shutdown().
            void DestroyAll();

            // Number of meshes currently resident on GPU.
            unsigned int GetResidentCount() const;

        private:
            // Pimpl avoids pulling std::unordered_map into the public header (PD-004).
            struct Impl;
            Impl* mImpl;
        };

    } // namespace Bgfx3D
} // namespace Dia
