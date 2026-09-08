#include "DiaGeometry3D/Intersection/IntersectionTests.h"
#include <cmath>

namespace Dia::Geometry3D
{
    namespace
    {
        static inline float SGClampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
        static inline float SGAbsf(float v) { return v < 0.0f ? -v : v; }
        static inline float SGMinf(float a, float b) { return a < b ? a : b; }
        static inline float SGMaxf(float a, float b) { return a > b ? a : b; }
    }

    // =========================================================================
    // Constructor
    // =========================================================================

    template<typename T, unsigned int MaxObjects, unsigned int MaxCells>
    SpatialGrid3D<T, MaxObjects, MaxCells>::SpatialGrid3D(const Def& def)
        : mFreeCount(0)
        , mOccupiedCount(0)
        , mCellSize(def.cellSize)
        , mWorldBounds(def.worldBounds)
    {
        Dia::Maths::Vector3D ext = mWorldBounds.CalculateExtents();
        // ext = half-extents; full size = 2 * ext
        mCellCountX = static_cast<int>(ceilf((ext.X() * 2.0f) / mCellSize));
        mCellCountY = static_cast<int>(ceilf((ext.Y() * 2.0f) / mCellSize));
        mCellCountZ = static_cast<int>(ceilf((ext.Z() * 2.0f) / mCellSize));
        if (mCellCountX < 1) mCellCountX = 1;
        if (mCellCountY < 1) mCellCountY = 1;
        if (mCellCountZ < 1) mCellCountZ = 1;

        DIA_ASSERT(
            (unsigned int)(mCellCountX * mCellCountY * mCellCountZ) <= MaxCells,
            "Spatial grid cell count (X*Y*Z) exceeds MaxCells template argument. Reduce cellSize, expand worldBounds, or instantiate with a larger MaxCells."
        );

        // Initialise slots: zero-generation, unoccupied
        for (unsigned int i = 0; i < MaxObjects; ++i)
        {
            Slot s{};
            s.generation = 0;
            s.occupied   = false;
            mSlots.Add(s);
        }

        // Free list: highest index first so first Insert returns slot 0
        for (int i = static_cast<int>(MaxObjects) - 1; i >= 0; --i)
            mFreeList[mFreeCount++] = static_cast<uint32_t>(i);
    }

    // =========================================================================
    // Private helpers
    // =========================================================================

    template<typename T, unsigned int MaxObjects, unsigned int MaxCells>
    int SpatialGrid3D<T, MaxObjects, MaxCells>::CellIndex(int cx, int cy, int cz) const
    {
        return cz * (mCellCountX * mCellCountY) + cy * mCellCountX + cx;
    }

    template<typename T, unsigned int MaxObjects, unsigned int MaxCells>
    bool SpatialGrid3D<T, MaxObjects, MaxCells>::WorldToCellClamped(float x, float y, float z, int& cx, int& cy, int& cz) const
    {
        float ox = x - mWorldBounds.GetMin().X();
        float oy = y - mWorldBounds.GetMin().Y();
        float oz = z - mWorldBounds.GetMin().Z();
        cx = static_cast<int>(floorf(ox / mCellSize));
        cy = static_cast<int>(floorf(oy / mCellSize));
        cz = static_cast<int>(floorf(oz / mCellSize));
        cx = static_cast<int>(SGClampf(static_cast<float>(cx), 0.0f, static_cast<float>(mCellCountX - 1)));
        cy = static_cast<int>(SGClampf(static_cast<float>(cy), 0.0f, static_cast<float>(mCellCountY - 1)));
        cz = static_cast<int>(SGClampf(static_cast<float>(cz), 0.0f, static_cast<float>(mCellCountZ - 1)));
        return true;
    }

