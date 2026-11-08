// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/f1-debug-host-integration.md (Command set: terrain slice)
// WorldGenSession preset.load/save, presets, layers and cell queries.
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/WorldGenSession.h>

#include <DiaPathfinding/HexTopology.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

using namespace CoW::WorldGeneration;

namespace
{

std::string FindShippedPresetDir()
{
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::absolute(std::filesystem::path(__FILE__), ec).parent_path();
    while (!dir.empty())
    {
        const std::filesystem::path candidate = dir / "Cluiche/Assets/CoW/Global/WorldGen/Presets";
        if (std::filesystem::is_directory(candidate, ec))
        {
            return candidate.string();
        }
        const std::filesystem::path parent = dir.parent_path();
        if (parent == dir)
        {
            break;
        }
        dir = parent;
    }
    return {};
}

// Fresh empty temp directory per test.
class TempDir
{
public:
    TempDir()
    {
        static int counter = 0;
        mPath = std::filesystem::temp_directory_path() / ("cow_wgsq_test_" + std::to_string(++counter) + "_" +
                std::to_string(reinterpret_cast<uintptr_t>(this)));
        std::error_code ec;
        std::filesystem::remove_all(mPath, ec);
        std::filesystem::create_directories(mPath, ec);
    }
    ~TempDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(mPath, ec);
    }
    std::string Str() const { return mPath.string(); }
    std::filesystem::path Path() const { return mPath; }
private:
    std::filesystem::path mPath;
};

Json::Value Obj(const char* key, const Json::Value& v)
{
    Json::Value o(Json::objectValue);
    o[key] = v;
    return o;
}

void WriteFile(const std::filesystem::path& p, const std::string& text)
{
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f << text;
}

void WritePreset(const std::string& dir, const char* id, const char* displayName, float landFraction)
{
    WorldShapePreset p;
    p.id = id;
    p.displayName = displayName;
    p.params.landFraction = landFraction;
    ASSERT_TRUE(SavePreset(dir, p, true).Ok());
}

Json::Value Args(const char* id)
{
    return Obj("id", id);
}

Json::Value SaveArgs(const char* id, const char* name, bool overwrite = false)
{
    Json::Value a(Json::objectValue);
    a["id"] = id;
    a["displayName"] = name;
    a["overwrite"] = overwrite;
    return a;
}

bool Ok(const Json::Value& r)
{
    return r.isObject() && r["success"].asBool();
}

void Generate(WorldGenSession& s)
{
    Dia::Maths::RandomStream rng(1);
    s.Tick(rng);
}

bool SameCells(const WorldStructure& a, const WorldStructure& b)
{
    return a.CellCount() == b.CellCount() &&
           std::memcmp(a.Cells(), b.Cells(), static_cast<size_t>(a.CellCount()) * sizeof(CoW::WorldGeneration::Cell)) == 0;
}

} // namespace

TEST(WorldGenSessionQueries, PresetLoadShippedHighlandsReplacesAllParamsKeepsSeed)
{
    const std::string shipped = FindShippedPresetDir();
    ASSERT_FALSE(shipped.empty());

    WorldGenSession s;
    s.SetPresetDirectories(shipped, "");
    Generate(s);
    s.Set(Obj("seed", 777));
    s.Set(Obj("rivers", 0.9));
    Generate(s);
    const WorldStructure before = s.World();
    const uint32_t revBefore = s.Revision();

    const Json::Value r = s.PresetLoad(Args("highlands"));
    ASSERT_TRUE(Ok(r)) << r.toStyledString();
    EXPECT_EQ(r["id"].asString(), "highlands");
    EXPECT_TRUE(r["warnings"].isArray());

    WorldShapePreset expected;
    RuleWarnings w;
    ASSERT_TRUE(LoadPreset(shipped, "highlands", expected, w).Ok());
    EXPECT_TRUE(s.Shape() == expected.params);
    EXPECT_EQ(s.Seed(), 777u);
    EXPECT_TRUE(s.IsDirty());

    const Json::Value p = s.Params();
    EXPECT_EQ(p["preset"].asString(), "highlands");
    EXPECT_FALSE(p["modified"].asBool());

    Dia::Maths::RandomStream rng(1);
    EXPECT_TRUE(s.Tick(rng));
    EXPECT_EQ(s.Revision(), revBefore + 1);
    EXPECT_FALSE(SameCells(before, s.World()));
}

