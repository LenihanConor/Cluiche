// Spec: worldsim-kit.md section 4 (AC 10b), plan task 29.
// S-1 wiring: WorldGenerationModule rebuilds its WorldCellFields on WorldGenerationComplete by calling the Draoi
// RegisterBiomeFields / RegisterTerrainFields. Verified by cell data against the committed world and layers.
#include <gtest/gtest.h>
#include <Modules/WorldGen/WorldGenerationModule.h>   // first: RandomStream.h must precede windows.h (min/max macros)
#include <Shared/WorldGeneration/CoWTesting/CoWTerrainRulesTestHelpers.h>
#include <Shared/WorldGeneration/CoWTesting/GuaranteeSpecTestHelpers.h>
#include <Shared/CoWNodeKinds.h>
#include <DraoiHexWorld/BiomeRegistry.h>
#include <DraoiHexWorld/WorldCellFields.h>
#include <DraoiHexWorld/CellSelector.h>
#include <DraoiHexWorldTerrain/Testing/TestGuarantees.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <Modules/MessageBusHostModule.h>
#include <Modules/Plumbing/RandomStreamModule.h>

#include <filesystem>
#include <memory>
#include <map>
#include <set>
#include <string>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace CoW;
using namespace Draoi::HexWorld;

namespace {

WorldGenerationModule* g_wg = nullptr;

std::string FindShippedBiomes()
{
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::absolute(std::filesystem::path(__FILE__), ec).parent_path();
    while (!dir.empty())
    {
        const std::filesystem::path candidate = dir / "Cluiche/Assets/CoW/Global/World/biomes.json";
        if (std::filesystem::is_regular_file(candidate, ec)) return candidate.string();
        const std::filesystem::path parent = dir.parent_path();
        if (parent == dir) break;
        dir = parent;
    }
    return {};
}

Module* CreateBus(const StringCRC& id) { return new Cluiche::AppFlow::MessageBusHostModule(id); }
Module* CreateRng(const StringCRC& id) { return new RandomStreamModule(id); }
Module* CreateWg(const StringCRC& id)
{
    g_wg = new WorldGenerationModule(id);
    g_wg->SetStrataPath(Draoi::HexWorld::Testing::ShippedStrataPath());
    g_wg->SetGuaranteesPath(CoW::WorldGeneration::Testing::ShippedGuaranteesPath());
    g_wg->SetBiomesPath(FindShippedBiomes());
    g_wg->SetRulesSidecarText(CoW::WorldGeneration::TestSidecarText());
    return g_wg;
}

constexpr float kDt = 1.0f / 30.0f;

struct Fixture
{
    TypeRegistry reg;
    ApplicationManifestV3 manifest;
    std::unique_ptr<Application> app;
    WorldGenerationModule* wg = nullptr;
    std::string config = "{\"seed\":1}";

    Fixture()
    {
        reg.Register(StringCRC("MessageBusHostModule"), &CreateBus);
        reg.Register(StringCRC("RandomStreamModule"), &CreateRng);
        reg.Register(StringCRC("WorldGenerationModule"), &CreateWg);
        manifest.version = 3;
        StageDeclaration run; run.name = StringCRC("Run");
        manifest.stages.Add(run);
        manifest.initialStage = StringCRC("Run");
        ProcessingUnitDeclaration pu;
        pu.instanceId = StringCRC("SimPU");
        pu.frequencyHz = 30.0f;
        pu.dedicatedThread = false;
        ModuleDeclaration busDecl;
        busDecl.instanceId = StringCRC("MessageBusHostModule");
        busDecl.typeId = StringCRC("MessageBusHostModule");
        busDecl.stages.Add(StringCRC("all"));
        pu.modules.Add(busDecl);
        ModuleDeclaration rngDecl;
        rngDecl.instanceId = StringCRC("RandomStreamModule");
        rngDecl.typeId = StringCRC("RandomStreamModule");
        rngDecl.stages.Add(StringCRC("all"));
        pu.modules.Add(rngDecl);
        ModuleDeclaration wgDecl;
        wgDecl.instanceId = StringCRC("WorldGenerationModule");
        wgDecl.typeId = StringCRC("WorldGenerationModule");
        wgDecl.stages.Add(StringCRC("all"));
        wgDecl.dependencies.Add(StringCRC("MessageBusHostModule"));
        wgDecl.dependencies.Add(StringCRC("RandomStreamModule"));
        wgDecl.configJson = config.c_str();
        pu.modules.Add(wgDecl);
        manifest.processingUnits.Add(pu);
    }

