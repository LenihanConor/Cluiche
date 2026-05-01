#pragma once
#ifndef DIA_GEOMETRY2D_BVH_H
#define DIA_GEOMETRY2D_BVH_H

#include "DiaCore/Core/Assert.h"
#include "DiaCore/Containers/Handle.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaGeometry2D/Spatial/ISpatialStructure.h"
#include "DiaGeometry2D/Shapes/AARect.h"
#include "DiaGeometry2D/Shapes/Circle.h"
#include "DiaGeometry2D/Shapes/Ray.h"
#include "DiaGeometry2D/Intersection/IntersectionTests.h"
#include "DiaMaths/Vector/Vector2D.h"

#include <cstring>
#include <cstdint>

namespace Dia { namespace Geometry2D {

//==============================================================================
// CLASS BVH (Bounding Volume Hierarchy)
//==============================================================================
// Static BVH built from a flat array of objects using SAH (Surface Area
// Heuristic). The structure is immutable after Build(); call Rebuild() to
// refresh after data changes.
//
// Insert / Remove / Update are not supported and will DIA_ASSERT.
//
// Template parameters:
//   T          - The stored object type
//   MaxObjects - Maximum number of objects (default 2048)
//==============================================================================
template<typename T, unsigned int MaxObjects = 2048>
class BVH : public ISpatialStructure<T>
{
public:
    struct BuildEntry
    {
        T      object;
        AARect bounds;
    };

    struct Def
    {
        int maxLeafObjects = 4;
    };

    explicit BVH(const Def& def);

    // Build / rebuild
    void Build   (const Dia::Core::Containers::DynamicArrayC<BuildEntry, MaxObjects>& entries);
    void Rebuild (const Dia::Core::Containers::DynamicArrayC<BuildEntry, MaxObjects>& entries);
    bool IsBuilt () const;

    // ISpatialStructure queries
    void QueryRegion  (const AARect& region,              Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const override;
    void QueryCircle  (const Circle& circle,              Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const override;
    void QueryPoint   (const Dia::Maths::Vector2D& point, Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const override;
    void QueryRay     (const Ray& ray,                    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const override;
    void QueryKNearest(const Dia::Maths::Vector2D& point, int k, Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const override;
    const T* Resolve  (Dia::Core::Handle<T> handle) const override;

    // Mutation — not supported; will DIA_ASSERT
    Dia::Core::Handle<T> Insert(const T& object, const AARect& bounds) override;
    void Remove(Dia::Core::Handle<T> handle) override;
    void Update(Dia::Core::Handle<T> handle, const AARect& newBounds) override;
    void Clear() override;

    int GetNodeCount()   const;
    int GetLeafCount()   const;
    int GetObjectCount() const;
    int GetDepth()       const;

private:
    static constexpr int kMaxNodes = 4096;

    struct Slot
    {
        T        object;
        uint32_t generation;
    };

    struct BVHNode
    {
        AARect bounds;
        int    leftChild;   // -1 = leaf node
        int    rightChild;
        int    objectStart; // index into mSortedSlots
        int    objectCount; // 0 for internal nodes
    };

    Def     mDef;
    BVHNode mNodes[kMaxNodes];
    int     mNodeCount;
    bool    mIsBuilt;

    Slot    mSlots[MaxObjects];
    AARect  mSlotBounds[MaxObjects];  // per-object bounds stored during Build()
    int     mSortedSlots[MaxObjects]; // indices sorted during build
    int     mObjectCount;

    // Build helpers
    int    BuildNode(int* sortedSlots, int count, int depth);
    AARect ComputeBounds(const int* sortedSlots, int count) const;
    void   InsertionSortByAxis(int* sortedSlots, int count, int axis) const;
    float  ComputeSAHSplit(const int* sortedSlots, int count, int axis, int& outSplitIdx) const;

    // Query helpers
    void QueryRegionNode  (int nodeIdx, const AARect& region,              Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const;
    void QueryCircleNode  (int nodeIdx, const Circle& circle,              Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const;
    void QueryPointNode   (int nodeIdx, const Dia::Maths::Vector2D& point, Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const;
    void QueryRayNode     (int nodeIdx, const Ray& ray,                    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const;
    int  ComputeDepthNode (int nodeIdx, int currentDepth) const;
    int  CountLeafNodes   (int nodeIdx) const;
};

}} // namespace Dia::Geometry2D

#include "DiaGeometry2D/Spatial/BVH.inl"

#endif // DIA_GEOMETRY2D_BVH_H
