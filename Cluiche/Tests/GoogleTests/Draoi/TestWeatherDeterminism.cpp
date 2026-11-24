// A4 weather ca-pipeline Task 10: purity, no-allocation and the determinism matrix.
// Spec: docs/specs/applications/cow/systems/a4-weather/ca-pipeline.md (ACs 2, 6, 29, 30, 31, 32)
#include <gtest/gtest.h>

#include <DiaParallelExecutor/SerialExecutor.h>
#include <DraoiHexWorld/BiomeRegistry.h>
#include <DraoiHexWorldWeather/Testing/WeatherTestExecutors.h>
#include <DraoiHexWorldWeather/Testing/WeatherTestFixtures.h>
#include <DraoiHexWorldWeather/WeatherSimulation.h>

#include <algorithm>
#include <limits>
#include <string.h>
#include <vector>

namespace
{
    using namespace Draoi::HexWorld;
    using namespace Draoi::HexWorld::Weather;
    namespace WT = Draoi::HexWorld::Weather::Testing;

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

    // Everything the caller owns and hands to Initialise / Capture, so a test can poison it.
    struct Scene
    {
        WeatherTerrain        terrain;
        WeatherParameters     params;
        Dia::Core::Containers::DynamicArray<WeatherForcing> forcings;   // the source list AddForcing is fed from
        Dia::Core::Containers::DynamicArray<uint32_t>       setCells;   // the source list RegisterCellSet is fed from
        SeasonalBaselineInput baseline;
        HydrologyInputs       hydro;
        InfluenceSnapshot     influences;
        uint32_t              n = 0;

        void Build(uint32_t w, uint32_t h, bool hydroPresent)
        {
            n = w * h;
            terrain = WeatherTerrain();
            terrain.width = w; terrain.height = h;
            terrain.elevation.Reserve(n); terrain.latitude.Reserve(n); terrain.waterSource.Reserve(n); terrain.biome.Reserve(n);
            Lcg rng(12345u + n);
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
            forcings = Dia::Core::Containers::DynamicArray<WeatherForcing>();
            setCells = Dia::Core::Containers::DynamicArray<uint32_t>();
            forcings.Reserve(8);
            setCells.Reserve(n);
            for (uint32_t c = 3; c < n; c += 7) setCells.Add(c);
            WeatherForcing f;
            f.kind = ForcingKind::PressureHold; f.cell = n / 2; f.value = 0.9f; f.weeksRemaining = 60; forcings.Add(f);
            f = WeatherForcing();
            f.kind = ForcingKind::MoistureHold; f.cellSet = 0; f.value = 0.8f; f.weeksRemaining = 60; forcings.Add(f);
            f = WeatherForcing();
            f.kind = ForcingKind::Rain; f.cell = n / 3; f.value = 0.4f; f.weeksRemaining = 10; forcings.Add(f);
            f = WeatherForcing();
            f.kind = ForcingKind::WindOverride; f.cell = n / 4; f.value = 1.0f; f.value2 = 0.7f; f.weeksRemaining = 0; forcings.Add(f);
            RefreshInputs(hydroPresent);
        }

        // The per-Capture inputs, rebuilt to their pristine values.
        void RefreshInputs(bool hydroPresent)
        {
            params = WT::MakeTestParams();
            baseline.temperature = Dia::Core::Containers::DynamicArray<float>();
            baseline.temperature.Reserve(n);
            Lcg rng(777u + n);
            for (uint32_t i = 0; i < n; ++i) baseline.temperature.Add(0.2f + 0.7f * rng.Next());
            hydro = HydrologyInputs();
            hydro.Resize(n);
            for (uint32_t i = 0; i < n; ++i)
            {
                hydro.surfaceWetness[i] = rng.Next();
                hydro.openWater[i] = rng.Next();
                hydro.glacierEligible[i] = rng.Next() > 0.7f ? 1 : 0;
            }
            hydro.present = hydroPresent;
        }

