#pragma once
#ifndef DIA_GEOMETRY3D_SPATIALGRID3D_H
#define DIA_GEOMETRY3D_SPATIALGRID3D_H

#include "DiaGeometry3D/Spatial/ISpatialStructure3D.h"
#include "DiaCore/Core/Assert.h"

#include <cstdint>

namespace Dia::Geometry3D
{
    // Not thread-safe. Concurrent reads after construction are safe;
    // concurrent Insert/Remove/Update/Query are not.
    template<typename T, unsigned int MaxObjects = 2048, unsigned int MaxCells = 4096>
    class SpatialGrid3D : public ISpatialStructure3D<T>
    {
    public:
        struct Def
        {
            AABB  worldBounds;
            float cellSize;
        };

        explicit SpatialGrid3D(const Def& def);

        Dia::Core::Handle<T> Insert(const T& object, const AABB& bounds) override;
        void Remove(Dia::Core::Handle<T> handle) override;
        void Update(Dia::Core::Handle<T> handle, const AABB& newBounds) override;
        void Clear() override;

        void QueryRegion  (const AABB& region,                          Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const override;
        void QuerySphere  (const Sphere& sphere,                        Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const override;
        void QueryPoint   (const Dia::Maths::Vector3D& point,           Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const override;
        void QueryRay     (const Ray& ray,                              Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const override;
        void QueryFrustum (const Frustum& frustum,                      Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const override;
        void QueryKNearest(const Dia::Maths::Vector3D& point, int k,    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const override;

        const T* Resolve(Dia::Core::Handle<T> handle) const override;

        int GetCellCount()   const;
        int GetObjectCount() const { return mOccupiedCount; }
        int GetCellCountX()  const { return mCellCountX; }
        int GetCellCountY()  const { return mCellCountY; }
        int GetCellCountZ()  const { return mCellCountZ; }
        float GetCellSize()  const { return mCellSize; }
        const AABB& GetWorldBounds() const { return mWorldBounds; }

    private:
        struct Slot
        {
            T        object;
            AABB     bounds;
            uint32_t generation;
            bool     occupied;
        };

        static constexpr int kMaxObjectsPerCell = 64;

        Dia::Core::Containers::DynamicArrayC<Slot, MaxObjects> mSlots;
        uint32_t mFreeList[MaxObjects];
        int      mFreeCount;
        int      mOccupiedCount;

        Dia::Core::Containers::DynamicArrayC<uint32_t, kMaxObjectsPerCell> mCells[MaxCells];

        int   mCellCountX;
        int   mCellCountY;
        int   mCellCountZ;
        float mCellSize;
        AABB  mWorldBounds;

        bool WorldToCellClamped(float x, float y, float z, int& cx, int& cy, int& cz) const;
        bool WorldToCellExact  (float x, float y, float z, int& cx, int& cy, int& cz) const;
        int  CellIndex(int cx, int cy, int cz) const;

        void GetCellRange(const AABB& bounds,
                          int& minCx, int& minCy, int& minCz,
                          int& maxCx, int& maxCy, int& maxCz) const;

        bool IsHandleValid(Dia::Core::Handle<T> h) const;
        void RemoveFromCells(uint32_t slotIdx, const AABB& bounds);
        void InsertIntoCells(uint32_t slotIdx, const AABB& bounds);

        AABB CellAABB(int cx, int cy, int cz) const;
    };
}

#include "DiaGeometry3D/Spatial/SpatialGrid3D.inl"

#endif
