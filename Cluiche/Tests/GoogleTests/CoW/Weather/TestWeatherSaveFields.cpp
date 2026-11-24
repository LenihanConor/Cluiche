// A4 weather-save feature, Task 4: the ten saved field arrays, their sidecar ranges and the round trip through
// real JSON text. The shipped sidecar (Global/Weather/weather_layers.schema.json) is the range source.
#include <gtest/gtest.h>

#include <cfloat>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include <DiaValueSchema/ValueSchemaLoader.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaDeferredStep/Testing/LogCapture.h>
#include <DiaParallelExecutor/SerialExecutor.h>
#include <DraoiHexWorld/Testing/BiomeTestRegistry.h>
#include <DraoiHexWorldWeather/Testing/WeatherTestFixtures.h>
#include <DraoiHexWorldWeather/WeatherSimulation.h>
#include <Shared/CoWCalendar.h>
#include <Shared/Weather/WeatherSaveFields.h>

namespace
{
using CoW::Weather::kWeatherSaveFieldCount;
using Draoi::HexWorld::SavedField;
using Draoi::HexWorld::SaveError;
using Draoi::HexWorld::Weather::WeatherFields;

const char* kLayersSidecarPath = "Cluiche/Assets/CoW/Global/Weather/weather_layers.schema.json";
constexpr double kTwoPi = 6.283185307179586;

std::string ReadShippedSidecar()
{
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::absolute(std::filesystem::path(__FILE__), ec).parent_path();
    while (!dir.empty())
    {
        const std::filesystem::path candidate = dir / kLayersSidecarPath;
        if (std::filesystem::is_regular_file(candidate, ec))
        {
            std::ifstream in(candidate, std::ios::binary);
            std::stringstream ss;
            ss << in.rdbuf();
            return ss.str();
        }
        const std::filesystem::path parent = dir.parent_path();
        if (parent == dir) break;
        dir = parent;
    }
    ADD_FAILURE() << "sidecar not found: " << kLayersSidecarPath;
    return std::string();
}

bool Publish(Dia::ValueSchema::ValueSchemaRegistry& reg, const std::string& text)
{
    Dia::ValueSchema::LoadResult r = Dia::ValueSchema::ValueSchemaLoader::LoadFromText(text.c_str(), "fixture.json");
    if (!r.mOk) { return false; }
    for (unsigned i = 0; i < r.mEntries.Size(); ++i)
    {
        if (reg.Register(r.mEntries[i]) != Dia::ValueSchema::RegisterResult::Ok) { return false; }
    }
    reg.Publish();
    return true;
}

// A dia.value_schema document for `forName` with one float field per name, all with [mn, mx].
std::string SchemaText(const char* forName, const std::vector<std::string>& names, double mn, double mx)
{
    Json::Value doc(Json::objectValue);
    doc["format"] = "dia.value_schema";
    doc["version"] = 1;
    doc["for"] = forName;
    doc["entries"] = Json::Value(Json::arrayValue);
    for (const std::string& n : names)
    {
        Json::Value e(Json::objectValue);
        e["key"] = n;
        e["kind"] = "field";
        e["label"] = "Label";
        e["description"] = "Desc";
        e["type"] = "float";
        e["min"] = mn;
        e["max"] = mx;
        doc["entries"].append(e);
    }
    Json::FastWriter w;
    return w.write(doc);
}

std::vector<std::string> FieldNames()
{
    std::vector<std::string> names;
    const CoW::WorldStep::FieldDecl* d = CoW::Weather::WeatherSaveFieldDecls();
    for (uint32_t i = 0; i < kWeatherSaveFieldCount; ++i) { names.push_back(d[i].name); }
    return names;
}

uint32_t Bits(float f) { uint32_t u; std::memcpy(&u, &f, sizeof(u)); return u; }

// Largest float whose double value is <= v; smallest float whose double value is >= v.
float FloatAtOrBelow(double v)
{
    float f = static_cast<float>(v);
    if (static_cast<double>(f) > v) { f = std::nextafterf(f, -FLT_MAX); }
    return f;
}
float FloatAtOrAbove(double v)
{
    float f = static_cast<float>(v);
    if (static_cast<double>(f) < v) { f = std::nextafterf(f, FLT_MAX); }
    return f;
}

// A ready record header for `n` cells (n x 1).
Json::Value Header(uint32_t n)
{
    Json::Value root(Json::objectValue);
    Draoi::HexWorld::SaveHeader h;
    h.width = n; h.height = 1; h.cellCount = n; h.stepDay = 7; h.paramsHash = 11; h.basinCount = 0; h.worldHash = 0;
    Draoi::HexWorld::WriteHeader(root, h);
    return root;
}

// Record -> real JSON text -> parsed Json::Value (the exact path a save file takes).
Json::Value ThroughText(const Json::Value& root)
{
    const std::string text = Draoi::HexWorld::Serialize(root);
    Json::Value parsed;
    Json::Reader reader;
    EXPECT_TRUE(reader.parse(text, parsed));
    return parsed;
}

bool Validate(const Json::Value& root, const SavedField (&fields)[kWeatherSaveFieldCount], SaveError& err)
{
    Draoi::HexWorld::Caps caps;
    return Draoi::HexWorld::Validate(root, fields, kWeatherSaveFieldCount, caps, err);
}

// A record where every array is zeros except array `which`, set to `values`.
Json::Value RecordWith(uint32_t which, const std::vector<float>& values)
{
    const uint32_t n = static_cast<uint32_t>(values.size());
    WeatherFields f;
    f.Resize(n);
    float* ptrs[kWeatherSaveFieldCount];
    CoW::Weather::WeatherFieldPointers(f, ptrs);
    for (uint32_t c = 0; c < n; ++c) { ptrs[which][c] = values[c]; }
    Json::Value root = Header(n);
    CoW::Weather::WriteWeatherFieldArrays(root, f);
    return root;
}

uint32_t IndexOf(const SavedField (&fields)[kWeatherSaveFieldCount], const char* name)
{
    for (uint32_t i = 0; i < kWeatherSaveFieldCount; ++i) { if (std::string(fields[i].name) == name) { return i; } }
    return kWeatherSaveFieldCount;
}

class WeatherSaveFieldsTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(Publish(layers, ReadShippedSidecar()));
        SaveError err;
        ASSERT_TRUE(CoW::Weather::ResolveWeatherSaveFields(layers, real, err)) << err.field.AsCStr();
    }

    // Round trip `values` through array `which` of a real record: write, text, parse, Validate, read; each element
    // must come back bit-identical. `fields` supplies the ranges Validate enforces.
    void ExpectRoundTrip(const SavedField (&fields)[kWeatherSaveFieldCount], uint32_t which,
                         const std::vector<float>& values, const char* what)
    {
        SCOPED_TRACE(std::string(fields[which].name) + " / " + what);
        const Json::Value parsed = ThroughText(RecordWith(which, values));
        SaveError err;
        ASSERT_TRUE(Validate(parsed, fields, err)) << err.field.AsCStr() << ": " << err.reason.AsCStr();
        WeatherFields back;
        ASSERT_TRUE(CoW::Weather::ReadWeatherFieldArrays(parsed, static_cast<uint32_t>(values.size()), back));
        const float* ptrs[kWeatherSaveFieldCount];
        CoW::Weather::WeatherFieldPointers(back, ptrs);
        for (size_t c = 0; c < values.size(); ++c)
        {
            // -0.0 is normalised to +0.0 on write; every other value is bit-identical.
            const float expected = (values[c] == 0.0f) ? 0.0f : values[c];
            EXPECT_EQ(Bits(expected), Bits(ptrs[which][c])) << "cell " << c << " value " << values[c];
        }
    }

    Dia::ValueSchema::ValueSchemaRegistry layers;
    SavedField real[kWeatherSaveFieldCount];
};
} // namespace

