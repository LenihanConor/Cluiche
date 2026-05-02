#pragma once
#ifndef DIA_GEOMETRY2D_HEXGRID_H
#define DIA_GEOMETRY2D_HEXGRID_H

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
// STRUCT HexCoord
//==============================================================================
// Axial (q, r) coordinate for a pointy-top hex grid.
// Cube coordinate s is derived on demand as -q - r.
//==============================================================================
struct HexCoord
{
    int q;
    int r;

    bool operator==(const HexCoord& rhs) const { return q == rhs.q && r == rhs.r; }
    bool operator!=(const HexCoord& rhs) const { return !(*this == rhs); }
};

//==============================================================================
// CLASS HexGrid
//==============================================================================
// Uniform hexagonal grid spatial acceleration structure (pointy-top orientation,
// axial coordinates). Objects are inserted with an AARect bounding box and may
// span multiple hex cells. Queries iterate covered cells and deduplicate.
//
// Also exposes hex-specific utilities: neighbour lookup, coordinate conversion,
// ring enumeration, and direct cell queries via HexCoord.
//
// Template parameters:
//   T          - The stored object type
//   MaxObjects - Maximum number of objects that can be inserted (default 2048)
//==============================================================================
template<typename T, unsigned int MaxObjects = 2048>
class HexGrid : public ISpatialStructure<T>
{
public:
    struct Def
    {
        AARect worldBounds;
        float  hexRadius;   // circumradius (center-to-vertex)
    };

    explicit HexGrid(const Def& def);

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

    // Hex-specific API
    HexCoord  WorldToHex(const Dia::Maths::Vector2D& worldPos) const;
    Dia::Maths::Vector2D HexToWorld(HexCoord hex) const;
    bool      IsValidHex(HexCoord hex) const;

    void GetNeighbours(HexCoord hex,
                       Dia::Core::Containers::DynamicArrayC<HexCoord, 6>& out) const;
    int  HexDistance(HexCoord a, HexCoord b) const;
    void GetRing(HexCoord center, int radius,
                 Dia::Core::Containers::DynamicArrayC<HexCoord, 256>& out) const;

    void QueryHex(HexCoord hex,
                  Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const;

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

    // Pointy-top axial direction vectors (q, r) for 6 neighbours
    static const int kNeighbourDQ[6];
    static const int kNeighbourDR[6];

    // Slot pool
    Dia::Core::Containers::DynamicArrayC<Slot, MaxObjects> mSlots;
    uint32_t mFreeList[MaxObjects];
    int      mFreeCount;
    int      mOccupiedCount;

    // Hex grid cells: each cell holds slot indices
    Dia::Core::Containers::DynamicArrayC<uint32_t, kMaxObjectsPerCell> mCells[kMaxCells];
    int   mMinQ;
    int   mMinR;
    int   mColCount;  // number of distinct q values
    int   mRowCount;  // number of distinct r values
    float mHexRadius;
    float mHexWidth;  // sqrt(3) * radius
    float mHexHeight; // 2 * radius
    AARect mWorldBounds;

    // Helpers
    int  HexToIndex(HexCoord hex) const;
    bool WorldToHexClamped(const Dia::Maths::Vector2D& pos, HexCoord& out) const;
    void GetHexRange(const AARect& bounds,
                     int& minQ, int& minR, int& maxQ, int& maxR) const;
    bool IsHandleValid(Dia::Core::Handle<T> handle) const;
    void RemoveFromCells(uint32_t slotIdx, const AARect& bounds);
    void InsertIntoCells(uint32_t slotIdx, const AARect& bounds);
    void QueryCellList(const int* cellIndices, int cellCount,
                       bool visited[],
                       Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const;
};

}} // namespace Dia::Geometry2D

#include "DiaGeometry2D/Spatial/HexGrid.inl"

#endif // DIA_GEOMETRY2D_HEXGRID_H
