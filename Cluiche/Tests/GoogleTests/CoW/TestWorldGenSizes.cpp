// S-1 World Generation system task 21: the pipeline at all three map sizes (Small 66x42, Standard 80x52, Large 104x64).
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/worldgenerationpipeline.md (Map Sizes),
//       f1-debug-host-integration.md (Goal 10 size picker).
// Per size: determinism (live preview == Regenerate terrain, Regenerate twice identical), dimensions and cell ranges,
// pipeline acceptance (shape, biomes, 10..16 contiguous regions, layout validation), export/import round trip,
// Small -> Standard -> Large -> Small switching, and the 12 KB compact debug-domain state.
// Large is SLOW_ (run with --all). Acceptance misses are reported with numbers, not tuned.
#include <gtest/gtest.h>
#include <DraoiHexWorld/Testing/BiomeTestRegistry.h>
#include <DiaMaths/Core/RandomStream.h>   // before the module header: windows.h defines min/max macros
#include <Shared/WorldGeneration/Testing/TerrainShapeChecks.h>
#include <Modules/WorldGen/WorldGenerationModule.h>

#include <Shared/WorldGeneration/WorldGenSession.h>
#include <Shared/WorldGeneration/WorldJson.h>
#include <Shared/WorldGeneration/Testing/BiomeChecks.h>
#include <Shared/WorldGeneration/Testing/RegionChecks.h>
#include <Shared/WorldGeneration/Testing/SessionDrive.h>
#include <Shared/WorldGeneration/Testing/TerrainShapeChecks.h>
#include <Shared/WorldGeneration/CoWTesting/CoWTerrainRulesTestHelpers.h>

#ifdef DIA_DEBUG
#include <Modules/WorldGen/WorldGenDebugDomain.h>
#endif

#include <DiaCore/Json/external/json/json.h>
#include <DiaMaths/Core/RandomStream.h>

#include <cmath>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

using namespace CoW::WorldGeneration;
using namespace Draoi::HexWorld;
using namespace CoW::WorldGeneration::Testing;
using namespace Draoi::HexWorld::Testing;

namespace
{
    namespace fs = std::filesystem;

    Json::Value Obj() { return Json::Value(Json::objectValue); }
    Json::Value Obj(const char* key, const Json::Value& v) { Json::Value o(Json::objectValue); o[key] = v; return o; }

    std::string FindShippedPresetDir()
    {
        std::error_code ec;
        fs::path        dir = fs::absolute(fs::path(__FILE__), ec).parent_path();
        while (!dir.empty())
        {
            const fs::path candidate = dir / "Cluiche/Assets/CoW/Global/WorldGen/Presets";
            if (fs::exists(candidate, ec)) { return candidate.string(); }
            if (dir == dir.parent_path()) { break; }
            dir = dir.parent_path();
        }
        return {};
    }

    const char* const kPresets[] = { "default", "continental", "archipelago", "highlands" };

    struct SizeCase { const char* name; int width; int height; };
    const SizeCase kSmall{ "Small", 66, 42 };
    const SizeCase kStandard{ "Standard", 80, 52 };
    const SizeCase kLarge{ "Large", 104, 64 };

    // A session configured for (size, preset), not yet generated.
    struct Sess
    {
        TestSession          s;
        Dia::Maths::RandomStream rng{ 1 };

        Sess(const SizeCase& size, const char* preset)
        {
            s.SetPresetDirectories(FindShippedPresetDir(), "");
            EXPECT_TRUE(s.SetSize(Obj("size", size.name))["success"].asBool());
            if (std::string(preset) != "default") { EXPECT_TRUE(s.PresetLoad(Obj("id", preset))["success"].asBool()) << preset; }
        }
        bool Regenerate(uint64_t seed)
        {
            Json::Value a = Obj();
            a["seed"] = static_cast<Json::UInt64>(seed);
            EXPECT_TRUE(s.Regenerate(a)["success"].asBool());
            TickUntilIdle(s, rng);
            return s.State() == WorldGenState::Complete;
        }
        void Preview(uint64_t seed)
        {
            Json::Value a = Obj();
            a["seed"] = static_cast<Json::UInt64>(seed);
            EXPECT_TRUE(s.Set(a)["success"].asBool());
            TickUntilIdle(s, rng);
        }
    };

    bool SameCell(const Cell& x, const Cell& y, bool withRegion)
    {
        return x.elevation == y.elevation && x.latitude == y.latitude && x.waterSource == y.waterSource && x.terrain == y.terrain &&
               x.stratum == y.stratum && x.landmass == y.landmass && x.terrainFlags == y.terrainFlags && x.downstream == y.downstream &&
               x.biome == y.biome && (!withRegion || x.region == y.region);
    }