TEST(WorldGenSessionQueries, PresetLoadThenSetClearsPresetAndMarksModified)
{
    const std::string shipped = FindShippedPresetDir();
    ASSERT_FALSE(shipped.empty());
    WorldGenSession s;
    s.SetPresetDirectories(shipped, "");
    ASSERT_TRUE(Ok(s.PresetLoad(Args("highlands"))));
    s.Set(Obj("lakes", 11));
    const Json::Value p = s.Params();
    EXPECT_EQ(p["preset"].asString(), "");
    EXPECT_TRUE(p["modified"].asBool());
}

TEST(WorldGenSessionQueries, PresetLoadUnknownIdFailsAndLeavesStateUnchanged)
{
    TempDir deployed;
    TempDir out;
    WorldGenSession s;
    s.SetPresetDirectories(deployed.Str(), out.Str());
    Generate(s);
    const WorldShapeParams shape = s.Shape();
    const Json::Value paramsBefore = s.Params();

    const Json::Value r = s.PresetLoad(Args("nope"));
    EXPECT_FALSE(Ok(r));
    EXPECT_FALSE(r["error"].asString().empty());
    EXPECT_TRUE(s.Shape() == shape);
    EXPECT_FALSE(s.IsDirty());
    EXPECT_EQ(s.Params(), paramsBefore);
}

TEST(WorldGenSessionQueries, PresetLoadMissingIdArgFails)
{
    WorldGenSession s;
    EXPECT_FALSE(Ok(s.PresetLoad(Json::Value(Json::objectValue))));
    EXPECT_FALSE(Ok(s.PresetLoad(Obj("id", 5))));
}

TEST(WorldGenSessionQueries, PresetLoadCorruptFileFailsAndLeavesStateUnchanged)
{
    TempDir deployed;
    WriteFile(deployed.Path() / "broken.worldshape.json", "{ not json");
    WorldGenSession s;
    s.SetPresetDirectories(deployed.Str(), "");
    Generate(s);
    const WorldShapeParams shape = s.Shape();

    const Json::Value r = s.PresetLoad(Args("broken"));
    EXPECT_FALSE(Ok(r));
    EXPECT_FALSE(r["error"].asString().empty());
    EXPECT_TRUE(s.Shape() == shape);
    EXPECT_FALSE(s.IsDirty());
    EXPECT_EQ(s.Params()["preset"].asString(), "");
}

TEST(WorldGenSessionQueries, PresetLoadOutDirCopyWinsOverDeployed)
{
    TempDir deployed;
    TempDir out;
    WritePreset(deployed.Str(), "dup", "Deployed", 0.30f);
    WritePreset(out.Str(), "dup", "Out", 0.55f);
    WorldGenSession s;
    s.SetPresetDirectories(deployed.Str(), out.Str());
    ASSERT_TRUE(Ok(s.PresetLoad(Args("dup"))));
    EXPECT_FLOAT_EQ(s.Shape().landFraction, 0.55f);
}

TEST(WorldGenSessionQueries, PresetLoadFallsBackToDeployedWhenNotInOut)
{
    TempDir deployed;
    TempDir out;
    WritePreset(deployed.Str(), "only_dep", "Deployed", 0.31f);
    WorldGenSession s;
    s.SetPresetDirectories(deployed.Str(), out.Str());
    ASSERT_TRUE(Ok(s.PresetLoad(Args("only_dep"))));
    EXPECT_FLOAT_EQ(s.Shape().landFraction, 0.31f);
}

TEST(WorldGenSessionQueries, PresetLoadSameParamsDoesNotDirtyButSetsPresetId)
{
    TempDir out;
    WorldGenSession s;
    s.SetPresetDirectories("", out.Str());
    WritePreset(out.Str(), "same", "Same", WorldShapeParams{}.landFraction);
    Generate(s);
    ASSERT_FALSE(s.IsDirty());
    ASSERT_TRUE(Ok(s.PresetLoad(Args("same"))));
    EXPECT_FALSE(s.IsDirty());
    EXPECT_EQ(s.Params()["preset"].asString(), "same");
    EXPECT_FALSE(s.Params()["modified"].asBool());
}

TEST(WorldGenSessionQueries, PresetLoadReportsWarnings)
{
    TempDir out;
    WorldShapePreset base;
    base.id = "warn";
    base.displayName = "W";
    Json::Value doc = PresetToJson(base);
    doc["params"]["landFraction"] = 5.0;   // out of range -> clamped + "range:" warning
    WriteFile(out.Path() / "warn.worldshape.json", doc.toStyledString());
    WorldGenSession s;
    s.SetPresetDirectories("", out.Str());
    const Json::Value r = s.PresetLoad(Args("warn"));
    ASSERT_TRUE(Ok(r)) << r.toStyledString();
    ASSERT_GE(r["warnings"].size(), 1u);
    EXPECT_TRUE(r["warnings"][0].isString());
    EXPECT_FLOAT_EQ(s.Shape().landFraction, 0.65f);
}