TEST_F(WeatherSaveFieldsTest, TableHoldsTheTenSnakeCaseFieldsInSavedOrder)
{
    static const char* kExpected[kWeatherSaveFieldCount] = { "moisture", "temperature", "pressure", "wind_direction",
        "wind_speed", "precipitation", "cloud_cover", "water_to_ground", "snow_pack", "pressure_anomaly" };
    const CoW::WorldStep::FieldDecl* d = CoW::Weather::WeatherSaveFieldDecls();
    for (uint32_t i = 0; i < kWeatherSaveFieldCount; ++i)
    {
        EXPECT_STREQ(kExpected[i], d[i].name);
        EXPECT_EQ(Draoi::HexWorld::SavedKind::F32, d[i].kind);
        EXPECT_EQ(Draoi::HexWorld::SavedScope::PerCell, d[i].scope);
        EXPECT_STREQ(kExpected[i], real[i].name);
    }
}

TEST_F(WeatherSaveFieldsTest, PointerTableFollowsTheDeclOrder)
{
    WeatherFields f;
    f.Resize(2);
    float* ptrs[kWeatherSaveFieldCount];
    CoW::Weather::WeatherFieldPointers(f, ptrs);
    for (uint32_t i = 0; i < kWeatherSaveFieldCount; ++i) { ptrs[i][0] = static_cast<float>(i + 1); ptrs[i][1] = -static_cast<float>(i + 1); }
    EXPECT_EQ(1.0f, f.moisture[0]);
    EXPECT_EQ(2.0f, f.temperature[0]);
    EXPECT_EQ(3.0f, f.pressure[0]);
    EXPECT_EQ(4.0f, f.windDirection[0]);
    EXPECT_EQ(5.0f, f.windSpeed[0]);
    EXPECT_EQ(6.0f, f.precipitation[0]);
    EXPECT_EQ(7.0f, f.cloudCover[0]);
    EXPECT_EQ(8.0f, f.waterToGround[0]);
    EXPECT_EQ(9.0f, f.snowPack[0]);
    EXPECT_EQ(10.0f, f.pressureAnomaly[0]);

    // An empty set has no array to point at.
    WeatherFields empty;
    const float* cptrs[kWeatherSaveFieldCount];
    CoW::Weather::WeatherFieldPointers(empty, cptrs);
    for (uint32_t i = 0; i < kWeatherSaveFieldCount; ++i) { EXPECT_EQ(nullptr, cptrs[i]); }
}

TEST_F(WeatherSaveFieldsTest, ShippedSidecarRangesMatchTheSimClamps)
{
    // From WeatherPipeline Finish: [0,1] except water_to_ground [0,2], pressure_anomaly [-0.5,0.5] and
    // wind_direction [0, 2pi] (inclusive bound; the sim wraps into [0, 2pi)).
    for (uint32_t i = 0; i < kWeatherSaveFieldCount; ++i)
    {
        const std::string n = real[i].name;
        double mn = 0.0, mx = 1.0;
        if (n == "water_to_ground") { mx = 2.0; }
        else if (n == "pressure_anomaly") { mn = -0.5; mx = 0.5; }
        else if (n == "wind_direction") { mx = kTwoPi; }
        EXPECT_EQ(mn, real[i].min) << n;
        EXPECT_EQ(mx, real[i].max) << n;
    }
}

TEST_F(WeatherSaveFieldsTest, ResolvesThroughTheServiceToo)
{
    Dia::ValueSchema::ValueSchemaService svc(layers);
    SavedField viaService[kWeatherSaveFieldCount];
    SaveError err;
    ASSERT_TRUE(CoW::Weather::ResolveWeatherSaveFields(svc, viaService, err));
    for (uint32_t i = 0; i < kWeatherSaveFieldCount; ++i)
    {
        EXPECT_EQ(real[i].min, viaService[i].min);
        EXPECT_EQ(real[i].max, viaService[i].max);
    }
}

// AC 1 on the shipped ranges: random finite floats, zero, denormals and each bound.
TEST_F(WeatherSaveFieldsTest, EveryArrayRoundTripsBitIdenticalThroughRealJsonText)
{
    std::mt19937 rng(0xA4C0FFEEu);
    for (uint32_t i = 0; i < kWeatherSaveFieldCount; ++i)
    {
        const double mn = real[i].min, mx = real[i].max;
        std::vector<float> values;
        std::uniform_real_distribution<double> dist(mn, mx);
        for (int k = 0; k < 200; ++k)
        {
            float v = static_cast<float>(dist(rng));
            if (static_cast<double>(v) > mx) { v = FloatAtOrBelow(mx); }
            if (static_cast<double>(v) < mn) { v = FloatAtOrAbove(mn); }
            values.push_back(v);
        }
        values.push_back(0.0f);
        values.push_back(FloatAtOrAbove(mn));                       // lower bound
        values.push_back(FloatAtOrBelow(mx));                       // upper bound
        values.push_back(1.0e-45f);                                 // smallest positive denormal
        values.push_back(1.17549421e-38f);                          // largest denormal
        values.push_back(1.17549435e-38f);                          // FLT_MIN
        if (mn < 0.0) { values.push_back(-1.0e-45f); values.push_back(-1.17549421e-38f); }
        ExpectRoundTrip(real, i, values, "random, zero, denormals, bounds");
    }
}

