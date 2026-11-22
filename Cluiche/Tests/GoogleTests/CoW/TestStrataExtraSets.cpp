// Generic strata task 4: extra (non-depth) stratum sets are assigned by their own assigner, stored as per-set cell
// layers on the world, written to and read from the world JSON, and listed in the viewer's layers.
// Spec: docs/specs/features/draoi/generic-strata.md (AC 4).
#include <gtest/gtest.h>

#include <DraoiHexWorld/Testing/BiomeTestRegistry.h>
#include <DraoiHexWorldTerrain/StrataAssigner.h>
#include <DraoiHexWorldTerrain/TerrainGenerator.h>
#include <DraoiHexWorldTerrain/WorldJson.h>
#include <DraoiHexWorldTerrain/WorldLayerSource.h>
#include <Shared/WorldGeneration/CoWTesting/CoWTerrainRulesTestHelpers.h>
#include <Shared/WorldGeneration/Testing/WorldJsonTestWorlds.h>

#include <DiaCore/Json/external/json/json.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;
using namespace Draoi::HexWorld;
using namespace Draoi::HexWorld::Testing;
using Dia::Maths::RandomStream;

namespace {

// Splits the land into N vertical bands by column; ignores every cost. Not ordered-aware (an unordered set).
class ColumnBandsAssigner : public IStrataAssigner
{
public:
    int calls = 0;
    void Assign(const StrataAssignContext& ctx, const StratumSetSpec& set, std::span<StratumIndex> out) override
    {
        ++calls;
        const int w = ctx.dims.width;
        for (size_t i = 0; i < out.size(); ++i)
        {
            const int x = static_cast<int>(i) % w;
            out[i] = ctx.landmass[i] == 0u ? 0 : static_cast<StratumIndex>(1 + std::min<int>(x * static_cast<int>(set.count) / w, static_cast<int>(set.count) - 1));
        }
    }
};

// Breaks the contract: a value above the set's count on one land cell.
class OutOfRangeAssigner : public IStrataAssigner
{
public:
    void Assign(const StrataAssignContext& ctx, const StratumSetSpec&, std::span<StratumIndex> out) override
    {
        bool done = false;
        for (size_t i = 0; i < out.size(); ++i)
        {
            out[i] = ctx.landmass[i] != 0u ? 1 : 0;
            if (!done && ctx.landmass[i] != 0u) { out[i] = 9; done = true; }
        }
    }
};

const std::vector<std::string> kShippedNames = { "FirstShores", "WildReaches", "AncientInteriors" };

// A strata table and biome registry declaring the shipped depth set plus the given extra sets.
struct Rig
{
    StrataAssignerRegistry assigners;
    ColumnBandsAssigner    columns;
    OutOfRangeAssigner     outOfRange;
    StratumTable           table;
    BiomeRegistry          biomes;

    explicit Rig(const std::vector<TestExtraSet>& extras)
    {
        assigners.Register("column_bands", &columns);
        assigners.Register("out_of_range", &outOfRange);
        StratumError err;
        if (!table.LoadFromText(TestStrataText(kShippedNames, extras).c_str(), TestStrataSidecar(kShippedNames, { 0.45f, 0.40f, 0.15f }, extras).c_str(),
                                err, assigners.TypeCheck()))
        {
            ADD_FAILURE() << "strata load: " << err.field << " " << err.reason;
        }
        RegistryError rerr;
        if (!biomes.LoadFromText(Detail::ReadFileText(ShippedBiomesPath()).c_str(), table, rerr))
        {
            ADD_FAILURE() << "biomes load: " << rerr.field.AsCStr() << " " << rerr.reason.AsCStr();
        }
    }