TEST(WorldGenSessionQueries, PresetSaveRefusesExistingWithoutOverwrite)
{
    TempDir out;
    WorldGenSession s;
    s.SetPresetDirectories("", out.Str());
    const Json::Value first = s.PresetSave(SaveArgs("mine", "Mine"));
    ASSERT_TRUE(Ok(first)) << first.toStyledString();
    EXPECT_FALSE(first["isSource"].asBool());
    EXPECT_EQ(first["writtenTo"].asString(), PresetFilePath(out.Str(), "mine"));
    EXPECT_TRUE(std::filesystem::exists(first["writtenTo"].asString()));

    const Json::Value second = s.PresetSave(SaveArgs("mine", "Mine"));
    EXPECT_FALSE(Ok(second));
    EXPECT_EQ(second["status"].asString(), "AlreadyExists");

    s.Set(Obj("rivers", 0.2));
    const Json::Value third = s.PresetSave(SaveArgs("mine", "Mine", true));
    EXPECT_TRUE(Ok(third)) << third.toStyledString();
}

TEST(WorldGenSessionQueries, PresetSaveBadIdRejected)
{
    TempDir out;
    WorldGenSession s;
    s.SetPresetDirectories("", out.Str());
    const Json::Value r = s.PresetSave(SaveArgs("Bad Id!", "x"));
    EXPECT_FALSE(Ok(r));
    EXPECT_EQ(r["status"].asString(), "InvalidId");
    EXPECT_FALSE(Ok(s.PresetSave(Json::Value(Json::objectValue))));
}

TEST(WorldGenSessionQueries, PresetSaveUnconfiguredOutDirErrors)
{
    TempDir deployed;
    WorldGenSession s;
    s.SetPresetDirectories(deployed.Str(), "");
    const Json::Value r = s.PresetSave(SaveArgs("mine", "Mine"));
    EXPECT_FALSE(Ok(r));
    EXPECT_FALSE(r["error"].asString().empty());
    EXPECT_TRUE(std::filesystem::is_empty(deployed.Path()));
}

TEST(WorldGenSessionQueries, PresetSaveNeverWritesDeployedDir)
{
    TempDir deployed;
    TempDir out;
    WorldGenSession s;
    s.SetPresetDirectories(deployed.Str(), out.Str());
    ASSERT_TRUE(Ok(s.PresetSave(SaveArgs("mine", "Mine"))));
    EXPECT_TRUE(std::filesystem::is_empty(deployed.Path()));
}

TEST(WorldGenSessionQueries, PresetSaveThenLoadRoundTripsShapeAndSetsPresetId)
{
    TempDir out;
    WorldGenSession s;
    s.SetPresetDirectories("", out.Str());
    s.Set(Obj("landFraction", 0.4321));
    s.Set(Obj("islands", 5));
    s.Set(Obj("barrierForm", "VolcanicChain"));
    s.Set(Obj("latitudeNorthDeg", 33.3));
    const WorldShapeParams saved = s.Shape();

    ASSERT_TRUE(Ok(s.PresetSave(SaveArgs("rt", "RT"))));
    EXPECT_EQ(s.Params()["preset"].asString(), "rt");
    EXPECT_FALSE(s.Params()["modified"].asBool());

    s.Set(Obj("landFraction", 0.25));
    s.Set(Obj("islands", 0));
    ASSERT_FALSE(s.Shape() == saved);
    ASSERT_TRUE(Ok(s.PresetLoad(Args("rt"))));
    EXPECT_TRUE(s.Shape() == saved);
}

TEST(WorldGenSessionQueries, PresetsListsUnionSortedOutWinsNoDuplicates)
{
    TempDir deployed;
    TempDir out;
    WritePreset(deployed.Str(), "zeta", "Zeta", 0.3f);
    WritePreset(deployed.Str(), "dup", "Deployed Dup", 0.3f);
    WritePreset(out.Str(), "alpha", "Alpha", 0.3f);
    WritePreset(out.Str(), "dup", "Out Dup", 0.3f);
    WorldGenSession s;
    s.SetPresetDirectories(deployed.Str(), out.Str());

    const Json::Value list = s.Presets();
    ASSERT_TRUE(list.isArray());
    ASSERT_EQ(list.size(), 3u);
    EXPECT_EQ(list[0]["id"].asString(), "alpha");
    EXPECT_EQ(list[1]["id"].asString(), "dup");
    EXPECT_EQ(list[2]["id"].asString(), "zeta");
    EXPECT_EQ(list[1]["displayName"].asString(), "Out Dup");
    EXPECT_EQ(list[1]["sourcePath"].asString(), PresetFilePath(out.Str(), "dup"));
    EXPECT_EQ(list[2]["sourcePath"].asString(), PresetFilePath(deployed.Str(), "zeta"));
    EXPECT_TRUE(list[0].isMember("description"));
    EXPECT_EQ(list[0]["version"].asUInt(), kWorldShapePresetVersion);
}

