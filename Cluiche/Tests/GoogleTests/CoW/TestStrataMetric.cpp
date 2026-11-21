// Generic strata task 5: the data-selected banding metric (coast_cost, elevation, latitude, named cell field), stratum
// attributes backed by DiaAttribute (schema in strata.json, values owned by the sidecar), and the live tweak of sidecar
// parameters (a share, a minimum depth, an attribute) through the session without a rebuild.
// Spec: docs/specs/features/draoi/generic-strata.md (AC 4a).
#include <gtest/gtest.h>

#include <Shared/World/BiomeRegistry.h>
#include <Shared/World/StratumTable.h>
#include <Shared/World/Testing/BiomeTestRegistry.h>
#include <Shared/WorldGeneration/StrataAssigner.h>
#include <Shared/WorldGeneration/TerrainGenerator.h>
#include <Shared/WorldGeneration/WorldGenSession.h>
#include <Shared/WorldGeneration/Testing/TerrainRulesTestHelpers.h>

#include <DiaAttribute/ParameterFile.h>
#include <DiaMaths/Core/RandomStream.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;
using namespace CoW::World;
using namespace CoW::World::Testing;
using Dia::Maths::RandomStream;

namespace {

constexpr int kW = 66;
constexpr int kH = 42;

const char* kNames[3] = { "FirstShores", "WildReaches", "AncientInteriors" };

const char* kSchema =
    "{\"danger\":{\"type\":\"float\",\"min\":0,\"max\":10,\"default\":1},"
    "\"hostile\":{\"type\":\"bool\",\"default\":false},"
    "\"tier\":{\"type\":\"int\",\"min\":0,\"max\":5,\"default\":0}}";

struct Opts
{
    std::string metric = "coast_cost";   // empty = no metric key
    bool        coastRules = true;       // innermost stratum declares main_landmass_only + min_coast_depth
    std::string schema;                  // attribute_schema object text, empty = none
    std::string attrs[3];                // per stratum "attributes" object text, empty = none
    std::vector<std::string> sidecarAttrs;   // extra sidecar entries
    std::string assigner = "coast_cost_bands";
};

std::string StrataDoc(const Opts& o)
{
    std::string strata;
    for (int i = 0; i < 3; ++i)
    {
        const bool inner = i == 2;
        strata += std::string(i == 0 ? "" : ",") + "{\"name\":\"" + kNames[i] + "\",\"regions_min\":" + (inner ? "2" : "4") + ",\"regions_max\":" + (inner ? "4" : "6");
        if (inner && o.coastRules) { strata += ",\"main_landmass_only\":true,\"min_coast_depth\":3"; }
        if (!o.attrs[i].empty()) { strata += ",\"attributes\":" + o.attrs[i]; }
        strata += "}";
    }
    return "{\"format\":\"cow.strata\",\"version\":1,\"sets\":[{\"id\":\"depth\",\"ordered\":true,"
           "\"assigner\":{\"type\":\"" + o.assigner + "\"" + (o.metric.empty() ? "" : ",\"metric\":\"" + o.metric + "\"") + "}," +
           (o.schema.empty() ? "" : "\"attribute_schema\":" + o.schema + ",") + "\"strata\":[" + strata + "]}]}";
}

std::string Num(double v)
{
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%g", v);
    return buf;
}

std::string Entry(const std::string& key, const std::string& type, const std::string& lo, const std::string& hi, const std::string& def)
{
    return "{\"key\":\"" + key + "\",\"kind\":\"parameter\",\"label\":\"l\",\"description\":\"d\",\"type\":\"" + type + "\"" +
           (lo.empty() ? "" : ",\"min\":" + lo + ",\"max\":" + hi) + ",\"default\":" + def + "}";
}

// The sidecar entries of the three attributes of kSchema for the shipped strata, defaults as given.
std::vector<std::string> AttrEntries(const double danger[3], const bool hostile[3], const int tier[3])
{
    std::vector<std::string> e;
    for (int i = 0; i < 3; ++i)
    {
        const std::string base = std::string("depth.") + kNames[i] + ".attr.";
        e.push_back(Entry(base + "danger", "float", "0", "10", Num(danger[i])));
        e.push_back(Entry(base + "hostile", "bool", "", "", hostile[i] ? "true" : "false"));
        e.push_back(Entry(base + "tier", "int", "0", "5", std::to_string(tier[i])));
    }
    return e;
}

std::string Sidecar(const Opts& o, const std::vector<double>& shares = { 0.45, 0.40, 0.15 })
{
    std::string entries;
    for (int i = 0; i < 3; ++i)
    {
        entries += std::string(i == 0 ? "" : ",") + Entry(std::string("depth.") + kNames[i] + ".share", "float", "0", "1", Num(shares[static_cast<size_t>(i)]));
    }
    if (o.coastRules) { entries += "," + Entry("depth.AncientInteriors.min_coast_depth", "float", "0", "16", "3"); }
    for (const std::string& e : o.sidecarAttrs) { entries += "," + e; }
    return "{\"format\":\"dia.value_schema\",\"version\":1,\"for\":\"cow.strata\",\"entries\":[" + entries + "]}";
}

// Attribute setup used by the attribute tests: schema + per-stratum values + the matching sidecar defaults.
Opts AttrOpts()
{
    Opts o;
    o.schema = kSchema;
    o.attrs[0] = "{\"danger\":3,\"tier\":1}";
    o.attrs[1] = "{\"hostile\":true}";
    o.attrs[2] = "{\"danger\":9.5,\"hostile\":true,\"tier\":5}";
    const double danger[3] = { 3, 1, 9.5 };
    const bool hostile[3] = { false, true, true };
    const int tier[3] = { 1, 0, 5 };
    o.sidecarAttrs = AttrEntries(danger, hostile, tier);
    return o;
}

struct Rig
{
    StrataAssignerRegistry assigners;
    StratumTable           table;
    BiomeRegistry          biomes;
    StratumError           err;
    bool                   loaded = false;
    int                    fieldCalls = 0;

