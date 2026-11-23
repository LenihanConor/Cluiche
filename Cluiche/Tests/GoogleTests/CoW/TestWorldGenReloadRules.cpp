// Suite: CoW
// WorldGenSession::ReloadRules: re-snapshot the rules from a ParameterFile after a world exists (worldsim kit task 32).

#include <gtest/gtest.h>

#include <DiaAttribute/ParameterFile.h>
#include <DraoiHexWorldTerrain/WorldGenSession.h>
#include <Shared/WorldGeneration/CoWTesting/CoWTerrainRulesTestHelpers.h>
#include <DiaMaths/Core/RandomStream.h>

#include <cstdint>
#include <cstring>
#include <string>

using Dia::Attribute::ParameterFile;
using Dia::Attribute::ParamError;
using Dia::Core::StringCRC;

namespace
{
    // FNV-1a over every cell's elevation bits, terrain and landmass.
    uint64_t CellDataHash(const Draoi::HexWorld::WorldStructure& w)
    {
        uint64_t h = 1469598103934665603ull;
        auto mix = [&h](uint64_t v) { h ^= v; h *= 1099511628211ull; };
        const Draoi::HexWorld::Cell* cells = w.Cells();
        for (int i = 0; i < w.CellCount(); ++i)
        {
            uint32_t bits = 0;
            static_assert(sizeof(bits) == sizeof(cells[i].elevation));
            std::memcpy(&bits, &cells[i].elevation, sizeof(bits));
            mix(bits);
            mix(static_cast<uint64_t>(cells[i].terrain));
            mix(cells[i].landmass);
        }
        return h;
    }
}

TEST(WorldGenReloadRules, ChangedParameterChangesTheNextGeneratedWorld)
{
    CoW::WorldGeneration::TestSession s;
    Dia::Maths::RandomStream rng(1);
    ASSERT_TRUE(s.Tick(rng));
    const uint64_t before = CellDataHash(s.World());
    const float oldCoast = s.Rules().kCoastElev;

    ParameterFile file;
    ParamError err;
    ASSERT_TRUE(file.Load(CoW::WorldGeneration::TestSidecarText().c_str(), "terrain_rules.schema.json", nullptr, err));
    ASSERT_TRUE(file.Set(StringCRC("kCoastElev"), Json::Value(0.1), err)) << err.key.AsCStr() << ": " << err.reason.AsCStr();

    ASSERT_TRUE(s.ReloadRules(file));
    EXPECT_NE(s.Rules().kCoastElev, oldCoast);
    EXPECT_FLOAT_EQ(s.Rules().kCoastElev, 0.1f);
    EXPECT_TRUE(s.IsDirty()) << "the next generate must use the new rules";

    ASSERT_TRUE(s.Tick(rng));
    EXPECT_NE(CellDataHash(s.World()), before) << "regenerated terrain must differ";
}

TEST(WorldGenReloadRules, UnloadedFileIsRefusedAndChangesNothing)
{
    CoW::WorldGeneration::TestSession s;
    Dia::Maths::RandomStream rng(1);
    ASSERT_TRUE(s.Tick(rng));
    const uint64_t before = CellDataHash(s.World());
    const float oldCoast = s.Rules().kCoastElev;

    ParameterFile unloaded;
    EXPECT_FALSE(s.ReloadRules(unloaded));
    EXPECT_FLOAT_EQ(s.Rules().kCoastElev, oldCoast);
    EXPECT_EQ(CellDataHash(s.World()), before);
}
