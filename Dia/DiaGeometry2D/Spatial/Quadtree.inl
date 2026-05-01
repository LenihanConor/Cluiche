//==============================================================================
// Quadtree.inl  — template implementation
//==============================================================================

#ifndef DIA_GEOMETRY2D_QUADTREE_H
#error "Do not include Quadtree.inl directly — include Quadtree.h"
#endif

#include <cstring>

namespace Dia { namespace Geometry2D {

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
Quadtree<T, MaxObjects>::Quadtree(const Def& def)
    : mNodeCount(0)
    , mRootNode(0)
    , mSplitThreshold(def.splitThreshold > 0 ? def.splitThreshold : 8)
    , mMaxDepth(def.maxDepth > 0 ? def.maxDepth : 8)
    , mFreeCount(0)
    , mOccupiedCount(0)
{
    // Initialise node pool
    for (int i = 0; i < kMaxNodes; ++i)
    {
        mNodes[i].children[0] = -1;
        mNodes[i].children[1] = -1;
        mNodes[i].children[2] = -1;
        mNodes[i].children[3] = -1;
        mNodes[i].depth  = 0;
        mNodes[i].isLeaf = true;
    }

    // Initialise slot pool
    for (unsigned int i = 0; i < MaxObjects; ++i)
    {
        Slot s;
        s.generation = 0u;
        s.occupied   = false;
        mSlots.Add(s);
        mFreeList[i] = i;
    }
    mFreeCount = static_cast<int>(MaxObjects);

    // Allocate root
    mRootNode = AllocNode();
    InitNode(mRootNode, def.worldBounds, 0);
}

//------------------------------------------------------------------------------
// Insert
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
Dia::Core::Handle<T> Quadtree<T, MaxObjects>::Insert(const T& object, const AARect& bounds)
{
    DIA_ASSERT(mFreeCount > 0, "Quadtree::Insert: slot pool exhausted");

    const uint32_t slotIdx = mFreeList[--mFreeCount];
    Slot& slot = mSlots[slotIdx];
    slot.generation = (slot.generation == 0u) ? 1u : slot.generation + 1u;
    slot.object     = object;
    slot.bounds     = bounds;
    slot.occupied   = true;

    InsertIntoNode(mRootNode, slotIdx);

    ++mOccupiedCount;
    return Dia::Core::Handle<T>(slotIdx, slot.generation);
}

//------------------------------------------------------------------------------
// Remove
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void Quadtree<T, MaxObjects>::Remove(Dia::Core::Handle<T> handle)
{
    DIA_ASSERT(IsHandleValid(handle), "Quadtree::Remove: invalid handle");

    const uint32_t slotIdx = handle.GetIndex();
    Slot& slot = mSlots[slotIdx];

    bool found = false;
    RemoveFromNode(mRootNode, slotIdx, found);
    DIA_ASSERT(found, "Quadtree::Remove: slot not found in tree");

    slot.occupied = false;
    slot.generation++;
    mFreeList[mFreeCount++] = slotIdx;
    --mOccupiedCount;
}

//------------------------------------------------------------------------------
// Update
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void Quadtree<T, MaxObjects>::Update(Dia::Core::Handle<T> handle, const AARect& newBounds)
{
    DIA_ASSERT(IsHandleValid(handle), "Quadtree::Update: invalid handle");

    const uint32_t slotIdx = handle.GetIndex();
    Slot& slot = mSlots[slotIdx];

    // Remove from current position
    bool found = false;
    RemoveFromNode(mRootNode, slotIdx, found);
    DIA_ASSERT(found, "Quadtree::Update: slot not found in tree");

    // Update bounds and re-insert
    slot.bounds = newBounds;
    InsertIntoNode(mRootNode, slotIdx);
}

//------------------------------------------------------------------------------
// Clear
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void Quadtree<T, MaxObjects>::Clear()
{
    // Reset all nodes, keep root
    for (int i = 0; i < mNodeCount; ++i)
    {
        mNodes[i].objectSlots.RemoveAll();
        mNodes[i].children[0] = -1;
        mNodes[i].children[1] = -1;
        mNodes[i].children[2] = -1;
        mNodes[i].children[3] = -1;
        mNodes[i].isLeaf = true;
    }

    // Reclaim all slots
    mFreeCount = 0;
    for (unsigned int i = 0; i < MaxObjects; ++i)
    {
        Slot& slot = mSlots[i];
        if (slot.occupied)
        {
            slot.occupied = false;
            slot.generation++;
        }
        mFreeList[mFreeCount++] = i;
    }
    mOccupiedCount = 0;

    // Reset node count but keep root allocation
    const AARect rootBounds = mNodes[mRootNode].bounds;
    mNodeCount = 0;
    mRootNode  = AllocNode();
    InitNode(mRootNode, rootBounds, 0);
}

//------------------------------------------------------------------------------
// Rebuild
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void Quadtree<T, MaxObjects>::Rebuild()
{
    // Collect all occupied slots
    struct OccupiedSlot { uint32_t idx; };
    OccupiedSlot occupied[MaxObjects];
    int count = 0;
    for (unsigned int i = 0; i < MaxObjects; ++i)
    {
        if (mSlots[i].occupied)
        {
            occupied[count++].idx = i;
        }
    }

    const AARect rootBounds = mNodes[mRootNode].bounds;

    // Reset tree structure
    for (int i = 0; i < mNodeCount; ++i)
    {
        mNodes[i].objectSlots.RemoveAll();
        mNodes[i].children[0] = -1;
        mNodes[i].children[1] = -1;
        mNodes[i].children[2] = -1;
        mNodes[i].children[3] = -1;
        mNodes[i].isLeaf = true;
    }
    mNodeCount = 0;
    mRootNode  = AllocNode();
    InitNode(mRootNode, rootBounds, 0);

    // Re-insert all
    for (int i = 0; i < count; ++i)
    {
        InsertIntoNode(mRootNode, occupied[i].idx);
    }
}

//------------------------------------------------------------------------------
// QueryRegion
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void Quadtree<T, MaxObjects>::QueryRegion(
    const AARect& region,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    bool visited[MaxObjects];
    memset(visited, 0, sizeof(visited));
    QueryRegionNode(mRootNode, region, visited, out);
}

//------------------------------------------------------------------------------
// QueryCircle
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void Quadtree<T, MaxObjects>::QueryCircle(
    const Circle& circle,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    bool visited[MaxObjects];
    memset(visited, 0, sizeof(visited));
    QueryCircleNode(mRootNode, circle, visited, out);
}

//------------------------------------------------------------------------------
// QueryPoint
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void Quadtree<T, MaxObjects>::QueryPoint(
    const Dia::Maths::Vector2D& point,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    bool visited[MaxObjects];
    memset(visited, 0, sizeof(visited));
    QueryPointNode(mRootNode, point, visited, out);
}

//------------------------------------------------------------------------------
// QueryRay
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void Quadtree<T, MaxObjects>::QueryRay(
    const Ray& ray,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    bool visited[MaxObjects];
    memset(visited, 0, sizeof(visited));
    QueryRayNode(mRootNode, ray, visited, out);
}

//------------------------------------------------------------------------------
// QueryKNearest
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void Quadtree<T, MaxObjects>::QueryKNearest(
    const Dia::Maths::Vector2D& point,
    int k,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    if (k <= 0) return;

    struct Candidate
    {
        Dia::Core::Handle<T> handle;
        float                sqDist;
    };

    static constexpr int kMaxCandidates = kMaxQueryResults;
    Candidate candidates[kMaxCandidates];
    int       candidateCount = 0;

    // Collect all objects
    bool visited[MaxObjects];
    memset(visited, 0, sizeof(visited));

    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults> allHandles;
    CollectAllNode(mRootNode, visited, allHandles);

    for (unsigned int i = 0; i < allHandles.Size() && candidateCount < kMaxCandidates; ++i)
    {
        const Dia::Core::Handle<T>& h = allHandles[i];
        if (!IsHandleValid(h)) continue;
        const Slot& slot   = mSlots[h.GetIndex()];
        const Dia::Maths::Vector2D center = slot.bounds.CalculateCenter();
        const float dx = center.x - point.x;
        const float dy = center.y - point.y;
        candidates[candidateCount].handle = h;
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
const T* Quadtree<T, MaxObjects>::Resolve(Dia::Core::Handle<T> handle) const
{
    if (!IsHandleValid(handle)) return nullptr;
    return &mSlots[handle.GetIndex()].object;
}

//------------------------------------------------------------------------------
// GetNodeCount / GetObjectCount / GetDepth
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
int Quadtree<T, MaxObjects>::GetNodeCount() const
{
    return mNodeCount;
}

template<typename T, unsigned int MaxObjects>
int Quadtree<T, MaxObjects>::GetObjectCount() const
{
    return mOccupiedCount;
}

template<typename T, unsigned int MaxObjects>
int Quadtree<T, MaxObjects>::GetDepth() const
{
    return ComputeDepthNode(mRootNode);
}

//==============================================================================
// Private helpers
//==============================================================================

template<typename T, unsigned int MaxObjects>
int Quadtree<T, MaxObjects>::AllocNode()
{
    DIA_ASSERT(mNodeCount < kMaxNodes, "Quadtree::AllocNode: node pool exhausted");
    const int idx = mNodeCount++;
    mNodes[idx].objectSlots.RemoveAll();
    mNodes[idx].children[0] = -1;
    mNodes[idx].children[1] = -1;
    mNodes[idx].children[2] = -1;
    mNodes[idx].children[3] = -1;
    mNodes[idx].depth  = 0;
    mNodes[idx].isLeaf = true;
    return idx;
}

template<typename T, unsigned int MaxObjects>
void Quadtree<T, MaxObjects>::InitNode(int nodeIdx, const AARect& bounds, int depth)
{
    mNodes[nodeIdx].bounds = bounds;
    mNodes[nodeIdx].depth  = depth;
}

template<typename T, unsigned int MaxObjects>
void Quadtree<T, MaxObjects>::Subdivide(int nodeIdx)
{
    QuadNode& node = mNodes[nodeIdx];
    DIA_ASSERT(node.isLeaf, "Quadtree::Subdivide: node is already subdivided");

    const Dia::Maths::Vector2D bl  = node.bounds.GetBottomLeft();
    const Dia::Maths::Vector2D tr  = node.bounds.GetTopRight();
    const Dia::Maths::Vector2D mid = node.bounds.CalculateCenter();

    // Four quadrants: SW, SE, NW, NE
    const AARect childBounds[4] = {
        AARect(bl,                                        mid),                                     // SW
        AARect(Dia::Maths::Vector2D(mid.x, bl.y),        Dia::Maths::Vector2D(tr.x, mid.y)),       // SE
        AARect(Dia::Maths::Vector2D(bl.x, mid.y),        Dia::Maths::Vector2D(mid.x, tr.y)),       // NW
        AARect(mid,                                       tr)                                        // NE
    };

    const int childDepth = node.depth + 1;
    for (int i = 0; i < 4; ++i)
    {
        node.children[i] = AllocNode();
        InitNode(node.children[i], childBounds[i], childDepth);
    }

    // Redistribute objects
    // Collect current slots, clear, re-insert
    uint32_t oldSlots[kMaxSlotsPerNode];
    const int oldCount = static_cast<int>(node.objectSlots.Size());
    for (int i = 0; i < oldCount; ++i)
        oldSlots[i] = node.objectSlots[i];

    node.objectSlots.RemoveAll();
    node.isLeaf = false;

    for (int i = 0; i < oldCount; ++i)
    {
        InsertIntoNode(nodeIdx, oldSlots[i]);
    }
}

template<typename T, unsigned int MaxObjects>
bool Quadtree<T, MaxObjects>::AARectFullyInsideChild(int nodeIdx, int childIdx, const AARect& bounds) const
{
    // Returns true if bounds is fully contained within child node's bounds
    const AARect& childBounds = mNodes[mNodes[nodeIdx].children[childIdx]].bounds;
    return (bounds.GetBottomLeft().x >= childBounds.GetBottomLeft().x &&
            bounds.GetBottomLeft().y >= childBounds.GetBottomLeft().y &&
            bounds.GetTopRight().x   <= childBounds.GetTopRight().x   &&
            bounds.GetTopRight().y   <= childBounds.GetTopRight().y);
}

template<typename T, unsigned int MaxObjects>
void Quadtree<T, MaxObjects>::InsertIntoNode(int nodeIdx, uint32_t slotIdx)
{
    QuadNode& node = mNodes[nodeIdx];
    const AARect& bounds = mSlots[slotIdx].bounds;

    if (node.isLeaf)
    {
        // Add to this node
        if (!node.objectSlots.IsFull())
        {
            node.objectSlots.Add(slotIdx);
        }
        else
        {
            DIA_ASSERT(false, "Quadtree::InsertIntoNode: node slot list full");
            return;
        }

        // Split if over threshold and under max depth
        if (static_cast<int>(node.objectSlots.Size()) > mSplitThreshold
            && node.depth < mMaxDepth)
        {
            Subdivide(nodeIdx);
        }
    }
    else
    {
        // Internal node: try to fit into a single child
        bool placed = false;
        for (int i = 0; i < 4; ++i)
        {
            if (node.children[i] >= 0 && AARectFullyInsideChild(nodeIdx, i, bounds))
            {
                InsertIntoNode(node.children[i], slotIdx);
                placed = true;
                break;
            }
        }
        if (!placed)
        {
            // Spans multiple children: keep in this node
            if (!node.objectSlots.IsFull())
            {
                node.objectSlots.Add(slotIdx);
            }
            else
            {
                DIA_ASSERT(false, "Quadtree::InsertIntoNode: internal node slot list full");
            }
        }
    }
}

template<typename T, unsigned int MaxObjects>
void Quadtree<T, MaxObjects>::RemoveFromNode(int nodeIdx, uint32_t slotIdx, bool& found)
{
    if (found) return;

    QuadNode& node = mNodes[nodeIdx];

    // Search in this node's slots
    for (unsigned int i = 0; i < node.objectSlots.Size(); ++i)
    {
        if (node.objectSlots[i] == slotIdx)
        {
            node.objectSlots.RemoveAt(i);
            found = true;
            return;
        }
    }

    // Recurse into children
    if (!node.isLeaf)
    {
        for (int i = 0; i < 4 && !found; ++i)
        {
            if (node.children[i] >= 0)
            {
                RemoveFromNode(node.children[i], slotIdx, found);
            }
        }
    }
}

template<typename T, unsigned int MaxObjects>
bool Quadtree<T, MaxObjects>::IsHandleValid(Dia::Core::Handle<T> handle) const
{
    if (!handle.IsValid()) return false;
    const uint32_t idx = handle.GetIndex();
    if (idx >= MaxObjects) return false;
    const Slot& slot = mSlots[idx];
    return slot.occupied && slot.generation == handle.GetGeneration();
}

//------------------------------------------------------------------------------
// Query node traversals
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void Quadtree<T, MaxObjects>::QueryRegionNode(
    int nodeIdx,
    const AARect& region,
    bool visited[],
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    if (out.IsFull()) return;

    const QuadNode& node = mNodes[nodeIdx];

    // Prune if this node's bounds don't intersect region
    if (IntersectionTests::IsIntersecting(node.bounds, region).IsNotIntersecting()) return;

    // Test all slots in this node
    for (unsigned int i = 0; i < node.objectSlots.Size() && !out.IsFull(); ++i)
    {
        const uint32_t slotIdx = node.objectSlots[i];
        if (visited[slotIdx]) continue;
        visited[slotIdx] = true;

        const Slot& slot = mSlots[slotIdx];
        if (!slot.occupied) continue;

        if (IntersectionTests::IsIntersecting(slot.bounds, region).IsIntersecting())
        {
            out.Add(Dia::Core::Handle<T>(slotIdx, slot.generation));
        }
    }

    // Recurse into children
    if (!node.isLeaf)
    {
        for (int i = 0; i < 4 && !out.IsFull(); ++i)
        {
            if (node.children[i] >= 0)
                QueryRegionNode(node.children[i], region, visited, out);
        }
    }
}

template<typename T, unsigned int MaxObjects>
void Quadtree<T, MaxObjects>::QueryCircleNode(
    int nodeIdx,
    const Circle& circle,
    bool visited[],
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    if (out.IsFull()) return;

    const QuadNode& node = mNodes[nodeIdx];

    if (IntersectionTests::IsIntersecting(node.bounds, circle).IsNotIntersecting()) return;

    for (unsigned int i = 0; i < node.objectSlots.Size() && !out.IsFull(); ++i)
    {
        const uint32_t slotIdx = node.objectSlots[i];
        if (visited[slotIdx]) continue;
        visited[slotIdx] = true;

        const Slot& slot = mSlots[slotIdx];
        if (!slot.occupied) continue;

        if (IntersectionTests::IsIntersecting(slot.bounds, circle).IsIntersecting())
        {
            out.Add(Dia::Core::Handle<T>(slotIdx, slot.generation));
        }
    }

    if (!node.isLeaf)
    {
        for (int i = 0; i < 4 && !out.IsFull(); ++i)
        {
            if (node.children[i] >= 0)
                QueryCircleNode(node.children[i], circle, visited, out);
        }
    }
}

template<typename T, unsigned int MaxObjects>
void Quadtree<T, MaxObjects>::QueryPointNode(
    int nodeIdx,
    const Dia::Maths::Vector2D& point,
    bool visited[],
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    if (out.IsFull()) return;

    const QuadNode& node = mNodes[nodeIdx];

    if (IntersectionTests::IsIntersecting(point, node.bounds).IsNotIntersecting()) return;

    for (unsigned int i = 0; i < node.objectSlots.Size() && !out.IsFull(); ++i)
    {
        const uint32_t slotIdx = node.objectSlots[i];
        if (visited[slotIdx]) continue;
        visited[slotIdx] = true;

        const Slot& slot = mSlots[slotIdx];
        if (!slot.occupied) continue;

        if (IntersectionTests::IsIntersecting(point, slot.bounds).IsIntersecting())
        {
            out.Add(Dia::Core::Handle<T>(slotIdx, slot.generation));
        }
    }

    if (!node.isLeaf)
    {
        for (int i = 0; i < 4 && !out.IsFull(); ++i)
        {
            if (node.children[i] >= 0)
                QueryPointNode(node.children[i], point, visited, out);
        }
    }
}

template<typename T, unsigned int MaxObjects>
void Quadtree<T, MaxObjects>::QueryRayNode(
    int nodeIdx,
    const Ray& ray,
    bool visited[],
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    if (out.IsFull()) return;

    const QuadNode& node = mNodes[nodeIdx];

    // Prune: does the ray hit this node at all?
    RaycastHit nodeHit;
    if (!Raycast::CastAARect(ray, node.bounds, nodeHit)) return;

    for (unsigned int i = 0; i < node.objectSlots.Size() && !out.IsFull(); ++i)
    {
        const uint32_t slotIdx = node.objectSlots[i];
        if (visited[slotIdx]) continue;
        visited[slotIdx] = true;

        const Slot& slot = mSlots[slotIdx];
        if (!slot.occupied) continue;

        RaycastHit hit;
        if (Raycast::CastAARect(ray, slot.bounds, hit))
        {
            out.Add(Dia::Core::Handle<T>(slotIdx, slot.generation));
        }
    }

    if (!node.isLeaf)
    {
        for (int i = 0; i < 4 && !out.IsFull(); ++i)
        {
            if (node.children[i] >= 0)
                QueryRayNode(node.children[i], ray, visited, out);
        }
    }
}

template<typename T, unsigned int MaxObjects>
void Quadtree<T, MaxObjects>::CollectAllNode(
    int nodeIdx,
    bool visited[],
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    if (out.IsFull()) return;

    const QuadNode& node = mNodes[nodeIdx];

    for (unsigned int i = 0; i < node.objectSlots.Size() && !out.IsFull(); ++i)
    {
        const uint32_t slotIdx = node.objectSlots[i];
        if (visited[slotIdx]) continue;
        visited[slotIdx] = true;

        const Slot& slot = mSlots[slotIdx];
        if (!slot.occupied) continue;

        out.Add(Dia::Core::Handle<T>(slotIdx, slot.generation));
    }

    if (!node.isLeaf)
    {
        for (int i = 0; i < 4 && !out.IsFull(); ++i)
        {
            if (node.children[i] >= 0)
                CollectAllNode(node.children[i], visited, out);
        }
    }
}

template<typename T, unsigned int MaxObjects>
int Quadtree<T, MaxObjects>::ComputeDepthNode(int nodeIdx) const
{
    const QuadNode& node = mNodes[nodeIdx];
    if (node.isLeaf) return node.depth;

    int maxChildDepth = node.depth;
    for (int i = 0; i < 4; ++i)
    {
        if (node.children[i] >= 0)
        {
            const int childDepth = ComputeDepthNode(node.children[i]);
            if (childDepth > maxChildDepth) maxChildDepth = childDepth;
        }
    }
    return maxChildDepth;
}

}} // namespace Dia::Geometry2D
