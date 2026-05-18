#include "DiaGeometry3D/Intersection/IntersectionTests.h"

#include <cmath>

namespace Dia::Geometry3D
{
    namespace
    {
        static inline float Clampf(float v, float lo, float hi)
        {
            return v < lo ? lo : (v > hi ? hi : v);
        }

        static inline float Maxf(float a, float b) { return a > b ? a : b; }
        static inline float Minf(float a, float b) { return a < b ? a : b; }
        static inline float Absf(float v)           { return v < 0.0f ? -v : v; }

        static inline IntersectionClassify SwapContainment(IntersectionClassify r)
        {
            if (r == IntersectionClassify::kAContainsB) return IntersectionClassify::kBContainsA;
            if (r == IntersectionClassify::kBContainsA) return IntersectionClassify::kAContainsB;
            return r;
        }

        static inline void ProjectAABB(const AABB& box,
                                       const Dia::Maths::Vector3D& axis,
                                       float& outC, float& outH)
        {
            Dia::Maths::Vector3D c = box.CalculateCenter();
            Dia::Maths::Vector3D e = box.CalculateExtents();
            outC = c.Dot(axis);
            outH = Absf(e.X() * axis.X()) + Absf(e.Y() * axis.Y()) + Absf(e.Z() * axis.Z());
        }

        static inline void ProjectOOBB(const OOBB& box,
                                       const Dia::Maths::Vector3D& axis,
                                       float& outC, float& outH)
        {
            Dia::Maths::Vector3D axX, axY, axZ;
            box.GetAxes(axX, axY, axZ);
            const Dia::Maths::Vector3D& he = box.GetHalfExtents();
            outC = box.GetCenter().Dot(axis);
            outH = Absf(he.X() * axX.Dot(axis))
                 + Absf(he.Y() * axY.Dot(axis))
                 + Absf(he.Z() * axZ.Dot(axis));
        }

        static inline bool OverlapOnAxis(float cA, float hA, float cB, float hB)
        {
            return Absf(cA - cB) <= hA + hB;
        }

        static IntersectionClassify OOBBvsOOBBSAT(const OOBB& a, const OOBB& b)
        {
            Dia::Maths::Vector3D axA[3], axB[3];
            a.GetAxes(axA[0], axA[1], axA[2]);
            b.GetAxes(axB[0], axB[1], axB[2]);

            Dia::Maths::Vector3D axes[15];
            axes[0] = axA[0]; axes[1] = axA[1]; axes[2] = axA[2];
            axes[3] = axB[0]; axes[4] = axB[1]; axes[5] = axB[2];
            int idx = 6;
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    axes[idx++] = axA[i].Cross(axB[j]);

            for (int i = 0; i < 15; ++i)
            {
                if (axes[i].SquareMagnitude() < 1e-10f) continue;
                float cA, hA, cB, hB;
                ProjectOOBB(a, axes[i], cA, hA);
                ProjectOOBB(b, axes[i], cB, hB);
                if (!OverlapOnAxis(cA, hA, cB, hB)) return IntersectionClassify::kNoIntersection;
            }

            bool aMightContainB = true, bMightContainA = true;
            for (int i = 0; i < 15; ++i)
            {
                if (axes[i].SquareMagnitude() < 1e-10f) continue;
                float cA, hA, cB, hB;
                ProjectOOBB(a, axes[i], cA, hA);
                ProjectOOBB(b, axes[i], cB, hB);
                if (!(cB - hB >= cA - hA && cB + hB <= cA + hA)) aMightContainB = false;
                if (!(cA - hA >= cB - hB && cA + hA <= cB + hB)) bMightContainA = false;
            }
            if (aMightContainB) return IntersectionClassify::kAContainsB;
            if (bMightContainA) return IntersectionClassify::kBContainsA;
            return IntersectionClassify::kPenetrating;
        }

        static inline Dia::Maths::Vector3D ClosestPointOnSegment(
            const Dia::Maths::Vector3D& a,
            const Dia::Maths::Vector3D& b,
            const Dia::Maths::Vector3D& p)
        {
            Dia::Maths::Vector3D ab = b - a;
            float len2 = ab.SquareMagnitude();
            if (len2 < 1e-12f) return a;
            float t = Clampf((p - a).Dot(ab) / len2, 0.0f, 1.0f);
            return a + ab * t;
        }

