// A4 weather ca-pipeline Task 12: per-layer cell-data goldens for the 52-week pipeline (AC 38, Open Q3).
// Goldens live in Tests/GoogleTests/Draoi/Golden/weather_pipeline.golden.json (the CoW sidecar-driven
// weather_layers.golden.json belongs to Feature 6). Draoi tests cannot read the CoW sidecar, so these use MakeTestParams().
//
// NOTE (spec Open Q3): the goldens are DEBUG-recorded. No compiler fp flags are set (MSVC default /fp:precise, no
// fast-math), so one hash set is expected to serve both configurations. If a Release run mismatches, per-configuration
// golden sets are needed.
//
// Re-baseline after an intended change:
//   $env:DIA_UPDATE_GOLDEN = "1"; dia run googletest --filter="WeatherGolden*" --shards 2; Remove-Item Env:DIA_UPDATE_GOLDEN
#include <gtest/gtest.h>

#include <DiaCellLayers/Testing/CellLayerGolden.h>
#include <DiaParallelExecutor/SerialExecutor.h>
#include <DraoiHexWorld/BiomeRegistry.h>
#include <DraoiHexWorldWeather/Testing/WeatherTestExecutors.h>
#include <DraoiHexWorldWeather/Testing/WeatherTestFixtures.h>
#include <DraoiHexWorldWeather/WeatherSimulation.h>

#include <filesystem>
#include <string>
#include <vector>

namespace
{
    using namespace Draoi::HexWorld;
    using namespace Draoi::HexWorld::Weather;
    namespace WT = Draoi::HexWorld::Weather::Testing;
    namespace Golden = Dia::CellLayers::Testing;
    namespace fs = std::filesystem;

    fs::path FindRepoRoot()
    {
        std::error_code ec;
        fs::path dir = fs::absolute(fs::path(__FILE__), ec).parent_path();
        while (!dir.empty())
        {
            if (fs::exists(dir / "Cluiche/Assets/CoW/Global/WorldGen/Presets", ec)) { return dir; }
            if (dir == dir.parent_path()) { break; }
            dir = dir.parent_path();
        }
        return {};
    }

    struct Lcg
    {
        uint32_t s;
        explicit Lcg(uint32_t seed) : s(seed) {}
        float Next()
        {
            s = s * 1664525u + 1013904223u;
            return float(s >> 8) / float(1u << 24);
        }
    };

    Season SeasonOf(uint32_t week) { return static_cast<Season>((week / 13u) % 4u); }

    // Runs 52 weeks on a hand-built Small (66x42) world and returns the committed fields.
    bool RunPipeline(bool forced, Dia::ParallelExecutor::IParallelExecutor& exec, WeatherFields& out)
    {
        const uint32_t w = 66, h = 42, n = w * h;
        WeatherTerrain terrain;
        terrain.width = w; terrain.height = h;
        terrain.elevation.Reserve(n); terrain.latitude.Reserve(n); terrain.waterSource.Reserve(n); terrain.biome.Reserve(n);
        Lcg rng(4242u);
        for (uint32_t i = 0; i < n; ++i)
        {
            const uint32_t col = i % w, row = i / w;
            terrain.elevation.Add(rng.Next());
            terrain.latitude.Add(float(row) / float(h - 1));
            WaterSource ws = WaterSource::None;
            if (col < 2) ws = WaterSource::Ocean;
            else if (col >= w / 2 && col < w / 2 + 5 && row >= h / 3 && row < h / 3 + 4) ws = WaterSource::Lake;
            terrain.waterSource.Add(ws);
            terrain.biome.Add(static_cast<BiomeIndex>(uint32_t(rng.Next() * 13.0f) % 13u));
        }

        const WeatherParameters params = WT::MakeTestParams();
        HydrologyInputs hydro;
        hydro.Resize(n);
        hydro.present = false;
        InfluenceSnapshot influences;

        WeatherSimulation sim;
        if (!sim.Initialise(terrain, params, WT::TestRegistry(), nullptr)) return false;
        if (forced)
        {
            Dia::Core::Containers::DynamicArray<uint32_t> cells;
            cells.Reserve(n);
            for (uint32_t c = 3; c < n; c += 7) cells.Add(c);
            if (sim.RegisterCellSet(cells) != 0) return false;
            WeatherForcing f;
            f.kind = ForcingKind::PressureHold; f.cellSet = 0; f.value = 0.9f; f.weeksRemaining = 60;
            if (!sim.AddForcing(f)) return false;
            f = WeatherForcing();
            f.kind = ForcingKind::Rain; f.cell = n / 3; f.value = 0.4f; f.weeksRemaining = 20;
            if (!sim.AddForcing(f)) return false;
            f = WeatherForcing();
            f.kind = ForcingKind::WindOverride; f.cell = n / 4; f.value = 1.0f; f.value2 = 0.7f; f.weeksRemaining = 0;
            if (!sim.AddForcing(f)) return false;
        }
        for (uint32_t week = 0; week < 52; ++week)
            sim.Step(params, SeasonOf(week), week % 52, WT::TestRegistry(), nullptr, hydro, influences, exec);
        WT::CloneFields(sim.Fields(), out);
        return true;
    }

