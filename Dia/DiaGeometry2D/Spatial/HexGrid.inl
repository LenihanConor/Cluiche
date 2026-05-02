//==============================================================================
// HexGrid.inl  — template implementation
//==============================================================================

#ifndef DIA_GEOMETRY2D_HEXGRID_H
#error "Do not include HexGrid.inl directly — include HexGrid.h"
#endif

#include <cmath>
#include <cstring>

namespace Dia { namespace Geometry2D {

//------------------------------------------------------------------------------
// Static neighbour direction tables (pointy-top axial)
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
const int HexGrid<T, MaxObjects>::kNeighbourDQ[6] = { 1,  1,  0, -1, -1,  0 };

template<typename T, unsigned int MaxObjects>
const int HexGrid<T, MaxObjects>::kNeighbourDR[6] = { 0, -1, -1,  0,  1,  1 };

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
HexGrid<T, MaxObjects>::HexGrid(const Def& def)
    : mFreeCount(0)
    , mOccupiedCount(0)
    , mMinQ(0)
    , mMinR(0)
    , mColCount(0)
    , mRowCount(0)
    , mHexRadius(def.hexRadius)
    , mHexWidth(0.0f)
    , mHexHeight(0.0f)
    , mWorldBounds(def.worldBounds)
{
    DIA_ASSERT(def.hexRadius > 0.0f, "HexGrid: hexRadius must be > 0");

    // Pointy-top geometry
    mHexWidth  = 1.7320508075688772f * def.hexRadius; // sqrt(3) * R
    mHexHeight = 2.0f * def.hexRadius;

    // Column spacing = mHexWidth, row spacing = 0.75 * mHexHeight
    const float colSpacing = mHexWidth;
    const float rowSpacing = 0.75f * mHexHeight;

    const float worldWidth  = def.worldBounds.GetTopRight().x - def.worldBounds.GetBottomLeft().x;
    const float worldHeight = def.worldBounds.GetTopRight().y - def.worldBounds.GetBottomLeft().y;

    mColCount = (worldWidth  > 0.0f) ? static_cast<int>(std::ceil(worldWidth  / colSpacing)) + 1 : 1;
    mRowCount = (worldHeight > 0.0f) ? static_cast<int>(std::ceil(worldHeight / rowSpacing)) + 1 : 1;
    if (mColCount < 1) mColCount = 1;
    if (mRowCount < 1) mRowCount = 1;

    DIA_ASSERT(mColCount * mRowCount <= kMaxCells,
        "HexGrid: Too many cells — reduce world size or increase hex radius");

    // Initialise free list (all slots free)
    mFreeCount = static_cast<int>(MaxObjects);
    for (unsigned int i = 0; i < MaxObjects; ++i)
        mFreeList[i] = i;

    // Pre-fill slot array with default-constructed slots
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
Dia::Core::Handle<T> HexGrid<T, MaxObjects>::Insert(const T& object, const AARect& bounds)
{
    DIA_ASSERT(mFreeCount > 0, "HexGrid::Insert: slot pool exhausted");

    const uint32_t slotIdx = mFreeList[--mFreeCount];

    Slot& slot      = mSlots[slotIdx];
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
void HexGrid<T, MaxObjects>::Remove(Dia::Core::Handle<T> handle)
{
    DIA_ASSERT(IsHandleValid(handle), "HexGrid::Remove: invalid handle");

    const uint32_t slotIdx = handle.GetIndex();
    Slot& slot = mSlots[slotIdx];

    RemoveFromCells(slotIdx, slot.bounds);

    slot.occupied = false;
    slot.generation++;

    mFreeList[mFreeCount++] = slotIdx;
    --mOccupiedCount;
}

//------------------------------------------------------------------------------
// Update
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void HexGrid<T, MaxObjects>::Update(Dia::Core::Handle<T> handle, const AARect& newBounds)
{
    DIA_ASSERT(IsHandleValid(handle), "HexGrid::Update: invalid handle");

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
void HexGrid<T, MaxObjects>::Clear()
{
    const int totalCells = mColCount * mRowCount;
    for (int i = 0; i < totalCells; ++i)
        mCells[i].RemoveAll();

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
void HexGrid<T, MaxObjects>::QueryRegion(
    const AARect& region,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    int minQ, minR, maxQ, maxR;
    GetHexRange(region, minQ, minR, maxQ, maxR);

    bool visited[MaxObjects];
    memset(visited, 0, sizeof(visited));

    for (int r = minR; r <= maxR && !out.IsFull(); ++r)
    {
        for (int q = minQ; q <= maxQ && !out.IsFull(); ++q)
        {
            const HexCoord hex = { q, r };
            if (!IsValidHex(hex)) continue;
            const int cellIdx = HexToIndex(hex);
            const auto& cell  = mCells[cellIdx];

            for (unsigned int k = 0; k < cell.Size() && !out.IsFull(); ++k)
            {
                const uint32_t slotIdx = cell[k];
                if (visited[slotIdx]) continue;
                visited[slotIdx] = true;

                const Slot& slot = mSlots[slotIdx];
                if (!slot.occupied) continue;

                if (IntersectionTests::IsIntersecting(region, slot.bounds).IsIntersecting())
                    out.Add(Dia::Core::Handle<T>(slotIdx, slot.generation));
            }
        }
    }
}

//------------------------------------------------------------------------------
// QueryCircle
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void HexGrid<T, MaxObjects>::QueryCircle(
    const Circle& circle,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    const float r = circle.GetRadius();
    const AARect circleBounds(
        Dia::Maths::Vector2D(circle.GetCenter().x - r, circle.GetCenter().y - r),
        Dia::Maths::Vector2D(circle.GetCenter().x + r, circle.GetCenter().y + r));

    int minQ, minR, maxQ, maxR;
    GetHexRange(circleBounds, minQ, minR, maxQ, maxR);

    bool visited[MaxObjects];
    memset(visited, 0, sizeof(visited));

    for (int rv = minR; rv <= maxR && !out.IsFull(); ++rv)
    {
        for (int q = minQ; q <= maxQ && !out.IsFull(); ++q)
        {
            const HexCoord hex = { q, rv };
            if (!IsValidHex(hex)) continue;
            const int cellIdx = HexToIndex(hex);
            const auto& cell  = mCells[cellIdx];

            for (unsigned int k = 0; k < cell.Size() && !out.IsFull(); ++k)
            {
                const uint32_t slotIdx = cell[k];
                if (visited[slotIdx]) continue;
                visited[slotIdx] = true;

                const Slot& slot = mSlots[slotIdx];
                if (!slot.occupied) continue;

                if (IntersectionTests::IsIntersecting(slot.bounds, circle).IsIntersecting())
                    out.Add(Dia::Core::Handle<T>(slotIdx, slot.generation));
            }
        }
    }
}

//------------------------------------------------------------------------------
// QueryPoint
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void HexGrid<T, MaxObjects>::QueryPoint(
    const Dia::Maths::Vector2D& point,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    HexCoord hex;
    if (!WorldToHexClamped(point, hex)) return;

    const int   cellIdx = HexToIndex(hex);
    const auto& cell    = mCells[cellIdx];

    for (unsigned int k = 0; k < cell.Size() && !out.IsFull(); ++k)
    {
        const uint32_t slotIdx = cell[k];
        const Slot& slot = mSlots[slotIdx];
        if (!slot.occupied) continue;

        if (IntersectionTests::IsIntersecting(point, slot.bounds).IsIntersecting())
            out.Add(Dia::Core::Handle<T>(slotIdx, slot.generation));
    }
}

//------------------------------------------------------------------------------
// QueryRay  (hex-DDA traversal along axial directions)
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void HexGrid<T, MaxObjects>::QueryRay(
    const Ray& ray,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    const Dia::Maths::Vector2D& origin = ray.GetOrigin();
    const Dia::Maths::Vector2D& dir    = ray.GetDirection();

    // Starting hex — clamp to valid grid
    HexCoord current;
    if (!WorldToHexClamped(origin, current))
    {
        // Clamp the origin into the world bounds and retry
        const float blX = mWorldBounds.GetBottomLeft().x;
        const float blY = mWorldBounds.GetBottomLeft().y;
        const float trX = mWorldBounds.GetTopRight().x;
        const float trY = mWorldBounds.GetTopRight().y;
        Dia::Maths::Vector2D clamped(
            origin.x < blX ? blX : (origin.x > trX ? trX : origin.x),
            origin.y < blY ? blY : (origin.y > trY ? trY : origin.y));
        if (!WorldToHexClamped(clamped, current)) return;
    }

    bool visited[MaxObjects];
    memset(visited, 0, sizeof(visited));

    // We do a simple step-based DDA: from the current hex center, determine
    // which of the 6 neighbours the ray direction most closely points toward,
    // then iterate up to kMaxCells steps.
    static constexpr int kMaxSteps = kMaxCells;

    for (int step = 0; step < kMaxSteps && IsValidHex(current); ++step)
    {
        // Collect objects in this cell
        if (!out.IsFull())
        {
            const int cellIdx = HexToIndex(current);
            const auto& cell  = mCells[cellIdx];

            for (unsigned int k = 0; k < cell.Size() && !out.IsFull(); ++k)
            {
                const uint32_t slotIdx = cell[k];
                if (visited[slotIdx]) continue;
                visited[slotIdx] = true;

                const Slot& slot = mSlots[slotIdx];
                if (!slot.occupied) continue;

                RaycastHit hit;
                if (Raycast::CastAARect(ray, slot.bounds, hit))
                    out.Add(Dia::Core::Handle<T>(slotIdx, slot.generation));
            }
        }

        // Determine next hex: pick the neighbour whose center is nearest to
        // (currentCenter + dir * smallStep), i.e., the direction of travel.
        const Dia::Maths::Vector2D currentCenter = HexToWorld(current);
        const Dia::Maths::Vector2D target(
            currentCenter.x + dir.x * mHexWidth,
            currentCenter.y + dir.y * mHexHeight);

        // Find the neighbour closest to the target
        int bestNeighbour = 0;
        float bestDist2 = 1e30f;
        for (int n = 0; n < 6; ++n)
        {
            const HexCoord neighbour = { current.q + kNeighbourDQ[n],
                                         current.r + kNeighbourDR[n] };
            if (!IsValidHex(neighbour)) continue;
            const Dia::Maths::Vector2D nc = HexToWorld(neighbour);
            const float dx = nc.x - target.x;
            const float dy = nc.y - target.y;
            const float d2 = dx * dx + dy * dy;
            if (d2 < bestDist2)
            {
                bestDist2    = d2;
                bestNeighbour = n;
            }
        }

        current = { current.q + kNeighbourDQ[bestNeighbour],
                    current.r + kNeighbourDR[bestNeighbour] };

        // Stop if we've moved off the grid
        if (!IsValidHex(current)) break;

        // Stop if the ray has clearly passed the current center (dot product negative)
        const Dia::Maths::Vector2D newCenter = HexToWorld(current);
        const float dx = newCenter.x - origin.x;
        const float dy = newCenter.y - origin.y;
        if ((dx * dir.x + dy * dir.y) < 0.0f) break;
    }
}

//------------------------------------------------------------------------------
// QueryKNearest
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
void HexGrid<T, MaxObjects>::QueryKNearest(
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

    // Expanding ring search outward from the hex containing the point
    HexCoord center;
    if (!WorldToHexClamped(point, center))
        return;

    bool visited[MaxObjects];
    memset(visited, 0, sizeof(visited));

    // Ring 0 (just the center) then ring 1, 2, ... until we have k results.
    // Cap at a generous ring radius to avoid infinite loop.
    const int kMaxRingRadius = (mColCount > mRowCount ? mColCount : mRowCount) + 1;

    for (int ringR = 0; ringR <= kMaxRingRadius && candidateCount < kMaxCandidates; ++ringR)
    {
        if (ringR == 0)
        {
            // Just the center hex
            if (IsValidHex(center))
            {
                const int cellIdx = HexToIndex(center);
                const auto& cell  = mCells[cellIdx];
                for (unsigned int ki = 0; ki < cell.Size() && candidateCount < kMaxCandidates; ++ki)
                {
                    const uint32_t slotIdx = cell[ki];
                    if (visited[slotIdx]) continue;
                    visited[slotIdx] = true;
                    const Slot& slot = mSlots[slotIdx];
                    if (!slot.occupied) continue;
                    const Dia::Maths::Vector2D c = slot.bounds.CalculateCenter();
                    const float dx = c.x - point.x;
                    const float dy = c.y - point.y;
                    candidates[candidateCount++] = { Dia::Core::Handle<T>(slotIdx, slot.generation), dx*dx + dy*dy };
                }
            }
        }
        else
        {
            // Walk around the ring using the 6 axial directions
            // Ring traversal: start at direction 4 offset, then walk 6 sides
            HexCoord hex = { center.q + kNeighbourDQ[4] * ringR,
                             center.r + kNeighbourDR[4] * ringR };
            for (int side = 0; side < 6; ++side)
            {
                for (int step = 0; step < ringR; ++step)
                {
                    if (IsValidHex(hex))
                    {
                        const int cellIdx = HexToIndex(hex);
                        const auto& cell  = mCells[cellIdx];
                        for (unsigned int ki = 0; ki < cell.Size() && candidateCount < kMaxCandidates; ++ki)
                        {
                            const uint32_t slotIdx = cell[ki];
                            if (visited[slotIdx]) continue;
                            visited[slotIdx] = true;
                            const Slot& slot = mSlots[slotIdx];
                            if (!slot.occupied) continue;
                            const Dia::Maths::Vector2D c = slot.bounds.CalculateCenter();
                            const float dx = c.x - point.x;
                            const float dy = c.y - point.y;
                            candidates[candidateCount++] = { Dia::Core::Handle<T>(slotIdx, slot.generation), dx*dx + dy*dy };
                        }
                    }
                    hex = { hex.q + kNeighbourDQ[side], hex.r + kNeighbourDR[side] };
                }
            }
        }

        // Early exit if we have enough candidates and the ring is beyond the farthest
        if (candidateCount >= k) break;
    }

    // Partial selection sort to pick k nearest
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
            Candidate tmp        = candidates[i];
            candidates[i]        = candidates[minIdx];
            candidates[minIdx]   = tmp;
        }
        out.Add(candidates[i].handle);
    }
}

//------------------------------------------------------------------------------
// Resolve
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
const T* HexGrid<T, MaxObjects>::Resolve(Dia::Core::Handle<T> handle) const
{
    if (!IsHandleValid(handle)) return nullptr;
    return &mSlots[handle.GetIndex()].object;
}

//==============================================================================
// Hex-specific API
//==============================================================================

template<typename T, unsigned int MaxObjects>
HexCoord HexGrid<T, MaxObjects>::WorldToHex(const Dia::Maths::Vector2D& worldPos) const
{
    // Pointy-top axial conversion
    const float blX = mWorldBounds.GetBottomLeft().x;
    const float blY = mWorldBounds.GetBottomLeft().y;
    const float lx  = worldPos.x - blX;
    const float ly  = worldPos.y - blY;

    // Fractional axial coordinates
    const float fq = (lx * 1.7320508075688772f / 3.0f - ly / 3.0f) / mHexRadius;
    const float fr = ly * 2.0f / (3.0f * mHexRadius);

    // Cube rounding
    float fs = -fq - fr;
    int q = static_cast<int>(std::round(fq));
    int r = static_cast<int>(std::round(fr));
    int s = static_cast<int>(std::round(fs));

    const float dq = std::abs(static_cast<float>(q) - fq);
    const float dr = std::abs(static_cast<float>(r) - fr);
    const float ds = std::abs(static_cast<float>(s) - fs);

    if (dq > dr && dq > ds)      q = -r - s;
    else if (dr > ds)            r = -q - s;

    return { q + mMinQ, r + mMinR };
}

template<typename T, unsigned int MaxObjects>
Dia::Maths::Vector2D HexGrid<T, MaxObjects>::HexToWorld(HexCoord hex) const
{
    const float blX = mWorldBounds.GetBottomLeft().x;
    const float blY = mWorldBounds.GetBottomLeft().y;

    const int lq = hex.q - mMinQ;
    const int lr = hex.r - mMinR;

    const float x = blX + mHexRadius * (1.7320508075688772f * static_cast<float>(lq)
                                       + 1.7320508075688772f / 2.0f * static_cast<float>(lr));
    const float y = blY + mHexRadius * (1.5f * static_cast<float>(lr));
    return Dia::Maths::Vector2D(x, y);
}

template<typename T, unsigned int MaxObjects>
bool HexGrid<T, MaxObjects>::IsValidHex(HexCoord hex) const
{
    const int lq = hex.q - mMinQ;
    const int lr = hex.r - mMinR;
    return lq >= 0 && lq < mColCount && lr >= 0 && lr < mRowCount;
}

template<typename T, unsigned int MaxObjects>
void HexGrid<T, MaxObjects>::GetNeighbours(
    HexCoord hex,
    Dia::Core::Containers::DynamicArrayC<HexCoord, 6>& out) const
{
    out.RemoveAll();
    for (int n = 0; n < 6; ++n)
    {
        const HexCoord neighbour = { hex.q + kNeighbourDQ[n], hex.r + kNeighbourDR[n] };
        if (IsValidHex(neighbour))
            out.Add(neighbour);
    }
}

template<typename T, unsigned int MaxObjects>
int HexGrid<T, MaxObjects>::HexDistance(HexCoord a, HexCoord b) const
{
    const int dq = a.q - b.q;
    const int dr = a.r - b.r;
    const int ds = (-a.q - a.r) - (-b.q - b.r);
    const int aq = dq < 0 ? -dq : dq;
    const int ar = dr < 0 ? -dr : dr;
    const int as_ = ds < 0 ? -ds : ds;
    return (aq + ar + as_) / 2;
}

template<typename T, unsigned int MaxObjects>
void HexGrid<T, MaxObjects>::GetRing(
    HexCoord center,
    int radius,
    Dia::Core::Containers::DynamicArrayC<HexCoord, 256>& out) const
{
    out.RemoveAll();
    if (radius == 0)
    {
        if (IsValidHex(center) && !out.IsFull())
            out.Add(center);
        return;
    }

    HexCoord hex = { center.q + kNeighbourDQ[4] * radius,
                     center.r + kNeighbourDR[4] * radius };

    for (int side = 0; side < 6 && !out.IsFull(); ++side)
    {
        for (int step = 0; step < radius && !out.IsFull(); ++step)
        {
            if (IsValidHex(hex))
                out.Add(hex);
            hex = { hex.q + kNeighbourDQ[side], hex.r + kNeighbourDR[side] };
        }
    }
}

template<typename T, unsigned int MaxObjects>
void HexGrid<T, MaxObjects>::QueryHex(
    HexCoord hex,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    if (!IsValidHex(hex)) return;

    const int   cellIdx = HexToIndex(hex);
    const auto& cell    = mCells[cellIdx];

    for (unsigned int k = 0; k < cell.Size() && !out.IsFull(); ++k)
    {
        const uint32_t slotIdx = cell[k];
        const Slot& slot = mSlots[slotIdx];
        if (!slot.occupied) continue;
        out.Add(Dia::Core::Handle<T>(slotIdx, slot.generation));
    }
}

template<typename T, unsigned int MaxObjects>
int HexGrid<T, MaxObjects>::GetCellCount() const
{
    return mColCount * mRowCount;
}

template<typename T, unsigned int MaxObjects>
int HexGrid<T, MaxObjects>::GetObjectCount() const
{
    return mOccupiedCount;
}

//==============================================================================
// Private helpers
//==============================================================================

template<typename T, unsigned int MaxObjects>
int HexGrid<T, MaxObjects>::HexToIndex(HexCoord hex) const
{
    return (hex.q - mMinQ) * mRowCount + (hex.r - mMinR);
}

template<typename T, unsigned int MaxObjects>
bool HexGrid<T, MaxObjects>::WorldToHexClamped(
    const Dia::Maths::Vector2D& pos, HexCoord& out) const
{
    out = WorldToHex(pos);
    if (!IsValidHex(out)) return false;
    return true;
}

template<typename T, unsigned int MaxObjects>
void HexGrid<T, MaxObjects>::GetHexRange(
    const AARect& bounds,
    int& minQ, int& minR, int& maxQ, int& maxR) const
{
    const HexCoord bl = WorldToHex(bounds.GetBottomLeft());
    const HexCoord tr = WorldToHex(bounds.GetTopRight());
    const HexCoord br = WorldToHex(Dia::Maths::Vector2D(bounds.GetTopRight().x,  bounds.GetBottomLeft().y));
    const HexCoord tl = WorldToHex(Dia::Maths::Vector2D(bounds.GetBottomLeft().x, bounds.GetTopRight().y));

    minQ = bl.q;
    maxQ = bl.q;
    minR = bl.r;
    maxR = bl.r;

    auto expandQ = [&](int q) { if (q < minQ) minQ = q; if (q > maxQ) maxQ = q; };
    auto expandR = [&](int r) { if (r < minR) minR = r; if (r > maxR) maxR = r; };
    expandQ(tr.q); expandR(tr.r);
    expandQ(br.q); expandR(br.r);
    expandQ(tl.q); expandR(tl.r);

    // Clamp to valid grid
    if (minQ < mMinQ)             minQ = mMinQ;
    if (minR < mMinR)             minR = mMinR;
    if (maxQ >= mMinQ + mColCount) maxQ = mMinQ + mColCount - 1;
    if (maxR >= mMinR + mRowCount) maxR = mMinR + mRowCount - 1;
}

template<typename T, unsigned int MaxObjects>
bool HexGrid<T, MaxObjects>::IsHandleValid(Dia::Core::Handle<T> handle) const
{
    if (!handle.IsValid()) return false;
    const uint32_t idx = handle.GetIndex();
    if (idx >= MaxObjects) return false;
    const Slot& slot = mSlots[idx];
    return slot.occupied && slot.generation == handle.GetGeneration();
}

template<typename T, unsigned int MaxObjects>
void HexGrid<T, MaxObjects>::RemoveFromCells(uint32_t slotIdx, const AARect& bounds)
{
    int minQ, minR, maxQ, maxR;
    GetHexRange(bounds, minQ, minR, maxQ, maxR);

    for (int r = minR; r <= maxR; ++r)
    {
        for (int q = minQ; q <= maxQ; ++q)
        {
            const HexCoord hex = { q, r };
            if (!IsValidHex(hex)) continue;
            auto& cell = mCells[HexToIndex(hex)];

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
void HexGrid<T, MaxObjects>::InsertIntoCells(uint32_t slotIdx, const AARect& bounds)
{
    int minQ, minR, maxQ, maxR;
    GetHexRange(bounds, minQ, minR, maxQ, maxR);

    for (int r = minR; r <= maxR; ++r)
    {
        for (int q = minQ; q <= maxQ; ++q)
        {
            const HexCoord hex = { q, r };
            if (!IsValidHex(hex)) continue;
            auto& cell = mCells[HexToIndex(hex)];

            if (!cell.IsFull())
                cell.Add(slotIdx);
            else
                DIA_ASSERT(false, "HexGrid::InsertIntoCells: cell is full (increase kMaxObjectsPerCell)");
        }
    }
}

template<typename T, unsigned int MaxObjects>
void HexGrid<T, MaxObjects>::QueryCellList(
    const int* cellIndices, int cellCount,
    bool visited[],
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
{
    for (int i = 0; i < cellCount && !out.IsFull(); ++i)
    {
        const auto& cell = mCells[cellIndices[i]];
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

}} // namespace Dia::Geometry2D