TEST(WorldGenSessionQueries, PresetsEmptyWhenUnconfigured)
{
    WorldGenSession s;
    EXPECT_TRUE(s.Presets().isArray());
    EXPECT_EQ(s.Presets().size(), 0u);
}

TEST(WorldGenSessionQueries, PresetsIncludesShippedHighlands)
{
    const std::string shipped = FindShippedPresetDir();
    ASSERT_FALSE(shipped.empty());
    WorldGenSession s;
    s.SetPresetDirectories(shipped, "");
    bool found = false;
    for (const Json::Value& e : s.Presets())
    {
        found = found || e["id"].asString() == "highlands";
    }
    EXPECT_TRUE(found);
}

TEST(WorldGenSessionQueries, LayersSummaryHasOneEntryPerImplementedStage)
{
    WorldGenSession s;
    Generate(s);
    const Json::Value r = s.Layers(Json::Value(Json::objectValue));
    ASSERT_TRUE(r.isMember("stages")) << r.toStyledString();
    EXPECT_EQ(r["rulesVersion"].asUInt(), kDefaultRulesVersion);
    EXPECT_GE(r["totalMs"].asDouble(), 0.0);
    ASSERT_EQ(static_cast<int>(r["stages"].size()), kImplementedStageCount);

    size_t layerTotal = 0;
    for (int i = 0; i < kImplementedStageCount; ++i)
    {
        const Json::Value& st = r["stages"][i];
        EXPECT_EQ(st["id"].asInt(), i);
        EXPECT_EQ(st["name"].asString(), TerrainStageName(static_cast<TerrainStage>(i)));
        EXPECT_TRUE(st["ranLastUpdate"].asBool());
        layerTotal += st["layers"].size();
    }
    EXPECT_EQ(static_cast<int>(layerTotal), TerrainLayers::GetLayerCount());
}

TEST(WorldGenSessionQueries, LayersSummaryElevationRangeWithinUnit)
{
    WorldGenSession s;
    Generate(s);
    const Json::Value r = s.Layers(Json::Value(Json::objectValue));
    bool found = false;
    for (const Json::Value& st : r["stages"])
    {
        for (const Json::Value& l : st["layers"])
        {
            if (l["name"].asString() == "elevation")
            {
                found = true;
                EXPECT_EQ(l["type"].asString(), "float");
                EXPECT_GE(l["min"].asDouble(), 0.0);
                EXPECT_LE(l["max"].asDouble(), 1.0);
                EXPECT_LT(l["min"].asDouble(), l["max"].asDouble());
            }
            if (l["name"].asString() == "landmass")
            {
                EXPECT_EQ(l["type"].asString(), "uint");
            }
        }
    }
    EXPECT_TRUE(found);
}

TEST(WorldGenSessionQueries, LayersSummaryTimingsReflectLastUpdate)
{
    WorldGenSession s;
    Generate(s);
    s.Set(Obj("rivers", 0.9));
    Generate(s);
    const Json::Value r = s.Layers(Json::Value(Json::objectValue));
    ASSERT_TRUE(r.isMember("stages")) << r.toStyledString();
    const Json::Value& landmass = r["stages"][static_cast<int>(TerrainStage::Landmass)];
    const Json::Value& rivers = r["stages"][static_cast<int>(TerrainStage::Rivers)];
    EXPECT_FALSE(landmass["ranLastUpdate"].asBool());
    EXPECT_TRUE(rivers["ranLastUpdate"].asBool());
}

TEST(WorldGenSessionQueries, LayersSummaryBeforeGenerationHasEmptyLayers)
{
    WorldGenSession s;
    const Json::Value r = s.Layers(Json::Value(Json::objectValue));
    ASSERT_TRUE(r.isMember("stages")) << r.toStyledString();
    ASSERT_EQ(static_cast<int>(r["stages"].size()), kImplementedStageCount);
    for (const Json::Value& st : r["stages"])
    {
        EXPECT_EQ(st["layers"].size(), 0u);
    }
}

