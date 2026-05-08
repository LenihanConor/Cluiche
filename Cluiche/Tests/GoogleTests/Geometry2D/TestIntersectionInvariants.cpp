#include <gtest/gtest.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/IntersectionClassify.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <random>

using namespace Dia::Geometry2D;
using namespace Dia::Maths;

static std::mt19937 MakeRng() { return std::mt19937(0xC0DE1234u); }

static const int kSamples = 300;

// ---------------------------------------------------------------------------
// Circle invariants
// ---------------------------------------------------------------------------

TEST(DiaMaths_IntersectionInvariants, Circle_PointInsideRadius_AlwaysIntersects)
{
    auto rng = MakeRng();
    std::uniform_real_distribution<float> posDist(-50.0f, 50.0f);
    std::uniform_real_distribution<float> radDist(0.5f, 20.0f);

    for (int i = 0; i < kSamples; ++i)
    {
        float cx = posDist(rng);
        float cy = posDist(rng);
        float r  = radDist(rng);

        Circle circle(r, Vector2D(cx, cy));

        // Pick a point strictly inside: offset by less than r in both axes
        std::uniform_real_distribution<float> innerDist(-r * 0.4f, r * 0.4f);
        Vector2D inside(cx + innerDist(rng), cy + innerDist(rng));

        IntersectionClassify result = circle.IsIntersecting(inside);
        EXPECT_TRUE(result.IsIntersecting())
            << "Point inside circle should intersect at sample " << i;
    }
}

TEST(DiaMaths_IntersectionInvariants, Circle_PointFarOutside_NeverIntersects)
{
    auto rng = MakeRng();
    std::uniform_real_distribution<float> radDist(0.5f, 5.0f);

    for (int i = 0; i < kSamples; ++i)
    {
        float r = radDist(rng);
        Circle circle(r, Vector2D(0.0f, 0.0f));

        // Point 3x the radius away — always outside
        float offset = r * 3.0f;
        Vector2D outside(offset, 0.0f);

        IntersectionClassify result = circle.IsIntersecting(outside);
        EXPECT_FALSE(result.IsIntersecting())
            << "Point far outside circle should not intersect at sample " << i;
    }
}

TEST(DiaMaths_IntersectionInvariants, Circle_SelfIntersection_AlwaysIntersects)
{
    auto rng = MakeRng();
    std::uniform_real_distribution<float> posDist(-50.0f, 50.0f);
    std::uniform_real_distribution<float> radDist(0.1f, 20.0f);

    for (int i = 0; i < kSamples; ++i)
    {
        float cx = posDist(rng);
        float cy = posDist(rng);
        float r  = radDist(rng);
        Circle circle(r, Vector2D(cx, cy));

        IntersectionClassify result = circle.IsIntersecting(circle);
        EXPECT_TRUE(result.IsIntersecting())
            << "Circle should intersect with itself at sample " << i;
    }
}

TEST(DiaMaths_IntersectionInvariants, Circle_Symmetry_TwoCirclesSymmetric)
{
    // IsIntersecting(A, B) == IsIntersecting(B, A) for two circles
    auto rng = MakeRng();
    std::uniform_real_distribution<float> posDist(-20.0f, 20.0f);
    std::uniform_real_distribution<float> radDist(0.5f, 5.0f);

    for (int i = 0; i < kSamples; ++i)
    {
        Circle a(radDist(rng), Vector2D(posDist(rng), posDist(rng)));
        Circle b(radDist(rng), Vector2D(posDist(rng), posDist(rng)));

        bool ab = a.IsIntersecting(b).IsIntersecting();
        bool ba = b.IsIntersecting(a).IsIntersecting();
        EXPECT_EQ(ab, ba)
            << "Circle intersection not symmetric at sample " << i;
    }
}

// ---------------------------------------------------------------------------
// AARect invariants
// ---------------------------------------------------------------------------

TEST(DiaMaths_IntersectionInvariants, AARect_PointInsideBounds_AlwaysIntersects)
{
    auto rng = MakeRng();
    std::uniform_real_distribution<float> posDist(-50.0f, 50.0f);
    std::uniform_real_distribution<float> sizeDist(1.0f, 20.0f);

    for (int i = 0; i < kSamples; ++i)
    {
        float x = posDist(rng);
        float y = posDist(rng);
        float w = sizeDist(rng);
        float h = sizeDist(rng);

        AARect rect(Vector2D(x, y), Vector2D(x + w, y + h));

        // Point at the center — always inside
        Vector2D center(x + w * 0.5f, y + h * 0.5f);
        IntersectionClassify result = rect.IsIntersecting(center);
        EXPECT_TRUE(result.IsIntersecting())
            << "Center point should be inside AARect at sample " << i;
    }
}

TEST(DiaMaths_IntersectionInvariants, AARect_PointFarOutside_NeverIntersects)
{
    auto rng = MakeRng();
    std::uniform_real_distribution<float> sizeDist(1.0f, 10.0f);

    for (int i = 0; i < kSamples; ++i)
    {
        float w = sizeDist(rng);
        float h = sizeDist(rng);

        AARect rect(Vector2D(0.0f, 0.0f), Vector2D(w, h));

        // Point far to the right
        Vector2D outside(w * 3.0f, h * 0.5f);
        IntersectionClassify result = rect.IsIntersecting(outside);
        EXPECT_FALSE(result.IsIntersecting())
            << "Point far outside AARect should not intersect at sample " << i;
    }
}

TEST(DiaMaths_IntersectionInvariants, AARect_CenterPoint_AlwaysInside)
{
    // A rect always contains its own center point
    AARect r(Vector2D(1.0f, 1.0f), Vector2D(3.0f, 3.0f));
    Vector2D center(2.0f, 2.0f);
    IntersectionClassify result = r.IsIntersecting(center);
    EXPECT_TRUE(result.IsIntersecting());
}
