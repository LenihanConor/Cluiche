// S-1 cow.worldgen.* registration through SimCommandSet (worldsim-kit task 30, AC 9 S-1 half).
#include <Modules/WorldGen/WorldGenerationModule.h>   // first: RandomStream.h must precede windows.h (min/max macros)
#include <gtest/gtest.h>

#include <Shared/Debug/SimCommandSet.h>
#include <Shared/Debug/Testing/FakeSimCommandHost.h>

#include <string>

namespace CoW {
namespace Debug {
namespace Testing {

namespace {

struct Expected
{
    const char* name;
    EntryKind kind;
};

// The 18 names and kinds the module registered before the migration, in registration order.
const Expected kExpected[] = {
    { "cow.worldgen.set", EntryKind::kCommand },
    { "cow.worldgen.set_size", EntryKind::kCommand },
    { "cow.worldgen.regenerate", EntryKind::kCommand },
    { "cow.worldgen.cancel", EntryKind::kCommand },
    { "cow.worldgen.preset.load", EntryKind::kCommand },
    { "cow.worldgen.preset.save", EntryKind::kCommand },
    { "cow.worldgen.export", EntryKind::kCommand },
    { "cow.worldgen.import", EntryKind::kCommand },
    { "cow.worldgen.validate", EntryKind::kCommand },
    { "cow.worldgen.worlds", EntryKind::kQuery },
    { "cow.worldgen.params", EntryKind::kCommand },
    { "cow.worldgen.status", EntryKind::kQuery },
    { "cow.worldgen.presets", EntryKind::kQuery },
    { "cow.worldgen.layers", EntryKind::kQuery },
    { "cow.worldgen.cell", EntryKind::kQuery },
    { "cow.worldgen.regions", EntryKind::kQuery },
    { "cow.worldgen.sites", EntryKind::kQuery },
    { "cow.worldgen.report", EntryKind::kQuery },
};
const uint32_t kExpectedCount = sizeof(kExpected) / sizeof(kExpected[0]);

}  // namespace

TEST(WorldGenCommandSet, AllEighteenRegisterWithTheirOriginalKinds)
{
    ASSERT_EQ(kExpectedCount, 18u);
    FakeSimCommandHost host;
    CoW::WorldGenerationModule module{ Dia::Core::StringCRC("WgSetTest") };
    ASSERT_TRUE(module.RegisterCommandsOn(host));
    EXPECT_TRUE(module.CommandsRegistered());
    ASSERT_EQ(host.regCount, kExpectedCount);
    for (uint32_t i = 0; i < kExpectedCount; ++i)
    {
        SCOPED_TRACE(kExpected[i].name);
        EXPECT_EQ(host.regNames[i], Dia::Core::StringCRC(kExpected[i].name));
        EXPECT_EQ(host.regKinds[i], kExpected[i].kind);
    }
    EXPECT_EQ(host.unregCount, 0u);
}

TEST(WorldGenCommandSet, ForcedFailureAtEntryNRollsBackEarlierEntriesByKind)
{
    for (uint32_t n : { 1u, 2u, 9u, 10u, 18u })
    {
        SCOPED_TRACE(n);
        FakeSimCommandHost host;
        host.failOnNth = n;
        CoW::WorldGenerationModule module{ Dia::Core::StringCRC("WgSetTest") };
        EXPECT_FALSE(module.RegisterCommandsOn(host));
        EXPECT_FALSE(module.CommandsRegistered());
        ASSERT_EQ(host.regCount, n - 1);
        ASSERT_EQ(host.unregCount, n - 1);
        for (uint32_t i = 0; i < n - 1; ++i)
        {
            // Unregistered in reverse order, each with the kind it registered with.
            const uint32_t r = n - 2 - i;
            EXPECT_EQ(host.unregNames[i], host.regNames[r]);
            EXPECT_EQ(host.unregKinds[i], kExpected[r].kind);
        }
    }
}

TEST(WorldGenCommandSet, UnregisterRemovesAllEighteenByKind)
{
    FakeSimCommandHost host;
    CoW::WorldGenerationModule module{ Dia::Core::StringCRC("WgSetTest") };
    ASSERT_TRUE(module.RegisterCommandsOn(host));
    module.UnregisterCommands();
    EXPECT_FALSE(module.CommandsRegistered());
    ASSERT_EQ(host.unregCount, kExpectedCount);
    for (uint32_t i = 0; i < kExpectedCount; ++i)
        EXPECT_EQ(host.unregKinds[i], kExpected[kExpectedCount - 1 - i].kind);
}

}  // namespace Testing
}  // namespace Debug
}  // namespace CoW