        // Overwrites every caller-owned input with garbage.
        void Poison()
        {
            memset(static_cast<void*>(&params), 0xFF, sizeof(params));
            const float nan = std::numeric_limits<float>::quiet_NaN();
            for (uint32_t i = 0; i < baseline.temperature.Size(); ++i) baseline.temperature[i] = nan;
            for (uint32_t i = 0; i < hydro.surfaceWetness.Size(); ++i) hydro.surfaceWetness[i] = nan;
            for (uint32_t i = 0; i < hydro.openWater.Size(); ++i) hydro.openWater[i] = nan;
            for (uint32_t i = 0; i < hydro.glacierEligible.Size(); ++i) hydro.glacierEligible[i] = 0xFF;
            hydro.present = !hydro.present;
            hydro.glacierCheckWeek = 0xDEADBEEFu;
            hydro.glacierSnowFloor = nan;
            for (uint32_t i = 0; i < forcings.Size(); ++i)
            {
                forcings[i].kind = ForcingKind::WindOverride;
                forcings[i].cell = 0xFFFFFFFFu;
                forcings[i].value = nan; forcings[i].value2 = nan; forcings[i].weeksRemaining = 0xFFFF;
            }
            forcings.RemoveAll();
            for (uint32_t i = 0; i < setCells.Size(); ++i) setCells[i] = 0xFFFFFFFFu;
            setCells.RemoveAll();
            for (uint32_t i = 0; i < terrain.elevation.Size(); ++i)
            {
                terrain.elevation[i] = nan; terrain.latitude[i] = nan;
                terrain.waterSource[i] = WaterSource::None; terrain.biome[i] = 0xFF;
            }
        }
    };

    // Initialise + cell set + forcings. Pass the baseline to Initialise only when useBaseline.
    bool Setup(WeatherSimulation& sim, const Scene& s, bool useBaseline)
    {
        if (!sim.Initialise(s.terrain, s.params, WT::TestRegistry(), useBaseline ? &s.baseline : nullptr)) return false;
        if (sim.RegisterCellSet(s.setCells) != 0) return false;
        for (uint32_t i = 0; i < s.forcings.Size(); ++i)
            if (!sim.AddForcing(s.forcings[i])) return false;
        return true;
    }

    Season SeasonOf(uint32_t week) { return static_cast<Season>((week / 13u) % 4u); }

    // The committed state worth comparing after a run.
    struct Outcome
    {
        WeatherFields committed;
        Dia::Core::Containers::DynamicArray<float> glacierInput;
        WeatherGridTotals totals;
        uint64_t week = 0;
    };

    void Snapshot(const WeatherSimulation& sim, Outcome& out)
    {
        WT::CloneFields(sim.Fields(), out.committed);
        const auto& g = sim.GetHandoff().glacierInput;
        out.glacierInput = Dia::Core::Containers::DynamicArray<float>();
        out.glacierInput.Reserve(g.Size());
        for (uint32_t i = 0; i < g.Size(); ++i) out.glacierInput.Add(g[i]);
        out.totals = sim.GetGridTotals();
        out.week = sim.GetCommittedWeek();
    }

    // Name of the first field array that differs, or nullptr.
    const char* FirstDifferingField(const WeatherFields& a, const WeatherFields& b)
    {
        if (!WT::SameFloats(a.moisture, b.moisture)) return "moisture";
        if (!WT::SameFloats(a.temperature, b.temperature)) return "temperature";
        if (!WT::SameFloats(a.pressure, b.pressure)) return "pressure";
        if (!WT::SameFloats(a.windDirection, b.windDirection)) return "windDirection";
        if (!WT::SameFloats(a.windSpeed, b.windSpeed)) return "windSpeed";
        if (!WT::SameFloats(a.precipitation, b.precipitation)) return "precipitation";
        if (!WT::SameFloats(a.cloudCover, b.cloudCover)) return "cloudCover";
        if (!WT::SameFloats(a.waterToGround, b.waterToGround)) return "waterToGround";
        if (!WT::SameFloats(a.snowPack, b.snowPack)) return "snowPack";
        if (!WT::SameFloats(a.pressureAnomaly, b.pressureAnomaly)) return "pressureAnomaly";
        return nullptr;
    }