    std::unique_ptr<TerrainGenerator> Generate(GridDimensions dims = GridDimensions{ 66, 42 })
    {
        auto gen = std::make_unique<TerrainGenerator>(TestRules(), &biomes);
        gen->SetAssignerRegistry(&assigners);
        RandomStream rng(1);
        gen->Update(5, WorldShapeParams{}, dims, rng);
        return gen;
    }
};

const TestExtraSet kHomeland = { "homeland", false, "column_bands", { "West", "East" } };
const TestExtraSet kTier     = { "tier", false, "column_bands", { "Low", "Mid", "High" } };

std::vector<uint32_t> Depth(const TerrainGenerator& gen)
{
    const uint32_t* z = gen.GetLayers().GetUInt(gen.GetLayers().FindLayer("stratum"));
    return std::vector<uint32_t>(z, z + 66 * 42);
}

WorldStructure Written(const TerrainGenerator& gen)
{
    WorldStructure w;
    gen.WriteTerrain(w);
    return w;
}

std::string Export(const WorldStructure& w, const BiomeRegistry& biomes)
{
    WorldJsonContext c;
    c.biomes = &biomes;
    std::string text;
    JsonIssue error;
    EXPECT_TRUE(ExportWorldText(w, c, text, error)) << error.field << ": " << error.message;
    return text;
}

WorldImportResult Import(const std::string& text, WorldStructure& out, const BiomeRegistry& biomes)
{
    WorldJsonContext c;
    c.biomes = &biomes;
    return ImportWorldText(text, c, out);
}

Json::Value Parse(const std::string& text)
{
    Json::Value root;
    Json::CharReaderBuilder b;
    std::string errors;
    std::unique_ptr<Json::CharReader> reader(b.newCharReader());
    EXPECT_TRUE(reader->parse(text.data(), text.data() + text.size(), &root, &errors)) << errors;
    return root;
}

std::string Write(const Json::Value& root)
{
    Json::StreamWriterBuilder b;
    b["indentation"] = "  ";
    return Json::writeString(b, root);
}

// A tiny world (4x3) with a hand-set "homeland" layer for the JSON tests.
WorldStructure TinyWithHomeland()
{
    WorldStructure w = MakeTinyWorld();
    ExtraStratumLayer layer;
    std::memcpy(layer.setId, "homeland", 8);
    layer.cells = { 0, 1, 2, 1, 2, 2, 1, 0, 1, 2, 1, 0 };
    w.extraStrata.push_back(layer);
    return w;
}
} // namespace

// ---------------------------------------------------------------------------
// Table API
// ---------------------------------------------------------------------------
TEST(StrataExtraSets, TableEnumeratesSetsAndNamesStrataWithinASet)
{
    Rig r({ kHomeland, kTier });
    EXPECT_EQ(r.table.SetCount(), 3u);
    EXPECT_EQ(r.table.ExtraSetCount(), 2u);
    EXPECT_EQ(r.table.SetIndexOf("depth"), 0);
    EXPECT_EQ(r.table.SetIndexOf("tier"), 2);
    EXPECT_EQ(r.table.SetIndexOf("nowhere"), -1);
    const StratumSetSpec* home = r.table.FindSet("homeland");
    ASSERT_NE(home, nullptr);
    EXPECT_STREQ(StratumTable::NameIn(*home, 0), "None");
    EXPECT_STREQ(StratumTable::NameIn(*home, 2), "East");
    EXPECT_EQ(StratumTable::NameIn(*home, 3), nullptr);
    EXPECT_EQ(StratumTable::IndexIn(*home, "West"), 1);
    EXPECT_EQ(StratumTable::IndexIn(*home, "None"), 0);
    EXPECT_EQ(StratumTable::IndexIn(*home, "FirstShores"), -1);   // a depth name is not in the set
}

TEST(StrataExtraSets, DepthOnlyTableHasNoExtraSets)
{
    Rig r({});
    EXPECT_EQ(r.table.ExtraSetCount(), 0u);
    const auto gen = r.Generate();
    EXPECT_TRUE(gen->GetExtraStrata().empty());
    EXPECT_TRUE(Written(*gen).extraStrata.empty());
}

