////////////////////////////////////////////////////////////////////////////////
// Filename: CellFieldGeometryCache.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaGraphics/CellField/CellGridDesc.h>
#include <DiaGraphics/CellField/ICellGridGeometryBuilder.h>

#include <stdint.h>
#include <unordered_map>
#include <vector>

namespace Dia
{
    namespace Bgfx
    {
        // Opaque GPU geometry id. Meaning is owned by the factory that produced it.
        using CellFieldGpuHandle = uint32_t;

        // Seam between cache bookkeeping (pure, testable without a GPU) and the GPU upload.
        class ICellFieldGpuGeometryFactory
        {
        public:
            virtual ~ICellFieldGpuGeometryFactory() = default;

            // Upload static vertex + index buffers. Returns false on failure.
            virtual bool Create(const Dia::Graphics::CellGridGeometry& geometry, CellFieldGpuHandle& outHandle) = 0;
            virtual void Destroy(CellFieldGpuHandle handle) = 0;
        };

        // Real bgfx implementation (render thread only). The handle packs the vertex buffer id in
        // the low 16 bits and the index buffer id in the high 16 bits; the renderer unpacks them
        // with the static accessors below. bgfx types stay in the .cpp.
        class BgfxCellFieldGpuGeometryFactory : public ICellFieldGpuGeometryFactory
        {
        public:
            bool Create(const Dia::Graphics::CellGridGeometry& geometry, CellFieldGpuHandle& outHandle) override;
            void Destroy(CellFieldGpuHandle handle) override;

            static unsigned short VertexBuffer(CellFieldGpuHandle h) { return static_cast<unsigned short>(h & 0xFFFFu); }
            static unsigned short IndexBuffer(CellFieldGpuHandle h)  { return static_cast<unsigned short>(h >> 16); }
        };

        struct CellFieldGeometryKey
        {
            Dia::Core::StringCRC topology;
            uint16_t             width = 0;
            uint16_t             height = 0;

            bool operator==(const CellFieldGeometryKey& o) const
            {
                return topology == o.topology && width == o.width && height == o.height;
            }
        };

        struct CellFieldGeometryStats
        {
            uint32_t geometryBuilds = 0;	// cumulative CPU builds + uploads
            uint32_t liveEntries = 0;
            uint64_t gpuBytes = 0;			// vertex + index bytes currently resident
        };

        // Reference-counted cache of static CellField GPU geometry, keyed by (topology, w, h).
        // Render thread only. The builders and factory are borrowed and must outlive the cache.
        class CellFieldGeometryCache
        {
        public:
            struct Entry
            {
                CellFieldGpuHandle gpu = 0;
                uint32_t           vertexCount = 0;
                uint32_t           indexCount = 0;
                uint32_t           refCount = 0;
                uint64_t           bytes = 0;
            };

            explicit CellFieldGeometryCache(ICellFieldGpuGeometryFactory& factory);
            ~CellFieldGeometryCache();

            // Registers a builder (looked up by Topology()). Borrowed pointer.
            void AddBuilder(const Dia::Graphics::ICellGridGeometryBuilder* builder);

            static CellFieldGeometryKey MakeKey(const Dia::Graphics::CellGridDesc& desc);

            // Builds + uploads if the key is new, else increments the refcount.
            // Returns null (and logs an error) on unknown topology or build/upload failure.
            // The pointer is valid until the matching Release drops the last reference.
            const Entry* Acquire(const Dia::Graphics::CellGridDesc& desc);

            // Decrements; at zero destroys the GPU buffers and removes the entry.
            void Release(const CellFieldGeometryKey& key);

            // Frees everything regardless of refcount.
            void Shutdown();

            CellFieldGeometryStats Stats() const;

        private:
            struct KeyHash
            {
                size_t operator()(const CellFieldGeometryKey& k) const noexcept
                {
                    return (static_cast<size_t>(k.topology.Value()) * 0x9E3779B1u) ^
                           (static_cast<size_t>(k.width) << 16) ^ static_cast<size_t>(k.height);
                }
            };

            ICellFieldGpuGeometryFactory&                                mFactory;
            std::vector<const Dia::Graphics::ICellGridGeometryBuilder*>  mBuilders;
            std::unordered_map<CellFieldGeometryKey, Entry, KeyHash>     mEntries;
            uint32_t                                                     mGeometryBuilds = 0;
        };

    } // namespace Bgfx
} // namespace Dia