    std::vector<Golden::GoldenLayerInput> Inputs(const WeatherFields& f)
    {
        auto in = [](const char* name, const Dia::Core::Containers::DynamicArray<float>& a)
        {
            return Golden::GoldenLayerInput{ name, a.Size() > 0 ? &a[0u] : nullptr, a.Size() };
        };
        return {
            in("weather.moisture", f.moisture), in("weather.temperature", f.temperature), in("weather.pressure", f.pressure),
            in("weather.wind_direction", f.windDirection), in("weather.wind_speed", f.windSpeed),
            in("weather.precipitation", f.precipitation), in("weather.cloud_cover", f.cloudCover),
            in("weather.water_to_ground", f.waterToGround), in("weather.snow_pack", f.snowPack),
            in("weather.pressure_anomaly", f.pressureAnomaly) };
    }

    Golden::GoldenStore MakeStore(const fs::path& root, bool update)
    {
        return Golden::GoldenStore(root / "Cluiche/Tests/GoogleTests/Draoi/Golden/weather_pipeline.golden.json",
                                   root / "Cluiche/out/Draoi/golden-dumps", update);
    }

    void CheckCase(const char* caseId, bool forced)
    {
        const fs::path root = FindRepoRoot();
        ASSERT_FALSE(root.empty()) << "repo root not found from " << __FILE__;
        Dia::ParallelExecutor::SerialExecutor serial;
        WeatherFields fields;
        ASSERT_TRUE(RunPipeline(forced, serial, fields));
        const Golden::GoldenStore store = MakeStore(root, Golden::UpdateRequested());
        const Golden::GoldenResult r = store.Check(caseId, Inputs(fields));
        std::string text;
        for (const std::string& line : r.report) text += line + "\n";
        EXPECT_TRUE(r.outcome == Golden::GoldenOutcome::Match || r.outcome == Golden::GoldenOutcome::Updated) << text;
    }
}

TEST(WeatherGolden, Small52Weeks_MatchesGolden)
{
    CheckCase("pipeline/small/52w", false);
}

TEST(WeatherGolden, Small52WeeksForced_MatchesGolden)
{
    CheckCase("pipeline/small/52w-forced", true);
}

TEST(WeatherGolden, FixedRange17ExecutorMatchesSerialGolden)
{
    const fs::path root = FindRepoRoot();
    ASSERT_FALSE(root.empty());
    // Always compare (never update): the golden was recorded under the serial executor.
    const Golden::GoldenStore store = MakeStore(root, false);
    for (int forced = 0; forced < 2; ++forced)
    {
        WT::FixedRangeExecutor fixed(17);
        WeatherFields fields;
        ASSERT_TRUE(RunPipeline(forced != 0, fixed, fields));
        const Golden::GoldenResult r = store.Check(forced ? "pipeline/small/52w-forced" : "pipeline/small/52w", Inputs(fields));
        std::string text;
        for (const std::string& line : r.report) text += line + "\n";
        EXPECT_EQ(r.outcome, Golden::GoldenOutcome::Match) << text;
    }
}

TEST(WeatherGolden, UpdateModeIsNotActiveDuringNormalRuns)
{
    EXPECT_FALSE(Golden::UpdateRequested()) << "DIA_UPDATE_GOLDEN is set; unset it after re-baselining";
}