    template<typename T, unsigned int MaxObjects, unsigned int MaxCells>
    bool SpatialGrid3D<T, MaxObjects, MaxCells>::WorldToCellExact(float x, float y, float z, int& cx, int& cy, int& cz) const
    {
        float ox = x - mWorldBounds.GetMin().X();
        float oy = y - mWorldBounds.GetMin().Y();
        float oz = z - mWorldBounds.GetMin().Z();
        cx = static_cast<int>(floorf(ox / mCellSize));
        cy = static_cast<int>(floorf(oy / mCellSize));
        cz = static_cast<int>(floorf(oz / mCellSize));
        return cx >= 0 && cx < mCellCountX
            && cy >= 0 && cy < mCellCountY
            && cz >= 0 && cz < mCellCountZ;
    }

    template<typename T, unsigned int MaxObjects, unsigned int MaxCells>
    void SpatialGrid3D<T, MaxObjects, MaxCells>::GetCellRange(const AABB& bounds,
        int& minCx, int& minCy, int& minCz,
        int& maxCx, int& maxCy, int& maxCz) const
    {
        WorldToCellClamped(bounds.GetMin().X(), bounds.GetMin().Y(), bounds.GetMin().Z(), minCx, minCy, minCz);
        WorldToCellClamped(bounds.GetMax().X(), bounds.GetMax().Y(), bounds.GetMax().Z(), maxCx, maxCy, maxCz);
        if (minCx > maxCx) { int t = minCx; minCx = maxCx; maxCx = t; }
        if (minCy > maxCy) { int t = minCy; minCy = maxCy; maxCy = t; }
        if (minCz > maxCz) { int t = minCz; minCz = maxCz; maxCz = t; }
    }

    template<typename T, unsigned int MaxObjects, unsigned int MaxCells>
    bool SpatialGrid3D<T, MaxObjects, MaxCells>::IsHandleValid(Dia::Core::Handle<T> h) const
    {
        if (!h.IsValid()) return false;
        uint32_t idx = h.GetIndex();
        if (idx >= MaxObjects) return false;
        const Slot& s = mSlots[idx];
        return s.occupied && s.generation == h.GetGeneration();
    }

    template<typename T, unsigned int MaxObjects, unsigned int MaxCells>
    void SpatialGrid3D<T, MaxObjects, MaxCells>::InsertIntoCells(uint32_t slotIdx, const AABB& bounds)
    {
        int minCx, minCy, minCz, maxCx, maxCy, maxCz;
        GetCellRange(bounds, minCx, minCy, minCz, maxCx, maxCy, maxCz);
        for (int cz = minCz; cz <= maxCz; ++cz)
            for (int cy = minCy; cy <= maxCy; ++cy)
                for (int cx = minCx; cx <= maxCx; ++cx)
                {
                    int ci = CellIndex(cx, cy, cz);
                    auto& cell = mCells[ci];
                    if (!cell.IsFull())
                        cell.Add(slotIdx);
                }
    }

    template<typename T, unsigned int MaxObjects, unsigned int MaxCells>
    void SpatialGrid3D<T, MaxObjects, MaxCells>::RemoveFromCells(uint32_t slotIdx, const AABB& bounds)
    {
        int minCx, minCy, minCz, maxCx, maxCy, maxCz;
        GetCellRange(bounds, minCx, minCy, minCz, maxCx, maxCy, maxCz);
        for (int cz = minCz; cz <= maxCz; ++cz)
            for (int cy = minCy; cy <= maxCy; ++cy)
                for (int cx = minCx; cx <= maxCx; ++cx)
                {
                    int ci = CellIndex(cx, cy, cz);
                    auto& cell = mCells[ci];
                    cell.RemoveFirst(slotIdx);
                }
    }

    template<typename T, unsigned int MaxObjects, unsigned int MaxCells>
    AABB SpatialGrid3D<T, MaxObjects, MaxCells>::CellAABB(int cx, int cy, int cz) const
    {
        float minX = mWorldBounds.GetMin().X() + cx       * mCellSize;
        float minY = mWorldBounds.GetMin().Y() + cy       * mCellSize;
        float minZ = mWorldBounds.GetMin().Z() + cz       * mCellSize;
        float maxX = mWorldBounds.GetMin().X() + (cx + 1) * mCellSize;
        float maxY = mWorldBounds.GetMin().Y() + (cy + 1) * mCellSize;
        float maxZ = mWorldBounds.GetMin().Z() + (cz + 1) * mCellSize;
        return AABB(Dia::Maths::Vector3D(minX, minY, minZ),
                    Dia::Maths::Vector3D(maxX, maxY, maxZ));
    }

