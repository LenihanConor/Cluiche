#include <gtest/gtest.h>
#include <DiaAICallout/CalloutRegistry.h>
#include <DiaAICallout/Testing/CalloutTestHelpers.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::AICallout;
using namespace Dia::AICallout::Testing;
using namespace Dia::Core::Containers;

TEST(CalloutClaimReleaseTests, ClaimReturnsTrueOnSuccess)
{
    CalloutRegistry registry;
    CalloutHandle handle = EmitTestCallout(registry, Dia::Core::StringCRC("HelpNeeded"), Dia::Maths::Vector2D(0.0f, 0.0f));
    EXPECT_TRUE(registry.Claim(handle, Dia::Core::StringCRC("EntityA")));
}

TEST(CalloutClaimReleaseTests, ClaimSetsIsClaimed)
{
    CalloutRegistry registry;
    CalloutHandle handle = EmitTestCallout(registry, Dia::Core::StringCRC("HelpNeeded"), Dia::Maths::Vector2D(0.0f, 0.0f));
    registry.Claim(handle, Dia::Core::StringCRC("EntityA"));
    EXPECT_TRUE(handle.IsClaimed());
}

TEST(CalloutClaimReleaseTests, DoubleClaimReturnsFalse)
{
    CalloutRegistry registry;
    CalloutHandle handle = EmitTestCallout(registry, Dia::Core::StringCRC("HelpNeeded"), Dia::Maths::Vector2D(0.0f, 0.0f));
    Dia::Core::StringCRC entity("EntityA");
    EXPECT_TRUE(registry.Claim(handle, entity));
    EXPECT_FALSE(registry.Claim(handle, entity));
}

TEST(CalloutClaimReleaseTests, DifferentClaimerCantClaim)
{
    CalloutRegistry registry;
    CalloutHandle handle = EmitTestCallout(registry, Dia::Core::StringCRC("HelpNeeded"), Dia::Maths::Vector2D(0.0f, 0.0f));
    EXPECT_TRUE(registry.Claim(handle, Dia::Core::StringCRC("EntityA")));
    EXPECT_FALSE(registry.Claim(handle, Dia::Core::StringCRC("EntityB")));
}

TEST(CalloutClaimReleaseTests, ReleaseUnclaims)
{
    CalloutRegistry registry;
    CalloutHandle handle = EmitTestCallout(registry, Dia::Core::StringCRC("HelpNeeded"), Dia::Maths::Vector2D(0.0f, 0.0f));
    Dia::Core::StringCRC entity("EntityA");
    registry.Claim(handle, entity);
    registry.Release(handle, entity);
    EXPECT_FALSE(handle.IsClaimed());
}

TEST(CalloutClaimReleaseTests, ReleaseWrongClaimerIsNoOp)
{
    CalloutRegistry registry;
    CalloutHandle handle = EmitTestCallout(registry, Dia::Core::StringCRC("HelpNeeded"), Dia::Maths::Vector2D(0.0f, 0.0f));
    registry.Claim(handle, Dia::Core::StringCRC("EntityA"));
    registry.Release(handle, Dia::Core::StringCRC("EntityB")); // wrong claimer
    EXPECT_TRUE(handle.IsClaimed()); // still claimed by EntityA
}

TEST(CalloutClaimReleaseTests, ReleaseOnAlreadyUnclaimedIsNoOp)
{
    CalloutRegistry registry;
    CalloutHandle handle = EmitTestCallout(registry, Dia::Core::StringCRC("HelpNeeded"), Dia::Maths::Vector2D(0.0f, 0.0f));
    // Release without ever claiming — should not crash
    registry.Release(handle, Dia::Core::StringCRC("EntityA"));
    EXPECT_FALSE(handle.IsClaimed());
}

TEST(CalloutClaimReleaseTests, ReleasedCalloutAppearsInQuery)
{
    CalloutRegistry registry;
    Dia::Core::StringCRC kind("HelpNeeded");
    CalloutHandle handle = EmitTestCallout(registry, kind, Dia::Maths::Vector2D(0.0f, 0.0f));
    Dia::Core::StringCRC entity("EntityA");
    registry.Claim(handle, entity);
    registry.Release(handle, entity);

    QueryFilter filter{ kind, Dia::Maths::Vector2D(0.0f, 0.0f), 200.0f, Dia::Core::StringCRC::kZero };
    DynamicArrayC<CalloutHandle, 16> results;
    registry.Query(filter, results);

    AssertInQueryResults(results, handle);
}

TEST(CalloutClaimReleaseTests, ReleaseOnExpiredHandleIsNoOp)
{
    CalloutRegistry registry;
    CalloutHandle handle = EmitTestCallout(registry, Dia::Core::StringCRC("HelpNeeded"), Dia::Maths::Vector2D(0.0f, 0.0f), 100.0f, 1.0f);
    registry.Update(1.0f); // expire the slot
    // Release on expired handle — should not crash
    registry.Release(handle, Dia::Core::StringCRC("EntityA"));
}

TEST(CalloutClaimReleaseTests, ClaimExpiredHandleReturnsFalse)
{
    CalloutRegistry registry;
    const CalloutHandle handle = EmitTestCallout(registry, Dia::Core::StringCRC("alert"),
                                                  Dia::Maths::Vector2D(0.0f, 0.0f), 100.0f, 1.0f);
    registry.Update(2.0f);
    ASSERT_FALSE(handle.IsValid());
    EXPECT_FALSE(registry.Claim(handle, Dia::Core::StringCRC("entity")));
}

TEST(CalloutClaimReleaseTests, GetOnClaimedHandleReturnsData)
{
    CalloutRegistry registry;
    const Dia::Core::StringCRC kind("supply");
    const CalloutHandle handle = EmitTestCallout(registry, kind,
                                                  Dia::Maths::Vector2D(1.0f, 2.0f));
    registry.Claim(handle, Dia::Core::StringCRC("entityA"));

    ASSERT_TRUE(handle.IsValid());
    ASSERT_TRUE(handle.IsClaimed());
    const Callout* data = handle.Get();
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->kind, kind);
}
