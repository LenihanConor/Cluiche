////////////////////////////////////////////////////////////////////////////////
// Filename: CellFieldGeometryCache.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaBgfx/Resources/CellFieldGeometryCache.h"

#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>

#include <bgfx/bgfx.h>

namespace Dia
{
    namespace Bgfx
    {
        // ---- Real bgfx factory -------------------------------------------------

        bool BgfxCellFieldGpuGeometryFactory::Create(const Dia::Graphics::CellGridGeometry& geometry, CellFieldGpuHandle& outHandle)
        {
            if (geometry.vertexCount == 0 || geometry.indexCount == 0)
                return false;

            bgfx::VertexLayout layout;
            layout.begin()
                .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)          // centre
                .add(bgfx::Attrib::TexCoord1, 2, bgfx::AttribType::Float)          // offset
                .add(bgfx::Attrib::TexCoord2, 4, bgfx::AttribType::Float)          // cell indices
                .add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)    // weights (unorm8)
                .end();

            const uint32_t vbBytes = geometry.vertexCount * static_cast<uint32_t>(sizeof(Dia::Graphics::VertexCellField));
            const uint32_t ibBytes = geometry.indexCount * static_cast<uint32_t>(sizeof(uint32_t));

            const bgfx::VertexBufferHandle vb = bgfx::createVertexBuffer(bgfx::copy(geometry.vertices, vbBytes), layout);
            const bgfx::IndexBufferHandle  ib = bgfx::createIndexBuffer(bgfx::copy(geometry.indices, ibBytes), BGFX_BUFFER_INDEX32);
            if (!bgfx::isValid(vb) || !bgfx::isValid(ib))
            {
                if (bgfx::isValid(vb)) bgfx::destroy(vb);
                if (bgfx::isValid(ib)) bgfx::destroy(ib);
                return false;
            }
            outHandle = static_cast<uint32_t>(vb.idx) | (static_cast<uint32_t>(ib.idx) << 16);
            return true;
        }

        void BgfxCellFieldGpuGeometryFactory::Destroy(CellFieldGpuHandle handle)
        {
            bgfx::destroy(bgfx::VertexBufferHandle{ VertexBuffer(handle) });
            bgfx::destroy(bgfx::IndexBufferHandle{ IndexBuffer(handle) });
        }

        // ---- Cache -------------------------------------------------------------

        CellFieldGeometryCache::CellFieldGeometryCache(ICellFieldGpuGeometryFactory& factory)
            : mFactory(factory)
        {}

        CellFieldGeometryCache::~CellFieldGeometryCache()
        {
            Shutdown();
        }

        void CellFieldGeometryCache::AddBuilder(const Dia::Graphics::ICellGridGeometryBuilder* builder)
        {
            if (builder)
                mBuilders.push_back(builder);
        }

        CellFieldGeometryKey CellFieldGeometryCache::MakeKey(const Dia::Graphics::CellGridDesc& desc)
        {
            CellFieldGeometryKey key;
            key.topology = desc.topology;
            key.width = desc.width;
            key.height = desc.height;
            return key;
        }

        const CellFieldGeometryCache::Entry* CellFieldGeometryCache::Acquire(const Dia::Graphics::CellGridDesc& desc)
        {
            const CellFieldGeometryKey key = MakeKey(desc);

            auto it = mEntries.find(key);
            if (it != mEntries.end())
            {
                ++it->second.refCount;
                return &it->second;
            }

            const Dia::Graphics::ICellGridGeometryBuilder* builder = nullptr;
            for (const Dia::Graphics::ICellGridGeometryBuilder* b : mBuilders)
            {
                if (b->Topology() == desc.topology)
                {
                    builder = b;
                    break;
                }
            }
            if (!builder)
            {
                DIA_LOG_ERROR("CFL", "CellFieldGeometryCache: no builder for topology (%ux%u)",
                              static_cast<unsigned>(desc.width), static_cast<unsigned>(desc.height));
                return nullptr;
            }

            DIA_PROFILE_SCOPE("CellField.BuildGeometry", Dia::Observation::Profile::Category::kDiaGraphics);

            Dia::Graphics::CellGridGeometry geometry;
            if (!builder->Build(desc, geometry))
            {
                DIA_LOG_ERROR("CFL", "CellFieldGeometryCache: geometry build failed (%ux%u)",
                              static_cast<unsigned>(desc.width), static_cast<unsigned>(desc.height));
                return nullptr;
            }

            Entry entry;
            if (!mFactory.Create(geometry, entry.gpu))
            {
                DIA_LOG_ERROR("CFL", "CellFieldGeometryCache: GPU upload failed (%ux%u)",
                              static_cast<unsigned>(desc.width), static_cast<unsigned>(desc.height));
                return nullptr;
            }

            ++mGeometryBuilds;
            entry.vertexCount = geometry.vertexCount;
            entry.indexCount = geometry.indexCount;
            entry.refCount = 1;
            entry.bytes = static_cast<uint64_t>(geometry.vertexCount) * sizeof(Dia::Graphics::VertexCellField) +
                          static_cast<uint64_t>(geometry.indexCount) * sizeof(uint32_t);
            return &mEntries.emplace(key, entry).first->second;
        }

        void CellFieldGeometryCache::Release(const CellFieldGeometryKey& key)
        {
            auto it = mEntries.find(key);
            if (it == mEntries.end())
                return;
            if (--it->second.refCount == 0)
            {
                mFactory.Destroy(it->second.gpu);
                mEntries.erase(it);
            }
        }

        void CellFieldGeometryCache::Shutdown()
        {
            for (auto& kv : mEntries)
                mFactory.Destroy(kv.second.gpu);
            mEntries.clear();
        }

        CellFieldGeometryStats CellFieldGeometryCache::Stats() const
        {
            CellFieldGeometryStats s;
            s.geometryBuilds = mGeometryBuilds;
            s.liveEntries = static_cast<uint32_t>(mEntries.size());
            for (const auto& kv : mEntries)
                s.gpuBytes += kv.second.bytes;
            return s;
        }

    } // namespace Bgfx
} // namespace Dia