        // Christer Ericson's barycentric closest point on triangle
        static Dia::Maths::Vector3D ClosestPointOnTriangle(
            const Dia::Maths::Vector3D& a,
            const Dia::Maths::Vector3D& b,
            const Dia::Maths::Vector3D& c,
            const Dia::Maths::Vector3D& p)
        {
            Dia::Maths::Vector3D ab = b - a, ac = c - a, ap = p - a;
            float d1 = ab.Dot(ap), d2 = ac.Dot(ap);
            if (d1 <= 0.0f && d2 <= 0.0f) return a;

            Dia::Maths::Vector3D bp = p - b;
            float d3 = ab.Dot(bp), d4 = ac.Dot(bp);
            if (d3 >= 0.0f && d4 <= d3) return b;

            float vc = d1*d4 - d3*d2;
            if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
                float v = d1 / (d1 - d3);
                return a + ab * v;
            }

            Dia::Maths::Vector3D cp = p - c;
            float d5 = ab.Dot(cp), d6 = ac.Dot(cp);
            if (d6 >= 0.0f && d5 <= d6) return c;

            float vb = d5*d2 - d1*d6;
            if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
                float w = d2 / (d2 - d6);
                return a + ac * w;
            }

            float va = d3*d6 - d5*d4;
            if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
                float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
                return b + (c - b) * w;
            }

            float denom = 1.0f / (va + vb + vc);
            float v = vb * denom;
            float w = vc * denom;
            return a + ab * v + ac * w;
        }
    } // anonymous namespace

    // =========================================================================
    // vs-Frustum
    // =========================================================================

    static inline int ClassifyAABBPlane(const AABB& box, const Plane& plane)
    {
        const Dia::Maths::Vector3D& n = plane.GetNormal();
        Dia::Maths::Vector3D c = box.CalculateCenter();
        Dia::Maths::Vector3D e = box.CalculateExtents();
        float r = Absf(e.X() * n.X()) + Absf(e.Y() * n.Y()) + Absf(e.Z() * n.Z());
        float d = n.X()*c.X() + n.Y()*c.Y() + n.Z()*c.Z() - plane.GetD();
        if (d - r >  0.0f) return  1;
        if (d + r <  0.0f) return -1;
        return 0;
    }

    IntersectionClassify IntersectionTests::Test(const AABB& a, const Frustum& b)
    {
        bool allInside = true;
        for (int i = 0; i < 6; ++i)
        {
            int side = ClassifyAABBPlane(a, b.GetPlane(static_cast<FrustumPlane>(i)));
            if (side < 0) return IntersectionClassify::kNoIntersection;
            if (side == 0) allInside = false;
        }
        if (allInside) return IntersectionClassify::kBContainsA;

        // Check AABB contains frustum via frustum's AABB bounds
        AABB frustumBounds = b.CalculateAABB();
        if (a.GetMin().X() <= frustumBounds.GetMin().X() && a.GetMin().Y() <= frustumBounds.GetMin().Y() && a.GetMin().Z() <= frustumBounds.GetMin().Z()
         && a.GetMax().X() >= frustumBounds.GetMax().X() && a.GetMax().Y() >= frustumBounds.GetMax().Y() && a.GetMax().Z() >= frustumBounds.GetMax().Z())
            return IntersectionClassify::kAContainsB;

        return IntersectionClassify::kPenetrating;
    }

    IntersectionClassify IntersectionTests::Test(const Frustum& a, const AABB& b)
    {
        return SwapContainment(Test(b, a));
    }

    IntersectionClassify IntersectionTests::Test(const Sphere& a, const Frustum& b)
    {
        bool allInside = true;
        for (int i = 0; i < 6; ++i)
        {
            float dist = b.GetPlane(static_cast<FrustumPlane>(i)).DistanceTo(a.GetCenter());
            if (dist < -a.GetRadius()) return IntersectionClassify::kNoIntersection;
            if (dist <  a.GetRadius()) allInside = false;
        }
        if (allInside) return IntersectionClassify::kBContainsA;
        return IntersectionClassify::kPenetrating;
    }

    IntersectionClassify IntersectionTests::Test(const Frustum& a, const Sphere& b)
    {
        return SwapContainment(Test(b, a));
    }

    IntersectionClassify IntersectionTests::Test(const Triangle& a, const Frustum& b)
    {
        int v0In = 1, v1In = 1, v2In = 1;
        for (int i = 0; i < 6; ++i)
        {
            const Plane& plane = b.GetPlane(static_cast<FrustumPlane>(i));
            if (plane.DistanceTo(a.GetV0()) < 0.0f) v0In = 0;
            if (plane.DistanceTo(a.GetV1()) < 0.0f) v1In = 0;
            if (plane.DistanceTo(a.GetV2()) < 0.0f) v2In = 0;
        }
        if (v0In == 0 && v1In == 0 && v2In == 0) return IntersectionClassify::kNoIntersection;
        if (v0In == 1 && v1In == 1 && v2In == 1) return IntersectionClassify::kBContainsA;
        return IntersectionClassify::kPenetrating;
    }

    IntersectionClassify IntersectionTests::Test(const Frustum& a, const Triangle& b)
    {
        return SwapContainment(Test(b, a));
    }

    // =========================================================================
    // Bounding-volume pairs
    // =========================================================================

    IntersectionClassify IntersectionTests::Test(const AABB& a, const AABB& b)
    {
        if (a.GetMax().X() < b.GetMin().X() || b.GetMax().X() < a.GetMin().X()) return IntersectionClassify::kNoIntersection;
        if (a.GetMax().Y() < b.GetMin().Y() || b.GetMax().Y() < a.GetMin().Y()) return IntersectionClassify::kNoIntersection;
        if (a.GetMax().Z() < b.GetMin().Z() || b.GetMax().Z() < a.GetMin().Z()) return IntersectionClassify::kNoIntersection;

        if (a.GetMin().X() <= b.GetMin().X() && a.GetMax().X() >= b.GetMax().X()
         && a.GetMin().Y() <= b.GetMin().Y() && a.GetMax().Y() >= b.GetMax().Y()
         && a.GetMin().Z() <= b.GetMin().Z() && a.GetMax().Z() >= b.GetMax().Z())
            return IntersectionClassify::kAContainsB;

        if (b.GetMin().X() <= a.GetMin().X() && b.GetMax().X() >= a.GetMax().X()
         && b.GetMin().Y() <= a.GetMin().Y() && b.GetMax().Y() >= a.GetMax().Y()
         && b.GetMin().Z() <= a.GetMin().Z() && b.GetMax().Z() >= a.GetMax().Z())
            return IntersectionClassify::kBContainsA;

        return IntersectionClassify::kPenetrating;
    }

    static IntersectionClassify AABBSphereImpl(const AABB& aabb, const Sphere& sphere)
    {
        float cx = Clampf(sphere.GetCenter().X(), aabb.GetMin().X(), aabb.GetMax().X());
        float cy = Clampf(sphere.GetCenter().Y(), aabb.GetMin().Y(), aabb.GetMax().Y());
        float cz = Clampf(sphere.GetCenter().Z(), aabb.GetMin().Z(), aabb.GetMax().Z());
        float dx = sphere.GetCenter().X() - cx;
        float dy = sphere.GetCenter().Y() - cy;
        float dz = sphere.GetCenter().Z() - cz;
        float sqDist = dx*dx + dy*dy + dz*dz;
        float r = sphere.GetRadius();

        if (sqDist > r * r) return IntersectionClassify::kNoIntersection;

        // Farthest AABB corner from sphere center
        float fx = sphere.GetCenter().X() < aabb.CalculateCenter().X() ? aabb.GetMax().X() : aabb.GetMin().X();
        float fy = sphere.GetCenter().Y() < aabb.CalculateCenter().Y() ? aabb.GetMax().Y() : aabb.GetMin().Y();
        float fz = sphere.GetCenter().Z() < aabb.CalculateCenter().Z() ? aabb.GetMax().Z() : aabb.GetMin().Z();
        float farSqDist = (fx - sphere.GetCenter().X())*(fx - sphere.GetCenter().X())
                        + (fy - sphere.GetCenter().Y())*(fy - sphere.GetCenter().Y())
                        + (fz - sphere.GetCenter().Z())*(fz - sphere.GetCenter().Z());
        if (farSqDist <= r * r) return IntersectionClassify::kBContainsA;

        if (sphere.GetCenter().X() - r >= aabb.GetMin().X() && sphere.GetCenter().X() + r <= aabb.GetMax().X()
         && sphere.GetCenter().Y() - r >= aabb.GetMin().Y() && sphere.GetCenter().Y() + r <= aabb.GetMax().Y()
         && sphere.GetCenter().Z() - r >= aabb.GetMin().Z() && sphere.GetCenter().Z() + r <= aabb.GetMax().Z())
            return IntersectionClassify::kAContainsB;

        return IntersectionClassify::kPenetrating;
    }

    IntersectionClassify IntersectionTests::Test(const AABB& a, const Sphere& b)   { return AABBSphereImpl(a, b); }
    IntersectionClassify IntersectionTests::Test(const Sphere& a, const AABB& b)   { return SwapContainment(AABBSphereImpl(b, a)); }

    IntersectionClassify IntersectionTests::Test(const Sphere& a, const Sphere& b)
    {
        float dx = a.GetCenter().X() - b.GetCenter().X();
        float dy = a.GetCenter().Y() - b.GetCenter().Y();
        float dz = a.GetCenter().Z() - b.GetCenter().Z();
        float distSq = dx*dx + dy*dy + dz*dz;
        float rSum = a.GetRadius() + b.GetRadius();
        if (distSq > rSum * rSum) return IntersectionClassify::kNoIntersection;
        float dist = sqrtf(distSq);
        if (dist + b.GetRadius() <= a.GetRadius()) return IntersectionClassify::kAContainsB;
        if (dist + a.GetRadius() <= b.GetRadius()) return IntersectionClassify::kBContainsA;
        return IntersectionClassify::kPenetrating;
    }

    static IntersectionClassify AABBOOBBImpl(const AABB& a, const OOBB& b)
    {
        Dia::Maths::Vector3D bX, bY, bZ;
        b.GetAxes(bX, bY, bZ);

        Dia::Maths::Vector3D worldAxes[3] = {
            Dia::Maths::Vector3D(1,0,0), Dia::Maths::Vector3D(0,1,0), Dia::Maths::Vector3D(0,0,1)
        };
        Dia::Maths::Vector3D obbAxes[3] = { bX, bY, bZ };

        Dia::Maths::Vector3D axes[15];
        axes[0] = worldAxes[0]; axes[1] = worldAxes[1]; axes[2] = worldAxes[2];
        axes[3] = obbAxes[0];   axes[4] = obbAxes[1];   axes[5] = obbAxes[2];
        int idx = 6;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                axes[idx++] = worldAxes[i].Cross(obbAxes[j]);

        for (int i = 0; i < 15; ++i)
        {
            if (axes[i].SquareMagnitude() < 1e-10f) continue;
            float cA, hA, cB, hB;
            ProjectAABB(a, axes[i], cA, hA);
            ProjectOOBB(b, axes[i], cB, hB);
            if (!OverlapOnAxis(cA, hA, cB, hB)) return IntersectionClassify::kNoIntersection;
        }

        bool aMightContainB = true, bMightContainA = true;
        for (int i = 0; i < 15; ++i)
        {
            if (axes[i].SquareMagnitude() < 1e-10f) continue;
            float cA, hA, cB, hB;
            ProjectAABB(a, axes[i], cA, hA);
            ProjectOOBB(b, axes[i], cB, hB);
            if (!(cB - hB >= cA - hA && cB + hB <= cA + hA)) aMightContainB = false;
            if (!(cA - hA >= cB - hB && cA + hA <= cB + hB)) bMightContainA = false;
        }
        if (aMightContainB) return IntersectionClassify::kAContainsB;
        if (bMightContainA) return IntersectionClassify::kBContainsA;
        return IntersectionClassify::kPenetrating;
    }

    IntersectionClassify IntersectionTests::Test(const AABB& a, const OOBB& b) { return AABBOOBBImpl(a, b); }
    IntersectionClassify IntersectionTests::Test(const OOBB& a, const AABB& b) { return SwapContainment(AABBOOBBImpl(b, a)); }
    IntersectionClassify IntersectionTests::Test(const OOBB& a, const OOBB& b) { return OOBBvsOOBBSAT(a, b); }

    // =========================================================================
    // Triangle-vs-volume
    // =========================================================================

    static IntersectionClassify TriangleAABBImpl(const Triangle& tri, const AABB& box)
    {
        Dia::Maths::Vector3D c = box.CalculateCenter();
        Dia::Maths::Vector3D e = box.CalculateExtents();

        Dia::Maths::Vector3D v0 = tri.GetV0() - c;
        Dia::Maths::Vector3D v1 = tri.GetV1() - c;
        Dia::Maths::Vector3D v2 = tri.GetV2() - c;

        Dia::Maths::Vector3D f0 = v1 - v0, f1 = v2 - v1, f2 = v0 - v2;

        // 9 cross-product axes (Akenine-Möller)
        Dia::Maths::Vector3D edgeAxes[9] = {
            Dia::Maths::Vector3D(0.0f,  -f0.Z(),  f0.Y()),
            Dia::Maths::Vector3D(0.0f,  -f1.Z(),  f1.Y()),
            Dia::Maths::Vector3D(0.0f,  -f2.Z(),  f2.Y()),
            Dia::Maths::Vector3D( f0.Z(), 0.0f,  -f0.X()),
            Dia::Maths::Vector3D( f1.Z(), 0.0f,  -f1.X()),
            Dia::Maths::Vector3D( f2.Z(), 0.0f,  -f2.X()),
            Dia::Maths::Vector3D(-f0.Y(),  f0.X(), 0.0f),
            Dia::Maths::Vector3D(-f1.Y(),  f1.X(), 0.0f),
            Dia::Maths::Vector3D(-f2.Y(),  f2.X(), 0.0f),
        };

        for (int i = 0; i < 9; ++i)
        {
            const Dia::Maths::Vector3D& ax = edgeAxes[i];
            float p0 = ax.Dot(v0), p1 = ax.Dot(v1), p2 = ax.Dot(v2);
            float pmin = Minf(p0, Minf(p1, p2));
            float pmax = Maxf(p0, Maxf(p1, p2));
            float r = e.X()*Absf(ax.X()) + e.Y()*Absf(ax.Y()) + e.Z()*Absf(ax.Z());
            if (pmin > r || pmax < -r) return IntersectionClassify::kNoIntersection;
        }

        // 3 AABB face-normal axes
        if (Maxf(v0.X(), Maxf(v1.X(), v2.X())) < -e.X() || Minf(v0.X(), Minf(v1.X(), v2.X())) > e.X())
            return IntersectionClassify::kNoIntersection;
        if (Maxf(v0.Y(), Maxf(v1.Y(), v2.Y())) < -e.Y() || Minf(v0.Y(), Minf(v1.Y(), v2.Y())) > e.Y())
            return IntersectionClassify::kNoIntersection;
        if (Maxf(v0.Z(), Maxf(v1.Z(), v2.Z())) < -e.Z() || Minf(v0.Z(), Minf(v1.Z(), v2.Z())) > e.Z())
            return IntersectionClassify::kNoIntersection;

        // Triangle normal axis
        Dia::Maths::Vector3D n = f0.Cross(f1);
        float d = n.Dot(v0);
        float r = e.X()*Absf(n.X()) + e.Y()*Absf(n.Y()) + e.Z()*Absf(n.Z());
        if (d > r || d < -r) return IntersectionClassify::kNoIntersection;

        bool triInBox = (v0.X() >= -e.X() && v0.X() <= e.X() && v0.Y() >= -e.Y() && v0.Y() <= e.Y() && v0.Z() >= -e.Z() && v0.Z() <= e.Z())
                     && (v1.X() >= -e.X() && v1.X() <= e.X() && v1.Y() >= -e.Y() && v1.Y() <= e.Y() && v1.Z() >= -e.Z() && v1.Z() <= e.Z())
                     && (v2.X() >= -e.X() && v2.X() <= e.X() && v2.Y() >= -e.Y() && v2.Y() <= e.Y() && v2.Z() >= -e.Z() && v2.Z() <= e.Z());
        if (triInBox) return IntersectionClassify::kBContainsA;

        return IntersectionClassify::kPenetrating;
    }

    IntersectionClassify IntersectionTests::Test(const Triangle& a, const AABB& b)   { return TriangleAABBImpl(a, b); }
    IntersectionClassify IntersectionTests::Test(const AABB& a, const Triangle& b)   { return SwapContainment(TriangleAABBImpl(b, a)); }

    static IntersectionClassify TriangleSphereImpl(const Triangle& tri, const Sphere& sphere)
    {
        Dia::Maths::Vector3D closest = ClosestPointOnTriangle(tri.GetV0(), tri.GetV1(), tri.GetV2(), sphere.GetCenter());
        float dx = sphere.GetCenter().X() - closest.X();
        float dy = sphere.GetCenter().Y() - closest.Y();
        float dz = sphere.GetCenter().Z() - closest.Z();
        float distSq = dx*dx + dy*dy + dz*dz;
        float r = sphere.GetRadius();
        if (distSq > r * r) return IntersectionClassify::kNoIntersection;
        return IntersectionClassify::kPenetrating;
    }

    IntersectionClassify IntersectionTests::Test(const Triangle& a, const Sphere& b) { return TriangleSphereImpl(a, b); }
    IntersectionClassify IntersectionTests::Test(const Sphere& a, const Triangle& b) { return SwapContainment(TriangleSphereImpl(b, a)); }

    // =========================================================================
    // Ray casts
    // =========================================================================

    IntersectionClassify IntersectionTests::Test(const Ray& ray, const AABB& box)
    {
        const Dia::Maths::Vector3D& o = ray.GetOrigin();
        const Dia::Maths::Vector3D& d = ray.GetDirection();

        float tmin = 0.0f, tmax = 1e30f;

        auto testSlab = [&](float orig, float dir, float bmin, float bmax) -> bool {
            if (Absf(dir) < 1e-8f) {
                return orig >= bmin && orig <= bmax;
            }
            float t1 = (bmin - orig) / dir;
            float t2 = (bmax - orig) / dir;
            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
            tmin = Maxf(tmin, t1);
            tmax = Minf(tmax, t2);
            return tmin <= tmax;
        };

        if (!testSlab(o.X(), d.X(), box.GetMin().X(), box.GetMax().X())) return IntersectionClassify::kNoIntersection;
        if (!testSlab(o.Y(), d.Y(), box.GetMin().Y(), box.GetMax().Y())) return IntersectionClassify::kNoIntersection;
        if (!testSlab(o.Z(), d.Z(), box.GetMin().Z(), box.GetMax().Z())) return IntersectionClassify::kNoIntersection;
        if (tmax < 0.0f) return IntersectionClassify::kNoIntersection;
        return IntersectionClassify::kPenetrating;
    }

    IntersectionClassify IntersectionTests::Test(const Ray& ray, const Sphere& sphere)
    {
        Dia::Maths::Vector3D oc = ray.GetOrigin() - sphere.GetCenter();
        float a = ray.GetDirection().Dot(ray.GetDirection());
        float b = 2.0f * oc.Dot(ray.GetDirection());
        float c = oc.Dot(oc) - sphere.GetRadius() * sphere.GetRadius();
        float disc = b*b - 4.0f*a*c;
        if (disc < 0.0f) return IntersectionClassify::kNoIntersection;
        float sqrtDisc = sqrtf(disc);
        float t1 = (-b + sqrtDisc) / (2.0f * a);
        if (t1 < 0.0f) return IntersectionClassify::kNoIntersection;
        return IntersectionClassify::kPenetrating;
    }

    IntersectionClassify IntersectionTests::Test(const Ray& ray, const Triangle& tri)
    {
        // Möller–Trumbore
        const float kEps = 1e-7f;
        Dia::Maths::Vector3D e1 = tri.GetV1() - tri.GetV0();
        Dia::Maths::Vector3D e2 = tri.GetV2() - tri.GetV0();
        Dia::Maths::Vector3D h  = ray.GetDirection().Cross(e2);
        float det = e1.Dot(h);
        if (Absf(det) < kEps) return IntersectionClassify::kNoIntersection;

        float invDet = 1.0f / det;
        Dia::Maths::Vector3D s = ray.GetOrigin() - tri.GetV0();
        float u = invDet * s.Dot(h);
        if (u < 0.0f || u > 1.0f) return IntersectionClassify::kNoIntersection;

        Dia::Maths::Vector3D q = s.Cross(e1);
        float v = invDet * ray.GetDirection().Dot(q);
        if (v < 0.0f || u + v > 1.0f) return IntersectionClassify::kNoIntersection;

        float t = invDet * e2.Dot(q);
        if (t < 0.0f) return IntersectionClassify::kNoIntersection;
        return IntersectionClassify::kPenetrating;
    }

    IntersectionClassify IntersectionTests::Test(const Ray& ray, const Plane& plane)
    {
        float denom = plane.GetNormal().Dot(ray.GetDirection());
        if (Absf(denom) < 1e-8f) return IntersectionClassify::kNoIntersection;
        float t = (plane.GetD() - plane.GetNormal().Dot(ray.GetOrigin())) / denom;
        if (t < 0.0f) return IntersectionClassify::kNoIntersection;
        return IntersectionClassify::kPenetrating;
    }

    // =========================================================================
    // Contains
    // =========================================================================

    bool IntersectionTests::Contains(const AABB& shape, const Dia::Maths::Vector3D& point)
    {
        return point.X() >= shape.GetMin().X() && point.X() <= shape.GetMax().X()
            && point.Y() >= shape.GetMin().Y() && point.Y() <= shape.GetMax().Y()
            && point.Z() >= shape.GetMin().Z() && point.Z() <= shape.GetMax().Z();
    }

    bool IntersectionTests::Contains(const Sphere& shape, const Dia::Maths::Vector3D& point)
    {
        float dx = point.X() - shape.GetCenter().X();
        float dy = point.Y() - shape.GetCenter().Y();
        float dz = point.Z() - shape.GetCenter().Z();
        return dx*dx + dy*dy + dz*dz <= shape.GetRadius() * shape.GetRadius();
    }

    bool IntersectionTests::Contains(const OOBB& shape, const Dia::Maths::Vector3D& point)
    {
        Dia::Maths::Vector3D local = shape.GetOrientation().Inverse().Rotate(point - shape.GetCenter());
        const Dia::Maths::Vector3D& he = shape.GetHalfExtents();
        return Absf(local.X()) <= he.X() && Absf(local.Y()) <= he.Y() && Absf(local.Z()) <= he.Z();
    }

    bool IntersectionTests::Contains(const Capsule& shape, const Dia::Maths::Vector3D& point)
    {
        Dia::Maths::Vector3D closest = ClosestPointOnSegment(shape.GetStartA(), shape.GetEndB(), point);
        float dx = point.X() - closest.X();
        float dy = point.Y() - closest.Y();
        float dz = point.Z() - closest.Z();
        return dx*dx + dy*dy + dz*dz <= shape.GetRadius() * shape.GetRadius();
    }

    bool IntersectionTests::Contains(const Cylinder& shape, const Dia::Maths::Vector3D& point)
    {
        Dia::Maths::Vector3D axis = shape.GetEndB() - shape.GetStartA();
        float len2 = axis.SquareMagnitude();
        if (len2 < 1e-12f) return false;
        float t = (point - shape.GetStartA()).Dot(axis) / len2;
        if (t < 0.0f || t > 1.0f) return false;
        Dia::Maths::Vector3D proj = shape.GetStartA() + axis * t;
        float dx = point.X() - proj.X();
        float dy = point.Y() - proj.Y();
        float dz = point.Z() - proj.Z();
        return dx*dx + dy*dy + dz*dz <= shape.GetRadius() * shape.GetRadius();
    }

    bool IntersectionTests::Contains(const Frustum& shape, const Dia::Maths::Vector3D& point)
    {
        for (int i = 0; i < 6; ++i)
            if (shape.GetPlane(static_cast<FrustumPlane>(i)).DistanceTo(point) < 0.0f) return false;
        return true;
    }

    // =========================================================================
    // ClosestPoint
    // =========================================================================

    Dia::Maths::Vector3D IntersectionTests::ClosestPoint(const AABB& shape, const Dia::Maths::Vector3D& point)
    {
        return Dia::Maths::Vector3D(
            Clampf(point.X(), shape.GetMin().X(), shape.GetMax().X()),
            Clampf(point.Y(), shape.GetMin().Y(), shape.GetMax().Y()),
            Clampf(point.Z(), shape.GetMin().Z(), shape.GetMax().Z())
        );
    }

    Dia::Maths::Vector3D IntersectionTests::ClosestPoint(const Sphere& shape, const Dia::Maths::Vector3D& point)
    {
        Dia::Maths::Vector3D d = point - shape.GetCenter();
        float len = d.Magnitude();
        if (len < 1e-8f) return shape.GetCenter() + Dia::Maths::Vector3D(shape.GetRadius(), 0.0f, 0.0f);
        return shape.GetCenter() + d * (shape.GetRadius() / len);
    }

    Dia::Maths::Vector3D IntersectionTests::ClosestPoint(const OOBB& shape, const Dia::Maths::Vector3D& point)
    {
        Dia::Maths::Vector3D local = shape.GetOrientation().Inverse().Rotate(point - shape.GetCenter());
        const Dia::Maths::Vector3D& he = shape.GetHalfExtents();
        Dia::Maths::Vector3D clamped(
            Clampf(local.X(), -he.X(), he.X()),
            Clampf(local.Y(), -he.Y(), he.Y()),
            Clampf(local.Z(), -he.Z(), he.Z())
        );
        return shape.GetCenter() + shape.GetOrientation().Rotate(clamped);
    }

    Dia::Maths::Vector3D IntersectionTests::ClosestPoint(const Triangle& shape, const Dia::Maths::Vector3D& point)
    {
        return ClosestPointOnTriangle(shape.GetV0(), shape.GetV1(), shape.GetV2(), point);
    }

    Dia::Maths::Vector3D IntersectionTests::ClosestPoint(const Capsule& shape, const Dia::Maths::Vector3D& point)
    {
        Dia::Maths::Vector3D axisClosest = ClosestPointOnSegment(shape.GetStartA(), shape.GetEndB(), point);
        Dia::Maths::Vector3D d = point - axisClosest;
        float len = d.Magnitude();
        if (len < 1e-8f) return axisClosest + Dia::Maths::Vector3D(shape.GetRadius(), 0.0f, 0.0f);
        return axisClosest + d * (shape.GetRadius() / len);
    }

    Dia::Maths::Vector3D IntersectionTests::ClosestPoint(const Cylinder& shape, const Dia::Maths::Vector3D& point)
    {
        Dia::Maths::Vector3D axis = shape.GetEndB() - shape.GetStartA();
        float len2 = axis.SquareMagnitude();
        float t = (len2 > 1e-12f)
            ? Clampf((point - shape.GetStartA()).Dot(axis) / len2, 0.0f, 1.0f)
            : 0.0f;
        Dia::Maths::Vector3D axisPoint = shape.GetStartA() + axis * t;
        Dia::Maths::Vector3D radial = point - axisPoint;
        float rlen = radial.Magnitude();
        if (rlen < 1e-8f) {
            // On axis — pick perpendicular that isn't parallel to axis direction
            Dia::Maths::Vector3D perp(1.0f, 0.0f, 0.0f);
            if (len2 > 1e-12f) {
                Dia::Maths::Vector3D axDir = axis * (1.0f / sqrtf(len2));
                if (Absf(axDir.Dot(perp)) > 0.9f)
                    perp = Dia::Maths::Vector3D(0.0f, 1.0f, 0.0f);
            }
            return axisPoint + perp * shape.GetRadius();
        }
        float clampedR = Minf(rlen, shape.GetRadius());
        return axisPoint + radial * (clampedR / rlen);
    }

} // namespace Dia::Geometry3D
