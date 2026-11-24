// A4 Weather Feature 1 (weather-data.plan.md Tasks 5-8): the shipped parameter sidecar, its baseline maps and the
// WeatherParameters binding. These assert A4's data (GDD section 14 values copied here as literals), not
// ParameterFile internals (DiaAttribute Feature 8 tests those).
#include <gtest/gtest.h>

#include <DiaAttribute/ParamBinding.h>
#include <DiaAttribute/ParameterFile.h>
#include <DiaCore/Json/external/json/json.h>
#include <DraoiHexWorld/BiomeRegistry.h>
#include <DraoiHexWorld/Testing/BiomeTestRegistry.h>
#include <DraoiHexWorldWeather/Testing/RegistryMapKeys.h>
#include <DraoiHexWorldWeather/WeatherParameters.h>

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

namespace
{
    using Dia::Attribute::AttributeKind;
    using Dia::Attribute::BindingReport;
    using Dia::Attribute::JsonShape;
    using Dia::Attribute::ParamBinding;
    using Dia::Attribute::ParameterFile;
    using Dia::Attribute::ParamError;
    using Dia::Attribute::ParamWarnings;
    using Dia::Core::StringCRC;
    using namespace Draoi::HexWorld;
    using namespace Draoi::HexWorld::Weather;
    using Draoi::HexWorld::Weather::Testing::RegistryMapKeySource;
    namespace T = Draoi::HexWorld::Testing;

    const char* kSidecarPath = "Cluiche/Assets/CoW/Global/Weather/weather_parameters.schema.json";