    ::testing::AssertionResult Identical(const Outcome& a, const Outcome& b)
    {
        if (const char* f = FirstDifferingField(a.committed, b.committed))
            return ::testing::AssertionFailure() << "field array differs: " << f;
        if (!WT::SameFloats(a.glacierInput, b.glacierInput)) return ::testing::AssertionFailure() << "glacierInput differs";
        if (memcmp(&a.totals.meanMoisture, &b.totals.meanMoisture, sizeof(double)) != 0)
            return ::testing::AssertionFailure() << "meanMoisture differs";
        if (memcmp(&a.totals.totalSnowPack, &b.totals.totalSnowPack, sizeof(double)) != 0)
            return ::testing::AssertionFailure() << "totalSnowPack differs";
        if (a.totals.rainingCellCount != b.totals.rainingCellCount) return ::testing::AssertionFailure() << "rainingCellCount differs";
        if (a.week != b.week) return ::testing::AssertionFailure() << "week counter differs: " << a.week << " vs " << b.week;
        return ::testing::AssertionSuccess();
    }

    // ---- Purity (AC 2) ---------------------------------------------------------------------------------------------

    // Twin A runs clean; twin B poisons every caller-owned input between Capture and Run, every week.
    void RunPurity(bool hydroPresent, bool perCellBaseline)
    {
        const uint32_t w = 66, h = 42, weeks = 30;   // covers the week-26 firn check
        Scene clean, dirty;
        clean.Build(w, h, hydroPresent);
        WeatherSimulation simA, simB;
        ASSERT_TRUE(Setup(simA, clean, perCellBaseline));
        ASSERT_TRUE(Setup(simB, clean, perCellBaseline));

        Dia::ParallelExecutor::SerialExecutor serial;
        for (uint32_t week = 0; week < weeks; ++week)
        {
            simA.Step(clean.params, SeasonOf(week), week % 52, WT::TestRegistry(), perCellBaseline ? &clean.baseline : nullptr,
                      clean.hydro, clean.influences, serial);

            dirty.Build(w, h, hydroPresent);
            simB.Capture(dirty.params, SeasonOf(week), week % 52, WT::TestRegistry(), perCellBaseline ? &dirty.baseline : nullptr,
                         dirty.hydro, dirty.influences);
            dirty.Poison();
            dirty.influences = InfluenceSnapshot();
            simB.Run(serial);
            simB.Commit();
        }

        Outcome a, b;
        Snapshot(simA, a);
        Snapshot(simB, b);
        EXPECT_TRUE(Identical(a, b));
        EXPECT_EQ(a.week, uint64_t(weeks));
    }
}

TEST(WeatherPurity, PoisonedInputsAfterCaptureChangeNothing_HydrologyPresent_TableBaseline)
{
    RunPurity(true, false);
}

TEST(WeatherPurity, PoisonedInputsAfterCaptureChangeNothing_HydrologyAbsent_TableBaseline)
{
    RunPurity(false, false);
}

TEST(WeatherPurity, PoisonedInputsAfterCaptureChangeNothing_HydrologyPresent_PerCellBaseline)
{
    RunPurity(true, true);
}

TEST(WeatherPurity, PoisonedInputsAfterCaptureChangeNothing_HydrologyAbsent_PerCellBaseline)
{
    RunPurity(false, true);
}

// ---- No allocation (AC 6) ---------------------------------------------------------------------------------------

namespace
{
    struct Block
    {
        const void* data;
        uint32_t    capacity;
        bool operator<(const Block& o) const
        {
            return data != o.data ? std::less<const void*>()(data, o.data) : capacity < o.capacity;
        }
        bool operator==(const Block& o) const { return data == o.data && capacity == o.capacity; }
    };