    explicit Rig(const Opts& o, bool withField = true, bool fieldAvailable = true)
    {
        if (withField)
        {
            // "wetness": a made-up cell field that grows with the column (west dry, east wet).
            assigners.RegisterField("wetness", [this](const StrataAssignContext& ctx, std::span<float> out)
            {
                ++fieldCalls;
                for (size_t i = 0; i < out.size(); ++i) { out[i] = static_cast<float>(static_cast<int>(i) % ctx.dims.width); }
                return true;
            });
            assigners.RegisterField("ghost", [fieldAvailable](const StrataAssignContext&, std::span<float>) { return fieldAvailable; });
        }
        loaded = table.LoadFromText(StrataDoc(o).c_str(), Sidecar(o).c_str(), err, assigners.TypeCheck(), assigners.MetricCheck());
        if (!loaded) { return; }
        RegistryError rerr;
        if (!biomes.LoadFromText(Detail::ReadFileText(ShippedBiomesPath()).c_str(), table, rerr))
        {
            ADD_FAILURE() << "biomes load: " << rerr.field.AsCStr() << " " << rerr.reason.AsCStr();
        }
    }

    std::unique_ptr<TerrainGenerator> Generate()
    {
        auto gen = std::make_unique<TerrainGenerator>(TestRules(), &biomes);
        gen->SetAssignerRegistry(&assigners);
        RandomStream rng(1);
        gen->Update(5, WorldShapeParams{}, GridDimensions{ kW, kH }, rng);
        return gen;
    }
};

std::vector<uint32_t> Depth(const TerrainGenerator& gen)
{
    const uint32_t* z = gen.GetLayers().GetUInt(gen.GetLayers().FindLayer("stratum"));
    return std::vector<uint32_t>(z, z + kW * kH);
}

// Per stratum 1..3: the min and max of metric(cell) over the cells assigned to it. Fails if a stratum is empty or ocean is non-zero.
template <class Metric>
void ExpectMonotone(const TerrainGenerator& gen, Metric metric)
{
    const std::vector<uint32_t> z = Depth(gen);
    const uint32_t* land = gen.GetLayers().GetUInt(gen.GetLayers().FindLayer("landmass"));
    float lo[4] = { 1e30f, 1e30f, 1e30f, 1e30f };
    float hi[4] = { -1e30f, -1e30f, -1e30f, -1e30f };
    int count[4] = {};
    for (int i = 0; i < kW * kH; ++i)
    {
        if (land[i] == 0u) { ASSERT_EQ(z[static_cast<size_t>(i)], 0u) << "ocean cell " << i; continue; }
        const uint32_t k = z[static_cast<size_t>(i)];
        ASSERT_GE(k, 1u);
        ASSERT_LE(k, 3u);
        const float m = metric(i);
        lo[k] = std::min(lo[k], m);
        hi[k] = std::max(hi[k], m);
        ++count[k];
    }
    for (int k = 1; k <= 3; ++k) { EXPECT_GT(count[k], 0) << "stratum " << k << " is empty"; }
    EXPECT_LE(hi[1], lo[2]);
    EXPECT_LE(hi[2], lo[3]);
}

Opts Plain(const std::string& metric)
{
    Opts o;
    o.metric = metric;
    o.coastRules = false;
    return o;
}

} // namespace