TEST_F(WeatherSaveFieldsTest, NegativeZeroIsWrittenAsPositiveZero)
{
    for (uint32_t i = 0; i < kWeatherSaveFieldCount; ++i)
    {
        const std::vector<float> values = { -0.0f, 0.25f * static_cast<float>(real[i].max), -0.0f };
        const Json::Value record = RecordWith(i, values);
        const std::string text = Draoi::HexWorld::Serialize(record);
        EXPECT_EQ(std::string::npos, text.find("-0")) << real[i].name;
        ExpectRoundTrip(real, i, values, "-0.0");
        const Json::Value parsed = ThroughText(record);
        WeatherFields back;
        ASSERT_TRUE(CoW::Weather::ReadWeatherFieldArrays(parsed, 3, back));
        const float* ptrs[kWeatherSaveFieldCount];
        CoW::Weather::WeatherFieldPointers(back, ptrs);
        EXPECT_EQ(0u, Bits(ptrs[i][0])) << real[i].name << " reads back as +0.0";
    }
}

// float::max()-scale values: no shipped range allows them, so a wide sidecar generated from the same decl table
// (a field added later is covered) carries the whole finite float range.
TEST_F(WeatherSaveFieldsTest, FullFloatRangeRoundTripsThroughAWideSidecar)
{
    Dia::ValueSchema::ValueSchemaRegistry wide;
    ASSERT_TRUE(Publish(wide, SchemaText("cow.weather.layers", FieldNames(), -static_cast<double>(FLT_MAX), static_cast<double>(FLT_MAX))));
    SavedField fields[kWeatherSaveFieldCount];
    SaveError err;
    ASSERT_TRUE(CoW::Weather::ResolveWeatherSaveFields(wide, fields, err));

    std::mt19937 rng(7u);
    std::uniform_int_distribution<uint32_t> bitsDist(0u, 0xFFFFFFFFu);
    for (uint32_t i = 0; i < kWeatherSaveFieldCount; ++i)
    {
        std::vector<float> values = { FLT_MAX, -FLT_MAX, FLT_MIN, -FLT_MIN, 1.0e-45f, -1.0e-45f, 0.0f, 1.0f / 3.0f, 16777217.0f };
        while (values.size() < 300)
        {
            const uint32_t u = bitsDist(rng);
            float v;
            std::memcpy(&v, &u, sizeof(v));
            if (std::isfinite(v)) { values.push_back(v); }
        }
        ExpectRoundTrip(fields, i, values, "float::max scale and random bit patterns");
    }
}

// AC 21: pressure_anomaly sits exactly on both bounds.
TEST_F(WeatherSaveFieldsTest, PressureAnomalyAcceptsPlusAndMinusHalfAndRejectsBeyond)
{
    const uint32_t idx = IndexOf(real, "pressure_anomaly");
    ASSERT_NE(kWeatherSaveFieldCount, idx);
    ExpectRoundTrip(real, idx, { 0.5f, -0.5f, 0.0f, 0.25f, -0.25f }, "at the bounds");

    SaveError err;
    EXPECT_FALSE(Validate(ThroughText(RecordWith(idx, { std::nextafterf(0.5f, 1.0f) })), real, err));
    EXPECT_STREQ("pressure_anomaly", err.field.AsCStr());
    EXPECT_FALSE(Validate(ThroughText(RecordWith(idx, { std::nextafterf(-0.5f, -1.0f) })), real, err));
    EXPECT_STREQ("pressure_anomaly", err.field.AsCStr());
}

// The sidecar upper bound of wind_direction is inclusive 2pi. The sim emits values strictly below float(2pi), so
// the largest float at or below the bound is accepted, and float(2pi) itself (a hair above the double 2pi) is not.
TEST_F(WeatherSaveFieldsTest, WindDirectionUpperEdgeIsTheInclusiveTwoPiBound)
{
    const uint32_t idx = IndexOf(real, "wind_direction");
    ASSERT_NE(kWeatherSaveFieldCount, idx);
    EXPECT_EQ(kTwoPi, real[idx].max);

    const float top = FloatAtOrBelow(kTwoPi);
    const float wrapMax = std::nextafterf(static_cast<float>(kTwoPi), 0.0f);   // largest value the sim's Wrap2Pi returns
    EXPECT_LE(static_cast<double>(wrapMax), kTwoPi);
    ExpectRoundTrip(real, idx, { top, wrapMax, 0.0f, 3.14159274f }, "upper edge");

    SaveError err;
    EXPECT_FALSE(Validate(ThroughText(RecordWith(idx, { std::nextafterf(top, FLT_MAX) })), real, err));
    EXPECT_STREQ("wind_direction", err.field.AsCStr());
    EXPECT_FALSE(Validate(ThroughText(RecordWith(idx, { -1.0e-45f })), real, err));
    EXPECT_STREQ("wind_direction", err.field.AsCStr());
}

// A field with no layer entry and no state entry is refused, naming that field.
TEST_F(WeatherSaveFieldsTest, AFieldWithNoSidecarEntryFailsNamingTheField)
{
    const std::vector<std::string> all = FieldNames();
    for (size_t skip = 0; skip < all.size(); ++skip)
    {
        SCOPED_TRACE(all[skip]);
        std::vector<std::string> names;
        for (size_t i = 0; i < all.size(); ++i) { if (i != skip) { names.push_back(all[i]); } }
        Dia::ValueSchema::ValueSchemaRegistry reg;
        ASSERT_TRUE(Publish(reg, SchemaText("cow.weather.layers", names, 0.0, 1.0)));
        SavedField f[kWeatherSaveFieldCount];
        SaveError err;
        EXPECT_FALSE(CoW::Weather::ResolveWeatherSaveFields(reg, f, err));
        EXPECT_STREQ(all[skip].c_str(), err.field.AsCStr());
    }
}