    template <class T>
    void Note(std::vector<Block>& out, const Dia::Core::Containers::DynamicArray<T>& a)
    {
        Block b;
        b.data = a.Size() > 0 ? static_cast<const void*>(&a[0u]) : nullptr;
        b.capacity = a.Capacity();
        out.push_back(b);
    }

    void NoteFields(std::vector<Block>& out, const WeatherFields& f)
    {
        Note(out, f.moisture); Note(out, f.temperature); Note(out, f.pressure); Note(out, f.windDirection);
        Note(out, f.windSpeed); Note(out, f.precipitation); Note(out, f.cloudCover); Note(out, f.waterToGround);
        Note(out, f.snowPack); Note(out, f.pressureAnomaly);
    }

    // Every array reachable by Run (plus the stores Capture reads), as (data pointer, capacity), order-independent
    // because Commit swaps which buffer is committed.
    std::vector<Block> Blocks(const WeatherSimulation& sim)
    {
        std::vector<Block> out;
        NoteFields(out, sim.Fields());
        NoteFields(out, sim.GetWorkingFields());
        Note(out, sim.GetHandoff().glacierInput); Note(out, sim.GetHandoff().influenceWater);
        const GridTables& t = sim.Tables();
        Note(out, t.neighbours); Note(out, t.moistureOut); Note(out, t.heatOut); Note(out, t.anomalyOut);
        Note(out, t.dirIndex); Note(out, t.dTemperature); Note(out, t.dMoisture); Note(out, t.claimed);
        const CapturedInputs& c = sim.Captured();
        Note(out, c.seasonBaseline); Note(out, c.orographicFactor); Note(out, c.forcings); Note(out, c.cellSets.cells);
        Note(out, c.hydrology.surfaceWetness); Note(out, c.hydrology.openWater); Note(out, c.hydrology.glacierEligible);
        Note(out, c.climate.tempOffset); Note(out, c.climate.evapMultiplier); Note(out, c.climate.windShift);
        const WeatherTerrain& te = sim.Terrain();
        Note(out, te.elevation); Note(out, te.latitude); Note(out, te.waterSource); Note(out, te.biome);
        Note(out, sim.LiveForcings()); Note(out, sim.LiveCellSets().cells); Note(out, sim.PendingOrographic());
        std::sort(out.begin(), out.end());
        return out;
    }

    void RunNoAllocation(bool hydroPresent, bool perCellBaseline)
    {
        Scene s;
        s.Build(66, 42, hydroPresent);
        WeatherSimulation sim;
        ASSERT_TRUE(Setup(sim, s, perCellBaseline));
        Dia::ParallelExecutor::SerialExecutor serial;

        // Baseline taken after the first Capture, when every latched store has been written once.
        sim.Capture(s.params, SeasonOf(0), 0, WT::TestRegistry(), perCellBaseline ? &s.baseline : nullptr, s.hydro, s.influences);
        const std::vector<Block> before = Blocks(sim);
        ASSERT_FALSE(before.empty());
        sim.Run(serial);
        EXPECT_TRUE(Blocks(sim) == before) << "an array was reallocated by Run";
        sim.Commit();
        EXPECT_TRUE(Blocks(sim) == before) << "an array was reallocated by Commit";

        for (uint32_t week = 1; week < 52; ++week)
        {
            sim.Step(s.params, SeasonOf(week), week % 52, WT::TestRegistry(), perCellBaseline ? &s.baseline : nullptr, s.hydro,
                     s.influences, serial);
            ASSERT_TRUE(Blocks(sim) == before) << "an array was reallocated by week " << week;
        }
        EXPECT_EQ(sim.GetCommittedWeek(), 52u);
    }
}

TEST(WeatherPurity, RunAndStepNeverReallocateAnyArray_HydrologyPresent)
{
    RunNoAllocation(true, false);
}

TEST(WeatherPurity, RunAndStepNeverReallocateAnyArray_HydrologyAbsentPerCellBaseline)
{
    RunNoAllocation(false, true);
}

// ---- Determinism matrix (ACs 29-32) -----------------------------------------------------------------------------