Json::Value PageArgs(const char* layer, const Json::Value& offset, const Json::Value& count)
{
    Json::Value o(Json::objectValue);
    o["layer"] = layer;
    if (!offset.isNull()) o["offset"] = offset;
    if (!count.isNull()) o["count"] = count;
    return o;
}

uint64_t RefFnv(const void* data, size_t bytes)
{
    const unsigned char* p = static_cast<const unsigned char*>(data);
    uint64_t h = 14695981039346656037ull;
    for (size_t i = 0; i < bytes; ++i) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}

std::string HexOf(uint64_t h)
{
    char b[17];
    std::snprintf(b, sizeof(b), "%016llx", static_cast<unsigned long long>(h));
    return b;
}

std::string SummaryHash(const Json::Value& summary, const char* layer)
{
    for (const Json::Value& st : summary["stages"])
        for (const Json::Value& l : st["layers"])
            if (l["name"].asString() == layer)
                return l.isMember("hash") ? l["hash"].asString() : std::string("<missing>");
    return "<nolayer>";
}

TEST(WorldGenSessionQueries, LayersFirstPageUsesDefaults)
{
    WorldGenSession s;
    Generate(s);
    const size_t total = static_cast<size_t>(s.Dimensions().width) * s.Dimensions().height;
    const Json::Value e = s.Layers(Obj("layer", "elevation"));
    ASSERT_TRUE(Ok(e)) << e.toStyledString();
    EXPECT_EQ(e["name"].asString(), "elevation");
    EXPECT_EQ(e["type"].asString(), "float");
    EXPECT_EQ(e["offset"].asUInt(), 0u);
    EXPECT_EQ(e["count"].asUInt(), 500u);
    EXPECT_EQ(e["total"].asUInt(), total);
    EXPECT_EQ(e["nextOffset"].asUInt(), 500u);
    EXPECT_EQ(e["revision"].asUInt(), s.Revision());
    EXPECT_EQ(e["values"].size(), 500u);
}

void ExpectReassembles(const char* layer, bool isFloat)
{
    WorldGenSession s;
    Generate(s);
    const size_t total = static_cast<size_t>(s.Dimensions().width) * s.Dimensions().height;
    const TerrainLayers& tl = s.Generator().GetLayers();
    const int li = tl.FindLayer(layer);
    const float* fd = isFloat ? tl.GetFloat(li) : nullptr;
    const uint32_t* ud = isFloat ? nullptr : tl.GetUInt(li);
    ASSERT_TRUE(isFloat ? fd != nullptr : ud != nullptr);

    size_t offset = 0;
    size_t got = 0;
    int pages = 0;
    for (;;)
    {
        const Json::Value r = s.Layers(PageArgs(layer, static_cast<Json::UInt>(offset), Json::Value()));
        ASSERT_TRUE(Ok(r)) << r.toStyledString();
        ASSERT_EQ(r["offset"].asUInt(), offset);
        ASSERT_EQ(r["values"].size(), r["count"].asUInt());
        for (Json::ArrayIndex k = 0; k < r["values"].size(); ++k)
        {
            const size_t i = offset + k;
            if (isFloat)
            {
                const float f = static_cast<float>(r["values"][k].asDouble());
                ASSERT_EQ(std::memcmp(&f, &fd[i], sizeof(float)), 0) << i;
            }
            else
            {
                ASSERT_EQ(r["values"][k].asUInt(), ud[i]) << i;
            }
        }
        got += r["values"].size();
        ++pages;
        ASSERT_LT(pages, 50);
        if (r["nextOffset"].isNull())
        {
            break;
        }
        ASSERT_EQ(r["nextOffset"].asUInt(), offset + r["count"].asUInt());
        offset = r["nextOffset"].asUInt();
    }
    EXPECT_EQ(got, total);
    EXPECT_EQ(total, 2772u);
}

TEST(WorldGenSessionQueries, LayersPagesReassembleFloatLayerExactly)
{
    ExpectReassembles("elevation", true);
}

TEST(WorldGenSessionQueries, LayersPagesReassembleUIntLayerExactly)
{
    ExpectReassembles("terrainType", false);
}