    std::string ReadShippedSidecar()
    {
        std::error_code ec;
        std::filesystem::path dir = std::filesystem::absolute(std::filesystem::path(__FILE__), ec).parent_path();
        while (!dir.empty())
        {
            const std::filesystem::path candidate = dir / kSidecarPath;
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
        ADD_FAILURE() << "sidecar not found: " << kSidecarPath;
        return std::string();
    }

    const std::string& SidecarText() { static const std::string text = ReadShippedSidecar(); return text; }

    const BiomeRegistry& Reg() { return T::ShippedBiomeRegistry(); }
    const RegistryMapKeySource& Keys() { static const RegistryMapKeySource keys(Reg()); return keys; }

    // A fresh file per caller; the ParameterFile keeps the key-source pointer, so Keys() has static lifetime.
    struct Loaded
    {
        ParameterFile file;
        ParamError    err;
        bool          ok = false;
        explicit Loaded(const std::string& text = SidecarText()) { ok = file.Load(text.c_str(), "weather_parameters", &Keys(), err); }
    };

    // GDD A4_TuningParameters section 14 (plus the A4 extension rows), copied as literals: key, min, max, default.
    struct Row { const char* key; double mn, mx, def; };
    const Row kRows[] = {
        { "solar.latitude_cooling_factor", 0.0, 0.5, 0.3 }, { "solar.cloud_albedo", 0.2, 0.6, 0.4 },
        { "solar.water_heat_capacity", 0.1, 0.5, 0.3 },     { "solar.land_heat_capacity", 0.6, 1.0, 0.8 },
        { "solar.cloud_decay", 0.0, 1.0, 0.5 },
        { "pressure.pressure_sensitivity", 0.3, 0.9, 0.6 },
        { "wind.prevailing_strength", 0.4, 0.8, 0.6 },      { "wind.pressure_strength", 0.2, 0.6, 0.4 },
        { "evaporation.ocean_evap_rate", 0.2, 0.5, 0.3 },   { "evaporation.ground_evap_rate", 0.02, 0.1, 0.05 },
        { "transport.moisture_transport_rate", 0.2, 0.6, 0.4 }, { "transport.heat_transport_rate", 0.1, 0.4, 0.2 },
        { "orographic.cooling_rate", 0.2, 0.5, 0.3 },       { "orographic.condensation_temp", 0.3, 0.5, 0.4 },
        { "orographic.precip_rate", 0.4, 0.8, 0.6 },
        { "precipitation.rain_threshold", 0.5, 0.8, 0.6 },  { "precipitation.snow_threshold", 0.2, 0.4, 0.3 },
        { "precipitation.precip_rate", 0.3, 0.7, 0.5 },
        { "snow.melt_threshold", 0.3, 0.45, 0.35 },         { "snow.melt_rate", 0.2, 0.5, 0.3 },
        { "snow.melt_cooling", 0.05, 0.2, 0.1 },
        { "init.initial_moisture_water", 0.0, 1.0, 0.8 },   { "init.initial_moisture_land", 0.0, 1.0, 0.3 },
        // boundary.edge_inflow is a bool, checked separately
        { "influence.max_strength", 0.5, 2.0, 1.0 },
        { "influence.max_delta_pressure", 0.05, 0.5, 0.3 }, { "influence.max_delta_moisture", 0.05, 0.5, 0.3 },
        { "influence.max_delta_temperature", 0.05, 0.5, 0.15 },
        { "influence.max_lifetime_weeks", 1, 52, 13 },      { "influence.max_active", 8, 256, 64 },
        { "influence.preview_fade_days", 0.25, 3.0, 1.0 },
        { "influence.pressure_anomaly_decay", 0.5, 0.9, 0.7 }, { "influence.pressure_anomaly_max", 0.2, 0.5, 0.5 },
        { "influence.cap_water", 0.0, 2.0, 1.0 },           { "influence.max_radius", 0, 64, 32 },
        { "influence.max_set_cells", 64, 65536, 8192 },     { "influence.ground_water_factor", 0.0, 1.0, 0.05 },
    };
    constexpr size_t kRowCount = sizeof(kRows) / sizeof(kRows[0]);   // 36 numeric rows + 1 bool = 37 scalar rows

    // key -> member, written out independently of the production binding so a typo in either is caught.
    struct Member { const char* key; float WeatherParameters::* f; int32_t WeatherParameters::* i; };
    const Member kMembers[] = {
        { "solar.latitude_cooling_factor", &WeatherParameters::latitudeCoolingFactor, nullptr },
        { "solar.cloud_albedo", &WeatherParameters::cloudAlbedo, nullptr },
        { "solar.water_heat_capacity", &WeatherParameters::waterHeatCapacity, nullptr },
        { "solar.land_heat_capacity", &WeatherParameters::landHeatCapacity, nullptr },
        { "solar.cloud_decay", &WeatherParameters::cloudDecay, nullptr },
        { "pressure.pressure_sensitivity", &WeatherParameters::pressureSensitivity, nullptr },
        { "wind.prevailing_strength", &WeatherParameters::prevailingStrength, nullptr },
        { "wind.pressure_strength", &WeatherParameters::pressureStrength, nullptr },
        { "evaporation.ocean_evap_rate", &WeatherParameters::oceanEvapRate, nullptr },
        { "evaporation.ground_evap_rate", &WeatherParameters::groundEvapRate, nullptr },
        { "transport.moisture_transport_rate", &WeatherParameters::moistureTransportRate, nullptr },
        { "transport.heat_transport_rate", &WeatherParameters::heatTransportRate, nullptr },
        { "orographic.cooling_rate", &WeatherParameters::orographicCoolingRate, nullptr },
        { "orographic.condensation_temp", &WeatherParameters::condensationTemp, nullptr },
        { "orographic.precip_rate", &WeatherParameters::orographicPrecipRate, nullptr },
        { "precipitation.rain_threshold", &WeatherParameters::rainThreshold, nullptr },
        { "precipitation.snow_threshold", &WeatherParameters::snowThreshold, nullptr },
        { "precipitation.precip_rate", &WeatherParameters::precipRate, nullptr },
        { "snow.melt_threshold", &WeatherParameters::meltThreshold, nullptr },
        { "snow.melt_rate", &WeatherParameters::meltRate, nullptr },
        { "snow.melt_cooling", &WeatherParameters::meltCooling, nullptr },
        { "init.initial_moisture_water", &WeatherParameters::initialMoistureWater, nullptr },
        { "init.initial_moisture_land", &WeatherParameters::initialMoistureLand, nullptr },
        { "influence.max_strength", &WeatherParameters::influenceMaxStrength, nullptr },
        { "influence.max_delta_pressure", &WeatherParameters::influenceMaxDeltaPressure, nullptr },
        { "influence.max_delta_moisture", &WeatherParameters::influenceMaxDeltaMoisture, nullptr },
        { "influence.max_delta_temperature", &WeatherParameters::influenceMaxDeltaTemperature, nullptr },
        { "influence.max_lifetime_weeks", nullptr, &WeatherParameters::influenceMaxLifetimeWeeks },
        { "influence.max_active", nullptr, &WeatherParameters::influenceMaxActive },
        { "influence.preview_fade_days", &WeatherParameters::influencePreviewFadeDays, nullptr },
        { "influence.pressure_anomaly_decay", &WeatherParameters::pressureAnomalyDecay, nullptr },
        { "influence.pressure_anomaly_max", &WeatherParameters::pressureAnomalyMax, nullptr },
        { "influence.cap_water", &WeatherParameters::influenceCapWater, nullptr },
        { "influence.max_radius", nullptr, &WeatherParameters::influenceMaxRadius },
        { "influence.max_set_cells", nullptr, &WeatherParameters::influenceMaxSetCells },
        { "influence.ground_water_factor", &WeatherParameters::influenceGroundWaterFactor, nullptr },
    };

    // ParameterFile::Set compares the value (double) with float bounds, so a bound like 0.2 must be passed as float(0.2).
    double F(double v) { return static_cast<double>(static_cast<float>(v)); }

    const Row* FindRow(const char* key)
    {
        for (const Row& r : kRows) if (std::strcmp(r.key, key) == 0) return &r;
        return nullptr;
    }

    bool Resolve(ParamBinding<WeatherParameters>& b, const ParameterFile& f, BindingReport& report)
    {
        BindWeatherParameters(b, Reg().Count());
        return b.Resolve(f.Schema(), report);
    }

    std::string Dump(const Json::Value& v)
    {
        Json::StreamWriterBuilder w;
        w["indentation"] = "";
        return Json::writeString(w, v);
    }

    Json::Value ParseText(const std::string& text)
    {
        Json::Value v;
        Json::CharReaderBuilder b;
        std::string errs;
        std::istringstream in(text);
        EXPECT_TRUE(Json::parseFromStream(b, in, &v, &errs)) << errs;
        return v;
    }

    std::string PackText(const ParameterFile& f, const std::string& body)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(f.SchemaHash()));
        return std::string("{\"format\":\"cow.weather_params\",\"version\":1,\"basedOn\":\"") + buf + "\"," + body + "}";
    }

    BiomeIndex B(const char* id) { return T::B(id); }
}