// ---------------------------------------------------------------------------
// Metric: selection and banding (AC 4a)
// ---------------------------------------------------------------------------
TEST(StrataMetric, DefaultAndExplicitCoastCostAreIdentical)
{
    Opts withMetric;
    Opts withoutMetric;
    withoutMetric.metric.clear();
    Rig a(withMetric);
    Rig b(withoutMetric);
    ASSERT_TRUE(a.loaded) << a.err.reason;
    ASSERT_TRUE(b.loaded) << b.err.reason;
    EXPECT_STREQ(b.table.Depth()->metric, "coast_cost");
    EXPECT_EQ(Depth(*a.Generate()), Depth(*b.Generate()));
}

TEST(StrataMetric, ElevationBandsByHeight)
{
    Rig r(Plain("elevation"));
    ASSERT_TRUE(r.loaded) << r.err.reason;
    const auto gen = r.Generate();
    EXPECT_TRUE(gen->GetStrataError().empty()) << gen->GetStrataError();
    const float* elev = gen->GetLayers().GetFloat(gen->GetLayers().FindLayer("elevationRaw"));
    ExpectMonotone(*gen, [&](int i) { return elev[i]; });
    // A different metric gives a different map than the coast cost.
    Rig coast(Opts{});
    EXPECT_NE(Depth(*gen), Depth(*coast.Generate()));
}

TEST(StrataMetric, LatitudeBandsByDistanceFromTheEquatorRow)
{
    Rig r(Plain("latitude"));
    ASSERT_TRUE(r.loaded) << r.err.reason;
    const auto gen = r.Generate();
    EXPECT_TRUE(gen->GetStrataError().empty()) << gen->GetStrataError();
    ExpectMonotone(*gen, [&](int i) { return std::fabs(2.0f * (static_cast<float>(i / kW) + 0.5f) / static_cast<float>(kH) - 1.0f); });
}

TEST(StrataMetric, NamedCellFieldBandsByThatField)
{
    Rig r(Plain("wetness"));
    ASSERT_TRUE(r.loaded) << r.err.reason;
    const auto gen = r.Generate();
    EXPECT_TRUE(gen->GetStrataError().empty()) << gen->GetStrataError();
    EXPECT_GT(r.fieldCalls, 0);
    ExpectMonotone(*gen, [&](int i) { return static_cast<float>(i % kW); });
}

TEST(StrataMetric, ElevationBandingHonoursTheShares)
{
    Rig r(Plain("elevation"));
    ASSERT_TRUE(r.loaded) << r.err.reason;
    const auto gen = r.Generate();
    const std::vector<uint32_t> z = Depth(*gen);
    const uint32_t* land = gen->GetLayers().GetUInt(gen->GetLayers().FindLayer("landmass"));
    const int main = gen->GetEffective().mainLandmass;
    int mainCells = 0, first = 0;
    for (int i = 0; i < kW * kH; ++i)
    {
        if (land[i] == static_cast<uint32_t>(main)) { ++mainCells; first += z[static_cast<size_t>(i)] == 1u; }
    }
    ASSERT_GT(mainCells, 0);
    EXPECT_NEAR(static_cast<double>(first) / mainCells, 0.45, 0.06);   // ties and the next-distinct fix-up move a cut a little
}