namespace
{
    enum class Kind { Serial, Fixed, Reverse };
    struct Variant { Kind kind; uint32_t n; };   // n == 0 on Reverse = the executor's default range count

    // One full 52-week run; fills `out` from the committed state.
    void RunMatrixCase(uint32_t w, uint32_t h, const Variant& v, Outcome& out)
    {
        Scene s;
        s.Build(w, h, true);
        WeatherSimulation sim;
        ASSERT_TRUE(Setup(sim, s, false));
        Dia::ParallelExecutor::SerialExecutor serial;
        WT::FixedRangeExecutor fixed(v.n);
        WT::ReverseRangeExecutor reverse(v.n);
        Dia::ParallelExecutor::IParallelExecutor* exec = &serial;
        if (v.kind == Kind::Fixed) exec = &fixed;
        else if (v.kind == Kind::Reverse) exec = &reverse;
        for (uint32_t week = 0; week < 52; ++week)
            sim.Step(s.params, SeasonOf(week), week % 52, WT::TestRegistry(), (week % 2 == 0) ? nullptr : &s.baseline, s.hydro,
                     s.influences, *exec);
        Snapshot(sim, out);
    }

    // The reduction the finish pass promises, redone independently in cell order from the committed fields.
    void ExpectTotalsMatchReference(const Outcome& o)
    {
        double moisture = 0.0, snow = 0.0;
        uint32_t raining = 0;
        const uint32_t n = o.committed.CellCount();
        for (uint32_t c = 0; c < n; ++c)
        {
            moisture += double(o.committed.moisture[c]);
            snow += double(o.committed.snowPack[c]);
            if (o.committed.precipitation[c] > 0.0f) ++raining;
        }
        const double mean = moisture / double(n);
        EXPECT_EQ(0, memcmp(&mean, &o.totals.meanMoisture, sizeof(double)));
        EXPECT_EQ(0, memcmp(&snow, &o.totals.totalSnowPack, sizeof(double)));
        EXPECT_EQ(raining, o.totals.rainingCellCount);
    }

    void RunMatrix(uint32_t w, uint32_t h, const std::vector<Variant>& variants)
    {
        const uint32_t n = w * h;
        Outcome reference;
        RunMatrixCase(w, h, Variant{ Kind::Serial, 0 }, reference);
        ASSERT_EQ(reference.committed.CellCount(), n);
        EXPECT_EQ(reference.week, 52u);
        ExpectTotalsMatchReference(reference);

        // The run is not trivially constant: weather has actually moved.
        EXPECT_GT(reference.totals.rainingCellCount, 0u);

        // AC 29: the same inputs twice.
        Outcome again;
        RunMatrixCase(w, h, Variant{ Kind::Serial, 0 }, again);
        EXPECT_TRUE(Identical(reference, again)) << "serial rerun";

        for (const Variant& v : variants)
        {
            Outcome o;
            RunMatrixCase(w, h, v, o);
            const uint32_t count = v.n == 0 ? 0u : (v.n == 0xFFFFFFFFu ? n : v.n);
            EXPECT_TRUE(Identical(reference, o)) << (v.kind == Kind::Fixed ? "fixed " : "reverse ") << count;
            ExpectTotalsMatchReference(o);
        }
    }
}

TEST(WeatherDeterminism, Small_AllExecutorVariantsMatchSerial_After52Weeks)
{
    const uint32_t n = 66 * 42;
    RunMatrix(66, 42, {
        { Kind::Fixed, 1 }, { Kind::Fixed, 3 }, { Kind::Fixed, 17 }, { Kind::Fixed, n },
        { Kind::Reverse, 0 }, { Kind::Reverse, 17 }, { Kind::Reverse, n } });
}

TEST(WeatherDeterminism, Large_SelectedExecutorVariantsMatchSerial_After52Weeks)
{
    const uint32_t n = 104 * 64;
    RunMatrix(104, 64, { { Kind::Fixed, 17 }, { Kind::Reverse, n } });
}
