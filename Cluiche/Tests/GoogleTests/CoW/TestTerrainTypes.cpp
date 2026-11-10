// S-1 Terrain Generator task 1: shared terrain vocabulary and shape parameters.
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/TerrainTypes.h>
#include <set>
#include <string>
#include <type_traits>

using namespace CoW::WorldGeneration;

static_assert(std::is_trivially_copyable_v<WorldShapeParams>, "WorldShapeParams must be trivially copyable");
static_assert(std::is_trivially_copyable_v<WorldProvenance>, "WorldProvenance must be trivially copyable");
static_assert(kTerrainGeneratorVersion == 1, "generator version");

TEST(TerrainTypes, DefaultsAreContinental)
{
    const WorldShapeParams p;
    EXPECT_FLOAT_EQ(p.landFraction, 0.42f);
    EXPECT_EQ(p.continents, 1);
    EXPECT_EQ(p.islands, 4);
    EXPECT_FLOAT_EQ(p.continentIslands, 0.7f);
    EXPECT_EQ(p.peninsulas, 2);
    EXPECT_FLOAT_EQ(p.coastline, 0.5f);
    EXPECT_FLOAT_EQ(p.mountainousness, 0.55f);
    EXPECT_FLOAT_EQ(p.spineToRing, 0.15f);
    EXPECT_FLOAT_EQ(p.orientationDeg, 30.0f);
    EXPECT_EQ(p.passes, 2);
    EXPECT_FLOAT_EQ(p.barrier, 0.6f);
    EXPECT_EQ(p.barrierForm, BarrierForm::Plateau);
    EXPECT_FLOAT_EQ(p.ruggedness, 0.4f);
    EXPECT_FLOAT_EQ(p.rivers, 0.5f);
    EXPECT_FLOAT_EQ(p.valleyDepth, 0.4f);
    EXPECT_EQ(p.lakes, 4);
    EXPECT_FLOAT_EQ(p.latitudeNorthDeg, 60.0f);
    EXPECT_FLOAT_EQ(p.latitudeSouthDeg, 20.0f);
    EXPECT_FLOAT_EQ(p.temperatureOffset, 0.0f);
    EXPECT_FLOAT_EQ(p.moistureOffset, 0.0f);
    EXPECT_TRUE(p == WorldShapeParams{});
}

TEST(TerrainTypes, ClampLeavesDefaultsUnchanged)
{
    WorldShapeParams p;
    p.Clamp();
    EXPECT_TRUE(p == WorldShapeParams{});
}

TEST(TerrainTypes, ClampHighValues)
{
    WorldShapeParams p;
    p.landFraction = 5.f; p.continents = 200; p.islands = 200; p.continentIslands = 5.f;
    p.peninsulas = 200; p.coastline = 5.f; p.mountainousness = 5.f; p.spineToRing = 5.f;
    p.passes = 200; p.barrier = 5.f; p.ruggedness = 5.f; p.rivers = 5.f; p.valleyDepth = 5.f;
    p.lakes = 200; p.latitudeNorthDeg = 500.f; p.latitudeSouthDeg = 500.f;
    p.temperatureOffset = 5.f; p.moistureOffset = 5.f;
    p.Clamp();
    EXPECT_FLOAT_EQ(p.landFraction, 0.65f);
    EXPECT_EQ(p.continents, 3);
    EXPECT_EQ(p.islands, 6);
    EXPECT_FLOAT_EQ(p.continentIslands, 1.f);
    EXPECT_EQ(p.peninsulas, 3);
    EXPECT_FLOAT_EQ(p.coastline, 1.f);
    EXPECT_FLOAT_EQ(p.mountainousness, 1.f);
    EXPECT_FLOAT_EQ(p.spineToRing, 1.f);
    EXPECT_EQ(p.passes, 4);
    EXPECT_FLOAT_EQ(p.barrier, 1.f);
    EXPECT_FLOAT_EQ(p.ruggedness, 1.f);
    EXPECT_FLOAT_EQ(p.rivers, 1.f);
    EXPECT_FLOAT_EQ(p.valleyDepth, 1.f);
    EXPECT_EQ(p.lakes, 12);
    EXPECT_FLOAT_EQ(p.latitudeNorthDeg, 90.f);
    EXPECT_FLOAT_EQ(p.latitudeSouthDeg, 90.f);
    EXPECT_FLOAT_EQ(p.temperatureOffset, 0.3f);
    EXPECT_FLOAT_EQ(p.moistureOffset, 0.3f);
}