// ---- Task 5: the sidecar's scalar rows ---------------------------------------------------------------------------

TEST(WeatherSidecar, ShippedSidecarLoadsStrictly)
{
    Loaded l;
    ASSERT_TRUE(l.ok) << l.err.key.AsCStr() << ": " << l.err.reason.AsCStr();
    EXPECT_TRUE(l.file.IsLoaded());
}

TEST(WeatherSidecar, EveryEntryHasADescriptionAndAKindMatchingItsRange)
{
    const Json::Value doc = ParseText(SidecarText());
    ASSERT_TRUE(doc["entries"].isArray());
    EXPECT_EQ(doc["for"].asString(), "cow.weather_params");
    for (const Json::Value& e : doc["entries"])
    {
        const std::string key = e["key"].asString();
        EXPECT_GT(e["description"].asString().size(), 10u) << key;
        EXPECT_EQ(e["kind"].asString(), "parameter") << key;
        const std::string type = e["type"].asString();
        if (type == "float" || type == "int")
        {
            ASSERT_TRUE(e.isMember("min") && e.isMember("max") && e.isMember("default")) << key;
            EXPECT_LE(e["min"].asDouble(), e["default"].asDouble()) << key;
            EXPECT_LE(e["default"].asDouble(), e["max"].asDouble()) << key;
        }
    }
}

TEST(WeatherSidecar, DefaultsAndRangesEqualTheGddLiterals)
{
    Loaded l;
    ASSERT_TRUE(l.ok);
    for (const Row& r : kRows)
    {
        const auto* a = l.file.Schema().FindAttribute(StringCRC(r.key));
        ASSERT_NE(a, nullptr) << r.key;
        EXPECT_FLOAT_EQ(a->default_value, static_cast<float>(r.def)) << r.key;
        EXPECT_FLOAT_EQ(a->minimum_value, static_cast<float>(r.mn)) << r.key;
        EXPECT_FLOAT_EQ(a->maximum_value, static_cast<float>(r.mx)) << r.key;
    }
    const auto* edge = l.file.Schema().FindAttribute(StringCRC("boundary.edge_inflow"));
    ASSERT_NE(edge, nullptr);
    EXPECT_EQ(edge->kind, AttributeKind::Bool);
    EXPECT_FLOAT_EQ(edge->default_value, 1.0f);
}

