////////////////////////////////////////////////////////////////////////////////
// Filename: MeshGpuCache.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaBgfx3D/Resources/MeshGpuCache.h"

// bgfx is included only in the .cpp — never in the public header (BG3-005).
#include <bgfx/bgfx.h>

#include <DiaMesh3D/Mesh3DAsset.h>
#include <DiaMesh3D/Vertex3D.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>

#include <unordered_map>

namespace Dia
{
    namespace Bgfx3D
    {
        // -----------------------------------------------------------------------
        // Pimpl
        // -----------------------------------------------------------------------
        struct MeshGpuCache::Impl
        {
            // Keyed by StringCRC value (uint32_t) — BG3-009 permits STL internally.
            std::unordered_map<uint32_t, GpuMesh> cache;
        };

        // -----------------------------------------------------------------------
        // Construction / destruction
        // -----------------------------------------------------------------------
        MeshGpuCache::MeshGpuCache()
            : mImpl(new Impl())
        {
        }

        MeshGpuCache::~MeshGpuCache()
        {
            // DestroyAll must be called explicitly before shutdown; this is a
            // safety net for any remaining entries (handles may already be invalid
            // if bgfx has shut down, so we only free the Impl allocation here).
            delete mImpl;
            mImpl = nullptr;
        }

        // -----------------------------------------------------------------------
        // GetOrUpload
        // -----------------------------------------------------------------------
        const GpuMesh* MeshGpuCache::GetOrUpload(const Dia::Mesh3D::Mesh3DAsset& asset)
        {
            // AC-5: return nullptr for non-Ready assets; renderer skips the draw.
            if (!asset.IsReady())
            {
                return nullptr;
            }

            const uint32_t key = asset.GetAssetId().Value();

            // Cache hit — return existing entry.
            auto it = mImpl->cache.find(key);
            if (it != mImpl->cache.end())
            {
                return &it->second;
            }

            // -----------------------------------------------------------------------
            // Build vertex layout matching Vertex3D (52 bytes).
            // Order must match the struct layout in Vertex3D.h exactly:
            //   position  Vector3D  3×float  12 bytes
            //   normal    Vector3D  3×float  12 bytes
            //   tangent   Vector4D  4×float  16 bytes
            //   uv0       Vector2D  2×float   8 bytes
            //   colour    uint32_t  RGBA8     4 bytes
            // -----------------------------------------------------------------------
            DIA_PROFILE_SCOPE("mesh_gpu_cache.upload", ::Dia::Observation::Profile::Category::kDiaGraphics);
            bgfx::VertexLayout layout;
            layout.begin()
                .add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
                .add(bgfx::Attrib::Normal,    3, bgfx::AttribType::Float)
                .add(bgfx::Attrib::Tangent,   4, bgfx::AttribType::Float)
                .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
                .add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true /*normalized*/)
            .end();

            // -----------------------------------------------------------------------
            // Upload vertex buffer.
            // bgfx::copy is used (not makeRef) so bgfx owns the memory immediately.
            // This avoids a UAF if AssetRuntime evicts the asset before the bgfx
            // frame boundary at which makeRef data would have been consumed.
            // -----------------------------------------------------------------------
            const auto& vertices  = asset.GetVertices();
            const uint32_t vCount = vertices.Size();
            const uint32_t vBytes = vCount * static_cast<uint32_t>(sizeof(Dia::Mesh3D::Vertex3D));

            bgfx::VertexBufferHandle vbh = bgfx::createVertexBuffer(
                bgfx::copy(&vertices[0], vBytes),
                layout
            );

            if (!bgfx::isValid(vbh))
            {
                DIA_LOG_ERROR("DiaBgfx3D", "MeshGpuCache: createVertexBuffer failed for asset 0x%08X", key);
                return nullptr;
            }

            const auto& indices   = asset.GetIndices();
            const uint32_t iCount = indices.Size();
            const uint32_t iBytes = iCount * static_cast<uint32_t>(sizeof(uint16_t));

            bgfx::IndexBufferHandle ibh = bgfx::createIndexBuffer(
                bgfx::copy(&indices[0], iBytes)
            );

            if (!bgfx::isValid(ibh))
            {
                DIA_LOG_ERROR("DiaBgfx3D", "MeshGpuCache: createIndexBuffer failed for asset 0x%08X", key);
                bgfx::destroy(vbh);
                return nullptr;
            }

            GpuMesh entry{ vbh.idx, ibh.idx, iCount };
            auto result = mImpl->cache.emplace(key, entry);
            return &result.first->second;
        }

        // -----------------------------------------------------------------------
        // GetResidentCount
        // -----------------------------------------------------------------------
        unsigned int MeshGpuCache::GetResidentCount() const
        {
            return static_cast<unsigned int>(mImpl->cache.size());
        }

        // -----------------------------------------------------------------------
        // Evict
        // -----------------------------------------------------------------------
        void MeshGpuCache::Evict(uint32_t assetId)
        {
            auto it = mImpl->cache.find(assetId);
            if (it == mImpl->cache.end())
                return;

            const GpuMesh& m = it->second;
            bgfx::VertexBufferHandle vbh{ m.vertexBuffer };
            bgfx::destroy(vbh);
            bgfx::IndexBufferHandle ibh{ m.indexBuffer };
            bgfx::destroy(ibh);

            mImpl->cache.erase(it);
        }

        // -----------------------------------------------------------------------
        // DestroyAll
        // -----------------------------------------------------------------------
        void MeshGpuCache::DestroyAll()
        {
            for (auto& pair : mImpl->cache)
            {
                const GpuMesh& m = pair.second;

                bgfx::VertexBufferHandle vbh{ m.vertexBuffer };
                bgfx::destroy(vbh);

                bgfx::IndexBufferHandle ibh{ m.indexBuffer };
                bgfx::destroy(ibh);
            }

            mImpl->cache.clear();
        }

    } // namespace Bgfx3D
} // namespace Dia