    // Terrain + biomes (+ regions and sites when full).
    bool SameWorld(const WorldStructure& a, const WorldStructure& b, bool full)
    {
        if (a.dimensions.width != b.dimensions.width || a.dimensions.height != b.dimensions.height || a.CellCount() != b.CellCount()) { return false; }
        for (int i = 0; i < a.CellCount(); ++i)
        {
            if (!SameCell(a.Cells()[i], b.Cells()[i], full)) { return false; }
        }
        if (!full) { return true; }
        if (!(a.regions == b.regions) || a.sites.Count() != b.sites.Count()) { return false; }
        for (int i = 0; i < a.sites.Count(); ++i)
        {
            if (!(a.sites.All()[i] == b.sites.All()[i])) { return false; }
        }
        return true;
    }

    // Dimensions match the size and every cell field is in range.
    void CheckRanges(const std::string& label, const WorldStructure& w, const SizeCase& size)
    {
        ASSERT_EQ(w.dimensions.width, size.width) << label;
        ASSERT_EQ(w.dimensions.height, size.height) << label;
        ASSERT_EQ(w.CellCount(), size.width * size.height) << label;
        for (int i = 0; i < w.CellCount(); ++i)
        {
            const Cell& c = w.Cells()[i];
            ASSERT_TRUE(std::isfinite(c.elevation) && c.elevation >= 0.0f && c.elevation <= 1.0f) << label << " elevation cell " << i << " = " << c.elevation;
            ASSERT_TRUE(std::isfinite(c.latitude) && c.latitude >= 0.0f && c.latitude <= 1.0f) << label << " latitude cell " << i << " = " << c.latitude;
            ASSERT_LT(static_cast<int>(c.biome), static_cast<int>(::Draoi::HexWorld::Testing::ShippedBiomeRegistry().Count())) << label << " biome cell " << i;
            ASSERT_LE(c.downstream, kNoDirection) << label << " downstream cell " << i;
            ASSERT_TRUE(c.region == kNoRegion || c.region <= w.regions.regions.Size()) << label << " region cell " << i << " = " << int(c.region);
            const bool ocean = c.waterSource == WaterSource::Ocean;
            ASSERT_EQ(ocean, c.landmass == 0 || c.biome == B("Biome.Ocean")) << label << " ocean/landmass/biome mismatch at cell " << i;
        }
    }

    // Pipeline acceptance on a Regenerate world: shape, biomes, regions, layout.
    void CheckAcceptance(const std::string& label, WorldGenSession& s)
    {
        const WorldStructure& w = s.World();
        const ShapeFailures   f = CheckWorldShape(s.Generator(), w, TestRules());
        EXPECT_TRUE(f.Empty()) << label << " terrain shape: " << f.Text();

        const TerrainLayers& L = s.Generator().GetLayers();
        const uint32_t*      biome = L.GetUInt(L.FindLayer("biomeIndex"));
        ASSERT_NE(biome, nullptr) << label;
        EXPECT_TRUE(FindForbiddenPairs(w.dimensions, biome).empty()) << label << " forbidden biome pairs";

        EXPECT_TRUE(s.RegionsFresh()) << label;
        const CheckErrors e = CheckAllRegions(w, -1.0f);
        EXPECT_TRUE(e.empty()) << label << " regions: " << ChkJoin(e);
        EXPECT_GE(int(w.regions.mainlandRegionCount), 10) << label << " mainland regions";
        EXPECT_LE(int(w.regions.mainlandRegionCount), 16) << label << " mainland regions";

        const Json::Value v = s.Validate(Obj());
        EXPECT_TRUE(v["success"].asBool() && v["ok"].asBool()) << label << " validate: " << v.toStyledString().substr(0, 600);
        const Json::Value st = s.Status();
        EXPECT_TRUE(st["layout"].isNull() || st["layout"]["ok"].asBool()) << label << " layout";
    }