TEST(WeatherSidecar, IntRowsAreWholeNumberKinds)
{
    Loaded l;
    ASSERT_TRUE(l.ok);
    for (const char* k : { "influence.max_lifetime_weeks", "influence.max_active", "influence.max_radius", "influence.max_set_cells" })
        EXPECT_EQ(l.file.Schema().FindAttribute(StringCRC(k))->kind, AttributeKind::Int) << k;
}

TEST(WeatherSidecar, HasExactlyTheExpectedRowsAndNoOwnedElsewhereRows)
{
    Loaded l;
    ASSERT_TRUE(l.ok);
    const auto& schema = l.file.Schema();
    uint32_t scalars = 0, mapColumns = 0;
    for (uint32_t i = 0; i < schema.GetAttributeCount(); ++i)
    {
        const std::string key = schema.GetAttributeByIndex(i).fullKey.AsCStr();
        if (key.rfind("baseline.", 0) == 0) ++mapColumns; else ++scalars;
        std::string lower = key;
        for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        EXPECT_EQ(lower.find("drain"), std::string::npos) << key;           // ground_water.drain_rate is A5's
        EXPECT_EQ(lower.find("storm_pressure"), std::string::npos) << key;  // removed from the GDD
    }
    EXPECT_EQ(scalars, kRowCount + 1);                    // + boundary.edge_inflow
    EXPECT_EQ(mapColumns, 4u * Reg().Count());            // four seasons x one column per registry biome
}

TEST(WeatherSidecar, RangeBoundariesAreAcceptedAndOutsideValuesRefused)
{
    for (const Row& r : kRows)
    {
        Loaded l;
        ASSERT_TRUE(l.ok);
        ParamError err;
        // snow_threshold's max (0.4) is above the default melt_threshold (0.35): raise the partner first, as an author would.
        if (std::strcmp(r.key, "precipitation.snow_threshold") == 0)
            ASSERT_TRUE(l.file.Set(StringCRC("snow.melt_threshold"), Json::Value(F(0.45)), err));
        EXPECT_TRUE(l.file.Set(StringCRC(r.key), Json::Value(F(r.mn)), err)) << r.key << " min";
        EXPECT_TRUE(l.file.Set(StringCRC(r.key), Json::Value(F(r.mx)), err)) << r.key << " max";
        const double step = (r.mx - r.mn) * 0.01 + 0.5 * (r.mx - r.mn >= 1.0 ? 1.0 : 0.0);
        EXPECT_FALSE(l.file.Set(StringCRC(r.key), Json::Value(F(r.mn) - step), err)) << r.key << " below";
        EXPECT_STREQ(err.key.AsCStr(), r.key);
        EXPECT_FALSE(l.file.Set(StringCRC(r.key), Json::Value(F(r.mx) + step), err)) << r.key << " above";
    }
}

TEST(WeatherSidecar, NonWholeValueOnAnIntRowIsRefused)
{
    Loaded l;
    ASSERT_TRUE(l.ok);
    ParamError err;
    EXPECT_FALSE(l.file.Set(StringCRC("influence.max_active"), Json::Value(64.5), err));
    EXPECT_TRUE(l.file.Set(StringCRC("influence.max_active"), Json::Value(65), err));
}

TEST(WeatherSidecar, SnowThresholdAboveMeltThresholdIsRefusedNamingBothKeys)
{
    Loaded l;
    ASSERT_TRUE(l.ok);
    ParamError err;
    EXPECT_FALSE(l.file.Set(StringCRC("precipitation.snow_threshold"), Json::Value(0.4), err));   // melt is 0.35
    const std::string both = std::string(err.key.AsCStr()) + "|" + err.otherKey.AsCStr();
    EXPECT_NE(both.find("precipitation.snow_threshold"), std::string::npos) << both;
    EXPECT_NE(both.find("snow.melt_threshold"), std::string::npos) << both;
    EXPECT_FLOAT_EQ(l.file.Values().GetValue(StringCRC("precipitation.snow_threshold")), 0.3f);   // unchanged
}

TEST(WeatherSidecar, EqualThresholdsAreAllowed)
{
    Loaded l;
    ASSERT_TRUE(l.ok);
    ParamError err;
    EXPECT_TRUE(l.file.Set(StringCRC("snow.melt_threshold"), Json::Value(F(0.3)), err));   // equals snow_threshold 0.3
}

