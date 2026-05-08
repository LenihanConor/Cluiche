#pragma once
#ifndef DIA_GEOMETRY2D_QUADTREE_H
#define DIA_GEOMETRY2D_QUADTREE_H

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
// CLASS Quadtree
//==============================================================================
// Adaptive quadtree spatial acceleration structure.
// Objects are inserted by AARect bounding box. Nodes split when they exceed
// splitThreshold objects and depth < maxDepth.
//
// Template parameters:
//   T          - The stored object type
//   MaxObjects - Maximum number of objects (default 2048)
//==============================================================================
template<typename T, unsigned int MaxObjects = 2048>
class Quadtree : public ISpatialStructure<T>
{
public:
    struct Def
    {
        AARect worldBounds;
        int    splitThreshold = 8;
        int    maxDepth       = 8;
    };

    explicit Quadtree(const Def& def);

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

    // Rebuild flattens all objects and re-inserts from scratch
    void Rebuild();
    int  GetNodeCount()   const;
    int  GetObjectCount() const;
    int  GetDepth()       const;

    // Traversal for debug/visualization (used by DiaGeometry2DVisualDebugger).
    // Visits every allocated node; callback receives (bounds, isLeaf).
    template<typename Callback>
    void VisitNodes(Callback&& cb) const
    {
        for (int i = 0; i < mNodeCount; ++i)
            cb(mNodes[i].bounds, mNodes[i].isLeaf);
    }

private:
    static constexpr int kMaxNodes        = 4096;
    static constexpr int kMaxSlotsPerNode = 64;

    struct Slot
    {
        T        object;
        AARect   bounds;
        uint32_t generation;
        bool     occupied;
    };

    struct QuadNode
    {
        AARect   bounds;
        Dia::Core::Containers::DynamicArrayC<uint32_t, kMaxSlotsPerNode> objectSlots;
        int      children[4]; // -1 = no child
        int      depth;
        bool     isLeaf;
    };

    Dia::Core::Containers::DynamicArrayC<Slot, MaxObjects> mSlots;
    QuadNode mNodes[kMaxNodes];
    int      mNodeCount;
    int      mRootNode;
    int      mSplitThreshold;
    int      mMaxDepth;

    uint32_t mFreeList[MaxObjects];
    int      mFreeCount;
    int      mOccupiedCount;

    // Helpers
    int  AllocNode();
    void InitNode(int nodeIdx, const AARect& bounds, int depth);
    void Subdivide(int nodeIdx);
    bool AARectFullyInsideChild(int nodeIdx, int childIdx, const AARect& bounds) const;
    void InsertIntoNode(int nodeIdx, uint32_t slotIdx);
    void RemoveFromNode(int nodeIdx, uint32_t slotIdx, bool& found);
    bool IsHandleValid(Dia::Core::Handle<T> handle) const;

    void QueryRegionNode  (int nodeIdx, const AARect& region, bool visited[], Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const;
    void QueryCircleNode  (int nodeIdx, const Circle& circle, bool visited[], Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const;
    void QueryPointNode   (int nodeIdx, const Dia::Maths::Vector2D& point, bool visited[], Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const;
    void QueryRayNode     (int nodeIdx, const Ray& ray, bool visited[], Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const;
    void CollectAllNode   (int nodeIdx, bool visited[], Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const;
    int  ComputeDepthNode (int nodeIdx) const;
};

}} // namespace Dia::Geometry2D

#include "DiaGeometry2D/Spatial/Quadtree.inl"

#endif // DIA_GEOMETRY2D_QUADTREE_H