    WorldStructure RoundTrip(WorldGenSession& a, const std::string& name, bool& ok, std::string& seedAndSize)
    {
        const fs::path dir = fs::temp_directory_path() / ("WorldGenSizesRoundTrip_" + name);   // unique per case: shards run concurrently
        std::error_code ec;
        fs::create_directories(dir, ec);
        TestSession out;
        out.SetPresetDirectories(FindShippedPresetDir(), "");
        WorldGenSession& src = a;
        src.SetWorldDirectories("", dir.string());
        Json::Value ex = Obj("name", name);
        ex["overwrite"] = true;
        const Json::Value exr = src.ExportWorld(ex);
        ok = exr["success"].asBool();
        out.SetWorldDirectories("", dir.string());
        const Json::Value im = out.ImportWorld(Obj("name", name));
        ok = ok && im["success"].asBool();
        if (!ok) { std::printf("[ round trip ] export: %s import: %s\n", exr.toStyledString().substr(0, 300).c_str(), im.toStyledString().substr(0, 300).c_str()); }
        seedAndSize = std::to_string(out.Seed()) + "/" + std::to_string(out.Dimensions().width) + "x" + std::to_string(out.Dimensions().height);
        WorldStructure copy;
        copy.Resize(out.World().dimensions);
        std::memcpy(copy.Cells(), out.World().Cells(), sizeof(Cell) * static_cast<size_t>(out.World().CellCount()));
        copy.regions = out.World().regions;
        copy.sites = out.World().sites;
        fs::remove_all(dir, ec);
        return copy;
    }

    // ---- per-size scenarios ----

    void RunDeterminismAndRanges(const SizeCase& size, std::initializer_list<uint64_t> seeds)
    {
        for (const char* preset : kPresets)
        {
            for (uint64_t seed : seeds)
            {
                const std::string label = std::string(size.name) + "/" + preset + "/seed " + std::to_string(seed);
                Sess a(size, preset);
                ASSERT_TRUE(a.Regenerate(seed)) << label << " Regenerate did not complete";
                CheckRanges(label, a.s.World(), size);

                // Same seed + params + size => identical world, regions and sites.
                Sess b(size, preset);
                ASSERT_TRUE(b.Regenerate(seed)) << label;
                EXPECT_TRUE(SameWorld(a.s.World(), b.s.World(), true)) << label << " two Regenerates differ";

                // Live preview terrain == Regenerate terrain (regions only come from Regenerate).
                Sess p(size, preset);
                p.Preview(seed);
                EXPECT_EQ(p.s.State(), WorldGenState::Complete) << label;
                EXPECT_FALSE(p.s.RegionsFresh()) << label;
                EXPECT_TRUE(SameWorld(p.s.World(), a.s.World(), false)) << label << " live preview != Regenerate terrain";
                CheckRanges(label + " preview", p.s.World(), size);
            }
        }
    }

    void RunAcceptanceAndRoundTrip(const SizeCase& size, std::initializer_list<uint64_t> seeds)
    {
        for (const char* preset : kPresets)
        {
            for (uint64_t seed : seeds)
            {
                const std::string label = std::string(size.name) + "/" + preset + "/seed " + std::to_string(seed);
                Sess a(size, preset);
                ASSERT_TRUE(a.Regenerate(seed)) << label << " Regenerate did not complete: " << a.s.Status().toStyledString().substr(0, 400);
                CheckAcceptance(label, a.s);

                bool        ok = false;
                std::string seedAndSize;
                const WorldStructure imported = RoundTrip(a.s, std::string("rt_") + size.name + "_" + preset + "_" + std::to_string(seed), ok, seedAndSize);
                ASSERT_TRUE(ok) << label << " export/import failed";
                EXPECT_EQ(seedAndSize, std::to_string(seed) + "/" + std::to_string(size.width) + "x" + std::to_string(size.height)) << label;
                EXPECT_TRUE(SameWorld(a.s.World(), imported, true)) << label << " import != exported world";
            }
        }
    }