// A field missing from the layers but present in cow.weather_state resolves from the state entry.
TEST_F(WeatherSaveFieldsTest, AFieldMayTakeItsRangeFromTheStateEntry)
{
    const std::vector<std::string> all = FieldNames();
    std::vector<std::string> names(all.begin(), all.end() - 1);
    // Both documents go into one registry before it is published.
    Dia::ValueSchema::ValueSchemaRegistry both;
    {
        Dia::ValueSchema::LoadResult a = Dia::ValueSchema::ValueSchemaLoader::LoadFromText(
            SchemaText("cow.weather.layers", names, 0.0, 1.0).c_str(), "layers.json");
        Dia::ValueSchema::LoadResult b = Dia::ValueSchema::ValueSchemaLoader::LoadFromText(
            SchemaText("cow.weather_state", { all.back() }, -0.5, 0.5).c_str(), "state.json");
        ASSERT_TRUE(a.mOk);
        ASSERT_TRUE(b.mOk);
        for (unsigned i = 0; i < a.mEntries.Size(); ++i) { ASSERT_EQ(Dia::ValueSchema::RegisterResult::Ok, both.Register(a.mEntries[i])); }
        for (unsigned i = 0; i < b.mEntries.Size(); ++i) { ASSERT_EQ(Dia::ValueSchema::RegisterResult::Ok, both.Register(b.mEntries[i])); }
        both.Publish();
    }
    SavedField f[kWeatherSaveFieldCount];
    SaveError err;
    ASSERT_TRUE(CoW::Weather::ResolveWeatherSaveFields(both, f, err)) << err.field.AsCStr();
    EXPECT_EQ(-0.5, f[kWeatherSaveFieldCount - 1].min);
    EXPECT_EQ(0.5, f[kWeatherSaveFieldCount - 1].max);
}

TEST_F(WeatherSaveFieldsTest, ShippedSidecarHasExactlyTheTenEntries)
{
    Json::Value doc;
    Json::Reader reader;
    ASSERT_TRUE(reader.parse(ReadShippedSidecar(), doc));
    EXPECT_EQ("cow.weather.layers", doc["for"].asString());
    EXPECT_EQ(kWeatherSaveFieldCount, doc["entries"].size());
}

TEST_F(WeatherSaveFieldsTest, ReadRejectsAMissingOrShortArray)
{
    const Json::Value good = ThroughText(RecordWith(0, { 0.1f, 0.2f }));
    WeatherFields f;
    EXPECT_TRUE(CoW::Weather::ReadWeatherFieldArrays(good, 2, f));
    EXPECT_FALSE(CoW::Weather::ReadWeatherFieldArrays(good, 3, f));
    Json::Value missing = good;
    missing.removeMember("snow_pack");
    EXPECT_FALSE(CoW::Weather::ReadWeatherFieldArrays(missing, 2, f));
    Json::Value bad = good;
    bad["moisture"][1u] = "x";
    EXPECT_FALSE(CoW::Weather::ReadWeatherFieldArrays(bad, 2, f));
}

// ---- A4 weather-save Task 5: the weather RecordSpec feeds the Draoi kit's Validate ----
namespace
{
class WeatherSaveValidateTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(Publish(layers, ReadShippedSidecar()));
        SaveError err;
        ASSERT_TRUE(CoW::Weather::ResolveWeatherSaveFields(layers, fields, err)) << err.field.AsCStr();
        spec = CoW::Weather::WeatherSaveRecordSpec(fields);
    }

    static bool ValidateSpec(const Json::Value& root, const CoW::WorldStep::RecordSpec& s, SaveError& err)
    {
        return Draoi::HexWorld::Validate(root, s.fields, s.fieldCount, s.caps, err);
    }

    // A valid 4-cell record (every value 0.25, inside every shipped range).
    static Json::Value GoodRecord(uint32_t n = 4)
    {
        WeatherFields f;
        f.Resize(n);
        float* ptrs[kWeatherSaveFieldCount];
        CoW::Weather::WeatherFieldPointers(f, ptrs);
        for (uint32_t i = 0; i < kWeatherSaveFieldCount; ++i) { for (uint32_t c = 0; c < n; ++c) { ptrs[i][c] = 0.25f; } }
        Json::Value root = Header(n);
        CoW::Weather::WriteWeatherFieldArrays(root, f);
        return root;
    }

    Dia::ValueSchema::ValueSchemaRegistry layers;
    SavedField fields[kWeatherSaveFieldCount];
    CoW::WorldStep::RecordSpec spec;
};
} // namespace

TEST_F(WeatherSaveValidateTest, SpecCarriesTenFieldsKitCapsAndBiome)
{
    EXPECT_EQ(fields, spec.fields);
    EXPECT_EQ(kWeatherSaveFieldCount, spec.fieldCount);
    EXPECT_EQ(0u, spec.caps.maxBasins);
    EXPECT_TRUE(spec.hasBiome);
    SaveError err;
    EXPECT_TRUE(ValidateSpec(GoodRecord(), spec, err)) << err.field.AsCStr() << ": " << err.reason.AsCStr();
}

TEST_F(WeatherSaveValidateTest, EveryFieldRejectsOutOfRangeWrongLengthMissingAndNonFiniteNamingTheField)
{
    for (uint32_t i = 0; i < kWeatherSaveFieldCount; ++i)
    {
        const char* name = fields[i].name;
        SCOPED_TRACE(name);
        SaveError err;

        Json::Value high = GoodRecord();
        high[name][1u] = fields[i].max + 1.0;
        EXPECT_FALSE(ValidateSpec(high, spec, err));
        EXPECT_STREQ(name, err.field.AsCStr());

        Json::Value low = GoodRecord();
        low[name][2u] = fields[i].min - 1.0;
        EXPECT_FALSE(ValidateSpec(low, spec, err));
        EXPECT_STREQ(name, err.field.AsCStr());

        Json::Value shortArr = GoodRecord();
        Json::Value trimmed(Json::arrayValue);
        for (uint32_t c = 0; c < 3; ++c) { trimmed.append(shortArr[name][c]); }
        shortArr[name] = trimmed;
        EXPECT_FALSE(ValidateSpec(shortArr, spec, err));
        EXPECT_STREQ(name, err.field.AsCStr());

        Json::Value longArr = GoodRecord();
        longArr[name].append(0.25);
        EXPECT_FALSE(ValidateSpec(longArr, spec, err));
        EXPECT_STREQ(name, err.field.AsCStr());

        Json::Value missing = GoodRecord();
        missing.removeMember(name);
        EXPECT_FALSE(ValidateSpec(missing, spec, err));
        EXPECT_STREQ(name, err.field.AsCStr());

        const double bad[] = { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
                               -std::numeric_limits<double>::infinity() };
        for (double v : bad)
        {
            Json::Value nonFinite = GoodRecord();
            nonFinite[name][0u] = v;
            EXPECT_FALSE(ValidateSpec(nonFinite, spec, err));
            EXPECT_STREQ(name, err.field.AsCStr());
        }

        Json::Value notNumber = GoodRecord();
        notNumber[name][0u] = "x";
        EXPECT_FALSE(ValidateSpec(notNumber, spec, err));
        EXPECT_STREQ(name, err.field.AsCStr());
    }
}

