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
// Static direction tables — pointy-top, odd-row-right stagger
//
// Neighbours in reading order (E, NE, NW, W, SW, SE).
// Even rows (r % 2 == 0):  odd rows shift one column right, so NE/SE point
// at q+0 whereas on odd rows they point at q+1.
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
const int HexGrid<T, MaxObjects>::kEvenDQ[6] = {  1,  0, -1, -1, -1,  0 };
template<typename T, unsigned int MaxObjects>
const int HexGrid<T, MaxObjects>::kEvenDR[6] = {  0,  1,  1,  0, -1, -1 };

template<typename T, unsigned int MaxObjects>
const int HexGrid<T, MaxObjects>::kOddDQ[6]  = {  1,  1,  0, -1,  0,  1 };
template<typename T, unsigned int MaxObjects>
const int HexGrid<T, MaxObjects>::kOddDR[6]  = {  0,  1,  1,  0, -1, -1 };

// Cube-coord directions (used by HexDistance / GetRing which work in cube space)
template<typename T, unsigned int MaxObjects>
const int HexGrid<T, MaxObjects>::kCubeDQ[6] = {  1,  1,  0, -1, -1,  0 };
template<typename T, unsigned int MaxObjects>
const int HexGrid<T, MaxObjects>::kCubeDR[6] = {  0, -1, -1,  0,  1,  1 };

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
template<typename T, unsigned int MaxObjects>
HexGrid<T, MaxObjects>::HexGrid(const Def& def)
    : mFreeCount(0)
    , mOccupiedCount(0)
    , mColCount(def.colCount)
    , mRowCount(def.rowCount)
    , mHexRadius(def.hexRadius)
    , mHexWidth(0.0f)
    , mHexHeight(0.0f)
    , mOrigin(def.origin)
{
    DIA_ASSERT(def.hexRadius > 0.0f, "HexGrid: hexRadius must be > 0");
    DIA_ASSERT(def.colCount  > 0,    "HexGrid: colCount must be > 0");
    DIA_ASSERT(def.rowCount  > 0,    "HexGrid: rowCount must be > 0");
    DIA_ASSERT(def.colCount * def.rowCount <= kMaxCells,
        "HexGrid: colCount * rowCount exceeds kMaxCells — reduce grid size");

    mHexWidth  = 1.7320508075688772f * def.hexRadius; // sqrt(3) * R
    mHexHeight = 2.0f * def.hexRadius;

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

    // Starting hex — WorldToHexClamped always returns a valid cell now
    HexCoord current;
    WorldToHexClamped(origin, current);

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

        // Find the neighbour closest to the target (use parity-correct tables)
        const int* dq = ((current.r & 1) != 0) ? kOddDQ : kEvenDQ;
        const int* dr = ((current.r & 1) != 0) ? kOddDR : kEvenDR;
        int bestNeighbour = 0;
        float bestDist2 = 1e30f;
        for (int n = 0; n < 6; ++n)
        {
            const HexCoord neighbour = { current.q + dq[n], current.r + dr[n] };
            if (!IsValidHex(neighbour)) continue;
            const Dia::Maths::Vector2D nc = HexToWorld(neighbour);
            const float ndx = nc.x - target.x;
            const float ndy = nc.y - target.y;
            const float d2 = ndx * ndx + ndy * ndy;
            if (d2 < bestDist2)
            {
                bestDist2     = d2;
                bestNeighbour = n;
            }
        }

        current = { current.q + dq[bestNeighbour], current.r + dr[bestNeighbour] };

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
        float                sqDist = 0.0f;
    };

    static constexpr int kMaxCandidates = kMaxQueryResults;
    Candidate candidates[kMaxCandidates] = {};
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
            // Walk around the ring in cube space, convert each to offset for lookup
            CubeCoord centerCube = OffsetToCube(center);
            CubeCoord hex = { centerCube.q + kCubeDQ[4] * ringR,
                              centerCube.r + kCubeDR[4] * ringR,
                              0 };
            hex.s = -hex.q - hex.r;

            for (int side = 0; side < 6; ++side)
            {
                for (int step = 0; step < ringR; ++step)
                {
                    const HexCoord off = CubeToOffset(hex);
                    if (IsValidHex(off))
                    {
                        const int cellIdx = HexToIndex(off);
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
                    hex.q += kCubeDQ[side];
                    hex.r += kCubeDR[side];
                    hex.s  = -hex.q - hex.r;
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
    // Pointy-top, odd-row-right offset coordinates.
    // Row (r) is determined purely by y; column (q) then depends on row parity.
    const float ly = worldPos.y - mOrigin.y;

    // Fractional row: each row is separated by mHexHeight * 0.75
    const float fr = ly / (mHexHeight * 0.75f);
    const int   r  = static_cast<int>(std::floor(fr + 0.5f));

    // Column offset: odd rows shift centers right by hexWidth/2
    const float rowShift = ((r & 1) != 0) ? (mHexWidth * 0.5f) : 0.0f;
    const float lx = worldPos.x - mOrigin.x - rowShift;
    const int   q  = static_cast<int>(std::floor(lx / mHexWidth + 0.5f));

    return { q, r };
}

template<typename T, unsigned int MaxObjects>
Dia::Maths::Vector2D HexGrid<T, MaxObjects>::HexToWorld(HexCoord hex) const
{
    // Center of cell (col, row) in world space.
    // Odd rows are offset right by hexWidth/2 (stagger).
    const float rowShift = ((hex.r & 1) != 0) ? (mHexWidth * 0.5f) : 0.0f;
    const float x = mOrigin.x + static_cast<float>(hex.q) * mHexWidth + rowShift + mHexWidth * 0.5f;
    const float y = mOrigin.y + static_cast<float>(hex.r) * mHexHeight * 0.75f + mHexRadius;
    return Dia::Maths::Vector2D(x, y);
}

template<typename T, unsigned int MaxObjects>
bool HexGrid<T, MaxObjects>::IsValidHex(HexCoord hex) const
{
    return hex.q >= 0 && hex.q < mColCount && hex.r >= 0 && hex.r < mRowCount;
}

template<typename T, unsigned int MaxObjects>
void HexGrid<T, MaxObjects>::GetNeighbours(
    HexCoord hex,
    Dia::Core::Containers::DynamicArrayC<HexCoord, 6>& out) const
{
    out.RemoveAll();
    const int* dq = ((hex.r & 1) != 0) ? kOddDQ : kEvenDQ;
    const int* dr = ((hex.r & 1) != 0) ? kOddDR : kEvenDR;
    for (int n = 0; n < 6; ++n)
    {
        const HexCoord neighbour = { hex.q + dq[n], hex.r + dr[n] };
        if (IsValidHex(neighbour))
            out.Add(neighbour);
    }
}

// Convert odd-row-right offset coord → cube coord
template<typename T, unsigned int MaxObjects>
typename HexGrid<T, MaxObjects>::CubeCoord
HexGrid<T, MaxObjects>::OffsetToCube(HexCoord hex)
{
    const int q = hex.q - (hex.r - (hex.r & 1)) / 2;
    const int r = hex.r;
    return { q, r, -q - r };
}

// Convert cube coord → odd-row-right offset coord
template<typename T, unsigned int MaxObjects>
HexCoord HexGrid<T, MaxObjects>::CubeToOffset(CubeCoord cube)
{
    const int col = cube.q + (cube.r - (cube.r & 1)) / 2;
    return { col, cube.r };
}

template<typename T, unsigned int MaxObjects>
int HexGrid<T, MaxObjects>::HexDistance(HexCoord a, HexCoord b) const
{
    const CubeCoord ca = OffsetToCube(a);
    const CubeCoord cb = OffsetToCube(b);
    const int dq = ca.q - cb.q; const int aq = dq < 0 ? -dq : dq;
    const int dr = ca.r - cb.r; const int ar = dr < 0 ? -dr : dr;
    const int ds = ca.s - cb.s; const int as_ = ds < 0 ? -ds : ds;
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

    // Work in cube space for correct ring traversal, then convert back to offset.
    CubeCoord cube = OffsetToCube(center);
    CubeCoord hex  = { cube.q + kCubeDQ[4] * radius,
                       cube.r + kCubeDR[4] * radius,
                       0 };
    hex.s = -hex.q - hex.r;

    for (int side = 0; side < 6 && !out.IsFull(); ++side)
    {
        for (int step = 0; step < radius && !out.IsFull(); ++step)
        {
            const HexCoord off = CubeToOffset(hex);
            if (IsValidHex(off))
                out.Add(off);
            hex.q += kCubeDQ[side];
            hex.r += kCubeDR[side];
            hex.s  = -hex.q - hex.r;
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
    return hex.q * mRowCount + hex.r;
}

template<typename T, unsigned int MaxObjects>
bool HexGrid<T, MaxObjects>::WorldToHexClamped(
    const Dia::Maths::Vector2D& pos, HexCoord& out) const
{
    out = WorldToHex(pos);
    // Clamp into valid range so callers get the nearest cell even for out-of-bounds pos
    if (out.q < 0)          out.q = 0;
    if (out.r < 0)          out.r = 0;
    if (out.q >= mColCount) out.q = mColCount - 1;
    if (out.r >= mRowCount) out.r = mRowCount - 1;
    return IsValidHex(out);
}

template<typename T, unsigned int MaxObjects>
void HexGrid<T, MaxObjects>::GetHexRange(
    const AARect& bounds,
    int& minQ, int& minR, int& maxQ, int& maxR) const
{
    // Sample all four corners plus mid-points on left/right edges to handle
    // the stagger case where an odd row protrudes beyond the corner samples.
    const Dia::Maths::Vector2D bl = bounds.GetBottomLeft();
    const Dia::Maths::Vector2D tr = bounds.GetTopRight();
    const Dia::Maths::Vector2D midY(bl.x, (bl.y + tr.y) * 0.5f);
    const Dia::Maths::Vector2D midYR(tr.x, (bl.y + tr.y) * 0.5f);

    const HexCoord c0 = WorldToHex(bl);
    const HexCoord c1 = WorldToHex(tr);
    const HexCoord c2 = WorldToHex(Dia::Maths::Vector2D(tr.x, bl.y));
    const HexCoord c3 = WorldToHex(Dia::Maths::Vector2D(bl.x, tr.y));
    const HexCoord c4 = WorldToHex(midY);
    const HexCoord c5 = WorldToHex(midYR);

    minQ = c0.q; maxQ = c0.q;
    minR = c0.r; maxR = c0.r;

    auto expand = [&](const HexCoord& c)
    {
        if (c.q < minQ) minQ = c.q; if (c.q > maxQ) maxQ = c.q;
        if (c.r < minR) minR = c.r; if (c.r > maxR) maxR = c.r;
    };
    expand(c1); expand(c2); expand(c3); expand(c4); expand(c5);

    // Add 1-cell margin for stagger overhang, then clamp to valid grid
    minQ -= 1; minR -= 1; maxQ += 1; maxR += 1;
    if (minQ < 0)         minQ = 0;
    if (minR < 0)         minR = 0;
    if (maxQ >= mColCount) maxQ = mColCount - 1;
    if (maxR >= mRowCount) maxR = mRowCount - 1;
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