TEST(WeatherSidecar, PackBreakingTheConstraintIsRejectedWhole)
{
    Loaded l;
    ASSERT_TRUE(l.ok);
    ParamWarnings warn;
    ParamError err;
    const Json::Value pack = ParseText(PackText(l.file, "\"precipitation.snow_threshold\":0.4,\"solar.cloud_albedo\":0.5"));
    EXPECT_FALSE(l.file.ApplyPack(pack, warn, err));
    EXPECT_FLOAT_EQ(l.file.Values().GetValue(StringCRC("solar.cloud_albedo")), 0.4f);   // the good key did not apply either
}

// ---- Task 6: baseline maps ---------------------------------------------------------------------------------------

TEST(WeatherSidecar, BaselineHasAColumnForEveryRegistryBiomeInEverySeason)
{
    Loaded l;
    ASSERT_TRUE(l.ok);
    for (const char* season : { "spring", "summer", "autumn", "winter" })
        for (uint32_t b = 0; b < Reg().Count(); ++b)
        {
            const std::string key = std::string("baseline.") + season + "." + Reg().IdText(static_cast<BiomeIndex>(b));
            const auto* a = l.file.Schema().FindAttribute(StringCRC(key.c_str()));
            ASSERT_NE(a, nullptr) << key;
            EXPECT_GE(a->default_value, 0.0f) << key;
            EXPECT_LE(a->default_value, 1.0f) << key;
        }
}

TEST(WeatherSidecar, AnExtraRegistryBiomeFailsTheLoadNamingTheMissingKey)
{
    const BiomeRegistry& extra = T::ExtraBiomeRegistry();
    ASSERT_GT(extra.Count(), Reg().Count());
    static const RegistryMapKeySource extraKeys(extra);
    ParameterFile file;
    ParamError err;
    EXPECT_FALSE(file.Load(SidecarText().c_str(), "weather_parameters", &extraKeys, err));
    const std::string where = std::string(err.key.AsCStr()) + " | " + err.otherKey.AsCStr() + " | " + err.reason.AsCStr();
    EXPECT_NE(where.find("baseline."), std::string::npos) << where;
    EXPECT_NE(where.find(extra.IdText(static_cast<BiomeIndex>(extra.Count() - 1))), std::string::npos) << where;
    EXPECT_FALSE(file.IsLoaded());
}

TEST(WeatherSidecar, SummerIsWarmerThanWinterForEveryLandBiome)
{
    Loaded l;
    ASSERT_TRUE(l.ok);
    ParamBinding<WeatherParameters> b;
    BindingReport report;
    ASSERT_TRUE(Resolve(b, l.file, report));
    WeatherParameters p;
    b.Snapshot(l.file.Values(), p);

    const TagId land = Reg().FindTag(StringCRC("land"));
    ASSERT_NE(land, kNoTag);
    uint32_t checked = 0;
    for (uint32_t i = 0; i < Reg().Count(); ++i)
    {
        if (!Reg().HasTag(static_cast<BiomeIndex>(i), land)) continue;
        EXPECT_GT(p.baselineSummer[i], p.baselineWinter[i]) << Reg().IdText(static_cast<BiomeIndex>(i));
        ++checked;
    }
    EXPECT_GE(checked, 10u);   // 11 land biomes today; the rule does not name any
}

TEST(WeatherSidecar, EveryBaselineValueIsInUnitRange)
{
    Loaded l;
    ASSERT_TRUE(l.ok);
    ParamBinding<WeatherParameters> b;
    BindingReport report;
    ASSERT_TRUE(Resolve(b, l.file, report));
    WeatherParameters p;
    b.Snapshot(l.file.Values(), p);
    for (uint32_t s = 0; s < kSeasonCount; ++s)
        for (uint32_t i = 0; i < Reg().Count(); ++i)
        {
            const float v = p.BaselineRow(static_cast<Season>(s))[i];
            EXPECT_GE(v, 0.0f);
            EXPECT_LE(v, 1.0f);
        }
}

