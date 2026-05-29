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
// Offset (col, row) coordinate for a pointy-top hex grid, odd-row-right stagger.
// q = column index (0..colCount-1), r = row index (0..rowCount-1).
// Odd rows are shifted right by hexWidth/2 relative to even rows.
//==============================================================================
struct HexCoord
{
    int q; // column
    int r; // row

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
        Dia::Maths::Vector2D origin     = {};   // world position of the bottom-left of cell (0,0)
        int                  colCount   = 1;    // number of columns
        int                  rowCount   = 1;    // number of rows
        float                hexRadius  = 1.0f; // circumradius (center-to-vertex)
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

    // Accessors for debug/visualization (used by DiaGeometry2DVisualDebugger)
    float                GetHexRadius() const { return mHexRadius; }
    int                  GetColCount()  const { return mColCount;  }
    int                  GetRowCount()  const { return mRowCount;  }
    Dia::Maths::Vector2D GetOrigin()    const { return mOrigin;    }

private:
    struct Slot
    {
        T        object     = {};
        AARect   bounds     = {};
        uint32_t generation = 0;
        bool     occupied   = false;
    };

    static constexpr int kMaxCells          = 4096;
    static constexpr int kMaxObjectsPerCell = 64;

    // Pointy-top offset-coord direction tables (6 neighbours, split by row parity).
    // Even rows (r % 2 == 0): kEvenDQ/kEvenDR.  Odd rows: kOddDQ/kOddDR.
    static const int kEvenDQ[6];
    static const int kEvenDR[6];
    static const int kOddDQ[6];
    static const int kOddDR[6];

    // Cube-coord direction vectors used internally for HexDistance / GetRing.
    static const int kCubeDQ[6];
    static const int kCubeDR[6];

    // Slot pool
    Dia::Core::Containers::DynamicArrayC<Slot, MaxObjects> mSlots;
    uint32_t mFreeList[MaxObjects];
    int      mFreeCount;
    int      mOccupiedCount;

    // Hex grid cells: each cell holds slot indices
    Dia::Core::Containers::DynamicArrayC<uint32_t, kMaxObjectsPerCell> mCells[kMaxCells];
    int                  mColCount;  // number of columns
    int                  mRowCount;  // number of rows
    float                mHexRadius;
    float                mHexWidth;  // sqrt(3) * radius
    float                mHexHeight; // 2 * radius
    Dia::Maths::Vector2D mOrigin;    // world position of cell (0,0) bottom-left

    // Cube-coord helpers (for HexDistance/GetRing which work in cube space)
    struct CubeCoord { int q, r, s; };
    static CubeCoord OffsetToCube(HexCoord hex);
    static HexCoord  CubeToOffset(CubeCoord cube);

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
