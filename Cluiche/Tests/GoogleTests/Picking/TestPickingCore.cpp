////////////////////////////////////////////////////////////////////////////////
// Filename: TestPickingCore.cpp
// Tests: PickResult, PickEvent, PickLayer, PickAddress, PickRouter
// AC1, AC9
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaPicking/PickResult.h>
#include <DiaPicking/PickEvent.h>
#include <DiaPicking/PickTrigger.h>
#include <DiaPicking/PickLayer.h>
#include <DiaPicking/PickAddress.h>
#include <DiaPicking/PickRouter.h>

using namespace Dia::Picking;
using namespace Dia::Mailbox;

// ---------------------------------------------------------------------------
// Minimal hit type for testing PickResult
// ---------------------------------------------------------------------------
struct TestHit
{
    int priority = 0;
    int id       = 0;
};

// ---------------------------------------------------------------------------
// PickResult
// ---------------------------------------------------------------------------

TEST(PickResult, EmptyHasNoHit)
{
    PickResult<TestHit> result;
    EXPECT_FALSE(result.HasHit());
    EXPECT_EQ(result.Count(), 0u);
}

TEST(PickResult, SingleHit)
{
    PickResult<TestHit> result;
    TestHit h; h.priority = 5; h.id = 1;
    result.Add(h);
    EXPECT_TRUE(result.HasHit());
    EXPECT_EQ(result.Count(), 1u);
    EXPECT_EQ(result.Best().id, 1);
}

TEST(PickResult, SortedByPriorityDescending)
{
    PickResult<TestHit> result;
    TestHit a; a.priority = 1; a.id = 1;
    TestHit b; b.priority = 10; b.id = 10;
    TestHit c; c.priority = 5; c.id = 5;
    result.Add(a);
    result.Add(b);
    result.Add(c);
    EXPECT_EQ(result.Count(), 3u);
    EXPECT_EQ(result.Best().priority, 10);
    EXPECT_EQ(result[0].priority, 10);
    EXPECT_EQ(result[1].priority, 5);
    EXPECT_EQ(result[2].priority, 1);
}

TEST(PickResult, AddDoesNotExceedMax)
{
    PickResult<TestHit, 2> result;
    TestHit h; h.priority = 1; h.id = 1;
    result.Add(h);
    result.Add(h);
    result.Add(h); // should be silently ignored
    EXPECT_EQ(result.Count(), 2u);
}

TEST(PickResult, ClearResetsToEmpty)
{
    PickResult<TestHit> result;
    TestHit h; h.priority = 1;
    result.Add(h);
    result.Clear();
    EXPECT_FALSE(result.HasHit());
    EXPECT_EQ(result.Count(), 0u);
}

// ---------------------------------------------------------------------------
// PickLayer
// ---------------------------------------------------------------------------

TEST(PickLayer, LayerMatchesOwnBit)
{
    EXPECT_TRUE(LayerMatches(PickLayer::kUnit, LayerBit(PickLayer::kUnit)));
    EXPECT_TRUE(LayerMatches(PickLayer::kTerrain, LayerBit(PickLayer::kTerrain)));
}

TEST(PickLayer, LayerDoesNotMatchOtherBit)
{
    EXPECT_FALSE(LayerMatches(PickLayer::kUnit, LayerBit(PickLayer::kTerrain)));
}

TEST(PickLayer, kAllMatchesAnyLayer)
{
    PickLayerMask all = static_cast<PickLayerMask>(PickLayer::kAll);
    EXPECT_TRUE(LayerMatches(PickLayer::kDefault, all));
    EXPECT_TRUE(LayerMatches(PickLayer::kUnit, all));
    EXPECT_TRUE(LayerMatches(PickLayer::kDebug, all));
}

// ---------------------------------------------------------------------------
// PickAddress
// ---------------------------------------------------------------------------

TEST(PickAddress, ForTriggerEncodesCorrectly)
{
    Address clickAddr = PickAddress::ForTrigger(PickTrigger::kClick);
    EXPECT_EQ(clickAddr.routerId, PickAddress::kRouterId);
    EXPECT_EQ(PickAddress::TriggerFromAddress(clickAddr), PickTrigger::kClick);

    Address hoverAddr = PickAddress::ForTrigger(PickTrigger::kHover);
    EXPECT_EQ(PickAddress::TriggerFromAddress(hoverAddr), PickTrigger::kHover);
}

TEST(PickAddress, DifferentTriggersHaveDifferentPayload)
{
    Address click = PickAddress::ForTrigger(PickTrigger::kClick);
    Address hover = PickAddress::ForTrigger(PickTrigger::kHover);
    EXPECT_NE(click.payload, hover.payload);
}

// ---------------------------------------------------------------------------
// PickRouter
// ---------------------------------------------------------------------------

TEST(PickRouter, GetRouterId_MatchesPickAddress)
{
    PickRouter router;
    EXPECT_EQ(router.GetRouterId(), PickAddress::kRouterId);
}

TEST(PickRouter, ResolveClickAddress_DeliversToClickSubscriber)
{
    PickRouter router;
    SubscriberId sub1; sub1.value = 1;
    SubscriberId sub2; sub2.value = 2;

    router.SubscribeToTrigger(PickTrigger::kClick, sub1);
    router.SubscribeToTrigger(PickTrigger::kHover, sub2);

    SubscriberSet live;
    SubscriberSet matched;
    router.Resolve(PickAddress::ForTrigger(PickTrigger::kClick), live, matched);

    EXPECT_EQ(matched.Size(), 1u);
    EXPECT_EQ(matched[0].value, 1u);
}

TEST(PickRouter, ResolveHoverAddress_DoesNotDeliverToClickSubscriber)
{
    PickRouter router;
    SubscriberId sub; sub.value = 99;
    router.SubscribeToTrigger(PickTrigger::kClick, sub);

    SubscriberSet live, matched;
    router.Resolve(PickAddress::ForTrigger(PickTrigger::kHover), live, matched);

    EXPECT_EQ(matched.Size(), 0u);
}

TEST(PickRouter, UnsubscribeRemovesSubscriber)
{
    PickRouter router;
    SubscriberId sub; sub.value = 7;
    router.SubscribeToTrigger(PickTrigger::kClick, sub);
    router.UnsubscribeFromTrigger(PickTrigger::kClick, sub);

    SubscriberSet live, matched;
    router.Resolve(PickAddress::ForTrigger(PickTrigger::kClick), live, matched);

    EXPECT_EQ(matched.Size(), 0u);
}

TEST(PickRouter, MultipleSubscribersForSameTrigger)
{
    PickRouter router;
    SubscriberId s1; s1.value = 1;
    SubscriberId s2; s2.value = 2;
    SubscriberId s3; s3.value = 3;
    router.SubscribeToTrigger(PickTrigger::kClick, s1);
    router.SubscribeToTrigger(PickTrigger::kClick, s2);
    router.SubscribeToTrigger(PickTrigger::kClick, s3);

    SubscriberSet live, matched;
    router.Resolve(PickAddress::ForTrigger(PickTrigger::kClick), live, matched);

    EXPECT_EQ(matched.Size(), 3u);
}

// ---------------------------------------------------------------------------
// PickEvent
// ---------------------------------------------------------------------------

TEST(PickEvent, ConstructedWithTriggerAndHits)
{
    PickEvent<TestHit> evt;
    evt.trigger = PickTrigger::kClick;
    TestHit h; h.priority = 5;
    evt.hits.Add(h);

    EXPECT_EQ(evt.trigger, PickTrigger::kClick);
    EXPECT_TRUE(evt.hits.HasHit());
}