    // =========================================================================
    // Insert / Remove / Update / Clear / Resolve
    // =========================================================================

    template<typename T, unsigned int MaxObjects, unsigned int MaxCells>
    Dia::Core::Handle<T> SpatialGrid3D<T, MaxObjects, MaxCells>::Insert(const T& object, const AABB& bounds)
    {
        DIA_ASSERT(mFreeCount > 0, "SpatialGrid3D: Insert called when no free slots remain.");
        if (mFreeCount <= 0)
            return Dia::Core::Handle<T>::Invalid();

        uint32_t slotIdx = mFreeList[--mFreeCount];
        Slot& s = mSlots[slotIdx];
        s.object   = object;
        s.bounds   = bounds;
        s.occupied = true;
        ++s.generation;
        if (s.generation == 0) s.generation = 1; // skip the "unused" sentinel

        InsertIntoCells(slotIdx, bounds);
        ++mOccupiedCount;
        return Dia::Core::Handle<T>(slotIdx, s.generation);
    }

    template<typename T, unsigned int MaxObjects, unsigned int MaxCells>
    void SpatialGrid3D<T, MaxObjects, MaxCells>::Remove(Dia::Core::Handle<T> handle)
    {
        DIA_ASSERT(IsHandleValid(handle), "SpatialGrid3D::Remove: invalid handle.");
        if (!IsHandleValid(handle)) return;

        uint32_t slotIdx = handle.GetIndex();
        Slot& s = mSlots[slotIdx];
        RemoveFromCells(slotIdx, s.bounds);
        s.occupied = false;
        mFreeList[mFreeCount++] = slotIdx;
        --mOccupiedCount;
    }

    template<typename T, unsigned int MaxObjects, unsigned int MaxCells>
    void SpatialGrid3D<T, MaxObjects, MaxCells>::Update(Dia::Core::Handle<T> handle, const AABB& newBounds)
    {
        DIA_ASSERT(IsHandleValid(handle), "SpatialGrid3D::Update: invalid handle.");
        if (!IsHandleValid(handle)) return;

        uint32_t slotIdx = handle.GetIndex();
        Slot& s = mSlots[slotIdx];
        RemoveFromCells(slotIdx, s.bounds);
        s.bounds = newBounds;
        InsertIntoCells(slotIdx, newBounds);
    }

    template<typename T, unsigned int MaxObjects, unsigned int MaxCells>
    void SpatialGrid3D<T, MaxObjects, MaxCells>::Clear()
    {
        for (unsigned int ci = 0; ci < MaxCells; ++ci)
            mCells[ci].RemoveAll();

        for (unsigned int i = 0; i < MaxObjects; ++i)
            mSlots[i].occupied = false;

        mFreeCount = 0;
        for (int i = static_cast<int>(MaxObjects) - 1; i >= 0; --i)
            mFreeList[mFreeCount++] = static_cast<uint32_t>(i);

        mOccupiedCount = 0;
    }

    template<typename T, unsigned int MaxObjects, unsigned int MaxCells>
    const T* SpatialGrid3D<T, MaxObjects, MaxCells>::Resolve(Dia::Core::Handle<T> handle) const
    {
        if (!IsHandleValid(handle)) return nullptr;
        return &mSlots[handle.GetIndex()].object;
    }

    template<typename T, unsigned int MaxObjects, unsigned int MaxCells>
    int SpatialGrid3D<T, MaxObjects, MaxCells>::GetCellCount() const
    {
        return mCellCountX * mCellCountY * mCellCountZ;
    }

    // =========================================================================
    // Query helpers — visited bitset (stack allocated)
    // =========================================================================

    // Stack bitset: MaxObjects bits
    template<unsigned int MaxObjects>
    struct VisitedBitset
    {
        static constexpr unsigned int kWords = (MaxObjects + 31) / 32;
        uint32_t words[kWords];

