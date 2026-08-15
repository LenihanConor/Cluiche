#include <gtest/gtest.h>
#include <DiaAICallout/CalloutRegistry.h>
#include <DiaAICallout/Testing/CalloutTestHelpers.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::AICallout;
using namespace Dia::AICallout::Testing;

TEST(CalloutEmitTests, EmitReturnsValidHandle)
{
    CalloutRegistry registry;
    CalloutHandle handle = EmitTestCallout(registry, Dia::Core::StringCRC("HelpNeeded"), Dia::Maths::Vector2D(0.0f, 0.0f));
    EXPECT_TRUE(handle.IsValid());
}

TEST(CalloutEmitTests, GetLiveCountIncrementsOnEmit)
{
    CalloutRegistry registry;
    EmitTestCallout(registry, Dia::Core::StringCRC("HelpNeeded"), Dia::Maths::Vector2D(0.0f, 0.0f));
    EmitTestCallout(registry, Dia::Core::StringCRC("HelpNeeded"), Dia::Maths::Vector2D(10.0f, 0.0f));
    EmitTestCallout(registry, Dia::Core::StringCRC("HelpNeeded"), Dia::Maths::Vector2D(20.0f, 0.0f));
    EXPECT_EQ(registry.GetLiveCount(), 3);
}

TEST(CalloutEmitTests, EmitDefaultConstructedHandleIsInvalid)
{
    CalloutHandle handle;
    EXPECT_FALSE(handle.IsValid());
}

TEST(CalloutEmitTests, EmitGetReturnsCalloutData)
{
    CalloutRegistry registry;
    Dia::Core::StringCRC kind("AttackPosition");
    CalloutHandle handle = EmitTestCallout(registry, kind, Dia::Maths::Vector2D(5.0f, 5.0f));

    const Callout* data = handle.Get();
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->kind, kind);
}

TEST(CalloutEmitTests, GenerationNonZeroAfterEmit)
{
    CalloutRegistry registry;
    CalloutHandle handle = EmitTestCallout(registry, Dia::Core::StringCRC("Retreat"), Dia::Maths::Vector2D(0.0f, 0.0f));
    EXPECT_NE(handle.GetGeneration(), 0u);
}

TEST(CalloutEmitTests, DefaultHandleIsClaimedFalse)
{
    const CalloutHandle handle;
    EXPECT_FALSE(handle.IsClaimed());
}

TEST(CalloutEmitTests, DefaultHandleGetReturnsNullptr)
{
    const CalloutHandle handle;
    EXPECT_EQ(handle.Get(), nullptr);
}
