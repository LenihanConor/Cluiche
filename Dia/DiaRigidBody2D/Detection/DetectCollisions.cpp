#include "DiaRigidBody2D/Detection/DetectCollisions.h"

#include "DiaRigidBody2D/World/BodyPairKey.h"
#include "DiaRigidBody2D/WorldShapeUtil.h"
#include "DiaGeometry2D/Shapes/Circle.h"
#include "DiaGeometry2D/Shapes/AARect.h"
#include "DiaGeometry2D/Shapes/ConvexPolygon.h"
#include "DiaCore/Containers/HashTables/HashTable.h"

#include <cmath>

namespace Dia::RigidBody2D {

// ---------------------------------------------------------------------------
// Shape accessor helpers — virtual dispatch on Body2DBase, O(1)
// ---------------------------------------------------------------------------

static Contact MakeContact(Body2DBase* a, Body2DBase* b,
    const Dia::Maths::Vector2D& normal, float depth, const Dia::Maths::Vector2D& pt)
{
    Contact c;
    c.normal = normal;
    c.depth  = depth;
    c.point  = pt;
    c.bodyA  = a;
    c.bodyB  = b;
    return c;
}

// ---------------------------------------------------------------------------
// Analytical contact computation — normal points from B toward A
// ---------------------------------------------------------------------------

static bool CircleVsCircle(const Dia::Geometry2D::Circle& A, const Dia::Geometry2D::Circle& B,
                            Dia::Maths::Vector2D& normal, float& depth, Dia::Maths::Vector2D& point)
{
    float dx = B.GetCenter().x - A.GetCenter().x;
    float dy = B.GetCenter().y - A.GetCenter().y;
    float distSq  = dx * dx + dy * dy;
    float radSum  = A.GetRadius() + B.GetRadius();
    if (distSq >= radSum * radSum) return false;

    float dist = std::sqrt(distSq);
    if (dist < 1e-6f)
    {
        normal = Dia::Maths::Vector2D(0.0f, 1.0f);
        depth  = radSum;
    }
    else
    {
        normal = Dia::Maths::Vector2D(dx / dist, dy / dist);
        depth  = radSum - dist;
    }
    point = Dia::Maths::Vector2D(
        A.GetCenter().x + normal.x * A.GetRadius(),
        A.GetCenter().y + normal.y * A.GetRadius());
    return true;
}

static bool AARectVsAARect(const Dia::Geometry2D::AARect& A, const Dia::Geometry2D::AARect& B,
                           Dia::Maths::Vector2D& normal, float& depth, Dia::Maths::Vector2D& point)
{
    Dia::Maths::Vector2D cA = A.CalculateCenter();
    Dia::Maths::Vector2D cB = B.CalculateCenter();

    float halfWA = (A.GetTopRight().x - A.GetBottomLeft().x) * 0.5f;
    float halfHA = (A.GetTopRight().y - A.GetBottomLeft().y) * 0.5f;
    float halfWB = (B.GetTopRight().x - B.GetBottomLeft().x) * 0.5f;
    float halfHB = (B.GetTopRight().y - B.GetBottomLeft().y) * 0.5f;

    float dx = cB.x - cA.x;
    float dy = cB.y - cA.y;

    float overlapX = (halfWA + halfWB) - (dx < 0.0f ? -dx : dx);
    float overlapY = (halfHA + halfHB) - (dy < 0.0f ? -dy : dy);

    if (overlapX <= 0.0f || overlapY <= 0.0f) return false;

    if (overlapX < overlapY)
    {
        normal = Dia::Maths::Vector2D(dx > 0.0f ? 1.0f : -1.0f, 0.0f);
        depth  = overlapX;
    }
    else
    {
        normal = Dia::Maths::Vector2D(0.0f, dy > 0.0f ? 1.0f : -1.0f);
        depth  = overlapY;
    }
    point = Dia::Maths::Vector2D(cA.x + normal.x * halfWA, cA.y + normal.y * halfHA);
    return true;
}

// circleIsA: true when circle is body A (normal points A→B = circ→rect)
static bool CircleVsAARect(const Dia::Geometry2D::Circle& circ, const Dia::Geometry2D::AARect& rect,
                           bool circleIsA,
                           Dia::Maths::Vector2D& normal, float& depth, Dia::Maths::Vector2D& point)
{
    Dia::Maths::Vector2D closest;
    rect.ClosestPointOnAARectTo(circ.GetCenter(), closest);

    float dx = circ.GetCenter().x - closest.x;
    float dy = circ.GetCenter().y - closest.y;
    float distSq = dx * dx + dy * dy;

    if (distSq >= circ.GetRadius() * circ.GetRadius()) return false;

    float dist = std::sqrt(distSq);
    if (dist < 1e-6f)
    {
        Dia::Maths::Vector2D toCenter = circ.GetCenter() - rect.CalculateCenter();
        float len = std::sqrt(toCenter.x * toCenter.x + toCenter.y * toCenter.y);
        normal = (len > 1e-6f)
            ? Dia::Maths::Vector2D(toCenter.x / len, toCenter.y / len)
            : Dia::Maths::Vector2D(0.0f, 1.0f);
        depth = circ.GetRadius();
    }
    else
    {
        normal = Dia::Maths::Vector2D(dx / dist, dy / dist);
        depth  = circ.GetRadius() - dist;
    }

    if (circleIsA)
    {
        normal.x = -normal.x;
        normal.y = -normal.y;
    }

    point = closest;
    return true;
}

static bool PolyVsPoly(const Dia::Geometry2D::ConvexPolygon& A, const Dia::Geometry2D::ConvexPolygon& B,
                       Dia::Maths::Vector2D& normal, float& depth, Dia::Maths::Vector2D& point)
{
    float minOverlap = 1e30f;
    Dia::Maths::Vector2D minAxis(0.0f, 1.0f);

    auto project = [](const Dia::Geometry2D::ConvexPolygon& poly,
                      const Dia::Maths::Vector2D& axis, float& mn, float& mx)
    {
        mn = 1e30f; mx = -1e30f;
        for (int i = 0; i < poly.GetVertexCount(); ++i)
        {
            float d = poly.GetVertex(i).x * axis.x + poly.GetVertex(i).y * axis.y;
            if (d < mn) mn = d;
            if (d > mx) mx = d;
        }
    };

    for (int pass = 0; pass < 2; ++pass)
    {
        const Dia::Geometry2D::ConvexPolygon& poly = (pass == 0) ? A : B;
        for (int i = 0; i < poly.GetVertexCount(); ++i)
        {
            const Dia::Maths::Vector2D& v0 = poly.GetVertex(i);
            const Dia::Maths::Vector2D& v1 = poly.GetVertex((i + 1) % poly.GetVertexCount());
            Dia::Maths::Vector2D edge(v1.x - v0.x, v1.y - v0.y);
            Dia::Maths::Vector2D axis(-edge.y, edge.x);
            float len = std::sqrt(axis.x * axis.x + axis.y * axis.y);
            if (len < 1e-6f) continue;
            axis.x /= len; axis.y /= len;

            float minA, maxA, minB, maxB;
            project(A, axis, minA, maxA);
            project(B, axis, minB, maxB);

            float minMax = maxA < maxB ? maxA : maxB;
            float maxMin = minA > minB ? minA : minB;
            float overlap = minMax - maxMin;
            if (overlap <= 0.0f) return false;

            if (overlap < minOverlap) { minOverlap = overlap; minAxis = axis; }
        }
    }

    Dia::Maths::Vector2D cA = A.CalculateCenter();
    Dia::Maths::Vector2D cB = B.CalculateCenter();
    float dot = (cB.x - cA.x) * minAxis.x + (cB.y - cA.y) * minAxis.y;
    if (dot < 0.0f) { minAxis.x = -minAxis.x; minAxis.y = -minAxis.y; }

    normal = minAxis;
    depth  = minOverlap;
    point  = Dia::Maths::Vector2D((cA.x + cB.x) * 0.5f, (cA.y + cB.y) * 0.5f);
    return true;
}

// ---------------------------------------------------------------------------
// Shape dispatch
// ---------------------------------------------------------------------------

static bool NarrowPhase(Body2DBase* a, Body2DBase* b,
    Dia::Maths::Vector2D& normal, float& depth, Dia::Maths::Vector2D& point)
{
    const bool hasCircA = a->GetCircleShape() != nullptr;
    const bool hasCircB = b->GetCircleShape() != nullptr;
    const bool hasPolyA = !hasCircA && a->GetPolyShape() != nullptr;
    const bool hasPolyB = !hasCircB && b->GetPolyShape() != nullptr;

    if (hasCircA && hasCircB)
    {
        Dia::Geometry2D::Circle wA = ComputeWorldCircle(a);
        Dia::Geometry2D::Circle wB = ComputeWorldCircle(b);
        return CircleVsCircle(wA, wB, normal, depth, point);
    }
    if (hasCircA && !hasCircB && !hasPolyB)
    {
        Dia::Geometry2D::Circle wA = ComputeWorldCircle(a);
        Dia::Geometry2D::AARect wB = ComputeWorldAABB(b);
        return CircleVsAARect(wA, wB, true, normal, depth, point);
    }
    if (!hasCircA && !hasPolyA && hasCircB)
    {
        Dia::Geometry2D::Circle wB = ComputeWorldCircle(b);
        Dia::Geometry2D::AARect wA = ComputeWorldAABB(a);
        return CircleVsAARect(wB, wA, false, normal, depth, point);
    }
    if (!hasCircA && !hasPolyA && !hasCircB && !hasPolyB)
    {
        Dia::Geometry2D::AARect wA = ComputeWorldAABB(a);
        Dia::Geometry2D::AARect wB = ComputeWorldAABB(b);
        return AARectVsAARect(wA, wB, normal, depth, point);
    }
    // Polygon cases — offset vertices by transform position
    // (PolyVsPoly already reads vertices directly; for now use stored shapes)
    if (hasPolyA && hasPolyB)
        return PolyVsPoly(*a->GetPolyShape(), *b->GetPolyShape(), normal, depth, point);

    // Fallback: compute AABBs
    Dia::Geometry2D::AARect wA = ComputeWorldAABB(a);
    Dia::Geometry2D::AARect wB = ComputeWorldAABB(b);
    return AARectVsAARect(wA, wB, normal, depth, point);
}

// ---------------------------------------------------------------------------
// ShouldCollide
// ---------------------------------------------------------------------------

bool ShouldCollide(const Body2DBase& a, const Body2DBase& b)
{
    return (a.GetLayer() & b.GetMask()) != 0 && (b.GetLayer() & a.GetMask()) != 0;
}

// ---------------------------------------------------------------------------
// DetectCollisions
// ---------------------------------------------------------------------------

void DetectCollisions(
    Dia::Core::Containers::DynamicArrayC<PointBody2D*, kMaxPointBodies>& pointBodies,
    Dia::Core::Containers::DynamicArrayC<RigidBody2D*, kMaxRigidBodies>& rigidBodies,
    Dia::Geometry2D::ISpatialStructure<Body2DBase*>*                      broadPhase,
    Dia::Core::Containers::DynamicArrayC<Contact, kMaxContacts>&          outContacts)
{
    outContacts.RemoveAll();

    if (!broadPhase) return;

    Dia::Core::Containers::HashTable<BodyPairKey, bool> visited;
    visited.SetSize(kMaxContacts, kMaxContacts * 2);

    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<Body2DBase*>, Dia::Geometry2D::kMaxQueryResults> queryResults;

    auto processBody = [&](Body2DBase* bodyA)
    {
        if (!bodyA->GetCircleShape() && !bodyA->GetPolyShape()) return;

        Dia::Geometry2D::AARect bounds = ComputeWorldAABB(bodyA);
        queryResults.RemoveAll();
        broadPhase->QueryRegion(bounds, queryResults);

        for (unsigned int qi = 0; qi < queryResults.Size(); ++qi)
        {
            Body2DBase* const* resolved = broadPhase->Resolve(queryResults[qi]);
            if (!resolved || !*resolved) continue;
            Body2DBase* bodyB = *resolved;
            if (bodyB == bodyA) continue;

            if (bodyA->GetBodyType() == BodyType::kStatic &&
                bodyB->GetBodyType() == BodyType::kStatic)
                continue;

            if (!ShouldCollide(*bodyA, *bodyB)) continue;

            if (!bodyA->IsAwake() && bodyB->IsAwake())  bodyA->Wake();
            if (!bodyB->IsAwake() && bodyA->IsAwake())  bodyB->Wake();

            BodyPairKey key(bodyA, bodyB);
            if (visited.ContainsKey(key)) continue;
            visited.Add(key, true);

            if (outContacts.IsFull()) return;

            Dia::Maths::Vector2D normal;
            float depth = 0.0f;
            Dia::Maths::Vector2D contactPt;

            if (NarrowPhase(bodyA, bodyB, normal, depth, contactPt))
            {
                outContacts.Add(MakeContact(bodyA, bodyB, normal, depth, contactPt));
            }
        }
    };

    for (unsigned int i = 0; i < pointBodies.Size(); ++i)
        processBody(pointBodies[i]);

    for (unsigned int i = 0; i < rigidBodies.Size(); ++i)
        processBody(rigidBodies[i]);
}

} // namespace Dia::RigidBody2D
