#include <gtest/gtest.h>
#include <DiaAICallout/CalloutRegistry.h>
#include <DiaAICallout/Testing/CalloutTestHelpers.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::AICallout;
using namespace Dia::AICallout::Testing;

TEST(CalloutTTLTests, UpdateDecreasesTTL)
{
    CalloutRegistry registry;
    CalloutHandle handle = EmitTestCallout(registry, Dia::Core::StringCRC("HelpNeeded"), Dia::Maths::Vector2D(0.0f, 0.0f), 100.0f, 5.0f);
    registry.Update(2.0f);
    EXPECT_TRUE(handle.IsValid()); // 3.0s remaining
}

TEST(CalloutTTLTests, UpdateExpiresAtZero)
{
    CalloutRegistry registry;
    CalloutHandle handle = EmitTestCallout(registry, Dia::Core::StringCRC("HelpNeeded"), Dia::Maths::Vector2D(0.0f, 0.0f), 100.0f, 1.0f);
    registry.Update(1.0f);
    EXPECT_FALSE(handle.IsValid());
}

TEST(CalloutTTLTests, UpdateExpiresAtNegative)
{
    CalloutRegistry registry;
    CalloutHandle handle = EmitTestCallout(registry, Dia::Core::StringCRC("HelpNeeded"), Dia::Maths::Vector2D(0.0f, 0.0f), 100.0f, 1.0f);
    registry.Update(2.0f);
    EXPECT_FALSE(handle.IsValid());
}

TEST(CalloutTTLTests, GetLiveCountDecreasesOnExpiry)
{
    CalloutRegistry registry;
    Dia::Core::StringCRC kind("HelpNeeded");
    EmitTestCallout(registry, kind, Dia::Maths::Vector2D(0.0f, 0.0f), 100.0f, 1.0f);
    EmitTestCallout(registry, kind, Dia::Maths::Vector2D(10.0f, 0.0f), 100.0f, 10.0f);
    EmitTestCallout(registry, kind, Dia::Maths::Vector2D(20.0f, 0.0f), 100.0f, 10.0f);

    registry.Update(1.0f); // expires first callout

    EXPECT_EQ(registry.GetLiveCount(), 2);
}

TEST(CalloutTTLTests, SlotReusedAfterExpiry)
{
    CalloutRegistry registry;
    Dia::Core::StringCRC kind("HelpNeeded");
    CalloutHandle first = EmitTestCallout(registry, kind, Dia::Maths::Vector2D(0.0f, 0.0f), 100.0f, 1.0f);
    uint32_t firstIndex = first.GetIndex();

    registry.Update(1.0f); // expire the slot

    // Emit a new callout — pool is otherwise empty so it reuses slot 0
    CalloutHandle second = EmitTestCallout(registry, kind, Dia::Maths::Vector2D(0.0f, 0.0f), 100.0f, 10.0f);

    EXPECT_EQ(second.GetIndex(), firstIndex);
    EXPECT_TRUE(second.IsValid());
}

TEST(CalloutTTLTests, GenerationIncrementedOnReuse)
{
    CalloutRegistry registry;
    Dia::Core::StringCRC kind("HelpNeeded");
    CalloutHandle first = EmitTestCallout(registry, kind, Dia::Maths::Vector2D(0.0f, 0.0f), 100.0f, 1.0f);
    uint32_t firstGen = first.GetGeneration();

    registry.Update(1.0f); // expire the slot

    CalloutHandle second = EmitTestCallout(registry, kind, Dia::Maths::Vector2D(0.0f, 0.0f), 100.0f, 10.0f);

    EXPECT_GT(second.GetGeneration(), firstGen);
}

TEST(CalloutTTLTests, StaleHandleInvalidAfterSlotReuse)
{
    CalloutRegistry registry;
    Dia::Core::StringCRC kind("HelpNeeded");
    CalloutHandle stale = EmitTestCallout(registry, kind, Dia::Maths::Vector2D(0.0f, 0.0f), 100.0f, 1.0f);

    registry.Update(1.0f); // expire the slot

    // Re-emit — slot reused at higher generation
    EmitTestCallout(registry, kind, Dia::Maths::Vector2D(0.0f, 0.0f), 100.0f, 10.0f);

    EXPECT_FALSE(stale.IsValid());
}

TEST(CalloutTTLTests, MultipleUpdatesAccumulate)
{
    CalloutRegistry registry;
    CalloutHandle handle = EmitTestCallout(registry, Dia::Core::StringCRC("HelpNeeded"), Dia::Maths::Vector2D(0.0f, 0.0f), 100.0f, 5.0f);
    registry.Update(2.0f); // remaining: 3.0
    registry.Update(2.0f); // remaining: 1.0
    registry.Update(2.0f); // remaining: -1.0 — expired
    EXPECT_FALSE(handle.IsValid());
}

TEST(CalloutTTLTests, UpdateOnlyExpiredNotAffectsOthers)
{
    CalloutRegistry registry;
    Dia::Core::StringCRC kind("HelpNeeded");
    CalloutHandle shortLived = EmitTestCallout(registry, kind, Dia::Maths::Vector2D(0.0f, 0.0f), 100.0f, 1.0f);
    CalloutHandle longLived  = EmitTestCallout(registry, kind, Dia::Maths::Vector2D(10.0f, 0.0f), 100.0f, 10.0f);

    registry.Update(1.0f); // expires shortLived only

    EXPECT_FALSE(shortLived.IsValid());
    EXPECT_TRUE(longLived.IsValid());
}

TEST(CalloutTTLTests, ExpiryWhileClaimed_ClearsClaimState)
{
    CalloutRegistry registry;
    const CalloutHandle handle = EmitTestCallout(registry, Dia::Core::StringCRC("rescue"),
                                                  Dia::Maths::Vector2D(0.0f, 0.0f), 100.0f, 1.0f);
    ASSERT_TRUE(registry.Claim(handle, Dia::Core::StringCRC("entityA")));
    ASSERT_TRUE(handle.IsClaimed());

    registry.Update(2.0f);

    EXPECT_FALSE(handle.IsValid());
    const CalloutHandle newHandle = EmitTestCallout(registry, Dia::Core::StringCRC("rescue"),
                                                     Dia::Maths::Vector2D(0.0f, 0.0f));
    EXPECT_TRUE(newHandle.IsValid());
    EXPECT_FALSE(newHandle.IsClaimed());
    EXPECT_EQ(registry.GetLiveCount(), 1);
}

TEST(CalloutTTLTests, ExpiryWhileClaimed_SlotQueryableAfterReemit)
{
    CalloutRegistry registry;
    const Dia::Core::StringCRC kind("guard");
    const Dia::Maths::Vector2D pos(0.0f, 0.0f);

    const CalloutHandle h1 = EmitTestCallout(registry, kind, pos, 100.0f, 1.0f);
    registry.Claim(h1, Dia::Core::StringCRC("entityA"));
    registry.Update(2.0f);

    const CalloutHandle h2 = EmitTestCallout(registry, kind, pos, 100.0f, 10.0f);

    Dia::Core::Containers::DynamicArrayC<CalloutHandle, 8> results;
    const QueryFilter filter{kind, pos, 200.0f, Dia::Core::StringCRC::kZero};
    registry.Query(filter, results);
    AssertInQueryResults(results, h2);
}