        VisitedBitset() { for (unsigned int i = 0; i < kWords; ++i) words[i] = 0; }
        bool Test(uint32_t idx) const { return (words[idx >> 5] >> (idx & 31)) & 1u; }
        void Set(uint32_t idx) { words[idx >> 5] |= (1u << (idx & 31)); }
    };

    // =========================================================================
    // QueryRegion
    // =========================================================================

    template<typename T, unsigned int MaxObjects, unsigned int MaxCells>
    void SpatialGrid3D<T, MaxObjects, MaxCells>::QueryRegion(
        const AABB& region,
        Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
    {
        VisitedBitset<MaxObjects> visited;
        int minCx, minCy, minCz, maxCx, maxCy, maxCz;
        GetCellRange(region, minCx, minCy, minCz, maxCx, maxCy, maxCz);

        for (int cz = minCz; cz <= maxCz && !out.IsFull(); ++cz)
            for (int cy = minCy; cy <= maxCy && !out.IsFull(); ++cy)
                for (int cx = minCx; cx <= maxCx && !out.IsFull(); ++cx)
                {
                    const auto& cell = mCells[CellIndex(cx, cy, cz)];
                    for (unsigned int k = 0; k < cell.Size() && !out.IsFull(); ++k)
                    {
                        uint32_t idx = cell[k];
                        if (visited.Test(idx)) continue;
                        visited.Set(idx);
                        const Slot& s = mSlots[idx];
                        if (!s.occupied) continue;
                        if (IntersectionTests::Test(s.bounds, region) != IntersectionClassify::kNoIntersection)
                            out.Add(Dia::Core::Handle<T>(idx, s.generation));
                    }
                }
    }

    // =========================================================================
    // QuerySphere
    // =========================================================================

    template<typename T, unsigned int MaxObjects, unsigned int MaxCells>
    void SpatialGrid3D<T, MaxObjects, MaxCells>::QuerySphere(
        const Sphere& sphere,
        Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
    {
        AABB sphereBounds = AABB::FromCenterExtents(sphere.GetCenter(),
            Dia::Maths::Vector3D(sphere.GetRadius(), sphere.GetRadius(), sphere.GetRadius()));

        VisitedBitset<MaxObjects> visited;
        int minCx, minCy, minCz, maxCx, maxCy, maxCz;
        GetCellRange(sphereBounds, minCx, minCy, minCz, maxCx, maxCy, maxCz);

        for (int cz = minCz; cz <= maxCz && !out.IsFull(); ++cz)
            for (int cy = minCy; cy <= maxCy && !out.IsFull(); ++cy)
                for (int cx = minCx; cx <= maxCx && !out.IsFull(); ++cx)
                {
                    const auto& cell = mCells[CellIndex(cx, cy, cz)];
                    for (unsigned int k = 0; k < cell.Size() && !out.IsFull(); ++k)
                    {
                        uint32_t idx = cell[k];
                        if (visited.Test(idx)) continue;
                        visited.Set(idx);
                        const Slot& s = mSlots[idx];
                        if (!s.occupied) continue;
                        if (IntersectionTests::Test(s.bounds, sphere) != IntersectionClassify::kNoIntersection)
                            out.Add(Dia::Core::Handle<T>(idx, s.generation));
                    }
                }
    }

    // =========================================================================
    // QueryPoint
    // =========================================================================

    template<typename T, unsigned int MaxObjects, unsigned int MaxCells>
    void SpatialGrid3D<T, MaxObjects, MaxCells>::QueryPoint(
        const Dia::Maths::Vector3D& point,
        Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
    {
        int cx, cy, cz;
        if (!WorldToCellExact(point.X(), point.Y(), point.Z(), cx, cy, cz)) return;

        const auto& cell = mCells[CellIndex(cx, cy, cz)];
        for (unsigned int k = 0; k < cell.Size() && !out.IsFull(); ++k)
        {
            uint32_t idx = cell[k];
            const Slot& s = mSlots[idx];
            if (!s.occupied) continue;
            if (IntersectionTests::Contains(s.bounds, point))
                out.Add(Dia::Core::Handle<T>(idx, s.generation));
        }
    }

    // =========================================================================
    // QueryRay — Amanatides–Woo 3D DDA
    // =========================================================================

    template<typename T, unsigned int MaxObjects, unsigned int MaxCells>
    void SpatialGrid3D<T, MaxObjects, MaxCells>::QueryRay(
        const Ray& ray,
        Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
    {
        VisitedBitset<MaxObjects> visited;

        const Dia::Maths::Vector3D& o = ray.GetOrigin();
        const Dia::Maths::Vector3D& d = ray.GetDirection();

        int cx, cy, cz;
        // Start from clamped origin cell
        float ox = SGClampf(o.X(), mWorldBounds.GetMin().X(), mWorldBounds.GetMax().X());
        float oy = SGClampf(o.Y(), mWorldBounds.GetMin().Y(), mWorldBounds.GetMax().Y());
        float oz = SGClampf(o.Z(), mWorldBounds.GetMin().Z(), mWorldBounds.GetMax().Z());
        WorldToCellClamped(ox, oy, oz, cx, cy, cz);

        // Step directions
        int stepX = d.X() > 0.0f ? 1 : (d.X() < 0.0f ? -1 : 0);
        int stepY = d.Y() > 0.0f ? 1 : (d.Y() < 0.0f ? -1 : 0);
        int stepZ = d.Z() > 0.0f ? 1 : (d.Z() < 0.0f ? -1 : 0);

        // Cell boundary t values
        float cellMinX = mWorldBounds.GetMin().X() + cx * mCellSize;
        float cellMinY = mWorldBounds.GetMin().Y() + cy * mCellSize;
        float cellMinZ = mWorldBounds.GetMin().Z() + cz * mCellSize;

        auto safeDiv = [](float a, float b) -> float {
            return (SGAbsf(b) < 1e-12f) ? 1e30f : a / b;
        };

        float tMaxX = safeDiv((stepX >= 0 ? cellMinX + mCellSize : cellMinX) - o.X(), d.X());
        float tMaxY = safeDiv((stepY >= 0 ? cellMinY + mCellSize : cellMinY) - o.Y(), d.Y());
        float tMaxZ = safeDiv((stepZ >= 0 ? cellMinZ + mCellSize : cellMinZ) - o.Z(), d.Z());

        float tDeltaX = safeDiv(mCellSize, SGAbsf(d.X()) < 1e-12f ? 1e-12f : d.X()) * (stepX != 0 ? 1.0f : 0.0f);
        float tDeltaY = safeDiv(mCellSize, SGAbsf(d.Y()) < 1e-12f ? 1e-12f : d.Y()) * (stepY != 0 ? 1.0f : 0.0f);
        float tDeltaZ = safeDiv(mCellSize, SGAbsf(d.Z()) < 1e-12f ? 1e-12f : d.Z()) * (stepZ != 0 ? 1.0f : 0.0f);
        if (tDeltaX < 0.0f) tDeltaX = -tDeltaX;
        if (tDeltaY < 0.0f) tDeltaY = -tDeltaY;
        if (tDeltaZ < 0.0f) tDeltaZ = -tDeltaZ;
        if (tDeltaX < 1e-12f && stepX != 0) tDeltaX = 1e30f;
        if (tDeltaY < 1e-12f && stepY != 0) tDeltaY = 1e30f;
        if (tDeltaZ < 1e-12f && stepZ != 0) tDeltaZ = 1e30f;

        for (int iter = 0; iter < mCellCountX + mCellCountY + mCellCountZ + 3 && !out.IsFull(); ++iter)
        {
            // Visit this cell
            const auto& cell = mCells[CellIndex(cx, cy, cz)];
            for (unsigned int k = 0; k < cell.Size() && !out.IsFull(); ++k)
            {
                uint32_t idx = cell[k];
                if (visited.Test(idx)) continue;
                visited.Set(idx);
                const Slot& s = mSlots[idx];
                if (!s.occupied) continue;
                if (IntersectionTests::Test(ray, s.bounds) != IntersectionClassify::kNoIntersection)
                    out.Add(Dia::Core::Handle<T>(idx, s.generation));
            }

            // Advance DDA
            if (tMaxX < tMaxY && tMaxX < tMaxZ)
            {
                cx += stepX;
                if (cx < 0 || cx >= mCellCountX) break;
                tMaxX += tDeltaX;
            }
            else if (tMaxY < tMaxZ)
            {
                cy += stepY;
                if (cy < 0 || cy >= mCellCountY) break;
                tMaxY += tDeltaY;
            }
            else
            {
                cz += stepZ;
                if (cz < 0 || cz >= mCellCountZ) break;
                tMaxZ += tDeltaZ;
            }
        }
    }

    // =========================================================================
    // QueryFrustum — three-level culling
    // =========================================================================

    template<typename T, unsigned int MaxObjects, unsigned int MaxCells>
    void SpatialGrid3D<T, MaxObjects, MaxCells>::QueryFrustum(
        const Frustum& frustum,
        Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
    {
        VisitedBitset<MaxObjects> visited;

        AABB frustumBounds = frustum.CalculateAABB();
        int minCx, minCy, minCz, maxCx, maxCy, maxCz;
        GetCellRange(frustumBounds, minCx, minCy, minCz, maxCx, maxCy, maxCz);

        for (int cz = minCz; cz <= maxCz && !out.IsFull(); ++cz)
            for (int cy = minCy; cy <= maxCy && !out.IsFull(); ++cy)
                for (int cx = minCx; cx <= maxCx && !out.IsFull(); ++cx)
                {
                    // Per-cell frustum vs cell AABB — skip wholly-outside cells
                    AABB cellBox = CellAABB(cx, cy, cz);
                    if (IntersectionTests::Test(cellBox, frustum) == IntersectionClassify::kNoIntersection)
                        continue;

                    const auto& cell = mCells[CellIndex(cx, cy, cz)];
                    for (unsigned int k = 0; k < cell.Size() && !out.IsFull(); ++k)
                    {
                        uint32_t idx = cell[k];
                        if (visited.Test(idx)) continue;
                        visited.Set(idx);
                        const Slot& s = mSlots[idx];
                        if (!s.occupied) continue;
                        if (IntersectionTests::Test(s.bounds, frustum) != IntersectionClassify::kNoIntersection)
                            out.Add(Dia::Core::Handle<T>(idx, s.generation));
                    }
                }
    }

    // =========================================================================
    // QueryKNearest — expanding-ring
    // =========================================================================

    template<typename T, unsigned int MaxObjects, unsigned int MaxCells>
    void SpatialGrid3D<T, MaxObjects, MaxCells>::QueryKNearest(
        const Dia::Maths::Vector3D& point, int k,
        Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const
    {
        if (k <= 0) return;

        // Gather candidates with squared-distance
        struct Candidate
        {
            float    sqDist;
            uint32_t slotIdx;
        };

        static constexpr int kMaxCandidates = 256;
        Dia::Core::Containers::DynamicArrayC<Candidate, kMaxCandidates> candidates;

        VisitedBitset<MaxObjects> visited;

        int pcx, pcy, pcz;
        WorldToCellClamped(point.X(), point.Y(), point.Z(), pcx, pcy, pcz);

        int maxShell = mCellCountX + mCellCountY + mCellCountZ;

        for (int shell = 0; shell <= maxShell && !candidates.IsFull(); ++shell)
        {
            // Expand a cubic shell of radius `shell` cells around (pcx, pcy, pcz)
            int minCx = pcx - shell, maxCx2 = pcx + shell;
            int minCy = pcy - shell, maxCy2 = pcy + shell;
            int minCz = pcz - shell, maxCz2 = pcz + shell;

            if (minCx < 0) minCx = 0;
            if (maxCx2 >= mCellCountX) maxCx2 = mCellCountX - 1;
            if (minCy < 0) minCy = 0;
            if (maxCy2 >= mCellCountY) maxCy2 = mCellCountY - 1;
            if (minCz < 0) minCz = 0;
            if (maxCz2 >= mCellCountZ) maxCz2 = mCellCountZ - 1;

            for (int cz = minCz; cz <= maxCz2; ++cz)
                for (int cy = minCy; cy <= maxCy2; ++cy)
                    for (int cx = minCx; cx <= maxCx2; ++cx)
                    {
                        // Only process outer shell cells on iterations > 0
                        if (shell > 0)
                        {
                            bool onShell = (cx == pcx - shell || cx == pcx + shell)
                                        || (cy == pcy - shell || cy == pcy + shell)
                                        || (cz == pcz - shell || cz == pcz + shell);
                            if (!onShell) continue;
                        }

                        const auto& cell = mCells[CellIndex(cx, cy, cz)];
                        for (unsigned int ki = 0; ki < cell.Size(); ++ki)
                        {
                            uint32_t idx = cell[ki];
                            if (visited.Test(idx)) continue;
                            visited.Set(idx);
                            const Slot& s = mSlots[idx];
                            if (!s.occupied) continue;

                            Dia::Maths::Vector3D c = s.bounds.CalculateCenter();
                            float dx = c.X() - point.X();
                            float dy = c.Y() - point.Y();
                            float dz = c.Z() - point.Z();
                            float sq = dx*dx + dy*dy + dz*dz;

                            if (!candidates.IsFull())
                            {
                                Candidate cand{sq, idx};
                                candidates.Add(cand);
                            }
                            else
                            {
                                // Find farthest candidate
                                int farthestIdx = 0;
                                float farthestSq = candidates[0].sqDist;
                                for (unsigned int ci = 1; ci < candidates.Size(); ++ci)
                                    if (candidates[ci].sqDist > farthestSq)
                                    {
                                        farthestSq = candidates[ci].sqDist;
                                        farthestIdx = static_cast<int>(ci);
                                    }
                                if (sq < farthestSq)
                                    candidates[farthestIdx] = {sq, idx};
                            }
                        }
                    }

            // Early termination: if we have >= k candidates and min shell distance > kth distance
            if (candidates.Size() >= static_cast<unsigned int>(k))
            {
                float shellMinSq = static_cast<float>(shell) * mCellSize;
                shellMinSq = shellMinSq > mCellSize ? (shellMinSq - mCellSize) : 0.0f;
                shellMinSq *= shellMinSq;

                // Find kth-best distance
                float kthSq = 0.0f;
                if (static_cast<int>(candidates.Size()) >= k)
                {
                    // Sort candidates by sqDist (simple insertion sort for small N)
                    for (unsigned int ci = 1; ci < candidates.Size(); ++ci)
                    {
                        Candidate key = candidates[ci];
                        int j = static_cast<int>(ci) - 1;
                        while (j >= 0 && candidates[j].sqDist > key.sqDist)
                        {
                            candidates[j + 1] = candidates[j];
                            --j;
                        }
                        candidates[j + 1] = key;
                    }
                    kthSq = candidates[k - 1].sqDist;
                }
                if (shellMinSq > kthSq) break;
            }
        }

        // Sort final candidates by sqDist (already sorted above if we hit early termination)
        for (unsigned int ci = 1; ci < candidates.Size(); ++ci)
        {
            Candidate key = candidates[ci];
            int j = static_cast<int>(ci) - 1;
            while (j >= 0 && candidates[j].sqDist > key.sqDist)
            {
                candidates[j + 1] = candidates[j];
                --j;
            }
            candidates[j + 1] = key;
        }

        int resultCount = SGMinf(static_cast<float>(k), static_cast<float>(candidates.Size()));
        for (int i = 0; i < resultCount && !out.IsFull(); ++i)
        {
            uint32_t idx = candidates[i].slotIdx;
            const Slot& s = mSlots[idx];
            out.Add(Dia::Core::Handle<T>(idx, s.generation));
        }
    }

} // namespace Dia::Geometry3D