TEST(WorldGenSessionQueries, LayersCustomCountAndClamp)
{
    WorldGenSession s;
    Generate(s);
    const Json::Value a = s.Layers(PageArgs("landmass", 10, 7));
    ASSERT_TRUE(Ok(a)) << a.toStyledString();
    EXPECT_EQ(a["offset"].asUInt(), 10u);
    EXPECT_EQ(a["count"].asUInt(), 7u);
    EXPECT_EQ(a["nextOffset"].asUInt(), 17u);

    const Json::Value b = s.Layers(PageArgs("landmass", 0, 5000));
    ASSERT_TRUE(Ok(b)) << b.toStyledString();
    EXPECT_EQ(b["count"].asUInt(), 1000u);
    EXPECT_EQ(b["values"].size(), 1000u);
    EXPECT_EQ(b["nextOffset"].asUInt(), 1000u);

    // Float pages are capped at 500 so the reply stays under the 16 KB sim payload.
    const Json::Value f = s.Layers(PageArgs("elevation", 0, 5000));
    ASSERT_TRUE(Ok(f)) << f.toStyledString();
    EXPECT_EQ(f["count"].asUInt(), 500u);
    EXPECT_EQ(f["nextOffset"].asUInt(), 500u);
}

TEST(WorldGenSessionQueries, LayersLastPageHasNullNextOffset)
{
    WorldGenSession s;
    Generate(s);
    const Json::Value r = s.Layers(PageArgs("landmass", 2700, Json::Value()));
    ASSERT_TRUE(Ok(r)) << r.toStyledString();
    EXPECT_EQ(r["count"].asUInt(), 72u);
    EXPECT_TRUE(r.isMember("nextOffset"));
    EXPECT_TRUE(r["nextOffset"].isNull());
}

TEST(WorldGenSessionQueries, LayersOffsetAtTotalIsEmptySuccessBeyondIsError)
{
    WorldGenSession s;
    Generate(s);
    const Json::Value r = s.Layers(PageArgs("landmass", 2772, Json::Value()));
    ASSERT_TRUE(Ok(r)) << r.toStyledString();
    EXPECT_EQ(r["count"].asUInt(), 0u);
    EXPECT_EQ(r["values"].size(), 0u);
    EXPECT_TRUE(r["nextOffset"].isNull());
    EXPECT_FALSE(Ok(s.Layers(PageArgs("landmass", 2773, Json::Value()))));
}

TEST(WorldGenSessionQueries, LayersBadOffsetOrCountIsError)
{
    WorldGenSession s;
    Generate(s);
    EXPECT_FALSE(Ok(s.Layers(PageArgs("landmass", -1, Json::Value()))));
    EXPECT_FALSE(Ok(s.Layers(PageArgs("landmass", Json::Value(), -5))));
    EXPECT_FALSE(Ok(s.Layers(PageArgs("landmass", 1.5, Json::Value()))));
    EXPECT_FALSE(Ok(s.Layers(PageArgs("landmass", Json::Value(), 2.5))));
    EXPECT_FALSE(Ok(s.Layers(PageArgs("landmass", "0", Json::Value()))));
    EXPECT_FALSE(Ok(s.Layers(PageArgs("landmass", Json::Value(), "10"))));
    const Json::Value e = s.Layers(PageArgs("landmass", -1, Json::Value()));
    EXPECT_TRUE(e["error"].isString());
}

TEST(WorldGenSessionQueries, LayersCountZeroIsEmptySuccessWithSameNextOffset)
{
    WorldGenSession s;
    Generate(s);
    const Json::Value r = s.Layers(PageArgs("landmass", 40, 0));
    ASSERT_TRUE(Ok(r)) << r.toStyledString();
    EXPECT_EQ(r["count"].asUInt(), 0u);
    EXPECT_EQ(r["values"].size(), 0u);
    EXPECT_EQ(r["nextOffset"].asUInt(), 40u);
}

TEST(WorldGenSessionQueries, LayersPagingValidationComesAfterLayerResolution)
{
    WorldGenSession s;
    EXPECT_EQ(s.Layers(PageArgs("elevation", -1, Json::Value()))["error"].asString(), "no world generated yet");
    Generate(s);
    const std::string err = s.Layers(PageArgs("nonsense", -1, Json::Value()))["error"].asString();
    EXPECT_NE(err.find("unknown layer"), std::string::npos) << err;
}

TEST(WorldGenSessionQueries, LayersWorstPageFitsInSimReply)
{
    WorldGenSession s;
    Generate(s);
    // Hard max count (1000) of every layer must serialise well under the 16 KB sim payload.
    for (int i = 0; i < TerrainLayers::GetLayerCount(); ++i)
    {
        const LayerInfo& info = TerrainLayers::GetLayerInfo(i);
        const Json::Value r = s.Layers(PageArgs(info.name, 0, 1000));
        ASSERT_TRUE(Ok(r)) << info.name;
        Json::StreamWriterBuilder builder;   // same writer SimQueryHost uses for the reply
        builder["indentation"] = "";
        EXPECT_LT(Json::writeString(builder, r).size(), 15000u) << info.name;
    }
}