TEST(TerrainTypes, ClampLowValues)
{
    WorldShapeParams p;
    p.landFraction = -5.f; p.continents = 0; p.islands = 0; p.continentIslands = -5.f;
    p.peninsulas = 0; p.coastline = -5.f; p.mountainousness = -5.f; p.spineToRing = -5.f;
    p.passes = 0; p.barrier = -5.f; p.ruggedness = -5.f; p.rivers = -5.f; p.valleyDepth = -5.f;
    p.lakes = 0; p.latitudeNorthDeg = -500.f; p.latitudeSouthDeg = -500.f;
    p.temperatureOffset = -5.f; p.moistureOffset = -5.f;
    p.Clamp();
    EXPECT_FLOAT_EQ(p.landFraction, 0.20f);
    EXPECT_EQ(p.continents, 1);
    EXPECT_EQ(p.islands, 0);
    EXPECT_FLOAT_EQ(p.continentIslands, 0.f);
    EXPECT_EQ(p.peninsulas, 0);
    EXPECT_FLOAT_EQ(p.coastline, 0.f);
    EXPECT_FLOAT_EQ(p.mountainousness, 0.f);
    EXPECT_FLOAT_EQ(p.spineToRing, 0.f);
    EXPECT_EQ(p.passes, 0);
    EXPECT_FLOAT_EQ(p.barrier, 0.f);
    EXPECT_FLOAT_EQ(p.ruggedness, 0.f);
    EXPECT_FLOAT_EQ(p.rivers, 0.f);
    EXPECT_FLOAT_EQ(p.valleyDepth, 0.f);
    EXPECT_EQ(p.lakes, 0);
    EXPECT_FLOAT_EQ(p.latitudeNorthDeg, -90.f);
    EXPECT_FLOAT_EQ(p.latitudeSouthDeg, -90.f);
    EXPECT_FLOAT_EQ(p.temperatureOffset, -0.3f);
    EXPECT_FLOAT_EQ(p.moistureOffset, -0.3f);
}

TEST(TerrainTypes, ClampWrapsOrientation)
{
    struct Case { float in; float out; };
    const Case cases[] = { {0.f, 0.f}, {359.f, 359.f}, {360.f, 0.f}, {370.f, 10.f},
                           {-10.f, 350.f}, {-360.f, 0.f}, {-370.f, 350.f}, {725.f, 5.f} };
    for (const Case& c : cases)
    {
        WorldShapeParams p;
        p.orientationDeg = c.in;
        p.Clamp();
        EXPECT_NEAR(p.orientationDeg, c.out, 1e-3f) << "in=" << c.in;
        EXPECT_GE(p.orientationDeg, 0.f);
        EXPECT_LT(p.orientationDeg, 360.f);
    }
}

TEST(TerrainTypes, ClampHandlesTinyNegativeOrientation)
{
    WorldShapeParams p;
    p.orientationDeg = -1e-9f;
    p.Clamp();
    EXPECT_GE(p.orientationDeg, 0.f);
    EXPECT_LT(p.orientationDeg, 360.f);
}

TEST(TerrainTypes, FirstStageAffectedTable)
{
    EXPECT_EQ(FirstStageAffected(ShapeParam::LandFraction),       TerrainStage::Landmass);
    EXPECT_EQ(FirstStageAffected(ShapeParam::Continents),         TerrainStage::Landmass);
    EXPECT_EQ(FirstStageAffected(ShapeParam::Islands),            TerrainStage::Landmass);
    EXPECT_EQ(FirstStageAffected(ShapeParam::ContinentIslands),   TerrainStage::Landmass);
    EXPECT_EQ(FirstStageAffected(ShapeParam::Peninsulas),         TerrainStage::Landmass);
    EXPECT_EQ(FirstStageAffected(ShapeParam::Coastline),          TerrainStage::Landmass);
    EXPECT_EQ(FirstStageAffected(ShapeParam::Mountainousness),    TerrainStage::Mountains);
    EXPECT_EQ(FirstStageAffected(ShapeParam::SpineToRing),        TerrainStage::Mountains);
    EXPECT_EQ(FirstStageAffected(ShapeParam::Orientation),        TerrainStage::Mountains);
    EXPECT_EQ(FirstStageAffected(ShapeParam::Passes),             TerrainStage::Mountains);
    EXPECT_EQ(FirstStageAffected(ShapeParam::Barrier),            TerrainStage::Barrier);
    EXPECT_EQ(FirstStageAffected(ShapeParam::BarrierForm),        TerrainStage::Barrier);
    EXPECT_EQ(FirstStageAffected(ShapeParam::Ruggedness),         TerrainStage::Elevation);
    EXPECT_EQ(FirstStageAffected(ShapeParam::Rivers),             TerrainStage::Rivers);
    EXPECT_EQ(FirstStageAffected(ShapeParam::ValleyDepth),        TerrainStage::Valleys);
    EXPECT_EQ(FirstStageAffected(ShapeParam::Lakes),              TerrainStage::Lakes);
    EXPECT_EQ(FirstStageAffected(ShapeParam::LatitudeNorth),      TerrainStage::Latitude);
    EXPECT_EQ(FirstStageAffected(ShapeParam::LatitudeSouth),      TerrainStage::Latitude);
    EXPECT_EQ(FirstStageAffected(ShapeParam::TemperatureOffset),  TerrainStage::Temperature);
    EXPECT_EQ(FirstStageAffected(ShapeParam::MoistureOffset),     TerrainStage::Moisture);
}