TEST(StrataMetric, UnknownMetricFailsTheLoadNamingIt)
{
    Rig r(Plain("moonphase"));
    EXPECT_FALSE(r.loaded);
    EXPECT_NE(r.err.reason.find("moonphase"), std::string::npos) << r.err.reason;
    EXPECT_NE(r.err.field.find("metric"), std::string::npos) << r.err.field;
}

TEST(StrataMetric, NamedFieldWithoutARegistryFailsTheDefaultLoad)
{
    StratumTable t;
    StratumError err;
    const Opts o = Plain("wetness");
    EXPECT_FALSE(t.LoadFromText(StrataDoc(o).c_str(), Sidecar(o).c_str(), err));   // default check knows only the built-in metrics
    EXPECT_NE(err.reason.find("wetness"), std::string::npos) << err.reason;
    // The built-ins load without a registry.
    EXPECT_TRUE(t.LoadFromText(StrataDoc(Plain("elevation")).c_str(), Sidecar(Plain("elevation")).c_str(), err)) << err.reason;
}

TEST(StrataMetric, FieldThatIsNotAvailableAtGenerationFailsLoudAndFallsBackToCoastCost)
{
    Rig ghost(Plain("ghost"), true, /*fieldAvailable=*/false);
    ASSERT_TRUE(ghost.loaded) << ghost.err.reason;
    const auto gen = ghost.Generate();
    EXPECT_NE(gen->GetStrataError().find("ghost"), std::string::npos) << gen->GetStrataError();
    // The world still gets a valid banding: the coast_cost one (no rules on the plain set).
    Rig coast(Plain("coast_cost"));
    ASSERT_TRUE(coast.loaded);
    EXPECT_EQ(Depth(*gen), Depth(*coast.Generate()));
}

TEST(StrataMetric, RegistryRefusesBuiltInAndDuplicateFieldNames)
{
    StrataAssignerRegistry reg;
    const StrataFieldProvider ok = [](const StrataAssignContext&, std::span<float>) { return true; };
    EXPECT_FALSE(reg.RegisterField("elevation", ok));
    EXPECT_FALSE(reg.RegisterField("coast_cost", ok));
    EXPECT_FALSE(reg.RegisterField("", ok));
    EXPECT_FALSE(reg.RegisterField("f", nullptr));
    EXPECT_TRUE(reg.RegisterField("f", ok));
    EXPECT_FALSE(reg.RegisterField("f", ok));
    EXPECT_TRUE(reg.HasMetric("f"));
    EXPECT_TRUE(reg.HasMetric("latitude"));
    EXPECT_FALSE(reg.HasMetric("g"));
}

TEST(StrataMetric, CoastRulesOnANonCoastMetricAreRejectedNamingTheStratum)
{
    Opts depthRule = Plain("elevation");
    depthRule.coastRules = true;   // declares main_landmass_only + min_coast_depth on AncientInteriors
    Rig r(depthRule);
    EXPECT_FALSE(r.loaded);
    EXPECT_NE(r.err.field.find("AncientInteriors"), std::string::npos) << r.err.field;
    EXPECT_NE(r.err.reason.find("coast_cost"), std::string::npos) << r.err.reason;

    // min_coast_depth alone.
    std::string text = StrataDoc(Plain("latitude"));
    const std::string needle = "\"regions_max\":4}";
    ASSERT_NE(text.find(needle), std::string::npos);
    text.replace(text.find(needle), needle.size(), "\"regions_max\":4,\"min_coast_depth\":2}");
    StratumTable t;
    StratumError err;
    EXPECT_FALSE(t.LoadFromText(text.c_str(), Sidecar(Plain("latitude")).c_str(), err));
    EXPECT_NE(err.reason.find("min_coast_depth"), std::string::npos) << err.reason;
    EXPECT_NE(err.reason.find("AncientInteriors"), std::string::npos) << err.reason;
}

TEST(StrataMetric, ShippedStrataFileKeepsCoastCostExplicit)
{
    const StratumSetSpec* depth = ShippedStratumTable().Depth();
    ASSERT_NE(depth, nullptr);
    EXPECT_STREQ(depth->metric, "coast_cost");
    EXPECT_NE(Detail::ReadFileText(ShippedStrataPath()).find("\"metric\": \"coast_cost\""), std::string::npos);
}