TEST(WeatherSidecar, BaselineAnchorsAreCalibratedAgainstTheSnowAndCondensationThresholds)
{
    Loaded l;
    ASSERT_TRUE(l.ok);
    ParamBinding<WeatherParameters> b;
    BindingReport report;
    ASSERT_TRUE(Resolve(b, l.file, report));
    WeatherParameters p;
    b.Snapshot(l.file.Values(), p);
    // Tundra is below the snow line (0.3) in winter and spring; rainforest is above the condensation temperature
    // (0.4) all year; the desert never drops under it.
    EXPECT_LT(p.baselineWinter[B("Biome.Tundra")], p.snowThreshold);
    EXPECT_LT(p.baselineSpring[B("Biome.Tundra")], p.snowThreshold);
    for (uint32_t s = 0; s < kSeasonCount; ++s)
    {
        EXPECT_GT(p.BaselineRow(static_cast<Season>(s))[B("Biome.Rainforest")], p.condensationTemp);
        EXPECT_GT(p.BaselineRow(static_cast<Season>(s))[B("Biome.Desert")], p.condensationTemp);
    }
}

TEST(WeatherSidecar, SparsePackOverridesOneBiomeAndLeavesTheRest)
{
    Loaded l;
    ASSERT_TRUE(l.ok);
    ParamWarnings warn;
    ParamError err;
    const Json::Value pack = ParseText(PackText(l.file, "\"baseline.summer\":{\"Biome.Desert\":0.9}"));
    ASSERT_TRUE(l.file.ApplyPack(pack, warn, err)) << err.reason.AsCStr();
    ParamBinding<WeatherParameters> b;
    BindingReport report;
    ASSERT_TRUE(Resolve(b, l.file, report));
    WeatherParameters p;
    b.Snapshot(l.file.Values(), p);
    EXPECT_FLOAT_EQ(p.baselineSummer[B("Biome.Desert")], 0.9f);
    EXPECT_FLOAT_EQ(p.baselineSummer[B("Biome.Grassland")], 0.72f);
}

TEST(WeatherSidecar, PackValueAboveOneIsClampedWithAWarning)
{
    Loaded l;
    ASSERT_TRUE(l.ok);
    ParamWarnings warn;
    ParamError err;
    const Json::Value pack = ParseText(PackText(l.file, "\"baseline.summer\":{\"Biome.Desert\":1.2}"));
    ASSERT_TRUE(l.file.ApplyPack(pack, warn, err));
    EXPECT_GE(warn.items.Size(), 1u);
    ParamBinding<WeatherParameters> b;
    BindingReport report;
    ASSERT_TRUE(Resolve(b, l.file, report));
    WeatherParameters p;
    b.Snapshot(l.file.Values(), p);
    EXPECT_FLOAT_EQ(p.baselineSummer[B("Biome.Desert")], 1.0f);
}

TEST(WeatherSidecar, PackNamingAnUnknownBiomeKeyIsDroppedWithAWarning)
{
    Loaded l;
    ASSERT_TRUE(l.ok);
    ParamWarnings warn;
    ParamError err;
    const Json::Value pack = ParseText(PackText(l.file, "\"baseline.summer\":{\"Biome.Nope\":0.9,\"Biome.Desert\":0.91}"));
    ASSERT_TRUE(l.file.ApplyPack(pack, warn, err));
    EXPECT_GE(warn.items.Size(), 1u);
    EXPECT_FLOAT_EQ(l.file.Values().GetValue(StringCRC("baseline.summer.Biome.Desert")), 0.91f);
}

// ---- Task 7: the binding ----------------------------------------------------------------------------------------

TEST(WeatherSidecar, BindingResolvesCleanAgainstTheShippedSidecar)
{
    Loaded l;
    ASSERT_TRUE(l.ok);
    ParamBinding<WeatherParameters> b;
    BindingReport report;
    EXPECT_TRUE(Resolve(b, l.file, report));
    EXPECT_EQ(report.unboundParameters.Size(), 0u);
    EXPECT_EQ(report.bindingsWithoutEntry.Size(), 0u);
    EXPECT_EQ(report.countMismatches.Size(), 0u);
    EXPECT_EQ(report.kindMismatches.Size(), 0u);
}

TEST(WeatherSidecar, SnapshotAtTheDefaultsMatchesTheGddLiterals)
{
    Loaded l;
    ASSERT_TRUE(l.ok);
    ParamBinding<WeatherParameters> b;
    BindingReport report;
    ASSERT_TRUE(Resolve(b, l.file, report));
    WeatherParameters p;
    b.Snapshot(l.file.Values(), p);
    for (const Member& m : kMembers)
    {
        const Row* r = FindRow(m.key);
        ASSERT_NE(r, nullptr) << m.key;
        if (m.f != nullptr) EXPECT_FLOAT_EQ(p.*(m.f), static_cast<float>(r->def)) << m.key;
        else                EXPECT_EQ(p.*(m.i), static_cast<int32_t>(r->def)) << m.key;
    }
    EXPECT_TRUE(p.edgeInflow);
}