// Changing only the sidecar max changes what is accepted; no code change.
TEST_F(WeatherSaveValidateTest, SidecarMaxDrivesWhatIsAccepted)
{
    const std::vector<std::string> names = FieldNames();
    for (uint32_t i = 0; i < kWeatherSaveFieldCount; ++i)
    {
        const char* name = names[i].c_str();
        SCOPED_TRACE(name);
        Dia::ValueSchema::ValueSchemaRegistry narrowReg, wideReg;
        ASSERT_TRUE(Publish(narrowReg, SchemaText("cow.weather.layers", names, 0.0, 1.0)));
        ASSERT_TRUE(Publish(wideReg, SchemaText("cow.weather.layers", names, 0.0, 3.0)));
        SavedField narrow[kWeatherSaveFieldCount], wide[kWeatherSaveFieldCount];
        SaveError err;
        ASSERT_TRUE(CoW::Weather::ResolveWeatherSaveFields(narrowReg, narrow, err));
        ASSERT_TRUE(CoW::Weather::ResolveWeatherSaveFields(wideReg, wide, err));
        const CoW::WorldStep::RecordSpec narrowSpec = CoW::Weather::WeatherSaveRecordSpec(narrow);
        const CoW::WorldStep::RecordSpec wideSpec = CoW::Weather::WeatherSaveRecordSpec(wide);

        Json::Value rec = GoodRecord();
        rec[name][0u] = 2.0;
        EXPECT_FALSE(ValidateSpec(rec, narrowSpec, err));
        EXPECT_STREQ(name, err.field.AsCStr());
        EXPECT_TRUE(ValidateSpec(rec, wideSpec, err)) << err.field.AsCStr() << ": " << err.reason.AsCStr();
    }
}

TEST_F(WeatherSaveValidateTest, WindDirectionAcceptsTheDeclaredUpperBound)
{
    Json::Value rec = GoodRecord();
    rec["wind_direction"][0u] = kTwoPi;
    SaveError err;
    EXPECT_TRUE(ValidateSpec(rec, spec, err)) << err.field.AsCStr() << ": " << err.reason.AsCStr();
    rec["wind_direction"][0u] = std::nextafter(kTwoPi, 10.0);
    EXPECT_FALSE(ValidateSpec(rec, spec, err));
    EXPECT_STREQ("wind_direction", err.field.AsCStr());
}

TEST_F(WeatherSaveValidateTest, CellCountNotWidthTimesHeightIsRejected)
{
    Json::Value rec = GoodRecord();
    rec["width"] = 3;
    rec["height"] = 2;   // 6 != cell_count 4
    SaveError err;
    EXPECT_FALSE(ValidateSpec(rec, spec, err));
    EXPECT_STREQ("cell_count", err.field.AsCStr());
}

// ---- A4 weather-save Task 7: the whole record (header, ten arrays, biome palette) ----
namespace
{
namespace WT = Draoi::HexWorld::Weather::Testing;
using Draoi::HexWorld::BiomeIndex;
using Draoi::HexWorld::BiomeRegistry;
using Draoi::HexWorld::Weather::WeatherSimulation;
typedef Dia::DeferredStep::Testing::LogCapture LogCapture;

constexpr uint32_t kW = 8, kH = 6, kN = kW * kH;

// A sim on an 8x6 world, biomes cycling through the registry, stepped `steps` weeks, committed week set to `week`.
void MakeSim(WeatherSimulation& sim, const BiomeRegistry& reg, uint32_t steps, uint64_t week)
{
    WT::HexTerrainOptions o;
    o.waterColumns = 2;
    o.latitudeRamp = true;
    Draoi::HexWorld::Weather::WeatherTerrain terrain = WT::MakeHexTerrain(kW, kH, o);
    Draoi::HexWorld::Weather::WeatherParameters params = WT::MakeTestParams();
    ASSERT_TRUE(sim.Initialise(terrain, params, reg, nullptr));
    for (uint32_t i = 0; i < kN; ++i) { ASSERT_TRUE(sim.SetCellBiome(i, static_cast<BiomeIndex>(i % reg.Count()))); }
    Draoi::HexWorld::Weather::HydrologyInputs noHydro;
    Draoi::HexWorld::Weather::InfluenceSnapshot noInfl;
    Dia::ParallelExecutor::SerialExecutor exec;
    for (uint32_t i = 0; i < steps; ++i)
    {
        sim.Step(params, static_cast<Draoi::HexWorld::Weather::Season>((i / 13u) % 4u), i % 52u, reg, nullptr, noHydro, noInfl, exec);
    }
    sim.SetCommittedWeek(week);
}

class WeatherSaveRecordTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(Publish(layers, ReadShippedSidecar()));
        SaveError err;
        ASSERT_TRUE(CoW::Weather::ResolveWeatherSaveFields(layers, fields, err)) << err.field.AsCStr();
        spec = CoW::Weather::WeatherSaveRecordSpec(fields);
    }

    // Encode -> real JSON text -> parse.
    Json::Value EncodeParsed(const WeatherSimulation& sim, const BiomeRegistry& reg, uint64_t hash = 0xABCDEF0123456789ull)
    {
        Json::Value out(Json::objectValue);
        EXPECT_TRUE(CoW::Weather::EncodeRecord(sim, hash, reg, out));
        return ThroughText(out);
    }

    bool Decode(const Json::Value& root, const BiomeRegistry& reg, CoW::Weather::WeatherRecord& rec, SaveError& err)
    {
        return CoW::Weather::DecodeRecord(root, spec, reg, rec, err);
    }

    Dia::ValueSchema::ValueSchemaRegistry layers;
    SavedField fields[kWeatherSaveFieldCount];
    CoW::WorldStep::RecordSpec spec;
};
} // namespace