// ---------------------------------------------------------------------------
// Attributes
// ---------------------------------------------------------------------------
TEST(StrataAttributes, DeclaredAttributesReadBackThroughTheTable)
{
    Rig r(AttrOpts());
    ASSERT_TRUE(r.loaded) << r.err.reason;
    const StratumTable& t = r.table;
    const uint32_t set = static_cast<uint32_t>(t.SetIndexOf("depth"));
    EXPECT_EQ(t.Depth()->attributeCount, 3u);
    EXPECT_FLOAT_EQ(t.Attribute(set, 1, "danger"), 3.0f);
    EXPECT_FLOAT_EQ(t.Attribute(set, 2, "danger"), 1.0f);   // not overridden: the schema default
    EXPECT_FLOAT_EQ(t.Attribute(set, 3, "danger"), 9.5f);
    EXPECT_FLOAT_EQ(t.Attribute(set, 1, "hostile"), 0.0f);
    EXPECT_FLOAT_EQ(t.Attribute(set, 2, "hostile"), 1.0f);
    EXPECT_FLOAT_EQ(t.Attribute(set, 1, "tier"), 1.0f);
    EXPECT_FLOAT_EQ(t.Attribute(set, 3, "tier"), 5.0f);
    float v = -1.0f;
    EXPECT_FALSE(t.TryAttribute(set, 1, "nope", v));
    EXPECT_FALSE(t.TryAttribute(set, 0, "danger", v));    // None has no attributes
    EXPECT_FALSE(t.TryAttribute(set, 4, "danger", v));    // beyond the last stratum
    EXPECT_FALSE(t.TryAttribute(7, 1, "danger", v));      // no such set
    EXPECT_FLOAT_EQ(t.Attribute(set, 4, "danger"), 0.0f);
}

TEST(StrataAttributes, ValuesLiveInTheDiaAttributeSidecar)
{
    Rig r(AttrOpts());
    ASSERT_TRUE(r.loaded) << r.err.reason;
    const Dia::Attribute::ParameterFile* file = r.table.Sidecar();
    ASSERT_NE(file, nullptr);
    const Dia::Core::StringCRC key("depth.AncientInteriors.attr.danger");
    ASSERT_NE(file->Schema().FindAttribute(key), nullptr);
    EXPECT_FLOAT_EQ(file->Values().GetValue(key), 9.5f);
    EXPECT_EQ(file->Schema().FindAttribute(key)->kind, Dia::Attribute::AttributeKind::Float);
    EXPECT_EQ(file->Schema().FindAttribute(Dia::Core::StringCRC("depth.WildReaches.attr.hostile"))->kind, Dia::Attribute::AttributeKind::Bool);
    EXPECT_EQ(file->Schema().FindAttribute(Dia::Core::StringCRC("depth.WildReaches.attr.tier"))->kind, Dia::Attribute::AttributeKind::Int);
}

TEST(StrataAttributes, SidecarValueOverridesTheAuthoredOne)
{
    Opts o = AttrOpts();
    const double danger[3] = { 7, 1, 9.5 };   // the sidecar owns FirstShores.danger = 7 while strata.json says 3
    const bool hostile[3] = { false, true, true };
    const int tier[3] = { 1, 0, 5 };
    o.sidecarAttrs = AttrEntries(danger, hostile, tier);
    Rig r(o);
    ASSERT_TRUE(r.loaded) << r.err.reason;
    EXPECT_FLOAT_EQ(r.table.Attribute(0, 1, "danger"), 7.0f);
}

namespace {
// Loads AttrOpts() with one edit and reports the error.
bool LoadsWith(const Opts& o, StratumError& err)
{
    StratumTable t;
    return t.LoadFromText(StrataDoc(o).c_str(), Sidecar(o).c_str(), err);
}
}

TEST(StrataAttributes, UnknownAttributeOnAStratumFailsNamingStratumAndAttribute)
{
    Opts o = AttrOpts();
    o.attrs[1] = "{\"luck\":2}";
    StratumError err;
    EXPECT_FALSE(LoadsWith(o, err));
    EXPECT_NE(err.field.find("WildReaches"), std::string::npos) << err.field;
    EXPECT_NE(err.field.find("luck"), std::string::npos) << err.field;
    EXPECT_NE(err.reason.find("luck"), std::string::npos) << err.reason;
}