// ---------------------------------------------------------------------------
// Assignment (AC 4)
// ---------------------------------------------------------------------------
TEST(StrataExtraSets, SecondSetIsAssignedByItsOwnAssignerIntoACellLayer)
{
    Rig r({ kHomeland });
    const auto gen = r.Generate();
    EXPECT_GT(r.columns.calls, 0);
    EXPECT_TRUE(gen->GetStrataError().empty()) << gen->GetStrataError();
    ASSERT_EQ(gen->GetExtraStrata().size(), 1u);
    const ExtraStratumLayer& layer = gen->GetExtraStrata()[0];
    EXPECT_STREQ(layer.setId, "homeland");
    ASSERT_EQ(layer.cells.size(), static_cast<size_t>(66 * 42));
    const uint32_t* land = gen->GetLayers().GetUInt(gen->GetLayers().FindLayer("landmass"));
    int west = 0, east = 0;
    for (int i = 0; i < 66 * 42; ++i)
    {
        const int x = i % 66;
        const uint32_t expect = land[i] == 0u ? 0u : static_cast<uint32_t>(1 + std::min(x * 2 / 66, 1));
        ASSERT_EQ(layer.cells[static_cast<size_t>(i)], expect) << "cell " << i;
        west += layer.cells[static_cast<size_t>(i)] == 1;
        east += layer.cells[static_cast<size_t>(i)] == 2;
    }
    EXPECT_GT(west, 0);
    EXPECT_GT(east, 0);

    const WorldStructure world = Written(*gen);
    ASSERT_EQ(world.extraStrata.size(), 1u);
    EXPECT_TRUE(world.extraStrata[0] == layer);
    EXPECT_EQ(world.FindExtraStrata("homeland"), &world.extraStrata[0]);
    EXPECT_EQ(world.FindExtraStrata("depth"), nullptr);
}

TEST(StrataExtraSets, DepthSetIsBitIdenticalWithAndWithoutExtraSets)
{
    Rig plain({});
    Rig extra({ kHomeland, kTier });
    const auto a = plain.Generate();
    const auto b = extra.Generate();
    const WorldStructure wa = Written(*a);
    const WorldStructure wb = Written(*b);
    ASSERT_EQ(wa.CellCount(), wb.CellCount());
    EXPECT_EQ(Depth(*a), Depth(*b));
    EXPECT_EQ(std::memcmp(wa.Cells(), wb.Cells(), sizeof(Cell) * static_cast<size_t>(wa.CellCount())), 0)
        << "extra sets must not disturb any Cell field";
    EXPECT_EQ(a->GetStage8Debug().mainCells, b->GetStage8Debug().mainCells);
}

TEST(StrataExtraSets, TwoExtraSetsAreBothAssignedInTableOrder)
{
    Rig r({ kHomeland, kTier });
    const auto gen = r.Generate();
    ASSERT_EQ(gen->GetExtraStrata().size(), 2u);
    EXPECT_STREQ(gen->GetExtraStrata()[0].setId, "homeland");
    EXPECT_STREQ(gen->GetExtraStrata()[1].setId, "tier");
    int maxTier = 0;
    for (uint8_t v : gen->GetExtraStrata()[1].cells) { maxTier = std::max<int>(maxTier, v); }
    EXPECT_EQ(maxTier, 3);
}

TEST(StrataExtraSets, ContractViolationIsRejectedNamingTheSetAndDepthStaysUntouched)
{
    Rig plain({});
    Rig r({ { "homeland", false, "out_of_range", { "West", "East" } } });
    const auto gen = r.Generate();
    EXPECT_NE(gen->GetStrataError().find("homeland"), std::string::npos) << gen->GetStrataError();
    EXPECT_NE(gen->GetStrataError().find("out_of_range"), std::string::npos) << gen->GetStrataError();
    EXPECT_NE(gen->GetStrataError().find("cell"), std::string::npos) << gen->GetStrataError();
    ASSERT_EQ(gen->GetExtraStrata().size(), 1u);
    for (uint8_t v : gen->GetExtraStrata()[0].cells) { ASSERT_EQ(v, 0u) << "a rejected set leaves its layer empty"; }
    EXPECT_EQ(Depth(*gen), Depth(*plain.Generate()));
}