TEST_F(WeatherSaveRecordTest, WholeRecordRoundTripsBitIdenticalThroughRealJsonText)
{
    const BiomeRegistry& reg = WT::TestRegistry();
    WeatherSimulation sim;
    MakeSim(sim, reg, 5, 5);
    const Json::Value parsed = EncodeParsed(sim, reg);

    CoW::Weather::WeatherRecord rec;
    SaveError err;
    ASSERT_TRUE(Decode(parsed, reg, rec, err)) << err.field.AsCStr() << ": " << err.reason.AsCStr();
    EXPECT_TRUE(rec.ready);
    EXPECT_EQ(5u, rec.week);
    EXPECT_EQ(0xABCDEF0123456789ull, rec.paramsHash);
    EXPECT_EQ(kW, rec.width);
    EXPECT_EQ(kH, rec.height);
    EXPECT_TRUE(WT::SameFields(sim.Fields(), rec.fields));
    ASSERT_EQ(kN, rec.biomes.Size());
    for (uint32_t i = 0; i < kN; ++i) { EXPECT_EQ(sim.Terrain().biome[i], rec.biomes[i]) << "cell " << i; }
}

TEST_F(WeatherSaveRecordTest, RecordPassesTheKitValidateAndCarriesNoInfluencesOrClimate)
{
    const BiomeRegistry& reg = WT::TestRegistry();
    WeatherSimulation sim;
    MakeSim(sim, reg, 3, 3);
    const Json::Value parsed = EncodeParsed(sim, reg);
    SaveError err;
    EXPECT_TRUE(Draoi::HexWorld::Validate(parsed, spec.fields, spec.fieldCount, spec.caps, err)) << err.field.AsCStr();
    EXPECT_FALSE(parsed.isMember("influences"));
    EXPECT_FALSE(parsed.isMember("climate"));
    EXPECT_EQ(0u, parsed["basin_count"].asUInt());
    EXPECT_EQ(0u, parsed["world_hash"].asUInt64());
    EXPECT_EQ(kN, parsed["cell_count"].asUInt());
}

TEST_F(WeatherSaveRecordTest, StepDayIsWeekTimesSevenAndRestoresTheWeekExactly)
{
    const BiomeRegistry& reg = WT::TestRegistry();
    const uint64_t weeks[] = { 0, 1, 52, 123456, 306783377 };   // last: 306783377 * 7 = 2147483639 <= INT32_MAX
    for (uint64_t week : weeks)
    {
        SCOPED_TRACE(week);
        WeatherSimulation sim;
        MakeSim(sim, reg, 0, week);
        const Json::Value parsed = EncodeParsed(sim, reg);
        EXPECT_EQ(static_cast<Json::UInt64>(week * CoW::kDaysPerWeek), parsed["step_day"].asUInt64());
        CoW::Weather::WeatherRecord rec;
        SaveError err;
        ASSERT_TRUE(Decode(parsed, reg, rec, err)) << err.field.AsCStr() << ": " << err.reason.AsCStr();
        EXPECT_EQ(week, rec.week);
    }
}

TEST_F(WeatherSaveRecordTest, AWeekWhoseStepDayOverflowsInt32IsRefusedAndOutIsUntouched)
{
    const BiomeRegistry& reg = WT::TestRegistry();
    WeatherSimulation sim;
    MakeSim(sim, reg, 0, 306783379ull);   // * 7 = 2147483653 > INT32_MAX
    Json::Value out(Json::objectValue);
    out["keep"] = 1;
    EXPECT_FALSE(CoW::Weather::EncodeRecord(sim, 1, reg, out));
    EXPECT_EQ(1u, out.size());
    EXPECT_EQ(1, out["keep"].asInt());
}

TEST_F(WeatherSaveRecordTest, AnUninitialisedSimWritesReadyFalseOnlyAndDecodesAsNotReady)
{
    const BiomeRegistry& reg = WT::TestRegistry();
    WeatherSimulation sim;
    Json::Value out(Json::objectValue);
    EXPECT_TRUE(CoW::Weather::EncodeRecord(sim, 7, reg, out));
    const Json::Value parsed = ThroughText(out);
    ASSERT_EQ(1u, parsed.size());
    ASSERT_TRUE(parsed.isMember("ready"));
    EXPECT_FALSE(parsed["ready"].asBool());

    CoW::Weather::WeatherRecord rec;
    SaveError err;
    ASSERT_TRUE(Decode(parsed, reg, rec, err));
    EXPECT_FALSE(rec.ready);
    EXPECT_EQ(0u, rec.fields.CellCount());
    EXPECT_EQ(0u, rec.biomes.Size());
}

TEST_F(WeatherSaveRecordTest, UnknownPaletteIdBecomesTheFallbackBiomeWithOneWarningPerId)
{
    const BiomeRegistry reg = Draoi::HexWorld::Testing::LoadShippedVariant({});   // fresh: its warned-id set starts empty
    WeatherSimulation sim;
    MakeSim(sim, reg, 2, 2);
    Json::Value parsed = EncodeParsed(sim, reg);
    // Palette entries 1 and 2 become two different ids unknown to the registry; many cells use them.
    parsed["biome_palette"][1u] = "Biome.NoSuchOne";
    parsed["biome_palette"][2u] = "Biome.NoSuchTwo";

    LogCapture log("HexWorld");
    CoW::Weather::WeatherRecord rec;
    SaveError err;
    ASSERT_TRUE(Decode(parsed, reg, rec, err)) << err.field.AsCStr() << ": " << err.reason.AsCStr();
    EXPECT_EQ(1u, log.Warnings("NoSuchOne"));
    EXPECT_EQ(1u, log.Warnings("NoSuchTwo"));
    EXPECT_EQ(2u, log.Warnings());
    ASSERT_EQ(kN, rec.biomes.Size());
    const BiomeIndex fallback = reg.ResolveOrFallback(Dia::Core::StringCRC("Biome.NoSuchOne"));
    for (uint32_t i = 0; i < kN; ++i)
    {
        const BiomeIndex original = sim.Terrain().biome[i];
        const BiomeIndex expected = (original == 1 || original == 2) ? fallback : original;
        EXPECT_EQ(expected, rec.biomes[i]) << "cell " << i;
    }
}