TEST(StrataAttributes, WrongTypedValuesFail)
{
    const char* bad[] = { "{\"danger\":\"high\"}", "{\"danger\":true}", "{\"hostile\":1}", "{\"hostile\":\"yes\"}", "{\"tier\":1.5}", "{\"tier\":true}" };
    for (const char* b : bad)
    {
        Opts o = AttrOpts();
        o.attrs[0] = b;
        StratumError err;
        EXPECT_FALSE(LoadsWith(o, err)) << b;
        EXPECT_NE(err.field.find("FirstShores"), std::string::npos) << b << " -> " << err.field;
    }
}

TEST(StrataAttributes, OutOfRangeValuesFail)
{
    const char* bad[] = { "{\"danger\":10.5}", "{\"danger\":-1}", "{\"tier\":6}", "{\"tier\":-1}" };
    for (const char* b : bad)
    {
        Opts o = AttrOpts();
        o.attrs[2] = b;
        StratumError err;
        EXPECT_FALSE(LoadsWith(o, err)) << b;
        EXPECT_NE(err.field.find("AncientInteriors"), std::string::npos) << b << " -> " << err.field;
    }
}

TEST(StrataAttributes, SchemaFaultsFailNamingTheAttribute)
{
    {
        Opts o = AttrOpts();
        o.schema = "{\"label\":{\"type\":\"string\"}}";
        StratumError err;
        EXPECT_FALSE(LoadsWith(o, err));
        EXPECT_NE(err.reason.find("label"), std::string::npos) << err.reason;
        EXPECT_NE(err.reason.find("string"), std::string::npos) << err.reason;
    }
    {
        Opts o = AttrOpts();
        o.schema = "{\"danger\":{\"type\":\"float\",\"min\":5,\"max\":1}}";
        StratumError err;
        EXPECT_FALSE(LoadsWith(o, err));
        EXPECT_NE(err.reason.find("danger"), std::string::npos) << err.reason;
    }
    {
        Opts o = AttrOpts();
        o.schema = "{\"danger\":{\"type\":\"float\",\"min\":0,\"max\":10,\"default\":11}}";
        StratumError err;
        EXPECT_FALSE(LoadsWith(o, err));
        EXPECT_NE(err.reason.find("danger"), std::string::npos) << err.reason;
    }
}

TEST(StrataAttributes, SidecarFaultsFail)
{
    {   // a missing key
        Opts o = AttrOpts();
        o.sidecarAttrs.pop_back();
        StratumError err;
        EXPECT_FALSE(LoadsWith(o, err));
    }
    {   // a key of the wrong kind (tier as float)
        Opts o = AttrOpts();
        o.sidecarAttrs.back() = Entry("depth.AncientInteriors.attr.tier", "float", "0", "5", "5");
        StratumError err;
        EXPECT_FALSE(LoadsWith(o, err));
        EXPECT_NE(err.reason.find("depth.AncientInteriors.attr.tier"), std::string::npos) << err.reason;
    }
    {   // a sidecar default outside the schema range
        Opts o = AttrOpts();
        o.sidecarAttrs[0] = Entry("depth.FirstShores.attr.danger", "float", "0", "20", "15");
        StratumError err;
        EXPECT_FALSE(LoadsWith(o, err));
        EXPECT_NE(err.reason.find("depth.FirstShores.attr.danger"), std::string::npos) << err.reason;
    }
}

TEST(StrataAttributes, ATableWithoutAttributesStillLoads)
{
    Rig r(Opts{});
    ASSERT_TRUE(r.loaded) << r.err.reason;
    EXPECT_EQ(r.table.Depth()->attributeCount, 0u);
    EXPECT_FLOAT_EQ(r.table.Attribute(0, 1, "danger"), 0.0f);
}