TEST(WorldGenSessionQueries, LayersSummaryHashMatchesIndependentFnv)
{
    WorldGenSession s;
    Generate(s);
    const Json::Value r = s.Layers(Json::Value(Json::objectValue));
    EXPECT_EQ(r["revision"].asUInt(), s.Revision());
    const size_t total = static_cast<size_t>(s.Dimensions().width) * s.Dimensions().height;
    const TerrainLayers& tl = s.Generator().GetLayers();
    for (int i = 0; i < TerrainLayers::GetLayerCount(); ++i)
    {
        const LayerInfo& info = TerrainLayers::GetLayerInfo(i);
        const void* data = info.type == LayerType::Float ? static_cast<const void*>(tl.GetFloat(i))
                                                         : static_cast<const void*>(tl.GetUInt(i));
        ASSERT_NE(data, nullptr) << info.name;
        EXPECT_EQ(SummaryHash(r, info.name), HexOf(RefFnv(data, total * 4))) << info.name;
    }
}

TEST(WorldGenSessionQueries, LayersSummaryHashChangesOnlyForAffectedLayers)
{
    WorldGenSession s;
    Generate(s);
    const Json::Value before = s.Layers(Json::Value(Json::objectValue));
    s.Set(Obj("rivers", 0.9));
    Generate(s);
    const Json::Value after = s.Layers(Json::Value(Json::objectValue));
    EXPECT_NE(SummaryHash(before, "riverMask"), SummaryHash(after, "riverMask"));
    EXPECT_EQ(SummaryHash(before, "landmass"), SummaryHash(after, "landmass"));
    EXPECT_GT(after["revision"].asUInt(), before["revision"].asUInt());
}

TEST(WorldGenSessionQueries, LayersUnknownLayerAndPreGenerationErrors)
{
    WorldGenSession s;
    const Json::Value pre = s.Layers(Obj("layer", "elevation"));
    EXPECT_FALSE(Ok(pre));
    EXPECT_EQ(pre["error"].asString(), "no world generated yet");

    Generate(s);
    EXPECT_FALSE(Ok(s.Layers(Obj("layer", "nonsense"))));
    EXPECT_FALSE(Ok(s.Layers(Obj("layer", 5))));
}

TEST(WorldGenSessionQueries, CellByXYAndIndexAgree)
{
    WorldGenSession s;
    Generate(s);
    const int w = s.Dimensions().width;
    Json::Value xy(Json::objectValue);
    xy["x"] = 10;
    xy["y"] = 7;
    const Json::Value a = s.Cell(xy);
    const Json::Value b = s.Cell(Obj("index", 7 * w + 10));
    ASSERT_TRUE(Ok(a)) << a.toStyledString();
    ASSERT_TRUE(Ok(b));
    EXPECT_EQ(a, b);
    EXPECT_EQ(a["index"].asInt(), 7 * w + 10);
    EXPECT_EQ(a["x"].asInt(), 10);
    EXPECT_EQ(a["y"].asInt(), 7);

    Json::Value both(Json::objectValue);
    both["x"] = 0;
    both["y"] = 0;
    both["index"] = 7 * w + 10;
    EXPECT_EQ(s.Cell(both), a);
}

TEST(WorldGenSessionQueries, CellFieldsMatchWorldCells)
{
    WorldGenSession s;
    Generate(s);
    const int count = s.World().CellCount();
    for (int i = 0; i < count; i += 37)
    {
        const CoW::WorldGeneration::Cell& c = s.World().Cells()[i];
        const Json::Value r = s.Cell(Obj("index", i));
        ASSERT_TRUE(Ok(r)) << r.toStyledString();
        EXPECT_FLOAT_EQ(static_cast<float>(r["elevation"].asDouble()), c.elevation);
        EXPECT_FLOAT_EQ(static_cast<float>(r["latitude"].asDouble()), c.latitude);
        EXPECT_EQ(r["waterSource"].asString(), WaterSourceName(c.waterSource));
        EXPECT_EQ(r["terrain"].asString(), TerrainTypeName(c.terrain));
        EXPECT_EQ(r["depthZone"].asString(), DepthZoneName(c.depthZone));
        EXPECT_EQ(r["landmass"].asUInt(), c.landmass);
        EXPECT_EQ(r["flags"].asUInt(), c.terrainFlags);
        if (c.downstream == kNoDirection)
        {
            EXPECT_TRUE(r["downstream"].isNull());
        }
        else
        {
            EXPECT_EQ(r["downstream"].asUInt(), c.downstream);
        }
    }
}

