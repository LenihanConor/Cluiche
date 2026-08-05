#pragma once

#include <DiaCore/Core/Assert.h>
#include <cmath>

namespace Dia::EntitySpatial {

// ---------------------------------------------------------------------------
// Internal geometry helpers
// ---------------------------------------------------------------------------

// Returns true if candidate circle (pos, r) overlaps query circle (center, queryRadius).
// Exact test: |pos - center|^2 <= (queryRadius + r)^2
inline bool SpatialCircleOverlapsCircle(
    const Dia::Maths::Vector2D& candidatePos, float candidateRadius,
    const Dia::Maths::Vector2D& queryCenter,  float queryRadius)
{
    const float sumR = queryRadius + candidateRadius;
    const float dx   = candidatePos.x - queryCenter.x;
    const float dy   = candidatePos.y - queryCenter.y;
    return (dx * dx + dy * dy) <= (sumR * sumR);
}

// Returns squared distance from point P to the segment [origin, origin + dir * maxDist].
// dir must already be normalised (Ray guarantees this).
inline float SpatialClosestDistSqToRaySegment(
    const Dia::Maths::Vector2D& point,
    const Dia::Maths::Vector2D& origin,
    const Dia::Maths::Vector2D& dir,
    float maxDist)
{
    const float dx = point.x - origin.x;
    const float dy = point.y - origin.y;
    float t = dx * dir.x + dy * dir.y;             // dot(toPoint, dir)
    if (t < 0.0f)     t = 0.0f;
    if (t > maxDist)  t = maxDist;
    const float cx = origin.x + dir.x * t - point.x;
    const float cy = origin.y + dir.y * t - point.y;
    return cx * cx + cy * cy;
}

// ---------------------------------------------------------------------------
// QueryCircle
// ---------------------------------------------------------------------------

template<unsigned int N>
void EntitySpatialModule::QueryCircle(
    const Dia::Geometry2D::Circle& circle,
    uint32_t mask,
    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, N>& out) const
{
    mResolveBuffer.RemoveAll();
    mIndex.QueryCircle(circle, mResolveBuffer);

    const uint32_t gameMask = mask & 0x7FFFFFFFu;

    for (uint32_t i = 0; i < mResolveBuffer.Size(); ++i)
    {
        const Dia::Entity::Entity* ep = mIndex.Resolve(mResolveBuffer[i]);
        if (!ep) continue;
        const Dia::Entity::Entity entity = *ep;
        if (!mDomain.IsAlive(entity)) continue;

        const SpatialComponent* sc = mDomain.GetComponent<SpatialComponent>(entity);
        if (!sc) continue;

        // Exact geometric rejection: circle vs circle
        if (!SpatialCircleOverlapsCircle(sc->position, sc->radius,
                                         circle.GetCenter(), circle.GetRadius()))
            continue;

        // Layer mask filter
        if ((sc->GetLayerMask() & gameMask) == 0) continue;

        DIA_ASSERT(!out.IsFull(), "EntitySpatialModule::QueryCircle: out buffer truncated");
        if (out.IsFull()) break;
        out.Add(entity);
    }
}

// ---------------------------------------------------------------------------
// QueryRegion
// ---------------------------------------------------------------------------
// AARect vs AARect is exact by construction — no geometric rejection step.

template<unsigned int N>
void EntitySpatialModule::QueryRegion(
    const Dia::Geometry2D::AARect& region,
    uint32_t mask,
    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, N>& out) const
{
    mResolveBuffer.RemoveAll();
    mIndex.QueryRegion(region, mResolveBuffer);

    const uint32_t gameMask = mask & 0x7FFFFFFFu;

    for (uint32_t i = 0; i < mResolveBuffer.Size(); ++i)
    {
        const Dia::Entity::Entity* ep = mIndex.Resolve(mResolveBuffer[i]);
        if (!ep) continue;
        const Dia::Entity::Entity entity = *ep;
        if (!mDomain.IsAlive(entity)) continue;

        const SpatialComponent* sc = mDomain.GetComponent<SpatialComponent>(entity);
        if (!sc) continue;

        // Layer mask filter (no extra geometric rejection — region query is already exact)
        if ((sc->GetLayerMask() & gameMask) == 0) continue;

        DIA_ASSERT(!out.IsFull(), "EntitySpatialModule::QueryRegion: out buffer truncated");
        if (out.IsFull()) break;
        out.Add(entity);
    }
}

// ---------------------------------------------------------------------------
// QueryKNearest
// ---------------------------------------------------------------------------
// Collects all valid candidates first, then sorts by distance ascending,
// then copies the first min(k, count) to out.

template<unsigned int N>
void EntitySpatialModule::QueryKNearest(
    const Dia::Maths::Vector2D& origin,
    int k,
    uint32_t mask,
    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, N>& out) const
{
    mResolveBuffer.RemoveAll();
    mIndex.QueryKNearest(origin, k, mResolveBuffer);

    const uint32_t gameMask = mask & 0x7FFFFFFFu;

    // Collect valid candidates into a fixed-capacity temp array.
    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, kMaxQueryResults> candidates;
    Dia::Core::Containers::DynamicArrayC<float, kMaxQueryResults> distSqArr;

    for (uint32_t i = 0; i < mResolveBuffer.Size(); ++i)
    {
        const Dia::Entity::Entity* ep = mIndex.Resolve(mResolveBuffer[i]);
        if (!ep) continue;
        const Dia::Entity::Entity entity = *ep;
        if (!mDomain.IsAlive(entity)) continue;

        const SpatialComponent* sc = mDomain.GetComponent<SpatialComponent>(entity);
        if (!sc) continue;

        // Exact rejection: must be within summed circle distance
        const float dx = sc->position.x - origin.x;
        const float dy = sc->position.y - origin.y;
        const float dSq = dx * dx + dy * dy;
        // For KNearest we include based on circle overlap (candidate circle touches query point region)
        // The spec says: "same as QueryCircle check" — so we treat it as a circle query centered
        // at origin with radius 0 overlapping candidate circle.
        // That means dist <= candidateRadius (point inside candidate circle).
        // Actually re-reading: spec says same broad check as QueryCircle (sum of radii).
        // Since QueryKNearest has no fixed query radius, we keep all candidates from the index
        // (which already did a KNearest distance sort) and only apply layer mask + alive filters.
        // The index broad-phase already did distance-based culling for the k nearest.

        // Layer mask filter
        if ((sc->GetLayerMask() & gameMask) == 0) continue;

        if (!candidates.IsFull())
        {
            candidates.Add(entity);
            distSqArr.Add(dSq);
        }
    }

    // Selection sort: ascending by distSqArr
    const uint32_t count = candidates.Size();
    for (uint32_t i = 0; i < count; ++i)
    {
        uint32_t minIdx = i;
        for (uint32_t j = i + 1; j < count; ++j)
        {
            if (distSqArr[j] < distSqArr[minIdx])
                minIdx = j;
        }
        if (minIdx != i)
        {
            // Swap candidates
            Dia::Entity::Entity tmpE = candidates[i];
            candidates[i] = candidates[minIdx];
            candidates[minIdx] = tmpE;

            float tmpD = distSqArr[i];
            distSqArr[i] = distSqArr[minIdx];
            distSqArr[minIdx] = tmpD;
        }
    }

    // Copy first min(k, count) to out
    const uint32_t toCopy = (static_cast<uint32_t>(k) < count) ? static_cast<uint32_t>(k) : count;
    for (uint32_t i = 0; i < toCopy; ++i)
    {
        DIA_ASSERT(!out.IsFull(), "EntitySpatialModule::QueryKNearest: out buffer truncated");
        if (out.IsFull()) break;
        out.Add(candidates[i]);
    }
}

// ---------------------------------------------------------------------------
// QueryRay
// ---------------------------------------------------------------------------
// Exact rejection: closest point on ray segment [origin, origin+dir*maxDist]
// to candidate.position must be <= candidate.radius.

template<unsigned int N>
void EntitySpatialModule::QueryRay(
    const Dia::Geometry2D::Ray& ray,
    float maxDist,
    uint32_t mask,
    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, N>& out) const
{
    mResolveBuffer.RemoveAll();
    mIndex.QueryRay(ray, mResolveBuffer);

    const uint32_t gameMask = mask & 0x7FFFFFFFu;

    for (uint32_t i = 0; i < mResolveBuffer.Size(); ++i)
    {
        const Dia::Entity::Entity* ep = mIndex.Resolve(mResolveBuffer[i]);
        if (!ep) continue;
        const Dia::Entity::Entity entity = *ep;
        if (!mDomain.IsAlive(entity)) continue;

        const SpatialComponent* sc = mDomain.GetComponent<SpatialComponent>(entity);
        if (!sc) continue;

        // Exact rejection: closest point on finite ray segment to candidate circle centre
        const float distSq = SpatialClosestDistSqToRaySegment(
            sc->position,
            ray.GetOrigin(),
            ray.GetDirection(),
            maxDist);

        if (distSq > sc->radius * sc->radius) continue;

        // Layer mask filter
        if ((sc->GetLayerMask() & gameMask) == 0) continue;

        DIA_ASSERT(!out.IsFull(), "EntitySpatialModule::QueryRay: out buffer truncated");
        if (out.IsFull()) break;
        out.Add(entity);
    }
}

// ---------------------------------------------------------------------------
// QuerySector
// ---------------------------------------------------------------------------
// Broad phase: QueryCircle with origin + radius.
// Then angle rejection: dot(normalize(pos - origin), dir) >= cos(halfAngle).

template<unsigned int N>
void EntitySpatialModule::QuerySector(
    const Dia::Maths::Vector2D& origin,
    const Dia::Maths::Vector2D& dir,
    float radius,
    float halfAngle,
    uint32_t mask,
    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, N>& out) const
{
    mResolveBuffer.RemoveAll();

    // Broad phase: circle centred at origin with the sector's radius
    const Dia::Geometry2D::Circle broadCircle(radius, origin);
    mIndex.QueryCircle(broadCircle, mResolveBuffer);

    const uint32_t gameMask   = mask & 0x7FFFFFFFu;
    const float    cosHalfAng = cosf(halfAngle);

    for (uint32_t i = 0; i < mResolveBuffer.Size(); ++i)
    {
        const Dia::Entity::Entity* ep = mIndex.Resolve(mResolveBuffer[i]);
        if (!ep) continue;
        const Dia::Entity::Entity entity = *ep;
        if (!mDomain.IsAlive(entity)) continue;

        const SpatialComponent* sc = mDomain.GetComponent<SpatialComponent>(entity);
        if (!sc) continue;

        // Exact geometric rejection step 1: circle vs broad circle (same as QueryCircle)
        if (!SpatialCircleOverlapsCircle(sc->position, sc->radius, origin, radius))
            continue;

        // Exact geometric rejection step 2: angle check
        const float dx   = sc->position.x - origin.x;
        const float dy   = sc->position.y - origin.y;
        const float dist = sqrtf(dx * dx + dy * dy);

        if (dist < 1e-6f)
        {
            // Entity is at origin — undefined angle, skip
            continue;
        }

        const float normX  = dx / dist;
        const float normY  = dy / dist;
        const float dotVal = normX * dir.x + normY * dir.y;

        if (dotVal < cosHalfAng) continue;

        // Layer mask filter
        if ((sc->GetLayerMask() & gameMask) == 0) continue;

        DIA_ASSERT(!out.IsFull(), "EntitySpatialModule::QuerySector: out buffer truncated");
        if (out.IsFull()) break;
        out.Add(entity);
    }
}

} // namespace Dia::EntitySpatial