// ---------------------------------------------------------------------------
// Live tweak (AC 4a)
// ---------------------------------------------------------------------------
TEST(StrataLiveTweak, SetParamsChangesSharesAttributesAndDepth)
{
    Rig r(AttrOpts());
    ASSERT_TRUE(r.loaded) << r.err.reason;
    StratumError err;
    bool changed = false;
    ASSERT_TRUE(r.table.SetParams({ { "depth.FirstShores.share", 0.30 }, { "depth.WildReaches.share", 0.55 },
                                    { "depth.FirstShores.attr.danger", 6.0 }, { "depth.WildReaches.attr.hostile", 0.0 },
                                    { "depth.AncientInteriors.min_coast_depth", 5.0 } }, err, &changed)) << err.field << ": " << err.reason;
    EXPECT_TRUE(changed);
    EXPECT_FLOAT_EQ(r.table.Depth()->strata[0].share, 0.30f);
    EXPECT_FLOAT_EQ(r.table.Depth()->strata[1].share, 0.55f);
    EXPECT_EQ(r.table.Depth()->strata[2].minCoastDepth, 5);
    EXPECT_FLOAT_EQ(r.table.Attribute(0, 1, "danger"), 6.0f);
    EXPECT_FLOAT_EQ(r.table.Attribute(0, 2, "hostile"), 0.0f);
    EXPECT_FLOAT_EQ(r.table.Attribute(0, 3, "danger"), 9.5f);   // untouched

    // A later call keeps the earlier changes.
    ASSERT_TRUE(r.table.SetParams({ { "depth.FirstShores.attr.danger", 2.0 } }, err, &changed)) << err.reason;
    EXPECT_FLOAT_EQ(r.table.Depth()->strata[0].share, 0.30f);
    EXPECT_FLOAT_EQ(r.table.Attribute(0, 1, "danger"), 2.0f);

    // The same values again: nothing changes.
    ASSERT_TRUE(r.table.SetParams({ { "depth.FirstShores.attr.danger", 2.0 }, { "depth.FirstShores.share", 0.30 } }, err, &changed));
    EXPECT_FALSE(changed);
}

TEST(StrataLiveTweak, BadChangesLeaveTheTableUnchangedNamingTheKey)
{
    Rig r(AttrOpts());
    ASSERT_TRUE(r.loaded) << r.err.reason;
    const float share = r.table.Depth()->strata[0].share;
    StratumError err;
    bool changed = true;
    // Shares stop completing 1.
    EXPECT_FALSE(r.table.SetParams({ { "depth.FirstShores.share", 0.6 } }, err, &changed));
    EXPECT_FALSE(changed);
    EXPECT_NE(err.reason.find("complete 1"), std::string::npos) << err.reason;
    // Unknown key.
    EXPECT_FALSE(r.table.SetParams({ { "depth.Nowhere.share", 0.1 } }, err));
    EXPECT_NE(err.field.find("depth.Nowhere.share"), std::string::npos) << err.field;
    // Out of the sidecar range.
    EXPECT_FALSE(r.table.SetParams({ { "depth.FirstShores.attr.danger", 11.0 } }, err));
    EXPECT_NE(err.field.find("depth.FirstShores.attr.danger"), std::string::npos) << err.field;
    // Not finite.
    EXPECT_FALSE(r.table.SetParams({ { "depth.FirstShores.share", std::nan("") } }, err));
    // A good change after the bad ones still works from the original state.
    EXPECT_FLOAT_EQ(r.table.Depth()->strata[0].share, share);
    EXPECT_TRUE(r.table.SetParams({ { "depth.FirstShores.share", 0.40 }, { "depth.WildReaches.share", 0.45 } }, err)) << err.reason;
}

TEST(StrataLiveTweak, AnEmptyTableRefusesSetParams)
{
    StratumTable t;
    StratumError err;
    EXPECT_FALSE(t.SetParams({ { "depth.A.share", 0.5 } }, err));
}