TEST(StrataExtraSets, BuiltInAssignerCanDriveAnExtraSetWithoutTouchingDepthDebug)
{
    Rig plain({});
    Rig r({ { "bands", true, "coast_cost_bands", { "Outer", "Inner" } } });
    const auto gen = r.Generate();
    EXPECT_TRUE(gen->GetStrataError().empty()) << gen->GetStrataError();
    ASSERT_EQ(gen->GetExtraStrata().size(), 1u);
    const uint32_t* land = gen->GetLayers().GetUInt(gen->GetLayers().FindLayer("landmass"));
    for (int i = 0; i < 66 * 42; ++i)
    {
        const uint8_t v = gen->GetExtraStrata()[0].cells[static_cast<size_t>(i)];
        if (land[i] == 0u) { ASSERT_EQ(v, 0u); } else { ASSERT_TRUE(v == 1 || v == 2) << i; }
    }
    const auto base = plain.Generate();
    EXPECT_EQ(gen->GetStage8Debug().t1, base->GetStage8Debug().t1);   // the depth banding report is the depth set's
    EXPECT_EQ(gen->GetStage8Debug().mainCells, base->GetStage8Debug().mainCells);
}

// ---------------------------------------------------------------------------
// World JSON (AC 4)
// ---------------------------------------------------------------------------
TEST(StrataExtraSets, WorldWithoutExtraSetsWritesNoStrataSetsKey)
{
    Rig r({ kHomeland });
    const std::string text = Export(MakeTinyWorld(), r.biomes);
    EXPECT_EQ(text.find("strata_sets"), std::string::npos);
    // The same tiny world under a depth-only table writes the identical text.
    Rig plain({});
    EXPECT_EQ(text, Export(MakeTinyWorld(), plain.biomes));
}

TEST(StrataExtraSets, ExtraSetRoundTripsThroughExportAndImport)
{
    Rig r({ kHomeland });
    const WorldStructure original = TinyWithHomeland();
    const std::string text = Export(original, r.biomes);
    EXPECT_NE(text.find("\"strata_sets\""), std::string::npos);
    EXPECT_NE(text.find("\"East\""), std::string::npos);
    WorldStructure imported;
    const WorldImportResult res = Import(text, imported, r.biomes);
    ASSERT_TRUE(res.ok) << res.error.field << ": " << res.error.message;
    ASSERT_EQ(imported.extraStrata.size(), 1u);
    EXPECT_TRUE(imported.extraStrata[0] == original.extraStrata[0]);
    EXPECT_EQ(Export(imported, r.biomes), text);   // byte-identical re-export
}

TEST(StrataExtraSets, GeneratedWorldWithTwoSetsRoundTrips)
{
    Rig r({ kHomeland, kTier });
    const auto gen = r.Generate(GridDimensions{ 20, 12 });
    const WorldStructure original = Written(*gen);
    ASSERT_EQ(original.extraStrata.size(), 2u);
    const std::string text = Export(original, r.biomes);
    WorldStructure imported;
    const WorldImportResult res = Import(text, imported, r.biomes);
    ASSERT_TRUE(res.ok) << res.error.field << ": " << res.error.message;
    EXPECT_TRUE(imported.extraStrata == original.extraStrata);
    EXPECT_EQ(Export(imported, r.biomes), text);
}

TEST(StrataExtraSets, ImportRefusesAnUnknownSetIdNamingTheField)
{
    Rig r({ kHomeland });
    Json::Value root = Parse(Export(TinyWithHomeland(), r.biomes));
    root["world"]["strata_sets"][0]["id"] = "nowhere";
    WorldStructure out = MakeTinyWorld();
    const WorldImportResult res = Import(Write(root), out, r.biomes);
    EXPECT_FALSE(res.ok);
    EXPECT_EQ(res.error.field, "world.strata_sets[0].id");
    EXPECT_TRUE(out.extraStrata.empty()) << "a refused import leaves the destination alone";
}

TEST(StrataExtraSets, ImportRefusesAFileSavedWithSetsTheCurrentTableLacks)
{
    Rig withSet({ kHomeland });
    const std::string text = Export(TinyWithHomeland(), withSet.biomes);
    Rig depthOnly({});
    WorldStructure out;
    const WorldImportResult res = Import(text, out, depthOnly.biomes);
    EXPECT_FALSE(res.ok);
    EXPECT_EQ(res.error.field, "world.strata_sets[0].id");
}

