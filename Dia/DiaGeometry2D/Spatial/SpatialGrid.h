#pragma once
#ifndef DIA_GEOMETRY2D_SPATIALGRID_H
#define DIA_GEOMETRY2D_SPATIALGRID_H

#include "DiaCore/Core/Assert.h"
#include "DiaCore/Containers/Handle.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaGeometry2D/Spatial/ISpatialStructure.h"
#include "DiaGeometry2D/Shapes/AARect.h"
#include "DiaGeometry2D/Shapes/Circle.h"
#include "DiaGeometry2D/Shapes/Ray.h"
#include "DiaGeometry2D/Intersection/IntersectionTests.h"
#include "DiaMaths/Vector/Vector2D.h"

#include <cmath>
#include <cstring>
#include <cstdint>

namespace Dia { namespace Geometry2D {

//==============================================================================
// CLASS SpatialGrid
//==============================================================================
// Uniform grid spatial acceleration structure.
// Objects are inserted using their AARect bounding box. Each object may span
// multiple cells. Queries iterate over the covered cells and deduplicate.
//
// Template parameters:
//   T          - The stored object type
//   MaxObjects - Maximum number of objects that can be inserted (default 2048)
//==============================================================================
template<typename T, unsigned int MaxObjects = 2048>
class SpatialGrid : public ISpatialStructure<T>
{
public:
    struct Def
    {
        AARect worldBounds;
        float  cellSize;
    };

    explicit SpatialGrid(const Def& def);

    // ISpatialStructure interface
    Dia::Core::Handle<T> Insert(const T& object, const AARect& bounds) override;
    void Remove(Dia::Core::Handle<T> handle) override;
    void Update(Dia::Core::Handle<T> handle, const AARect& newBounds) override;
    void Clear() override;

    void QueryRegion  (const AARect& region,              Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const override;
    void QueryCircle  (const Circle& circle,              Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const override;
    void QueryPoint   (const Dia::Maths::Vector2D& point, Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const override;
    void QueryRay     (const Ray& ray,                    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const override;
    void QueryKNearest(const Dia::Maths::Vector2D& point, int k, Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const override;

    const T* Resolve(Dia::Core::Handle<T> handle) const override;

    int GetCellCount() const;
    int GetObjectCount() const;

private:
    struct Slot
    {
        T        object;
        AARect   bounds;
        uint32_t generation;
        bool     occupied;
    };

    static constexpr int kMaxCells          = 4096;
    static constexpr int kMaxObjectsPerCell = 64;

    // Slot pool
    Dia::Core::Containers::DynamicArrayC<Slot, MaxObjects>              mSlots;
    uint32_t mFreeList[MaxObjects];
    int      mFreeCount;
    int      mOccupiedCount;

    // Grid cells: each cell holds slot indices
    Dia::Core::Containers::DynamicArrayC<uint32_t, kMaxObjectsPerCell>  mCells[kMaxCells];
    int   mCellCountX;
    int   mCellCountY;
    float mCellSize;
    AARect mWorldBounds;

    // Helpers
    bool WorldToCellClamped(float x, float y, int& cx, int& cy) const;
    void GetCellRange(const AARect& bounds, int& minCx, int& minCy, int& maxCx, int& maxCy) const;
    bool IsHandleValid(Dia::Core::Handle<T> handle) const;
    void RemoveFromCells(uint32_t slotIdx, const AARect& bounds);
    void InsertIntoCells(uint32_t slotIdx, const AARect& bounds);
    void QueryCellRange(int minCx, int minCy, int maxCx, int maxCy,
                        bool visited[],
                        Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const;
};

}} // namespace Dia::Geometry2D

#include "DiaGeometry2D/Spatial/SpatialGrid.inl"

#endif // DIA_GEOMETRY2D_SPATIALGRID_H