    void RunSwitching(const char* preset, uint64_t seed)
    {
        Sess a(kSmall, preset);
        ASSERT_TRUE(a.Regenerate(seed));
        const std::string params = a.s.Params()["params"].toStyledString();   // 'effective' legitimately varies with size
        const uint64_t    seed0 = a.s.Seed();
        // Snapshot the first Small world.
        WorldStructure first;
        first.Resize(a.s.World().dimensions);
        std::memcpy(first.Cells(), a.s.World().Cells(), sizeof(Cell) * static_cast<size_t>(first.CellCount()));
        first.regions = a.s.World().regions;
        first.sites = a.s.World().sites;

        for (const SizeCase* size : { &kStandard, &kLarge, &kSmall })
        {
            ASSERT_TRUE(a.s.SetSize(Obj("size", size->name))["success"].asBool()) << size->name;
            TickUntilIdle(a.s, a.rng);
            EXPECT_EQ(a.s.Seed(), seed0) << size->name;
            EXPECT_EQ(a.s.Params()["params"].toStyledString(), params) << size->name << " params changed by SetSize";
            EXPECT_EQ(a.s.Status()["size"].asString(), size->name);
            CheckRanges(std::string("switch ") + size->name, a.s.World(), *size);
        }
        // Back at Small: the live-preview terrain equals the first Regenerate's terrain.
        EXPECT_TRUE(SameWorld(first, a.s.World(), false)) << preset << " Small after the round trip (preview)";
        // A Regenerate restores regions and sites: the whole world equals the first Small world.
        ASSERT_TRUE(a.s.Regenerate(Json::Value(Json::objectValue))["success"].asBool());
        TickUntilIdle(a.s, a.rng);
        ASSERT_EQ(a.s.State(), WorldGenState::Complete);
        EXPECT_TRUE(SameWorld(first, a.s.World(), true)) << preset << " Small after the round trip (Regenerate)";
    }

#ifdef DIA_DEBUG
    void RunDomainState(const SizeCase& size, const char* preset)
    {
        CoW::WorldGenerationModule module{ Dia::Core::StringCRC("WorldGenerationModule") };
        module.SetRulesSidecarText(CoW::WorldGeneration::TestSidecarText()); module.LoadRulesSidecar(); module.GetSession().SetBiomeRegistry(&::Draoi::HexWorld::Testing::ShippedBiomeRegistry());
        CoW::WorldGenDebugDomain   domain{ module };
        Dia::Maths::RandomStream   rng{ 1 };
        module.GetSession().SetPresetDirectories(FindShippedPresetDir(), "");
        domain.OnCommand(Dia::Core::StringCRC("setSize"), Obj("size", size.name));
        if (std::string(preset) != "default") { domain.OnCommand(Dia::Core::StringCRC("presetLoad"), Obj("id", preset)); }
        domain.OnCommand(Dia::Core::StringCRC("regenerate"), Obj("seed", 5));
        TickUntilIdle(module.GetSession(), rng);
        Json::Value cell = Obj(); cell["x"] = 10; cell["y"] = 10;
        domain.OnCommand(Dia::Core::StringCRC("selectCell"), cell);
        domain.ForceFullFrame();
        Json::Value st;
        domain.GetJSONState(st);
        Json::StreamWriterBuilder b;
        b["indentation"] = "";
        const size_t bytes = Json::writeString(b, st).size();
        EXPECT_EQ(st["stats"]["size"].asString(), size.name);
        EXPECT_EQ(st["stats"]["width"].asInt(), size.width);
        EXPECT_EQ(st["stats"]["height"].asInt(), size.height);
        EXPECT_TRUE(st["cell"].isObject());
        std::printf("[ measured ] compact WorldGen state at %s/%s: %zu bytes\n", size.name, preset, bytes);
        EXPECT_LT(bytes, 12u * 1024u) << size.name << "/" << preset;
    }
#endif
}

// ---- Small ----

TEST(WorldGenSizes, SmallDeterminismAndRanges)       { RunDeterminismAndRanges(kSmall, { 1, 2 }); }
TEST(WorldGenSizes, SmallAcceptanceAndRoundTrip)     { RunAcceptanceAndRoundTrip(kSmall, { 1, 2 }); }

// ---- Standard ----

TEST(WorldGenSizes, StandardDeterminismAndRanges)    { RunDeterminismAndRanges(kStandard, { 1, 2 }); }
TEST(WorldGenSizes, StandardAcceptanceAndRoundTrip)  { RunAcceptanceAndRoundTrip(kStandard, { 1, 2 }); }

// ---- Large (slow) ----

TEST(SLOW_WorldGenSizes, LargeDeterminismAndRanges)   { RunDeterminismAndRanges(kLarge, { 1, 2 }); }
TEST(SLOW_WorldGenSizes, LargeAcceptanceAndRoundTrip) { RunAcceptanceAndRoundTrip(kLarge, { 1, 2 }); }

// ---- Switching ----

TEST(WorldGenSizes, SwitchingSmallStandardLargeSmallKeepsSeedParamsAndRegeneratesIdentically)
{
    for (const char* preset : kPresets) { RunSwitching(preset, 3); }
}

// ---- Debug-domain compact state (12 KB contract) ----

#ifdef DIA_DEBUG
TEST(WorldGenSizes, DomainStateUnderTwelveKbAtSmallAndStandard)
{
    for (const char* preset : kPresets)
    {
        RunDomainState(kSmall, preset);
        RunDomainState(kStandard, preset);
    }
}

TEST(SLOW_WorldGenSizes, DomainStateUnderTwelveKbAtLarge)
{
    for (const char* preset : kPresets) { RunDomainState(kLarge, preset); }
}
#endif