    bool Boot()
    {
        g_wg = nullptr;
        app = std::make_unique<Application>(manifest, reg);
        if (!app->Start()) return false;
        wg = g_wg;
        if (!wg) return false;
        for (int i = 0; i < 20; ++i)
        {
            app->Update(kDt);
            if (wg->GetState() == ModuleState::kActive) return true;
        }
        return false;
    }
    void Pump(int n) { for (int i = 0; i < n; ++i) app->Update(kDt); }
};

void ExpectFieldsMatchWorld(WorldGenerationModule& wg)
{
    WorldCellFields& f = wg.GetCellFields();
    const WorldGenSession& s = wg.GetSession();
    const WorldStructure& w = s.World();
    const BiomeRegistry& reg = wg.GetBiomeRegistry();
    const TerrainLayers& layers = s.Generator().GetLayers();
    const uint32_t n = static_cast<uint32_t>(w.CellCount());
    ASSERT_GT(n, 0u);

    const int elev = layers.FindLayer("elevation");
    ASSERT_GE(elev, 0);
    const TagId land = reg.FindTag(StringCRC("land"));
    ASSERT_NE(land, kNoTag);
    ASSERT_TRUE(f.Has(StringCRC("biome"), StringCRC("tag.land")));
    ASSERT_TRUE(f.Has(StringCRC("biome"), StringCRC("index")));

    std::set<uint32_t> kinds;
    for (const SiteKindDecl& k : s.GetGuaranteeSpec().siteKinds)
        if (k.isProtected) kinds.insert(StringCRC(k.id.c_str()).Value());
    kinds.insert(CoW::WorldGeneration::CoWNodeKindIds().corridor.Value());
    ASSERT_GT(s.GetGuaranteeSpec().siteKinds.size(), 0u);
    std::vector<bool> prot(n, false);
    for (uint32_t c = 0; c < n; ++c)
    {
        const uint8_t fl = w.Cells()[c].terrainFlags;
        if ((fl & TerrainFlag::Pass) || (fl & TerrainFlag::RiverCrossesBarrier)) prot[c] = true;
    }
    for (const Site& site : w.sites.All())
        if (kinds.count(site.kindCrc)) prot[site.cell] = true;

    int protCount = 0;
    for (uint32_t c = 0; c < n; ++c)
    {
        f.SetCell(c);
        ASSERT_EQ(f.GetFloat(StringCRC("terrain"), StringCRC("elevation")), layers.GetFloat(elev)[c]) << c;
        const BiomeIndex b = w.Cells()[c].biome;
        ASSERT_EQ(f.GetFloat(StringCRC("biome"), StringCRC("index")), static_cast<float>(b)) << c;
        ASSERT_EQ(f.GetBool(StringCRC("biome"), StringCRC("tag.land")), reg.HasTag(b, land)) << c;
        ASSERT_EQ(f.GetBool(StringCRC("terrain"), StringCRC("is_protected")), static_cast<bool>(prot[c])) << c;
        if (prot[c]) ++protCount;
    }
    EXPECT_GT(protCount, 0);
}

} // namespace

TEST(WorldCellFieldsWiring, FieldsReturnCommittedValuesAfterSmallSeed1World)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.Pump(3);
    ASSERT_EQ(f.wg->GetWorldRevision(), 1u);
    ExpectFieldsMatchWorld(*f.wg);
}

TEST(WorldCellFieldsWiring, FieldsReResolveAfterRegenerate)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.Pump(3);
    Json::Value a(Json::objectValue); a["seed"] = 77;
    f.wg->GetSession().Set(a);
    f.wg->NotifyAction();
    f.Pump(3);
    ASSERT_EQ(f.wg->GetWorldRevision(), 2u);
    ExpectFieldsMatchWorld(*f.wg);
}

// AC 23: terrain.region returns each cell's committed region id; a selector over it resolves to the hand-computed cells.
TEST(WorldCellFieldsWiring, TerrainRegionFieldAndSelectorMatchCommittedRegions)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.Pump(3);
    WorldCellFields& fields = f.wg->GetCellFields();
    const WorldStructure& w = f.wg->GetSession().World();
    const uint32_t n = static_cast<uint32_t>(w.CellCount());
    ASSERT_GT(n, 0u);
    ASSERT_TRUE(fields.Has(StringCRC("terrain"), StringCRC("region")));

    std::map<uint32_t, uint32_t> counts;
    for (uint32_t c = 0; c < n; ++c)
    {
        fields.SetCell(c);
        ASSERT_EQ(fields.GetFloat(StringCRC("terrain"), StringCRC("region")), static_cast<float>(w.Cells()[c].region)) << c;
        ++counts[w.Cells()[c].region];
    }
    // NOTE: the region partition is not yet wired into the generation pipeline, so every committed Cell::region is
    // currently 0 (none). The test is written against whatever is committed and becomes non-trivial once it is wired.

    uint32_t target = 0, best = 0;
    for (const auto& kv : counts) if (kv.second > best) { best = kv.second; target = kv.first; }
    // Second selector: an id that no cell carries must resolve to an empty list.
    uint32_t absent = 1;
    while (counts.count(absent)) ++absent;

    Json::Value block;
    Json::Reader reader;
    const std::string text = "{\"selectors\":{\"in_region\":{\"op\":\"==\",\"slot\":\"terrain\",\"field\":\"region\",\"value\":"
                             + std::to_string(target)
                             + "},\"absent\":{\"op\":\"==\",\"slot\":\"terrain\",\"field\":\"region\",\"value\":" + std::to_string(absent) + "}}}";
    ASSERT_TRUE(reader.parse(text, block));
    CellSelectorSet set; SelectorError err;
    ASSERT_TRUE(set.Load(block, fields, err)) << err.item.AsCStr() << ": " << err.reason.AsCStr();
    ASSERT_TRUE(set.Resolve(fields, n, err)) << err.reason.AsCStr();
    const int sel = set.FindSelector(StringCRC("in_region"));
    ASSERT_GE(sel, 0);

    std::vector<uint32_t> expected;
    for (uint32_t c = 0; c < n; ++c) if (w.Cells()[c].region == target) expected.push_back(c);
    ASSERT_EQ(set.ResolvedCount(static_cast<uint32_t>(sel)), static_cast<uint32_t>(expected.size()));
    for (size_t i = 0; i < expected.size(); ++i)
        ASSERT_EQ(set.ResolvedCells(static_cast<uint32_t>(sel))[i], expected[i]) << i;
    const int none = set.FindSelector(StringCRC("absent"));
    ASSERT_GE(none, 0);
    EXPECT_EQ(set.ResolvedCount(static_cast<uint32_t>(none)), 0u);
}
