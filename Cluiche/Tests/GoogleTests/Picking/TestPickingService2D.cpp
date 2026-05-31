////////////////////////////////////////////////////////////////////////////////
// Filename: TestPickingService2D.cpp
// Tests: PickingService2D::Pick — priority ordering, layer mask, no hits
//        PickingService2D::PickArea — overlapping, non-overlapping
// AC6, AC7, AC8
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaGeometry2DPicking/PickingService2D.h>
#include <DiaGeometry2DPicking/IPickable2D.h>
#include <DiaGeometry2DPicking/PickHit2D.h>
#include <DiaPicking/PickLayer.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::Geometry2D;
using namespace Dia::Maths;
using namespace Dia::Geometry2DPicking;
using namespace Dia::Picking;

// ---------------------------------------------------------------------------
// Stub IPickable2D for testing
// ---------------------------------------------------------------------------
class StubPickable : public IPickable2D
{
public:
    Dia::Core::StringCRC mId;
    int                  mPriority;
    PickLayer            mLayer;
    bool                 mShouldHit;
    AARect               mBounds; // for PickArea hit check

    StubPickable(const char* id, int priority, PickLayer layer, bool hit,
                 AARect bounds = AARect())
        : mId(id), mPriority(priority), mLayer(layer), mShouldHit(hit), mBounds(bounds) {}

    Dia::Core::StringCRC GetPickableId() const override { return mId; }
    int                  GetPriority()   const override { return mPriority; }
    PickLayer            GetLayer()      const override { return mLayer; }

    bool TryPick(const Vector2D& /*worldPos*/, PickHit2D& out) const override
    {
        if (!mShouldHit) return false;
        out.pickableId = mId;
        out.priority   = mPriority;
        out.kind       = PickHit2D::Kind::kObject;
        out.objectIdx  = 0;
        return true;
    }

    void PickArea(const AARect& worldRect,
                  Dia::Core::Containers::DynamicArrayC<PickHit2D, kMaxAreaHits>& out) const override
    {
        // Check if bounds overlaps worldRect
        const Vector2D bl1 = mBounds.GetBottomLeft();
        const Vector2D tr1 = mBounds.GetTopRight();
        const Vector2D bl2 = worldRect.GetBottomLeft();
        const Vector2D tr2 = worldRect.GetTopRight();
        if (tr1.x < bl2.x || bl1.x > tr2.x || tr1.y < bl2.y || bl1.y > tr2.y) return;
        if (out.IsFull()) return;
        PickHit2D hit;
        hit.pickableId = mId;
        hit.priority   = mPriority;
        hit.kind       = PickHit2D::Kind::kObject;
        out.Add(hit);
    }
};

// ---------------------------------------------------------------------------
// Pick tests
// ---------------------------------------------------------------------------

TEST(PickingService2D_Pick, NoPickables_ReturnsEmpty)
{
    PickingService2D service;
    auto result = service.Pick(Vector2D(0.0f, 0.0f));
    EXPECT_FALSE(result.HasHit());
}

TEST(PickingService2D_Pick, OneHit_ReturnsThatHit)
{
    PickingService2D service;
    StubPickable p("A", 5, PickLayer::kDefault, true);
    service.Register(&p);

    auto result = service.Pick(Vector2D(0.0f, 0.0f));
    EXPECT_TRUE(result.HasHit());
    EXPECT_EQ(result.Best().pickableId, Dia::Core::StringCRC("A"));
}

TEST(PickingService2D_Pick, MultipleHits_SortedByPriorityDescending)
{
    PickingService2D service;
    StubPickable low("low", 1, PickLayer::kDefault, true);
    StubPickable mid("mid", 5, PickLayer::kDefault, true);
    StubPickable hi("hi",  10, PickLayer::kDefault, true);
    service.Register(&low);
    service.Register(&mid);
    service.Register(&hi);

    auto result = service.Pick(Vector2D(0.0f, 0.0f));
    EXPECT_EQ(result.Count(), 3u);
    EXPECT_EQ(result.Best().priority, 10);
    EXPECT_EQ(result[1].priority, 5);
    EXPECT_EQ(result[2].priority, 1);
}

TEST(PickingService2D_Pick, LayerMaskExcludesNonMatchingPickable)
{
    PickingService2D service;
    StubPickable unit("unit",    5, PickLayer::kUnit,    true);
    StubPickable terrain("ter",  5, PickLayer::kTerrain, true);
    service.Register(&unit);
    service.Register(&terrain);

    auto result = service.Pick(Vector2D(0.0f, 0.0f),
                               static_cast<PickLayerMask>(PickLayer::kUnit));
    EXPECT_EQ(result.Count(), 1u);
    EXPECT_EQ(result.Best().pickableId, Dia::Core::StringCRC("unit"));
}

TEST(PickingService2D_Pick, UnregisterRemovesPickable)
{
    PickingService2D service;
    StubPickable p("X", 5, PickLayer::kDefault, true);
    service.Register(&p);
    service.Unregister(&p);

    auto result = service.Pick(Vector2D(0.0f, 0.0f));
    EXPECT_FALSE(result.HasHit());
}

TEST(PickingService2D_Pick, MissingPickable_NoHit)
{
    PickingService2D service;
    StubPickable p("miss", 5, PickLayer::kDefault, false);
    service.Register(&p);

    auto result = service.Pick(Vector2D(0.0f, 0.0f));
    EXPECT_FALSE(result.HasHit());
}

// ---------------------------------------------------------------------------
// PickArea tests
// ---------------------------------------------------------------------------

TEST(PickingService2D_PickArea, OverlappingPickable_ReturnsHit)
{
    PickingService2D service;
    AARect bounds(Vector2D(0.0f, 0.0f), Vector2D(10.0f, 10.0f));
    StubPickable p("A", 5, PickLayer::kDefault, false, bounds);
    service.Register(&p);

    AARect query(Vector2D(5.0f, 5.0f), Vector2D(15.0f, 15.0f));
    auto result = service.PickArea(query);
    EXPECT_TRUE(result.HasHit());
}

TEST(PickingService2D_PickArea, NonOverlappingPickable_NoHit)
{
    PickingService2D service;
    AARect bounds(Vector2D(0.0f, 0.0f), Vector2D(5.0f, 5.0f));
    StubPickable p("A", 5, PickLayer::kDefault, false, bounds);
    service.Register(&p);

    AARect query(Vector2D(100.0f, 100.0f), Vector2D(200.0f, 200.0f));
    auto result = service.PickArea(query);
    EXPECT_FALSE(result.HasHit());
}

TEST(PickingService2D_PickArea, LayerMaskFilters)
{
    PickingService2D service;
    AARect bounds(Vector2D(0.0f, 0.0f), Vector2D(10.0f, 10.0f));
    StubPickable unit("unit",    5, PickLayer::kUnit,    false, bounds);
    StubPickable terrain("ter",  5, PickLayer::kTerrain, false, bounds);
    service.Register(&unit);
    service.Register(&terrain);

    AARect query(Vector2D(0.0f, 0.0f), Vector2D(15.0f, 15.0f));
    auto result = service.PickArea(query, static_cast<PickLayerMask>(PickLayer::kUnit));
    EXPECT_EQ(result.Count(), 1u);
    EXPECT_EQ(result.Best().pickableId, Dia::Core::StringCRC("unit"));
}