TEST(WeatherSidecar, SnapshotFillsEveryMemberFromItsOwnKey)
{
    Loaded l;
    ASSERT_TRUE(l.ok);
    // Give every scalar a distinct in-range value, so a binding that points at the wrong member shows up.
    double expected[sizeof(kMembers) / sizeof(kMembers[0])];
    ParamError err;
    for (size_t k = 0; k < sizeof(kMembers) / sizeof(kMembers[0]); ++k)
    {
        const Row* r = FindRow(kMembers[k].key);
        ASSERT_NE(r, nullptr);
        double v = r->mn + (r->mx - r->mn) * (0.30 + 0.011 * static_cast<double>(k));
        if (kMembers[k].i != nullptr) v = std::floor(v);
        expected[k] = v;
        ASSERT_TRUE(l.file.Set(StringCRC(kMembers[k].key), Json::Value(v), err)) << kMembers[k].key << ": " << err.reason.AsCStr();
    }
    ASSERT_TRUE(l.file.Set(StringCRC("boundary.edge_inflow"), Json::Value(false), err));

    ParamBinding<WeatherParameters> b;
    BindingReport report;
    ASSERT_TRUE(Resolve(b, l.file, report));
    WeatherParameters p;
    b.Snapshot(l.file.Values(), p);
    for (size_t k = 0; k < sizeof(kMembers) / sizeof(kMembers[0]); ++k)
    {
        if (kMembers[k].f != nullptr) EXPECT_FLOAT_EQ(p.*(kMembers[k].f), static_cast<float>(expected[k])) << kMembers[k].key;
        else                          EXPECT_EQ(p.*(kMembers[k].i), static_cast<int32_t>(expected[k])) << kMembers[k].key;
    }
    EXPECT_FALSE(p.edgeInflow);
}

TEST(WeatherSidecar, BaselineColumnsSnapshotInRegistryOrderAndLeaveTheTailUntouched)
{
    Loaded l;
    ASSERT_TRUE(l.ok);
    ParamBinding<WeatherParameters> b;
    BindingReport report;
    ASSERT_TRUE(Resolve(b, l.file, report));
    WeatherParameters p;
    for (uint32_t i = Reg().Count(); i < kMaxBaselineBiomes; ++i) p.baselineSpring[i] = -7.0f;
    b.Snapshot(l.file.Values(), p);
    EXPECT_FLOAT_EQ(p.baselineSpring[B("Biome.Grassland")], 0.55f);
    EXPECT_FLOAT_EQ(p.baselineWinter[B("Biome.Tundra")], 0.02f);
    EXPECT_FLOAT_EQ(p.baselineSpring[Reg().Count()], -7.0f);
    EXPECT_FLOAT_EQ(p.baselineSpring[kMaxBaselineBiomes - 1], -7.0f);
}

TEST(WeatherSidecar, HashIsStableAcrossLoadsAndChangesWhenTheLastParameterChanges)
{
    Loaded a, b;
    ASSERT_TRUE(a.ok && b.ok);
    EXPECT_EQ(a.file.Hash(), b.file.Hash());
    const uint32_t last = a.file.Schema().GetAttributeCount() - 1;
    const std::string lastKey = a.file.Schema().GetAttributeByIndex(last).fullKey.AsCStr();
    EXPECT_NE(lastKey.find("baseline.winter."), std::string::npos) << lastKey;
    ParamError err;
    ASSERT_TRUE(b.file.Set(StringCRC(lastKey.c_str()), Json::Value(0.41), err));
    EXPECT_NE(a.file.Hash(), b.file.Hash());
}

// ---- Task 8: broken sidecars ------------------------------------------------------------------------------------

namespace
{
    struct Broken { const char* name; const char* expectKey; void (*mutate)(Json::Value&); };