TEST(TerrainTypes, StageOrdinalsMatchSpec)
{
    EXPECT_EQ(static_cast<int>(TerrainStage::Landmass), 1);
    EXPECT_EQ(static_cast<int>(TerrainStage::Mountains), 2);
    EXPECT_EQ(static_cast<int>(TerrainStage::Barrier), 3);
    EXPECT_EQ(static_cast<int>(TerrainStage::Elevation), 4);
    EXPECT_EQ(static_cast<int>(TerrainStage::Rivers), 6);
    EXPECT_EQ(static_cast<int>(TerrainStage::Lakes), 7);
    EXPECT_EQ(static_cast<int>(TerrainStage::Valleys), 9);
    EXPECT_EQ(static_cast<int>(TerrainStage::Latitude), 11);
    EXPECT_EQ(static_cast<int>(TerrainStage::Temperature), 12);
    EXPECT_EQ(static_cast<int>(TerrainStage::Moisture), 13);
}

template <typename E, typename F>
static void ExpectUniqueNonEmptyNames(int count, F nameOf)
{
    std::set<std::string> names;
    for (int i = 0; i < count; ++i)
    {
        const char* n = nameOf(static_cast<E>(i));
        ASSERT_NE(n, nullptr);
        EXPECT_NE(std::string(n), "");
        EXPECT_NE(std::string(n), "Unknown") << "index " << i;
        names.insert(n);
    }
    EXPECT_EQ(static_cast<int>(names.size()), count);
}

TEST(TerrainTypes, EnumNamesAreUniqueAndNonEmpty)
{
    ExpectUniqueNonEmptyNames<WaterSource>(4, [](WaterSource e) { return WaterSourceName(e); });
    ExpectUniqueNonEmptyNames<DepthZone>(4, [](DepthZone e) { return DepthZoneName(e); });
    ExpectUniqueNonEmptyNames<TerrainType>(static_cast<int>(TerrainType::Count), [](TerrainType e) { return TerrainTypeName(e); });
    ExpectUniqueNonEmptyNames<BarrierForm>(2, [](BarrierForm e) { return BarrierFormName(e); });
    ExpectUniqueNonEmptyNames<ShapeParam>(static_cast<int>(ShapeParam::Count), [](ShapeParam e) { return ShapeParamName(e); });
    ExpectUniqueNonEmptyNames<TerrainStage>(static_cast<int>(TerrainStage::Count), [](TerrainStage e) { return TerrainStageName(e); });
}

TEST(TerrainTypes, OutOfRangeEnumNamesAreUnknown)
{
    EXPECT_STREQ(TerrainTypeName(TerrainType::Count), "Unknown");
    EXPECT_STREQ(ShapeParamName(ShapeParam::Count), "Unknown");
    EXPECT_STREQ(TerrainStageName(TerrainStage::Count), "Unknown");
}

TEST(TerrainTypes, TerrainTypeIdsAreUniqueAndPrefixed)
{
    std::set<uint32_t> ids;
    for (int i = 0; i < static_cast<int>(TerrainType::Count); ++i)
    {
        const TerrainType t = static_cast<TerrainType>(i);
        const std::string expected = std::string("Terrain.") + TerrainTypeName(t);
        EXPECT_EQ(TerrainTypeId(t), Dia::Core::StringCRC(expected.c_str()));
        ids.insert(static_cast<uint32_t>(TerrainTypeId(t).Value()));
    }
    EXPECT_EQ(static_cast<int>(ids.size()), static_cast<int>(TerrainType::Count));
    EXPECT_STREQ(TerrainTypeName(TerrainType::Mountain), "Mountain");
}

TEST(TerrainTypes, TerrainFlagsAreDistinctBits)
{
    const uint8_t flags[] = { TerrainFlag::Coast, TerrainFlag::Ridge, TerrainFlag::Barrier,
                              TerrainFlag::Pass, TerrainFlag::Volcanic, TerrainFlag::RiverCrossesBarrier };
    uint8_t all = 0;
    for (uint8_t f : flags)
    {
        EXPECT_EQ(f & (f - 1), 0);
        EXPECT_EQ(all & f, 0);
        all |= f;
    }
    EXPECT_EQ(kNoDirection, 7);
}

TEST(TerrainTypes, WorldStructureResizeAndAccess)
{
    WorldStructure w;
    EXPECT_EQ(w.CellCount(), 0);
    w.Resize({4, 3});
    EXPECT_EQ(w.dimensions.width, 4);
    EXPECT_EQ(w.dimensions.height, 3);
    EXPECT_EQ(w.CellCount(), 12);
    ASSERT_NE(w.Cells(), nullptr);
    EXPECT_EQ(w.Cells()[5].downstream, kNoDirection);
    EXPECT_EQ(w.Cells()[5].terrain, TerrainType::DeepWater);
    w.Cells()[7].landmass = 2;
    const WorldStructure& cw = w;
    EXPECT_EQ(cw.Cells()[7].landmass, 2);
    EXPECT_EQ(&cw.At(3, 1), &cw.Cells()[7]);
    w.Resize({0, 0});
    EXPECT_EQ(w.CellCount(), 0);
}