TEST(StrataLiveTweak, ChangingAShareReBandsTheWorldAndAnUnchangedValueDoesNot)
{
    Rig r(Opts{});
    ASSERT_TRUE(r.loaded) << r.err.reason;
    TerrainGenerator gen(TestRules(), &r.biomes);
    gen.SetAssignerRegistry(&r.assigners);
    RandomStream rng(1);
    gen.Update(5, WorldShapeParams{}, GridDimensions{ kW, kH }, rng);
    const std::vector<uint32_t> before = Depth(gen);

    // Nothing changed: nothing re-runs, the world is identical.
    StratumError err;
    bool changed = true;
    ASSERT_TRUE(r.biomes.SetStrataParams({ { "depth.FirstShores.share", 0.45 } }, err, &changed));
    EXPECT_FALSE(changed);
    gen.Update(5, WorldShapeParams{}, GridDimensions{ kW, kH }, rng);
    EXPECT_EQ(Depth(gen), before);

    // A real change, then MarkStrataDirty: the depth layer changes without a rebuild.
    ASSERT_TRUE(r.biomes.SetStrataParams({ { "depth.FirstShores.share", 0.20 }, { "depth.WildReaches.share", 0.65 } }, err, &changed)) << err.reason;
    EXPECT_TRUE(changed);
    gen.MarkStrataDirty();
    gen.Update(5, WorldShapeParams{}, GridDimensions{ kW, kH }, rng);
    const std::vector<uint32_t> after = Depth(gen);
    EXPECT_NE(after, before);
    const auto firsts = [](const std::vector<uint32_t>& z) { return std::count(z.begin(), z.end(), 1u); };
    EXPECT_LT(firsts(after), firsts(before));

    // Equals a fresh generation under the same values.
    Rig fresh(Opts{});
    ASSERT_TRUE(fresh.biomes.SetStrataParams({ { "depth.FirstShores.share", 0.20 }, { "depth.WildReaches.share", 0.65 } }, err));
    EXPECT_EQ(Depth(*fresh.Generate()), after);
}

TEST(StrataLiveTweak, SessionSetStrataObjectReBandsOnTheNextTick)
{
    Rig r(Opts{});
    ASSERT_TRUE(r.loaded) << r.err.reason;
    WorldGenSession session(TestRules(), nullptr);
    session.SetBiomeRegistry(&r.biomes);   // write access: Set's "strata" object may tweak the table
    session.SetStrataAssignerRegistry(&r.assigners);
    RandomStream rng(1);
    ASSERT_TRUE(session.Tick(rng));
    const auto strataCells = [&](uint8_t z)
    {
        const WorldStructure& w = session.World();
        int n = 0;
        for (int i = 0; i < w.CellCount(); ++i) { n += w.Cells()[i].stratum == z; }
        return n;
    };
    const int firstBefore = strataCells(1);
    ASSERT_GT(firstBefore, 0);

    Json::Value args(Json::objectValue);
    args["strata"]["depth.FirstShores.share"] = 0.45;   // unchanged
    Json::Value reply = session.Set(args);
    EXPECT_TRUE(reply["success"].asBool());
    EXPECT_EQ(reply["applied"].size(), 1u);
    EXPECT_FALSE(session.Tick(rng));   // nothing to regenerate
    EXPECT_EQ(strataCells(1), firstBefore);

    args["strata"]["depth.FirstShores.share"] = 0.15;
    args["strata"]["depth.WildReaches.share"] = 0.70;
    reply = session.Set(args);
    EXPECT_TRUE(reply["success"].asBool());
    EXPECT_EQ(reply["rejected"].size(), 0u) << reply.toStyledString();
    ASSERT_TRUE(session.Tick(rng));
    EXPECT_LT(strataCells(1), firstBefore);

    // A bad change is rejected loudly and leaves the world alone.
    Json::Value bad(Json::objectValue);
    bad["strata"]["depth.FirstShores.share"] = 0.9;   // shares no longer complete 1
    reply = session.Set(bad);
    EXPECT_EQ(reply["rejected"].size(), 1u);
    EXPECT_NE(reply["strataError"].asString().find("complete 1"), std::string::npos) << reply.toStyledString();
    EXPECT_FALSE(session.Tick(rng));
}

TEST(StrataLiveTweak, SessionWithoutWriteAccessRejectsTheStrataObject)
{
    Rig r(Opts{});
    ASSERT_TRUE(r.loaded) << r.err.reason;
    const BiomeRegistry& readOnly = r.biomes;
    WorldGenSession session(TestRules(), &readOnly);
    Json::Value args(Json::objectValue);
    args["strata"]["depth.FirstShores.share"] = 0.3;
    const Json::Value reply = session.Set(args);
    EXPECT_EQ(reply["rejected"].size(), 1u);
}