    const Broken kBroken[] = {
        { "default above its range", "solar.latitude_cooling_factor",
          [](Json::Value& d) { d["entries"][0]["default"] = 9.0; } },
        { "default below its range", "solar.cloud_albedo",
          [](Json::Value& d) { d["entries"][1]["default"] = -1.0; } },
        { "missing default", "solar.water_heat_capacity",
          [](Json::Value& d) { d["entries"][2].removeMember("default"); } },
        { "constraint violated by the defaults", nullptr,
          [](Json::Value& d)
          {
              for (Json::Value& e : d["entries"])
                  if (e["key"].asString() == "precipitation.snow_threshold") e["default"] = 0.4;   // melt default is 0.35
          } },
        { "bad band", "solar.latitude_cooling_factor",
          [](Json::Value& d)
          {
              Json::Value bands(Json::objectValue);
              Json::Value range(Json::arrayValue);
              range.append(0.4);
              range.append(0.1);
              bands["inverted"] = range;
              d["entries"][0]["bands"] = bands;
          } },
        { "wrong format", nullptr, [](Json::Value& d) { d["format"] = "not.a.value_schema"; } },
        { "unknown constraint key", nullptr,
          [](Json::Value& d) { d["constraints"][0]["b"] = "snow.no_such_key"; } },
    };
}

TEST(WeatherSidecar, BrokenSidecarsFailToLoadNamingTheFaultAndLeaveTheFileUnloaded)
{
    for (const Broken& c : kBroken)
    {
        Json::Value doc = ParseText(SidecarText());
        c.mutate(doc);
        Loaded l(Dump(doc));
        EXPECT_FALSE(l.ok) << c.name;
        EXPECT_FALSE(l.file.IsLoaded()) << c.name;
        EXPECT_GT(std::strlen(l.err.reason.AsCStr()), 0u) << c.name;
        if (c.expectKey != nullptr) EXPECT_STREQ(l.err.key.AsCStr(), c.expectKey) << c.name;
    }
}

TEST(WeatherSidecar, UntouchedRoundTripThroughTheMutationHelperStillLoads)
{
    Loaded l(Dump(ParseText(SidecarText())));   // guards the fixtures above: only the mutation breaks the file
    EXPECT_TRUE(l.ok) << l.err.key.AsCStr() << ": " << l.err.reason.AsCStr();
}

// ---- Gap-audit edges (Task 9) -----------------------------------------------------------------------------------

TEST(WeatherSidecar, PackKeyInTheWrongGroupIsDroppedWithAWarning)
{
    Loaded l;
    ASSERT_TRUE(l.ok);
    ParamWarnings warn;
    ParamError err;
    const Json::Value pack = ParseText(PackText(l.file, "\"solar.snow_threshold\":0.25,\"snow.melt_rate\":0.31"));
    ASSERT_TRUE(l.file.ApplyPack(pack, warn, err));
    EXPECT_GE(warn.items.Size(), 1u);
    EXPECT_EQ(l.file.Schema().FindAttribute(StringCRC("solar.snow_threshold")), nullptr);
    EXPECT_FLOAT_EQ(l.file.Values().GetValue(StringCRC("precipitation.snow_threshold")), 0.3f);
    EXPECT_FLOAT_EQ(l.file.Values().GetValue(StringCRC("snow.melt_rate")), 0.31f);   // the valid key still applied
}

TEST(WeatherSidecar, TheLastRegistryBiomeIsBoundInEverySeasonColumn)
{
    Loaded l;
    ASSERT_TRUE(l.ok);
    ParamBinding<WeatherParameters> b;
    BindingReport report;
    ASSERT_TRUE(Resolve(b, l.file, report));
    WeatherParameters p;
    for (uint32_t s = 0; s < kSeasonCount; ++s) p.BaselineRow(static_cast<Season>(s))[Reg().Count() - 1] = -3.0f;
    b.Snapshot(l.file.Values(), p);
    const BiomeIndex last = static_cast<BiomeIndex>(Reg().Count() - 1);
    EXPECT_FLOAT_EQ(p.baselineSpring[last], 0.70f);   // Biome.DesertifiedPlain
    EXPECT_FLOAT_EQ(p.baselineSummer[last], 0.88f);
    EXPECT_FLOAT_EQ(p.baselineAutumn[last], 0.72f);
    EXPECT_FLOAT_EQ(p.baselineWinter[last], 0.50f);
}

TEST(WeatherSidecar, ResolveAgainstTheWrongBiomeCountReportsCountMismatches)
{
    Loaded l;
    ASSERT_TRUE(l.ok);
    ParamBinding<WeatherParameters> b;
    BindWeatherParameters(b, Reg().Count() - 1);   // a stale count after the registry grew
    BindingReport report;
    EXPECT_FALSE(b.Resolve(l.file.Schema(), report));
    EXPECT_EQ(report.countMismatches.Size(), 4u);
}
