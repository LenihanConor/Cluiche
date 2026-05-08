//==============================================================================
// BVH.inl  — template implementation
//==============================================================================

#ifndef DIA_GEOMETRY2D_BVH_H
#error "Do not include BVH.inl directly — include BVH.h"
#endif

#include <cstring>
#include <cfloat>

namespace Dia { namespace Geometry2D {

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
BVH<T, MaxObjects>::BVH(const Def& def)
    : mDef(def)
    , mNodeCount(0)
    , mIsBuilt(false)
    , mObjectCount(0)
{
    memset(mSortedSlots, 0, sizeof(mSortedSlots));
    for (int i = 0; i < kMaxNodes; ++i)
    {
        mNodes[i].leftChild   = -1;
        mNodes[i].rightChild  = -1;
        mNodes[i].objectStart = 0;
        mNodes[i].objectCount = 0;
    }
}

//------------------------------------------------------------------------------
// Build
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void BVH<T, MaxObjects>::Build(
    const Dia::Core::Containers::DynamicArrayC<BuildEntry, MaxObjects>& entries)
{
    mNodeCount   = 0;
    mObjectCount = static_cast<int>(entries.Size());
    DIA_ASSERT(mObjectCount <= static_cast<int>(MaxObjects), "BVH::Build: too many objects");

    if (mObjectCount == 0)
    {
        mIsBuilt = true;
        return;
    }

    // Fill slots, bounds and sorted indices
    for (int i = 0; i < mObjectCount; ++i)
    {
        mSlots[i].object     = entries[i].object;
        // generation = 1 for freshly built objects (handle index == sorted position)
        mSlots[i].generation = 1u;
        mSlotBounds[i]       = entries[i].bounds;
        mSortedSlots[i]      = i;
    }

    // Build the tree recursively
    BuildNode(mSortedSlots, mObjectCount, 0);

    mIsBuilt = true;
}

//------------------------------------------------------------------------------
// Rebuild
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void BVH<T, MaxObjects>::Rebuild(
    const Dia::Core::Containers::DynamicArrayC<BuildEntry, MaxObjects>& entries)
{
    mIsBuilt = false;
    Build(entries);
}

//------------------------------------------------------------------------------
// IsBuilt
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
bool BVH<T, MaxObjects>::IsBuilt() const
{
    return mIsBuilt;
}

//------------------------------------------------------------------------------
// Clear
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void BVH<T, MaxObjects>::Clear()
{
    mNodeCount   = 0;
    mObjectCount = 0;
    mIsBuilt     = false;
}

//------------------------------------------------------------------------------
// Mutation stubs — not supported
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
Dia::Core::Handle<T> BVH<T, MaxObjects>::Insert(const T& /*object*/, const AARect& /*bounds*/)
{
    DIA_ASSERT(false, "BVH::Insert: BVH is a static structure — use Build() to construct it");
    return Dia::Core::Handle<T>::Invalid();
}

template<typename T, unsigned int MaxObjects>
void BVH<T, MaxObjects>::Remove(Dia::Core::Handle<T> /*handle*/)
{
    DIA_ASSERT(false, "BVH::Remove: BVH is a static structure — use Rebuild() to modify it");
}

template<typename T, unsigned int MaxObjects>
void BVH<T, MaxObjects>::Update(Dia::Core::Handle<T> /*handle*/, const AARect& /*newBounds*/)
{
    DIA_ASSERT(false, "BVH::Update: BVH is a static structure — use Rebuild() to modify it");
}

//------------------------------------------------------------------------------
// QueryRegion
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void BVH<T, MaxObjects>::QueryRegion(
    const AARect& region,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    if (!mIsBuilt || mNodeCount == 0) return;
    QueryRegionNode(0, region, out);
}

//------------------------------------------------------------------------------
// QueryCircle
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void BVH<T, MaxObjects>::QueryCircle(
    const Circle& circle,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    if (!mIsBuilt || mNodeCount == 0) return;
    QueryCircleNode(0, circle, out);
}

//------------------------------------------------------------------------------
// QueryPoint
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void BVH<T, MaxObjects>::QueryPoint(
    const Dia::Maths::Vector2D& point,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    if (!mIsBuilt || mNodeCount == 0) return;
    QueryPointNode(0, point, out);
}

//------------------------------------------------------------------------------
// QueryRay
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void BVH<T, MaxObjects>::QueryRay(
    const Ray& ray,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    if (!mIsBuilt || mNodeCount == 0) return;
    QueryRayNode(0, ray, out);
}

//------------------------------------------------------------------------------
// QueryKNearest
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void BVH<T, MaxObjects>::QueryKNearest(
    const Dia::Maths::Vector2D& point,
    int k,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    if (!mIsBuilt || mNodeCount == 0 || k <= 0) return;

    struct Candidate
    {
        Dia::Core::Handle<T> handle;
        float                sqDist;
    };

    // Use a fixed-size candidate buffer bounded by MaxObjects
    static constexpr int kBVHKNearestMax = static_cast<int>(MaxObjects);
    Candidate candidates[kBVHKNearestMax];
    int       candidateCount = 0;

    // Walk all objects using per-slot bounds (stored during Build())
    for (int i = 0; i < mObjectCount && candidateCount < kBVHKNearestMax; ++i)
    {
        const Dia::Maths::Vector2D center = mSlotBounds[i].CalculateCenter();
        const float dx = center.x - point.x;
        const float dy = center.y - point.y;
        candidates[candidateCount].handle = Dia::Core::Handle<T>(static_cast<uint32_t>(i), mSlots[i].generation);
        candidates[candidateCount].sqDist = dx * dx + dy * dy;
        ++candidateCount;
    }

    const int resultCount = (k < candidateCount) ? k : candidateCount;
    for (int i = 0; i < resultCount && !out.IsFull(); ++i)
    {
        int minIdx = i;
        for (int j = i + 1; j < candidateCount; ++j)
        {
            if (candidates[j].sqDist < candidates[minIdx].sqDist)
                minIdx = j;
        }
        if (minIdx != i)
        {
            Candidate tmp      = candidates[i];
            candidates[i]      = candidates[minIdx];
            candidates[minIdx] = tmp;
        }
        out.Add(candidates[i].handle);
    }
}

//------------------------------------------------------------------------------
// Resolve
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
const T* BVH<T, MaxObjects>::Resolve(Dia::Core::Handle<T> handle) const
{
    if (!handle.IsValid()) return nullptr;
    const uint32_t idx = handle.GetIndex();
    if (static_cast<int>(idx) >= mObjectCount) return nullptr;
    if (mSlots[idx].generation != handle.GetGeneration()) return nullptr;
    return &mSlots[idx].object;
}

//------------------------------------------------------------------------------
// Stats
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
int BVH<T, MaxObjects>::GetNodeCount() const
{
    return mNodeCount;
}

template<typename T, unsigned int MaxObjects>
int BVH<T, MaxObjects>::GetLeafCount() const
{
    if (mNodeCount == 0) return 0;
    return CountLeafNodes(0);
}

template<typename T, unsigned int MaxObjects>
int BVH<T, MaxObjects>::GetObjectCount() const
{
    return mObjectCount;
}

template<typename T, unsigned int MaxObjects>
int BVH<T, MaxObjects>::GetDepth() const
{
    if (mNodeCount == 0) return 0;
    return ComputeDepthNode(0, 0);
}

//==============================================================================
// Private — build helpers
//==============================================================================

template<typename T, unsigned int MaxObjects>
int BVH<T, MaxObjects>::BuildNode(int* sortedSlots, int count, int depth)
{
    DIA_ASSERT(mNodeCount < kMaxNodes, "BVH::BuildNode: node pool exhausted");
    const int nodeIdx = mNodeCount++;
    BVHNode& node = mNodes[nodeIdx];
    node.bounds       = ComputeBounds(sortedSlots, count);
    node.leftChild    = -1;
    node.rightChild   = -1;
    node.objectStart  = static_cast<int>(sortedSlots - mSortedSlots);
    node.objectCount  = count;

    // Make leaf if small enough
    if (count <= mDef.maxLeafObjects)
    {
        return nodeIdx;
    }

    // Try SAH split on both axes, pick best
    int   bestAxis       = 0;
    int   bestSplitIdx   = count / 2;
    float bestCost       = FLT_MAX;

    // Temp buffers for each axis sort (stack alloc — MaxObjects is a compile-time constant)
    int tempSlots0[MaxObjects];
    int tempSlots1[MaxObjects];

    int*  axisBuffers[2] = { tempSlots0, tempSlots1 };

    for (int axis = 0; axis < 2; ++axis)
    {
        int* buf = axisBuffers[axis];
        memcpy(buf, sortedSlots, count * sizeof(int));
        InsertionSortByAxis(buf, count, axis);

        int   splitIdx;
        float cost = ComputeSAHSplit(buf, count, axis, splitIdx);
        if (cost < bestCost)
        {
            bestCost     = cost;
            bestAxis     = axis;
            bestSplitIdx = splitIdx;
        }
    }

    // Apply the best sort order back to sortedSlots
    memcpy(sortedSlots, axisBuffers[bestAxis], count * sizeof(int));

    // Clamp split to ensure both halves are non-empty
    if (bestSplitIdx <= 0)       bestSplitIdx = 1;
    if (bestSplitIdx >= count)   bestSplitIdx = count - 1;

    // Recurse
    node.objectCount = 0; // internal node has no direct objects
    const int leftNodeIdx  = BuildNode(sortedSlots,               bestSplitIdx,         depth + 1);
    const int rightNodeIdx = BuildNode(sortedSlots + bestSplitIdx, count - bestSplitIdx, depth + 1);

    // Re-fetch node ref after recursive allocation (pointer may be stale)
    mNodes[nodeIdx].leftChild  = leftNodeIdx;
    mNodes[nodeIdx].rightChild = rightNodeIdx;

    return nodeIdx;
}

template<typename T, unsigned int MaxObjects>
AARect BVH<T, MaxObjects>::ComputeBounds(const int* sortedSlots, int count) const
{
    if (count == 0) return AARect(Dia::Maths::Vector2D(0.0f, 0.0f), Dia::Maths::Vector2D(0.0f, 0.0f));

    float minX = mSlotBounds[sortedSlots[0]].GetBottomLeft().x;
    float minY = mSlotBounds[sortedSlots[0]].GetBottomLeft().y;
    float maxX = mSlotBounds[sortedSlots[0]].GetTopRight().x;
    float maxY = mSlotBounds[sortedSlots[0]].GetTopRight().y;

    for (int i = 1; i < count; ++i)
    {
        const AARect& b = mSlotBounds[sortedSlots[i]];
        if (b.GetBottomLeft().x < minX) minX = b.GetBottomLeft().x;
        if (b.GetBottomLeft().y < minY) minY = b.GetBottomLeft().y;
        if (b.GetTopRight().x   > maxX) maxX = b.GetTopRight().x;
        if (b.GetTopRight().y   > maxY) maxY = b.GetTopRight().y;
    }
    return AARect(Dia::Maths::Vector2D(minX, minY), Dia::Maths::Vector2D(maxX, maxY));
}

template<typename T, unsigned int MaxObjects>
void BVH<T, MaxObjects>::InsertionSortByAxis(int* sortedSlots, int count, int axis) const
{
    for (int i = 1; i < count; ++i)
    {
        const int key = sortedSlots[i];
        const Dia::Maths::Vector2D centroidKey = mSlotBounds[key].CalculateCenter();
        const float keyVal = (axis == 0) ? centroidKey.x : centroidKey.y;

        int j = i - 1;
        while (j >= 0)
        {
            const Dia::Maths::Vector2D centroidJ = mSlotBounds[sortedSlots[j]].CalculateCenter();
            const float jVal = (axis == 0) ? centroidJ.x : centroidJ.y;
            if (jVal > keyVal)
            {
                sortedSlots[j + 1] = sortedSlots[j];
                --j;
            }
            else break;
        }
        sortedSlots[j + 1] = key;
    }
}

template<typename T, unsigned int MaxObjects>
float BVH<T, MaxObjects>::ComputeSAHSplit(const int* sortedSlots, int count, int axis, int& outSplitIdx) const
{
    // Evaluate SAH cost for each possible split position
    // cost(split) = leftSA * leftCount + rightSA * rightCount
    // SA for 2D AARect = 2*(width + height) = perimeter

    outSplitIdx = count / 2;
    float bestCost = FLT_MAX;

    for (int split = 1; split < count; ++split)
    {
        // Left bounds
        float lMinX = mSlotBounds[sortedSlots[0]].GetBottomLeft().x;
        float lMinY = mSlotBounds[sortedSlots[0]].GetBottomLeft().y;
        float lMaxX = mSlotBounds[sortedSlots[0]].GetTopRight().x;
        float lMaxY = mSlotBounds[sortedSlots[0]].GetTopRight().y;
        for (int i = 1; i < split; ++i)
        {
            const AARect& b = mSlotBounds[sortedSlots[i]];
            if (b.GetBottomLeft().x < lMinX) lMinX = b.GetBottomLeft().x;
            if (b.GetBottomLeft().y < lMinY) lMinY = b.GetBottomLeft().y;
            if (b.GetTopRight().x   > lMaxX) lMaxX = b.GetTopRight().x;
            if (b.GetTopRight().y   > lMaxY) lMaxY = b.GetTopRight().y;
        }
        const float leftSA = 2.0f * ((lMaxX - lMinX) + (lMaxY - lMinY));

        // Right bounds
        float rMinX = mSlotBounds[sortedSlots[split]].GetBottomLeft().x;
        float rMinY = mSlotBounds[sortedSlots[split]].GetBottomLeft().y;
        float rMaxX = mSlotBounds[sortedSlots[split]].GetTopRight().x;
        float rMaxY = mSlotBounds[sortedSlots[split]].GetTopRight().y;
        for (int i = split + 1; i < count; ++i)
        {
            const AARect& b = mSlotBounds[sortedSlots[i]];
            if (b.GetBottomLeft().x < rMinX) rMinX = b.GetBottomLeft().x;
            if (b.GetBottomLeft().y < rMinY) rMinY = b.GetBottomLeft().y;
            if (b.GetTopRight().x   > rMaxX) rMaxX = b.GetTopRight().x;
            if (b.GetTopRight().y   > rMaxY) rMaxY = b.GetTopRight().y;
        }
        const float rightSA = 2.0f * ((rMaxX - rMinX) + (rMaxY - rMinY));

        const float cost = leftSA * static_cast<float>(split) + rightSA * static_cast<float>(count - split);
        if (cost < bestCost)
        {
            bestCost    = cost;
            outSplitIdx = split;
        }
    }

    return bestCost;
}

//==============================================================================
// Private — query helpers
//==============================================================================

template<typename T, unsigned int MaxObjects>
void BVH<T, MaxObjects>::QueryRegionNode(
    int nodeIdx,
    const AARect& region,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    if (out.IsFull()) return;

    const BVHNode& node = mNodes[nodeIdx];

    if (IntersectionTests::IsIntersecting(node.bounds, region).IsNotIntersecting()) return;

    if (node.leftChild == -1)
    {
        // Leaf
        for (int i = node.objectStart; i < node.objectStart + node.objectCount && !out.IsFull(); ++i)
        {
            const int slotIdx = mSortedSlots[i];
            if (IntersectionTests::IsIntersecting(mSlotBounds[slotIdx], region).IsIntersecting())
            {
                out.Add(Dia::Core::Handle<T>(static_cast<uint32_t>(slotIdx), mSlots[slotIdx].generation));
            }
        }
    }
    else
    {
        QueryRegionNode(node.leftChild,  region, out);
        QueryRegionNode(node.rightChild, region, out);
    }
}

template<typename T, unsigned int MaxObjects>
void BVH<T, MaxObjects>::QueryCircleNode(
    int nodeIdx,
    const Circle& circle,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    if (out.IsFull()) return;

    const BVHNode& node = mNodes[nodeIdx];

    if (IntersectionTests::IsIntersecting(node.bounds, circle).IsNotIntersecting()) return;

    if (node.leftChild == -1)
    {
        for (int i = node.objectStart; i < node.objectStart + node.objectCount && !out.IsFull(); ++i)
        {
            const int slotIdx = mSortedSlots[i];
            if (IntersectionTests::IsIntersecting(mSlotBounds[slotIdx], circle).IsIntersecting())
            {
                out.Add(Dia::Core::Handle<T>(static_cast<uint32_t>(slotIdx), mSlots[slotIdx].generation));
            }
        }
    }
    else
    {
        QueryCircleNode(node.leftChild,  circle, out);
        QueryCircleNode(node.rightChild, circle, out);
    }
}

template<typename T, unsigned int MaxObjects>
void BVH<T, MaxObjects>::QueryPointNode(
    int nodeIdx,
    const Dia::Maths::Vector2D& point,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    if (out.IsFull()) return;

    const BVHNode& node = mNodes[nodeIdx];

    if (IntersectionTests::IsIntersecting(point, node.bounds).IsNotIntersecting()) return;

    if (node.leftChild == -1)
    {
        for (int i = node.objectStart; i < node.objectStart + node.objectCount && !out.IsFull(); ++i)
        {
            const int slotIdx = mSortedSlots[i];
            if (IntersectionTests::IsIntersecting(point, mSlotBounds[slotIdx]).IsIntersecting())
            {
                out.Add(Dia::Core::Handle<T>(static_cast<uint32_t>(slotIdx), mSlots[slotIdx].generation));
            }
        }
    }
    else
    {
        QueryPointNode(node.leftChild,  point, out);
        QueryPointNode(node.rightChild, point, out);
    }
}

template<typename T, unsigned int MaxObjects>
void BVH<T, MaxObjects>::QueryRayNode(
    int nodeIdx,
    const Ray& ray,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    if (out.IsFull()) return;

    const BVHNode& node = mNodes[nodeIdx];

    RaycastHit nodeHit;
    if (!Raycast::CastAARect(ray, node.bounds, nodeHit)) return;

    if (node.leftChild == -1)
    {
        for (int i = node.objectStart; i < node.objectStart + node.objectCount && !out.IsFull(); ++i)
        {
            const int slotIdx = mSortedSlots[i];
            RaycastHit hit;
            if (Raycast::CastAARect(ray, mSlotBounds[slotIdx], hit))
            {
                out.Add(Dia::Core::Handle<T>(static_cast<uint32_t>(slotIdx), mSlots[slotIdx].generation));
            }
        }
    }
    else
    {
        QueryRayNode(node.leftChild,  ray, out);
        QueryRayNode(node.rightChild, ray, out);
    }
}

template<typename T, unsigned int MaxObjects>
int BVH<T, MaxObjects>::ComputeDepthNode(int nodeIdx, int currentDepth) const
{
    const BVHNode& node = mNodes[nodeIdx];
    if (node.leftChild == -1) return currentDepth;

    const int leftDepth  = ComputeDepthNode(node.leftChild,  currentDepth + 1);
    const int rightDepth = ComputeDepthNode(node.rightChild, currentDepth + 1);
    return (leftDepth > rightDepth) ? leftDepth : rightDepth;
}

template<typename T, unsigned int MaxObjects>
int BVH<T, MaxObjects>::CountLeafNodes(int nodeIdx) const
{
    const BVHNode& node = mNodes[nodeIdx];
    if (node.leftChild == -1) return 1;
    return CountLeafNodes(node.leftChild) + CountLeafNodes(node.rightChild);
}

}} // namespace Dia::Geometry2D