TEST_F(WeatherSaveRecordTest, RegistryHashMismatchWithACleanRemapIsOneInfoAndNoWarning)
{
    const BiomeRegistry reg = Draoi::HexWorld::Testing::LoadShippedVariant({});
    WeatherSimulation sim;
    MakeSim(sim, reg, 2, 2);
    Json::Value parsed = EncodeParsed(sim, reg);
    parsed["biome_palette_hash"] = Json::UInt64(reg.Hash() ^ 0x5Aull);

    LogCapture log("HexWorld");
    CoW::Weather::WeatherRecord rec;
    SaveError err;
    ASSERT_TRUE(Decode(parsed, reg, rec, err)) << err.field.AsCStr();
    EXPECT_EQ(1u, log.Infos("hash differs"));
    EXPECT_EQ(0u, log.Warnings());
    for (uint32_t i = 0; i < kN; ++i) { EXPECT_EQ(sim.Terrain().biome[i], rec.biomes[i]); }
}

TEST_F(WeatherSaveRecordTest, APaletteIndexPastThePaletteIsRejectedNamingTheBiomeField)
{
    const BiomeRegistry& reg = WT::TestRegistry();
    WeatherSimulation sim;
    MakeSim(sim, reg, 1, 1);
    Json::Value parsed = EncodeParsed(sim, reg);
    parsed["biome"][4u] = parsed["biome_palette"].size();

    CoW::Weather::WeatherRecord rec;
    SaveError err;
    EXPECT_FALSE(Decode(parsed, reg, rec, err));
    EXPECT_STREQ("biome", err.field.AsCStr());
    EXPECT_FALSE(rec.ready);
    EXPECT_EQ(0u, rec.fields.CellCount());
    EXPECT_EQ(0u, rec.biomes.Size());
}

TEST_F(WeatherSaveRecordTest, ARecordWithAnOutOfRangeFieldOrMissingBiomeFailsAndLeavesTheResultEmpty)
{
    const BiomeRegistry& reg = WT::TestRegistry();
    WeatherSimulation sim;
    MakeSim(sim, reg, 1, 1);
    const Json::Value good = EncodeParsed(sim, reg);

    Json::Value badField = good;
    badField["snow_pack"][3u] = 5.0;
    CoW::Weather::WeatherRecord rec;
    SaveError err;
    EXPECT_FALSE(Decode(badField, reg, rec, err));
    EXPECT_STREQ("snow_pack", err.field.AsCStr());
    EXPECT_EQ(0u, rec.fields.CellCount());

    Json::Value noBiome = good;
    noBiome.removeMember("biome");
    EXPECT_FALSE(Decode(noBiome, reg, rec, err));
    EXPECT_STREQ("biome", err.field.AsCStr());
    EXPECT_EQ(0u, rec.fields.CellCount());
    EXPECT_EQ(0u, rec.biomes.Size());
}

// The appended Savanna biome saves and loads with no code change; a registry without it falls back with one warning.
TEST_F(WeatherSaveRecordTest, AnAppendedBiomeFromTheExtraFixtureRegistrySavesAndLoads)
{
    const BiomeRegistry& extra = Draoi::HexWorld::Testing::ExtraBiomeRegistry();
    const BiomeIndex savanna = Draoi::HexWorld::Testing::B(extra, "Biome.Savanna");
    WeatherSimulation sim;
    MakeSim(sim, extra, 2, 2);
    ASSERT_TRUE(sim.SetCellBiome(5, savanna));
    ASSERT_TRUE(sim.SetCellBiome(9, savanna));
    const Json::Value parsed = EncodeParsed(sim, extra);

    CoW::Weather::WeatherRecord rec;
    SaveError err;
    ASSERT_TRUE(Decode(parsed, extra, rec, err)) << err.field.AsCStr() << ": " << err.reason.AsCStr();
    EXPECT_EQ(savanna, rec.biomes[5]);
    EXPECT_EQ(savanna, rec.biomes[9]);
    for (uint32_t i = 0; i < kN; ++i) { EXPECT_EQ(sim.Terrain().biome[i], rec.biomes[i]); }

    const BiomeRegistry plain = Draoi::HexWorld::Testing::LoadShippedVariant({});
    LogCapture log("HexWorld");
    CoW::Weather::WeatherRecord rec2;
    ASSERT_TRUE(Decode(parsed, plain, rec2, err));
    EXPECT_EQ(1u, log.Warnings("Biome.Savanna"));
    EXPECT_EQ(plain.ResolveOrFallback(Dia::Core::StringCRC("Biome.Savanna")), rec2.biomes[5]);
}

TEST_F(WeatherSaveRecordTest, IncomingInfluencesAndClimateBlocksAreIgnoredWithOneInfo)
{
    const BiomeRegistry& reg = WT::TestRegistry();
    WeatherSimulation sim;
    MakeSim(sim, reg, 2, 2);
    Json::Value parsed = EncodeParsed(sim, reg);
    {
        LogCapture log("WTH");
        CoW::Weather::WeatherRecord rec;
        SaveError err;
        ASSERT_TRUE(Decode(parsed, reg, rec, err));
        EXPECT_EQ(0u, log.Total());   // a clean record logs nothing on the weather channel
    }
    parsed["influences"] = Json::Value(Json::objectValue);
    parsed["climate"] = Json::Value(Json::objectValue);
    LogCapture log("WTH");
    CoW::Weather::WeatherRecord rec;
    SaveError err;
    ASSERT_TRUE(Decode(parsed, reg, rec, err));
    EXPECT_EQ(1u, log.Infos("influences"));
    EXPECT_EQ(1u, log.Total());
    EXPECT_TRUE(WT::SameFields(sim.Fields(), rec.fields));
}

// ---- A4 weather-save Task 8: the cow.weather_state export file ----
namespace
{
class WeatherSaveStateTest : public WeatherSaveRecordTest
{
protected:
    Json::Value EncodeState(const WeatherSimulation& sim, const BiomeRegistry& reg)
    {
        Json::Value out(Json::objectValue);
        EXPECT_TRUE(CoW::Weather::EncodeState(sim, 0xABCDEF0123456789ull, reg, out));
        return ThroughText(out);
    }

    bool DecodeStateFile(const Json::Value& root, const BiomeRegistry& reg, CoW::Weather::WeatherRecord& rec, SaveError& err)
    {
        return CoW::Weather::DecodeState(root, spec, reg, rec, err);
    }

    // Marks `rec` so a failed decode that touches it is visible.
    static void Mark(CoW::Weather::WeatherRecord& rec) { rec.week = 99; rec.width = 7; rec.paramsHash = 5; }
    static bool Untouched(const CoW::Weather::WeatherRecord& rec)
    {
        return rec.week == 99 && rec.width == 7 && rec.paramsHash == 5 && rec.fields.CellCount() == 0 && rec.biomes.Size() == 0;
    }
};
} // namespace