TEST(StrataExtraSets, ImportRefusesAnUnknownStratumNameNamingTheCell)
{
    Rig r({ kHomeland });
    Json::Value root = Parse(Export(TinyWithHomeland(), r.biomes));
    root["world"]["strata_sets"][0]["cells"][3] = "North";
    WorldStructure out;
    const WorldImportResult res = Import(Write(root), out, r.biomes);
    EXPECT_FALSE(res.ok);
    EXPECT_EQ(res.error.field, "world.strata_sets[0].cells[3]");
    EXPECT_NE(res.error.message.find("homeland"), std::string::npos) << res.error.message;
}

TEST(StrataExtraSets, ImportRefusesTheDepthSetAsAnExtraSetAndAWrongCellCount)
{
    Rig r({ kHomeland });
    {
        Json::Value root = Parse(Export(TinyWithHomeland(), r.biomes));
        root["world"]["strata_sets"][0]["id"] = "depth";
        WorldStructure out;
        const WorldImportResult res = Import(Write(root), out, r.biomes);
        EXPECT_FALSE(res.ok);
        EXPECT_EQ(res.error.field, "world.strata_sets[0].id");
    }
    {
        Json::Value root = Parse(Export(TinyWithHomeland(), r.biomes));
        Json::Value removed;
        root["world"]["strata_sets"][0]["cells"].removeIndex(0, &removed);
        WorldStructure out;
        const WorldImportResult res = Import(Write(root), out, r.biomes);
        EXPECT_FALSE(res.ok);
        EXPECT_EQ(res.error.field, "world.strata_sets[0].cells");
    }
}

TEST(StrataExtraSets, ExportRefusesALayerTheTableDoesNotDeclare)
{
    Rig r({ kHomeland });
    WorldStructure w = TinyWithHomeland();
    std::memcpy(w.extraStrata[0].setId, "ghost\0\0\0", 8);
    WorldJsonContext c;
    c.biomes = &r.biomes;
    std::string text;
    JsonIssue error;
    EXPECT_FALSE(ExportWorldText(w, c, text, error));
    EXPECT_EQ(error.field, "world.strata_sets[0].id");
    EXPECT_TRUE(text.empty());
}

// ---------------------------------------------------------------------------
// Viewer layer source (AC 4)
// ---------------------------------------------------------------------------
TEST(StrataExtraSets, LayerSourceListsOneCategoryLayerPerExtraSet)
{
    Rig r({ kHomeland, kTier });
    const std::vector<ExtraStrataLayerDesc> layers = WorldLayerSource::ExtraStrataLayers(&r.biomes);
    ASSERT_EQ(layers.size(), 2u);
    EXPECT_EQ(layers[0].id, "strata.homeland");
    EXPECT_EQ(layers[0].setId, "homeland");
    EXPECT_EQ(layers[0].categoryCount, 3u);
    EXPECT_EQ(layers[1].id, "strata.tier");
    EXPECT_EQ(layers[1].categoryCount, 4u);

    Rig plain({});
    EXPECT_TRUE(WorldLayerSource::ExtraStrataLayers(&plain.biomes).empty());
    EXPECT_TRUE(WorldLayerSource::ExtraStrataLayers(nullptr).empty());
}

TEST(StrataExtraSets, SessionKeepsTheExtraLayersAndTheLayerReaderReturnsThem)
{
    Rig r({ kHomeland });
    TestSession session;
    session.SetBiomeRegistry(&r.biomes);
    session.SetStrataAssignerRegistry(&r.assigners);
    RandomStream rng(12345);
    Json::Value args(Json::objectValue);
    args["seed"] = Json::Value(static_cast<Json::UInt64>(7));
    session.Regenerate(args);
    ASSERT_TRUE(session.Tick(rng));
    const WorldStructure& world = session.World();
    ASSERT_EQ(world.extraStrata.size(), 1u);
    const uint32_t n = static_cast<uint32_t>(world.CellCount());
    std::vector<float> values(n, -1.0f);
    WorldLayerSource::ReadExtraStrata(session, "homeland", values.data(), n);
    for (uint32_t i = 0; i < n; ++i) { ASSERT_EQ(values[i], static_cast<float>(world.extraStrata[0].cells[i])) << i; }
    WorldLayerSource::ReadExtraStrata(session, "nowhere", values.data(), n);
    for (uint32_t i = 0; i < n; ++i) { ASSERT_EQ(values[i], 0.0f) << i; }
}
