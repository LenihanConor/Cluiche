#include <Modules/WorldGen/WorldGenerationModule.h>   // first: RandomStream.h must precede windows.h (min/max macros)

#include <gtest/gtest.h>

#include <string.h>
#include <string>

#include <DraoiHexWorldTerrain/WorldLayerSource.h>
#include <Shared/World/WorldSimNames.h>

// Worldsim-kit AC 14: every name S-1 registers matches the WorldSimNames pattern.
// Spec: docs/specs/applications/cow/systems/worldstep/worldsim-kit.md (section 5)

namespace CoW {
namespace World {
namespace Testing {

namespace {

// True when every '.'-separated segment of verb is snake_case (S-1 has dotted subcommands, e.g. preset.load).
bool IsDottedSnakeCase(const std::string& verb)
{
    size_t start = 0;
    while (true)
    {
        const size_t dot = verb.find('.', start);
        const std::string seg = verb.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
        if (!IsSnakeCase(seg.c_str())) { return false; }
        if (dot == std::string::npos) { return true; }
        start = dot + 1;
    }
}

}  // namespace

TEST(WorldSimNamesRegistrationsTest, S1CommandNamesMatchPattern)
{
    // Prefix comes from the builder, not a literal.
    const std::string prefix = std::string(CommandName(kWorldgen, "x").c_str()).substr(0, strlen("cow.worldgen."));
    ASSERT_STREQ("cow.worldgen.", prefix.c_str());

    ASSERT_EQ(18u, CoW::WorldGenerationModule::kCommandCount);
    for (unsigned i = 0; i < CoW::WorldGenerationModule::kCommandCount; ++i)
    {
        const std::string name = CoW::WorldGenerationModule::kCommandNames[i];
        ASSERT_EQ(0u, name.find(prefix)) << name;
        const std::string verb = name.substr(prefix.size());
        EXPECT_TRUE(IsDottedSnakeCase(verb)) << name;
        if (verb.find('.') == std::string::npos)
        {
            // Undotted verbs must be exactly what the builder produces.
            EXPECT_STREQ(name.c_str(), CommandName(kWorldgen, verb.c_str()).c_str()) << name;
        }
    }
}

TEST(WorldSimNamesRegistrationsTest, S1SaveIdAndStageIds)
{
    // S-1 registers no save participant (worlds export to .wld.json); the builder gives the reserved id.
    EXPECT_STREQ("cow.worldgen", SaveId(kWorldgen).c_str());

    ASSERT_EQ(3u, kStageOrderCount);
    for (size_t i = 0; i < kStageOrderCount; ++i)
    {
        EXPECT_TRUE(IsKnownToken(kStageOrder[i])) << kStageOrder[i];
        EXPECT_STREQ(kStageOrder[i], StageId(kStageOrder[i]).c_str());
    }
    EXPECT_STREQ("weather", kStageOrder[0]);
    EXPECT_STREQ("hydro", kStageOrder[1]);
    EXPECT_STREQ("ecology", kStageOrder[2]);
}

TEST(WorldSimNamesRegistrationsTest, S1LayerSchemaFamilyMatchesBuilder)
{
    const std::string key = LayerSchemaKey(kWorldgen, "x").c_str();
    const std::string family = std::string(Draoi::HexWorld::kWorldLayerSchemaFor) + "/x";
    EXPECT_EQ(family, key);
}

// DISABLED until Task 34 renames the 35 layer ids (today camelCase, no "worldgen." prefix) and their schema keys.
// Task 34 removes the DISABLED_ prefix.
TEST(WorldSimNamesRegistrationsTest, DISABLED_S1LayerIdsMatchPattern)
{
    const uint32_t count = Draoi::HexWorld::WorldLayerSource::Count();
    ASSERT_EQ(35u, count);
    const std::string idPrefix = std::string(kWorldgen) + ".";
    for (uint32_t i = 0; i < count; ++i)
    {
        const Draoi::HexWorld::WorldLayerDesc& d = Draoi::HexWorld::WorldLayerSource::At(i);
        const std::string id = d.id;
        ASSERT_EQ(0u, id.find(idPrefix)) << id;
        const std::string field = id.substr(idPrefix.size());
        EXPECT_STREQ(id.c_str(), LayerId(kWorldgen, field.c_str()).c_str()) << id;
        EXPECT_STREQ(d.schemaKey, LayerSchemaKey(kWorldgen, field.c_str()).c_str()) << id;
    }
}

}  // namespace Testing
}  // namespace World
}  // namespace CoW
