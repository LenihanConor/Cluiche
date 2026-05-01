//==============================================================================
// SpatialGrid.inl  — template implementation
//==============================================================================

#ifndef DIA_GEOMETRY2D_SPATIALGRID_H
#error "Do not include SpatialGrid.inl directly — include SpatialGrid.h"
#endif

#include <cmath>
#include <cstring>

namespace Dia { namespace Geometry2D {

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
SpatialGrid<T, MaxObjects>::SpatialGrid(const Def& def)
    : mFreeCount(0)
    , mOccupiedCount(0)
    , mCellCountX(0)
    , mCellCountY(0)
    , mCellSize(def.cellSize)
    , mWorldBounds(def.worldBounds)
{
    DIA_ASSERT(def.cellSize > 0.0f, "SpatialGrid: cellSize must be > 0");

    const float worldWidth  = def.worldBounds.GetTopRight().x - def.worldBounds.GetBottomLeft().x;
    const float worldHeight = def.worldBounds.GetTopRight().y - def.worldBounds.GetBottomLeft().y;

    mCellCountX = (worldWidth  > 0.0f) ? static_cast<int>(std::ceil(worldWidth  / def.cellSize)) : 1;
    mCellCountY = (worldHeight > 0.0f) ? static_cast<int>(std::ceil(worldHeight / def.cellSize)) : 1;
    if (mCellCountX < 1) mCellCountX = 1;
    if (mCellCountY < 1) mCellCountY = 1;

    DIA_ASSERT(mCellCountX * mCellCountY <= kMaxCells, "SpatialGrid: Too many cells — reduce world size or increase cell size");

    // Initialise free list (all slots free, highest index on top)
    mFreeCount = static_cast<int>(MaxObjects);
    for (unsigned int i = 0; i < MaxObjects; ++i)
    {
        mFreeList[i] = i;
    }

    // Pre-fill slot array with default-constructed slots marked unoccupied
    for (unsigned int i = 0; i < MaxObjects; ++i)
    {
        Slot s;
        s.generation = 0u;
        s.occupied   = false;
        mSlots.Add(s);
    }
}

//------------------------------------------------------------------------------
// Insert
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
Dia::Core::Handle<T> SpatialGrid<T, MaxObjects>::Insert(const T& object, const AARect& bounds)
{
    DIA_ASSERT(mFreeCount > 0, "SpatialGrid::Insert: slot pool exhausted");

    // Pop a free slot index
    const uint32_t slotIdx = mFreeList[--mFreeCount];

    Slot& slot = mSlots[slotIdx];
    // Bump generation (never 0 — 0 is the invalid sentinel)
    slot.generation = (slot.generation == 0u) ? 1u : slot.generation + 1u;
    slot.object     = object;
    slot.bounds     = bounds;
    slot.occupied   = true;

    InsertIntoCells(slotIdx, bounds);

    ++mOccupiedCount;
    return Dia::Core::Handle<T>(slotIdx, slot.generation);
}

//------------------------------------------------------------------------------
// Remove
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void SpatialGrid<T, MaxObjects>::Remove(Dia::Core::Handle<T> handle)
{
    DIA_ASSERT(IsHandleValid(handle), "SpatialGrid::Remove: invalid handle");

    const uint32_t slotIdx = handle.GetIndex();
    Slot& slot = mSlots[slotIdx];

    RemoveFromCells(slotIdx, slot.bounds);

    slot.occupied = false;
    slot.generation++;  // invalidate outstanding handles

    mFreeList[mFreeCount++] = slotIdx;
    --mOccupiedCount;
}

//------------------------------------------------------------------------------
// Update
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void SpatialGrid<T, MaxObjects>::Update(Dia::Core::Handle<T> handle, const AARect& newBounds)
{
    DIA_ASSERT(IsHandleValid(handle), "SpatialGrid::Update: invalid handle");

    const uint32_t slotIdx = handle.GetIndex();
    Slot& slot = mSlots[slotIdx];

    RemoveFromCells(slotIdx, slot.bounds);
    slot.bounds = newBounds;
    InsertIntoCells(slotIdx, newBounds);
}

//------------------------------------------------------------------------------
// Clear
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void SpatialGrid<T, MaxObjects>::Clear()
{
    // Clear all cells
    const int totalCells = mCellCountX * mCellCountY;
    for (int i = 0; i < totalCells; ++i)
    {
        mCells[i].RemoveAll();
    }

    // Invalidate all occupied slots and rebuild free list
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
}

//------------------------------------------------------------------------------
// QueryRegion
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void SpatialGrid<T, MaxObjects>::QueryRegion(
    const AARect& region,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    int minCx, minCy, maxCx, maxCy;
    GetCellRange(region, minCx, minCy, maxCx, maxCy);

    bool visited[MaxObjects];
    memset(visited, 0, sizeof(visited));

    for (int cy = minCy; cy <= maxCy && !out.IsFull(); ++cy)
    {
        for (int cx = minCx; cx <= maxCx && !out.IsFull(); ++cx)
        {
            const int cellIdx = cy * mCellCountX + cx;
            const auto& cell  = mCells[cellIdx];

            for (unsigned int k = 0; k < cell.Size() && !out.IsFull(); ++k)
            {
                const uint32_t slotIdx = cell[k];
                if (visited[slotIdx]) continue;
                visited[slotIdx] = true;

                const Slot& slot = mSlots[slotIdx];
                if (!slot.occupied) continue;

                // Check region overlaps slot bounds
                if (IntersectionTests::IsIntersecting(region, slot.bounds).IsIntersecting())
                {
                    out.Add(Dia::Core::Handle<T>(slotIdx, slot.generation));
                }
            }
        }
    }
}

//------------------------------------------------------------------------------
// QueryCircle
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void SpatialGrid<T, MaxObjects>::QueryCircle(
    const Circle& circle,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    // Build AABB for the circle and query those cells
    const float r  = circle.GetRadius();
    const AARect circleBounds(
        Dia::Maths::Vector2D(circle.GetCenter().x - r, circle.GetCenter().y - r),
        Dia::Maths::Vector2D(circle.GetCenter().x + r, circle.GetCenter().y + r));

    int minCx, minCy, maxCx, maxCy;
    GetCellRange(circleBounds, minCx, minCy, maxCx, maxCy);

    bool visited[MaxObjects];
    memset(visited, 0, sizeof(visited));

    for (int cy = minCy; cy <= maxCy && !out.IsFull(); ++cy)
    {
        for (int cx = minCx; cx <= maxCx && !out.IsFull(); ++cx)
        {
            const int  cellIdx = cy * mCellCountX + cx;
            const auto& cell   = mCells[cellIdx];

            for (unsigned int k = 0; k < cell.Size() && !out.IsFull(); ++k)
            {
                const uint32_t slotIdx = cell[k];
                if (visited[slotIdx]) continue;
                visited[slotIdx] = true;

                const Slot& slot = mSlots[slotIdx];
                if (!slot.occupied) continue;

                if (IntersectionTests::IsIntersecting(slot.bounds, circle).IsIntersecting())
                {
                    out.Add(Dia::Core::Handle<T>(slotIdx, slot.generation));
                }
            }
        }
    }
}

//------------------------------------------------------------------------------
// QueryPoint
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void SpatialGrid<T, MaxObjects>::QueryPoint(
    const Dia::Maths::Vector2D& point,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    int cx, cy;
    if (!WorldToCellClamped(point.x, point.y, cx, cy)) return;

    const int  cellIdx = cy * mCellCountX + cx;
    const auto& cell   = mCells[cellIdx];

    for (unsigned int k = 0; k < cell.Size() && !out.IsFull(); ++k)
    {
        const uint32_t slotIdx = cell[k];
        const Slot& slot = mSlots[slotIdx];
        if (!slot.occupied) continue;

        if (IntersectionTests::IsIntersecting(point, slot.bounds).IsIntersecting())
        {
            out.Add(Dia::Core::Handle<T>(slotIdx, slot.generation));
        }
    }
}

//------------------------------------------------------------------------------
// QueryRay  (DDA grid traversal)
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void SpatialGrid<T, MaxObjects>::QueryRay(
    const Ray& ray,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    const Dia::Maths::Vector2D& origin = ray.GetOrigin();
    const Dia::Maths::Vector2D& dir    = ray.GetDirection();

    // Starting cell (clamp into grid)
    int cx, cy;
    // Use the origin clamped into grid for start
    const float ox = origin.x - mWorldBounds.GetBottomLeft().x;
    const float oy = origin.y - mWorldBounds.GetBottomLeft().y;

    cx = static_cast<int>(ox / mCellSize);
    cy = static_cast<int>(oy / mCellSize);
    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;
    if (cx >= mCellCountX) cx = mCellCountX - 1;
    if (cy >= mCellCountY) cy = mCellCountY - 1;

    // DDA step direction
    const int stepX = (dir.x >= 0.0f) ? 1 : -1;
    const int stepY = (dir.y >= 0.0f) ? 1 : -1;

    // Distance to first cell boundary
    const float cellW = mCellSize;
    const float cellH = mCellSize;
    const float blX   = mWorldBounds.GetBottomLeft().x;
    const float blY   = mWorldBounds.GetBottomLeft().y;

    float tMaxX, tMaxY, tDeltaX, tDeltaY;

    if (dir.x != 0.0f)
    {
        tDeltaX = cellW / (dir.x < 0.0f ? -dir.x : dir.x);
        const float nextBoundaryX = blX + (cx + (stepX > 0 ? 1 : 0)) * cellW;
        tMaxX = (nextBoundaryX - origin.x) / dir.x;
    }
    else
    {
        tDeltaX = 1e30f;
        tMaxX   = 1e30f;
    }

    if (dir.y != 0.0f)
    {
        tDeltaY = cellH / (dir.y < 0.0f ? -dir.y : dir.y);
        const float nextBoundaryY = blY + (cy + (stepY > 0 ? 1 : 0)) * cellH;
        tMaxY = (nextBoundaryY - origin.y) / dir.y;
    }
    else
    {
        tDeltaY = 1e30f;
        tMaxY   = 1e30f;
    }

    bool visited[MaxObjects];
    memset(visited, 0, sizeof(visited));

    // Traverse until we leave the grid
    while (cx >= 0 && cx < mCellCountX && cy >= 0 && cy < mCellCountY)
    {
        if (!out.IsFull())
        {
            const int  cellIdx = cy * mCellCountX + cx;
            const auto& cell   = mCells[cellIdx];

            for (unsigned int k = 0; k < cell.Size() && !out.IsFull(); ++k)
            {
                const uint32_t slotIdx = cell[k];
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
        }

        // Advance to next cell
        if (tMaxX < tMaxY)
        {
            tMaxX += tDeltaX;
            cx    += stepX;
        }
        else
        {
            tMaxY += tDeltaY;
            cy    += stepY;
        }
    }
}

//------------------------------------------------------------------------------
// QueryKNearest
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void SpatialGrid<T, MaxObjects>::QueryKNearest(
    const Dia::Maths::Vector2D& point,
    int k,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    if (k <= 0) return;

    // Gather all candidates via a wide QueryRegion then sort by distance
    // Expand search radius incrementally until we have enough candidates.
    // Simple approach: query the entire world region, filter, sort by sq-dist.

    // Gather candidates into a flat array with sq-dist
    struct Candidate
    {
        Dia::Core::Handle<T> handle;
        float                sqDist;
    };

    static constexpr int kMaxCandidates = kMaxQueryResults;
    Candidate candidates[kMaxCandidates];
    int       candidateCount = 0;

    // Full world query for candidates
    bool visited[MaxObjects];
    memset(visited, 0, sizeof(visited));

    const int totalCells = mCellCountX * mCellCountY;
    for (int cellIdx = 0; cellIdx < totalCells && candidateCount < kMaxCandidates; ++cellIdx)
    {
        const auto& cell = mCells[cellIdx];
        for (unsigned int k2 = 0; k2 < cell.Size() && candidateCount < kMaxCandidates; ++k2)
        {
            const uint32_t slotIdx = cell[k2];
            if (visited[slotIdx]) continue;
            visited[slotIdx] = true;

            const Slot& slot = mSlots[slotIdx];
            if (!slot.occupied) continue;

            const Dia::Maths::Vector2D center = slot.bounds.CalculateCenter();
            const float dx = center.x - point.x;
            const float dy = center.y - point.y;
            const float sq = dx * dx + dy * dy;

            candidates[candidateCount].handle = Dia::Core::Handle<T>(slotIdx, slot.generation);
            candidates[candidateCount].sqDist = sq;
            ++candidateCount;
        }
    }

    // Partial selection sort to pick the k nearest
    const int resultCount = (k < candidateCount) ? k : candidateCount;
    for (int i = 0; i < resultCount && !out.IsFull(); ++i)
    {
        // Find min from [i, candidateCount)
        int minIdx = i;
        for (int j = i + 1; j < candidateCount; ++j)
        {
            if (candidates[j].sqDist < candidates[minIdx].sqDist)
                minIdx = j;
        }
        // Swap
        if (minIdx != i)
        {
            Candidate tmp       = candidates[i];
            candidates[i]       = candidates[minIdx];
            candidates[minIdx]  = tmp;
        }
        out.Add(candidates[i].handle);
    }
}

//------------------------------------------------------------------------------
// Resolve
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
const T* SpatialGrid<T, MaxObjects>::Resolve(Dia::Core::Handle<T> handle) const
{
    if (!IsHandleValid(handle)) return nullptr;
    return &mSlots[handle.GetIndex()].object;
}

//------------------------------------------------------------------------------
// GetCellCount / GetObjectCount
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
int SpatialGrid<T, MaxObjects>::GetCellCount() const
{
    return mCellCountX * mCellCountY;
}

template<typename T, unsigned int MaxObjects>
int SpatialGrid<T, MaxObjects>::GetObjectCount() const
{
    return mOccupiedCount;
}

//==============================================================================
// Private helpers
//==============================================================================

template<typename T, unsigned int MaxObjects>
bool SpatialGrid<T, MaxObjects>::WorldToCellClamped(float x, float y, int& cx, int& cy) const
{
    const float blX = mWorldBounds.GetBottomLeft().x;
    const float blY = mWorldBounds.GetBottomLeft().y;
    cx = static_cast<int>((x - blX) / mCellSize);
    cy = static_cast<int>((y - blY) / mCellSize);
    if (cx < 0 || cx >= mCellCountX || cy < 0 || cy >= mCellCountY) return false;
    return true;
}

template<typename T, unsigned int MaxObjects>
void SpatialGrid<T, MaxObjects>::GetCellRange(
    const AARect& bounds, int& minCx, int& minCy, int& maxCx, int& maxCy) const
{
    const float blX = mWorldBounds.GetBottomLeft().x;
    const float blY = mWorldBounds.GetBottomLeft().y;

    int rawMinX = static_cast<int>((bounds.GetBottomLeft().x - blX) / mCellSize);
    int rawMinY = static_cast<int>((bounds.GetBottomLeft().y - blY) / mCellSize);
    int rawMaxX = static_cast<int>((bounds.GetTopRight().x   - blX) / mCellSize);
    int rawMaxY = static_cast<int>((bounds.GetTopRight().y   - blY) / mCellSize);

    minCx = (rawMinX < 0)               ? 0               : rawMinX;
    minCy = (rawMinY < 0)               ? 0               : rawMinY;
    maxCx = (rawMaxX >= mCellCountX)    ? mCellCountX - 1 : rawMaxX;
    maxCy = (rawMaxY >= mCellCountY)    ? mCellCountY - 1 : rawMaxY;
}

template<typename T, unsigned int MaxObjects>
bool SpatialGrid<T, MaxObjects>::IsHandleValid(Dia::Core::Handle<T> handle) const
{
    if (!handle.IsValid()) return false;
    const uint32_t idx = handle.GetIndex();
    if (idx >= MaxObjects) return false;
    const Slot& slot = mSlots[idx];
    return slot.occupied && slot.generation == handle.GetGeneration();
}

template<typename T, unsigned int MaxObjects>
void SpatialGrid<T, MaxObjects>::RemoveFromCells(uint32_t slotIdx, const AARect& bounds)
{
    int minCx, minCy, maxCx, maxCy;
    GetCellRange(bounds, minCx, minCy, maxCx, maxCy);

    for (int cy = minCy; cy <= maxCy; ++cy)
    {
        for (int cx = minCx; cx <= maxCx; ++cx)
        {
            const int  cellIdx = cy * mCellCountX + cx;
            auto&      cell    = mCells[cellIdx];

            for (unsigned int k = 0; k < cell.Size(); ++k)
            {
                if (cell[k] == slotIdx)
                {
                    cell.RemoveAt(k);
                    break;
                }
            }
        }
    }
}

template<typename T, unsigned int MaxObjects>
void SpatialGrid<T, MaxObjects>::InsertIntoCells(uint32_t slotIdx, const AARect& bounds)
{
    int minCx, minCy, maxCx, maxCy;
    GetCellRange(bounds, minCx, minCy, maxCx, maxCy);

    for (int cy = minCy; cy <= maxCy; ++cy)
    {
        for (int cx = minCx; cx <= maxCx; ++cx)
        {
            const int cellIdx = cy * mCellCountX + cx;
            auto&     cell    = mCells[cellIdx];

            if (!cell.IsFull())
            {
                cell.Add(slotIdx);
            }
            else
            {
                DIA_ASSERT(false, "SpatialGrid::InsertIntoCells: cell is full (increase kMaxObjectsPerCell)");
            }
        }
    }
}

template<typename T, unsigned int MaxObjects>
void SpatialGrid<T, MaxObjects>::QueryCellRange(
    int minCx, int minCy, int maxCx, int maxCy,
    bool visited[],
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    for (int cy = minCy; cy <= maxCy && !out.IsFull(); ++cy)
    {
        for (int cx = minCx; cx <= maxCx && !out.IsFull(); ++cx)
        {
            const int  cellIdx = cy * mCellCountX + cx;
            const auto& cell   = mCells[cellIdx];

            for (unsigned int k = 0; k < cell.Size() && !out.IsFull(); ++k)
            {
                const uint32_t slotIdx = cell[k];
                if (visited[slotIdx]) continue;
                visited[slotIdx] = true;

                const Slot& slot = mSlots[slotIdx];
                if (!slot.occupied) continue;

                out.Add(Dia::Core::Handle<T>(slotIdx, slot.generation));
            }
        }
    }
}

}} // namespace Dia::Geometry2D