TEST(WorldGenSessionQueries, CellDownstreamNumericOnRiverCell)
{
    WorldGenSession s;
    Generate(s);
    bool sawRiver = false;
    for (int i = 0; i < s.World().CellCount() && !sawRiver; ++i)
    {
        if (s.World().Cells()[i].downstream != kNoDirection)
        {
            sawRiver = true;
            EXPECT_TRUE(s.Cell(Obj("index", i))["downstream"].isIntegral());
        }
    }
    EXPECT_TRUE(sawRiver);
}

TEST(WorldGenSessionQueries, CellLayersContainEveryRegisteredLayer)
{
    WorldGenSession s;
    Generate(s);
    const Json::Value r = s.Cell(Obj("index", 100));
    ASSERT_TRUE(Ok(r));
    ASSERT_TRUE(r["layers"].isObject());
    EXPECT_EQ(static_cast<int>(r["layers"].size()), TerrainLayers::GetLayerCount());
    for (int i = 0; i < TerrainLayers::GetLayerCount(); ++i)
    {
        const LayerInfo& info = TerrainLayers::GetLayerInfo(i);
        ASSERT_TRUE(r["layers"].isMember(info.name)) << info.name;
        if (info.type == LayerType::Float)
        {
            const float* d = s.Generator().GetLayers().GetFloat(i);
            EXPECT_FLOAT_EQ(static_cast<float>(r["layers"][info.name].asDouble()), d[100]) << info.name;
        }
        else
        {
            const uint32_t* d = s.Generator().GetLayers().GetUInt(i);
            EXPECT_EQ(r["layers"][info.name].asUInt(), d[100]) << info.name;
        }
    }
}

TEST(WorldGenSessionQueries, CellNeighboursEdgeAndSymmetric)
{
    WorldGenSession s;
    Generate(s);
    const int w = s.Dimensions().width;
    const int h = s.Dimensions().height;
    const Dia::Pathfinding::HexTopology topo(w, h);

    const Json::Value corner = s.Cell(Obj("index", 0));
    ASSERT_TRUE(Ok(corner));
    ASSERT_EQ(corner["neighbours"].size(), 6u);
    bool sawEdge = false;
    for (const Json::Value& n : corner["neighbours"])
    {
        sawEdge = sawEdge || (n.isString() && n.asString() == "edge");
    }
    EXPECT_TRUE(sawEdge);

    const int mid = (h / 2) * w + w / 2;
    const Json::Value interior = s.Cell(Obj("index", mid));
    ASSERT_TRUE(Ok(interior));
    ASSERT_EQ(interior["neighbours"].size(), 6u);
    for (uint32_t d = 0; d < 6; ++d)
    {
        const Json::Value& n = interior["neighbours"][d];
        ASSERT_TRUE(n.isObject()) << d;
        EXPECT_EQ(n["dir"].asUInt(), d);
        const uint32_t ni = static_cast<uint32_t>(n["y"].asInt() * w + n["x"].asInt());
        EXPECT_EQ(ni, topo.NeighbourAt(static_cast<uint32_t>(mid), d));
        EXPECT_EQ(topo.NeighbourAt(ni, (d + 3) % 6), static_cast<uint32_t>(mid));
    }
}

TEST(WorldGenSessionQueries, CellBadArgsFail)
{
    WorldGenSession s;
    const Json::Value pre = s.Cell(Obj("index", 0));
    EXPECT_FALSE(Ok(pre));

    Generate(s);
    const int count = s.World().CellCount();
    EXPECT_FALSE(Ok(s.Cell(Json::Value(Json::objectValue))));
    EXPECT_FALSE(Ok(s.Cell(Obj("index", count))));
    EXPECT_FALSE(Ok(s.Cell(Obj("index", -1))));
    EXPECT_FALSE(Ok(s.Cell(Obj("index", "3"))));
    EXPECT_FALSE(Ok(s.Cell(Obj("index", 1.5))));
    Json::Value a(Json::objectValue);
    a["x"] = 66;
    a["y"] = 0;
    EXPECT_FALSE(Ok(s.Cell(a)));
    a["x"] = 0;
    a["y"] = 42;
    EXPECT_FALSE(Ok(s.Cell(a)));
    a["x"] = -1;
    a["y"] = 0;
    EXPECT_FALSE(Ok(s.Cell(a)));
    a["x"] = 1;
    a.removeMember("y");
    EXPECT_FALSE(Ok(s.Cell(a)));
    a["y"] = "q";
    EXPECT_FALSE(Ok(s.Cell(a)));
}
