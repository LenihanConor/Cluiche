// S-1 bus messages must be trivially copyable (the bus recorder asserts it).
#include <gtest/gtest.h>
#include <Modules/WorldGenerationMessages.h>
#include <type_traits>

using namespace CoW;

static_assert(std::is_trivially_copyable_v<WorldGenerationComplete>,
              "WorldGenerationComplete must be trivially copyable to ride the message bus");

TEST(WorldGenMessages, CompleteIsTriviallyCopyable)
{
    EXPECT_TRUE(std::is_trivially_copyable_v<WorldGenerationComplete>);
}

TEST(WorldGenMessages, CompleteKeepsItsTypeId)
{
    EXPECT_EQ(WorldGenerationComplete::kTypeId, Dia::Core::StringCRC("CoW.WorldGenerationComplete"));
}

TEST(WorldGenMessages, CompleteDefaultsToZero)
{
    const WorldGenerationComplete m{};
    EXPECT_EQ(m.seed, 0u);
    EXPECT_EQ(m.retryAttempts, 0u);
    EXPECT_EQ(m.worldRevision, 0u);
}