TEST_F(WeatherSaveStateTest, StateFileRoundTripsBitIdenticalThroughRealJsonText)
{
    const BiomeRegistry& reg = WT::TestRegistry();
    WeatherSimulation sim;
    MakeSim(sim, reg, 5, 5);
    const Json::Value parsed = EncodeState(sim, reg);
    EXPECT_EQ("cow.weather_state", parsed["format"].asString());
    EXPECT_EQ(1, parsed["version"].asInt());
    EXPECT_TRUE(parsed["ready"].asBool());
    EXPECT_FALSE(parsed.isMember("influences"));
    EXPECT_FALSE(parsed.isMember("climate"));

    CoW::Weather::WeatherRecord rec;
    SaveError err;
    ASSERT_TRUE(DecodeStateFile(parsed, reg, rec, err)) << err.field.AsCStr() << ": " << err.reason.AsCStr();
    EXPECT_TRUE(rec.ready);
    EXPECT_EQ(5u, rec.week);
    EXPECT_EQ(0xABCDEF0123456789ull, rec.paramsHash);
    EXPECT_EQ(kW, rec.width);
    EXPECT_EQ(kH, rec.height);
    EXPECT_TRUE(WT::SameFields(sim.Fields(), rec.fields));
    ASSERT_EQ(kN, rec.biomes.Size());
    for (uint32_t i = 0; i < kN; ++i) { EXPECT_EQ(sim.Terrain().biome[i], rec.biomes[i]) << "cell " << i; }
}

TEST_F(WeatherSaveStateTest, WrongOrMissingFormatIsRejectedNamingFormat)
{
    const BiomeRegistry& reg = WT::TestRegistry();
    WeatherSimulation sim;
    MakeSim(sim, reg, 1, 1);
    const Json::Value good = EncodeState(sim, reg);
    CoW::Weather::WeatherRecord rec;
    SaveError err;

    Json::Value wrong = good;
    wrong["format"] = "cow.terrain_state";
    Mark(rec);
    EXPECT_FALSE(DecodeStateFile(wrong, reg, rec, err));
    EXPECT_STREQ("format", err.field.AsCStr());
    EXPECT_TRUE(Untouched(rec));

    Json::Value missing = good;
    missing.removeMember("format");
    EXPECT_FALSE(DecodeStateFile(missing, reg, rec, err));
    EXPECT_STREQ("format", err.field.AsCStr());

    // A committed save record (no envelope) is not a state file.
    Json::Value bare(Json::objectValue);
    ASSERT_TRUE(CoW::Weather::EncodeRecord(sim, 1, reg, bare));
    EXPECT_FALSE(DecodeStateFile(ThroughText(bare), reg, rec, err));
    EXPECT_STREQ("format", err.field.AsCStr());
}

TEST_F(WeatherSaveStateTest, MissingOrUnsupportedVersionIsRejectedNamingVersionAndLeavesOutUntouched)
{
    const BiomeRegistry& reg = WT::TestRegistry();
    WeatherSimulation sim;
    MakeSim(sim, reg, 1, 1);
    const Json::Value good = EncodeState(sim, reg);
    CoW::Weather::WeatherRecord rec;
    SaveError err;

    Json::Value missing = good;
    missing.removeMember("version");
    Mark(rec);
    EXPECT_FALSE(DecodeStateFile(missing, reg, rec, err));
    EXPECT_STREQ("version", err.field.AsCStr());
    EXPECT_TRUE(Untouched(rec));

    for (int v : { 0, 2 })
    {
        SCOPED_TRACE(v);
        Json::Value bad = good;
        bad["version"] = v;
        EXPECT_FALSE(DecodeStateFile(bad, reg, rec, err));
        EXPECT_STREQ("version", err.field.AsCStr());
        EXPECT_TRUE(Untouched(rec));
    }

    Json::Value text = good;
    text["version"] = "1";
    EXPECT_FALSE(DecodeStateFile(text, reg, rec, err));
    EXPECT_STREQ("version", err.field.AsCStr());
}

TEST_F(WeatherSaveStateTest, ANotReadyBodyIsRejectedForAStateFile)
{
    const BiomeRegistry& reg = WT::TestRegistry();
    Json::Value body(Json::objectValue);
    body["format"] = "cow.weather_state";
    body["version"] = 1;
    body["ready"] = false;
    CoW::Weather::WeatherRecord rec;
    SaveError err;
    Mark(rec);
    EXPECT_FALSE(DecodeStateFile(ThroughText(body), reg, rec, err));
    EXPECT_STREQ("ready", err.field.AsCStr());
    EXPECT_TRUE(Untouched(rec));
}

TEST_F(WeatherSaveStateTest, AnUninitialisedSimIsNotExportedAndOutIsUntouched)
{
    const BiomeRegistry& reg = WT::TestRegistry();
    WeatherSimulation sim;
    Json::Value out(Json::objectValue);
    out["keep"] = 1;
    EXPECT_FALSE(CoW::Weather::EncodeState(sim, 1, reg, out));
    EXPECT_EQ(1u, out.size());
}

TEST_F(WeatherSaveStateTest, ARecordDefectIsRejectedNamingTheKeyAndLeavesOutUntouched)
{
    const BiomeRegistry& reg = WT::TestRegistry();
    WeatherSimulation sim;
    MakeSim(sim, reg, 1, 1);
    const Json::Value good = EncodeState(sim, reg);
    CoW::Weather::WeatherRecord rec;
    SaveError err;

    Json::Value badField = good;
    badField["snow_pack"][3u] = 5.0;
    Mark(rec);
    EXPECT_FALSE(DecodeStateFile(badField, reg, rec, err));
    EXPECT_STREQ("snow_pack", err.field.AsCStr());
    EXPECT_TRUE(Untouched(rec));

    Json::Value noBiome = good;
    noBiome.removeMember("biome");
    EXPECT_FALSE(DecodeStateFile(noBiome, reg, rec, err));
    EXPECT_STREQ("biome", err.field.AsCStr());
    EXPECT_TRUE(Untouched(rec));

    EXPECT_FALSE(DecodeStateFile(Json::Value(Json::arrayValue), reg, rec, err));
    EXPECT_TRUE(Untouched(rec));
}
