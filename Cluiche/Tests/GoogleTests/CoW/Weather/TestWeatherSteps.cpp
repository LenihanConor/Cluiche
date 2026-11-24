// A4 WeatherModule Task 3: LatchInputs / RunStep / PublishCommit through the real stage (ACs 11-17, 24, 25, 27, 31, 32,
// 36 and the A4-only half of 33). The reference is always a direct WeatherSimulation Initialise + Step loop with the
// calendar written out by hand.
#include <gtest/gtest.h>

#include <string.h>
#include <functional>
#include <thread>
#include <vector>

#include <DiaObservation/Log/ISink.h>
#include <DiaObservation/Log/LogEntry.h>
#include <DiaObservation/Log/Logger.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaParallelExecutor/SerialExecutor.h>
#include <DraoiHexWorld/BiomeRegistry.h>
#include <DraoiHexWorldWeather/Testing/WeatherTestFixtures.h>
#include <Shared/CoWCalendar.h>
#include <Shared/Weather/WeatherStage.h>

#include "WeatherTestWorld.h"

namespace
{
using CoW::Weather::IWeatherWorldSource;
using CoW::Weather::WeatherStage;
using Draoi::HexWorld::Weather::HydrologyInputs;
using Draoi::HexWorld::Weather::InfluenceSnapshot;
using Draoi::HexWorld::Weather::Season;
using Draoi::HexWorld::Weather::SeasonalBaselineInput;
using Draoi::HexWorld::Weather::WeatherSimulation;
using Draoi::HexWorld::Weather::WeatherTerrain;
namespace WT = Draoi::HexWorld::Weather::Testing;

constexpr uint32_t kSpinUpWeeks = 6;
constexpr uint32_t kCells = 48;   // 8 x 6

class FakeWorldSource : public IWeatherWorldSource
{
public:
    bool GetWorld(CoW::WorldStep::WorldView& view, WeatherTerrain& terrain) override
    {
        if (!hasWorld) { return false; }
        WT::HexTerrainOptions o;
        o.waterColumns = 2;
        o.latitudeRamp = true;
        WeatherTestWorld::CopyTerrain(WT::MakeHexTerrain(width, height, o), terrain);
        view.present = true;
        view.width = width;
        view.height = height;
        return true;
    }
    uint32_t width = 8, height = 6;
    bool hasWorld = true;
};

struct FakeHold : public CoW::WorldStep::IGenerationHold
{
    void Hold() override {}
    void Release() override {}
};

class WthSink : public Dia::Observation::Log::ISink
{
public:
    void OnLogEntry(const Dia::Observation::Log::LogEntry& e) override
    {
        if (strstr(e.message, "WTH") == nullptr) { return; }
        if (e.level == Dia::Observation::Log::LogLevel::kError) { ++errors; }
        if (e.level == Dia::Observation::Log::LogLevel::kInfo && strstr(e.message, "baseline provider") != nullptr) { ++baselineInfos; }
    }
    const char* GetName() const override { return "WeatherStepsSink"; }
    unsigned errors = 0, baselineInfos = 0;
};

// Baseline provider: value = values[biome] + 0.01 * season. Records whether any call came from another thread.
class TestBaseline : public CoW::Weather::IBiomeSeasonalBaseline
{
public:
    TestBaseline() : owner(std::this_thread::get_id())
    {
        for (uint32_t b = 0; b < 32; ++b) { values[b] = 0.3f + 0.02f * static_cast<float>(b); }
    }
    float GetBaseline(uint32_t biome, Season season) const override
    {
        ++calls;
        if (std::this_thread::get_id() != owner) { wrongThread = true; }
        return values[biome < 32 ? biome : 0] + 0.01f * static_cast<float>(season);
    }
    float values[32];
    std::thread::id owner;
    mutable uint32_t calls = 0;
    mutable bool wrongThread = false;
};

class TestHydro : public CoW::Weather::IHydrologyInputsProvider
{
public:
    TestHydro() : owner(std::this_thread::get_id()) {}
    bool GetInputs(HydrologyInputs& out) override
    {
        ++calls;
        if (std::this_thread::get_id() != owner) { wrongThread = true; }
        Fill(out, wetness);
        return result;
    }
    static void Fill(HydrologyInputs& out, float wet)
    {
        out.Resize(kCells);
        for (uint32_t i = 0; i < kCells; ++i)
        {
            out.surfaceWetness[i] = wet;
            out.openWater[i] = (i % 8 < 2) ? 1.0f : 0.1f;
            out.glacierEligible[i] = (i % 5 == 0) ? 1 : 0;
        }
        out.present = true;
    }
    float wetness = 0.4f;
    bool result = true;
    std::thread::id owner;
    uint32_t calls = 0;
    bool wrongThread = false;
};

// Delegates to a serial executor; runs a callback in the middle of the first ParallelFor of the step.
class ProbeExecutor : public Dia::ParallelExecutor::IParallelExecutor
{
public:
    void ParallelFor(uint32_t count, uint32_t minRange, const Dia::ParallelExecutor::RangeFn& fn) override
    {
        if (onFirstFor) { std::function<void()> cb = onFirstFor; onFirstFor = nullptr; cb(); }
        mInner.ParallelFor(count, minRange, fn);
    }
    uint32_t GetLastRangeCount() const override { return mInner.GetLastRangeCount(); }
    std::function<void()> onFirstFor;
private:
    Dia::ParallelExecutor::SerialExecutor mInner;
};

Season SeasonOfDay(uint64_t day) { return static_cast<Season>((day / (7u * 13u)) % 4u); }
uint32_t WeekOfYear(uint64_t day) { return static_cast<uint32_t>((day / 7u) % 52u); }

// The reference run: spin-up then live steps with the calendar written out by hand.
struct Reference
{
    explicit Reference(uint32_t weeks = kSpinUpWeeks)
    {
        FakeWorldSource src;
        CoW::WorldStep::WorldView view;
        src.GetWorld(view, terrain);
        params = WT::MakeTestParams();
        EXPECT_TRUE(sim.Initialise(terrain, params, WT::TestRegistry(), nullptr));
        for (uint32_t i = 0; i < weeks; ++i)
        {
            sim.Step(params, static_cast<Season>((i / 13u) % 4u), i % 52u, WT::TestRegistry(), nullptr, noHydro, noInfl, exec);
        }
    }
    void Live(uint64_t day, const SeasonalBaselineInput* baseline = nullptr, const HydrologyInputs* hydro = nullptr)
    {
        sim.Step(params, SeasonOfDay(day), WeekOfYear(day), WT::TestRegistry(), baseline, hydro ? *hydro : noHydro, noInfl, exec);
        sim.SetCommittedWeek(day / 7u);
    }
    SeasonalBaselineInput BaselineFor(const TestBaseline& p, Season season) const
    {
        SeasonalBaselineInput in;
        in.temperature.Reserve(terrain.biome.Size());
        for (uint32_t i = 0; i < terrain.biome.Size(); ++i)
        {
            const float v = p.GetBaseline(static_cast<uint32_t>(terrain.biome[i]), season);
            in.temperature.Add(v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v));
        }
        return in;
    }
    WeatherTerrain terrain;
    Draoi::HexWorld::Weather::WeatherParameters params;
    WeatherSimulation sim;
    HydrologyInputs noHydro;
    InfluenceSnapshot noInfl;
    Dia::ParallelExecutor::SerialExecutor exec;
};

struct Published
{
    uint64_t week;
    Season season;
    uint64_t committedWeekSeen;
};

uint64_t DiscardedCount()
{
    auto* c = Dia::Observation::Metric::MetricRegistry::Instance().FindCounter(
        Dia::Core::StringCRC(CoW::World::MetricName(CoW::World::kWeather, "discarded_steps").c_str()));
    return c != nullptr ? c->Value() : 0;
}

class WeatherStepsTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        auto& l = Dia::Observation::Log::Logger::Instance();
        l.RegisterThreadBuffer();
        l.RegisterSink(&sink);
        stage.SetWorldSource(&source);
        stage.SetRegistry(&WT::TestRegistry());
        stage.SetParameters(WT::MakeTestParams());
        stage.SetSpinupWeeks(kSpinUpWeeks);
        stage.SetGenerationHold(&hold);
        stage.SetLaunchMode(WeatherStage::LaunchMode::kManual);
        stage.SetPublishSeam([this](uint64_t week, Season season) {
            Published p;
            p.week = week;
            p.season = season;
            p.committedWeekSeen = stage.GetSnapshot().week;
            published.push_back(p);
        });
        stage.Start();
    }
    void TearDown() override
    {
        stage.Stop();
        auto& l = Dia::Observation::Log::Logger::Instance();
        l.FlushSync();
        l.UnregisterSink(&sink);
        l.UnregisterThreadBuffer();
    }
    void SpinUp(uint32_t revision)
    {
        stage.OnWorldComplete(revision);
        while (stage.RunNextJob()) {}
        stage.Update();
    }
    void Step(uint64_t day)
    {
        stage.Capture(static_cast<Dia::DeferredStep::StepKey>(day));
        stage.Run(exec);
        stage.Commit();
    }
    unsigned Errors()
    {
        Dia::Observation::Log::Logger::Instance().FlushSync();
        return sink.errors;
    }
    unsigned BaselineInfos()
    {
        Dia::Observation::Log::Logger::Instance().FlushSync();
        return sink.baselineInfos;
    }
    bool Matches(const Reference& ref)
    {
        const CoW::Weather::WeatherSnapshot snap = stage.GetSnapshot();
        return snap.fields != nullptr && WT::SameFields(ref.sim.Fields(), *snap.fields) && ref.sim.GetCommittedWeek() == snap.week;
    }
    bool HealthIs(Dia::Observation::Health::HealthStatus status) const { return stage.GetHealth().status == status; }

    FakeWorldSource source;
    FakeHold hold;
    WthSink sink;
    WeatherStage stage;
    Dia::ParallelExecutor::SerialExecutor exec;
    std::vector<Published> published;
};
}

// AC 11
TEST_F(WeatherStepsTest, StepDaysMapToSeasonsAndPublishTheirWeek)
{
    SpinUp(1);
    ASSERT_TRUE(stage.IsReadyForRevision(1));
    for (uint32_t s = 0; s < 4; ++s)
    {
        const uint64_t day = s * CoW::kDaysPerSeason;
        Step(day);
        ASSERT_EQ(s + 1u, published.size());
        EXPECT_EQ(static_cast<Season>(s), published[s].season);
        EXPECT_EQ(CoW::WeekOf(day), published[s].week);
        EXPECT_EQ(CoW::WeekOf(day), stage.GetSnapshot().week);
    }
}

// Task 2 left open: the first live step's season comes from its step day, not from the spin-up.
TEST_F(WeatherStepsTest, FirstLiveStepSeasonComesFromItsStepDay)
{
    SpinUp(1);
    const uint64_t day = 2 * CoW::kDaysPerSeason;   // autumn, week-of-year 26
    Step(day);
    Reference ref;
    ref.Live(day);
    EXPECT_TRUE(Matches(ref));
    ASSERT_EQ(1u, published.size());
    EXPECT_EQ(Season::Autumn, published[0].season);
}

// AC 12
TEST_F(WeatherStepsTest, RetriedStepWithEarlierDayUsesThatDaysWeekAndSeason)
{
    SpinUp(1);
    stage.Capture(static_cast<Dia::DeferredStep::StepKey>(300));   // captured, then retried with an earlier represented day
    const uint64_t earlier = 100;
    stage.Capture(static_cast<Dia::DeferredStep::StepKey>(earlier));
    stage.Run(exec);
    stage.Commit();
    ASSERT_EQ(1u, published.size());
    EXPECT_EQ(CoW::WeekOf(earlier), published[0].week);
    EXPECT_EQ(Season::Summer, published[0].season);
    Reference ref;
    ref.Live(earlier);
    EXPECT_TRUE(Matches(ref));
}

// AC 13
TEST_F(WeatherStepsTest, OneMessagePerAppliedStepAfterCommitWithCommittedWeekSet)
{
    SpinUp(1);
    for (uint32_t i = 0; i < 3; ++i)
    {
        Step(i * CoW::kDaysPerWeek);
        ASSERT_EQ(i + 1u, published.size());
        EXPECT_EQ(CoW::WeekOf(i * CoW::kDaysPerWeek), published[i].committedWeekSeen);
    }
}

TEST_F(WeatherStepsTest, NoMessageDuringSpinUp)
{
    SpinUp(1);
    EXPECT_TRUE(published.empty());
}

// AC 14
TEST_F(WeatherStepsTest, WorldRegeneratedBeforeCommitDiscardsTheStepWithoutPublishing)
{
    SpinUp(1);
    const WeatherSimulation* oldSim = stage.ReadableSim();
    ASSERT_NE(nullptr, oldSim);
    const uint64_t weekBefore = oldSim->GetCommittedWeek();
    const uint64_t discardedBefore = DiscardedCount();
    stage.Capture(static_cast<Dia::DeferredStep::StepKey>(50));
    stage.Run(exec);
    stage.OnWorldComplete(2);   // world r+1 arrives mid-step
    stage.Commit();
    EXPECT_TRUE(published.empty());
    EXPECT_EQ(weekBefore, oldSim->GetCommittedWeek());
    EXPECT_EQ(discardedBefore + 1, DiscardedCount());
    while (stage.RunNextJob()) {}
    stage.Update();
    ASSERT_TRUE(stage.IsReadyForRevision(2));
    Reference ref;   // the new world, spun up only
    EXPECT_TRUE(Matches(ref));
}

// AC 15
TEST_F(WeatherStepsTest, PromotionOnlyInsideCapture)
{
    SpinUp(1);
    ProbeExecutor probe;
    const WeatherSimulation* before = stage.ReadableSim();
    const WeatherSimulation* during = nullptr;
    probe.onFirstFor = [&]() {
        stage.OnWorldComplete(2);        // world event
        while (stage.RunNextJob()) {}    // spin-up finishes
        stage.Update();                  // result becomes pending, never active
        during = stage.ReadableSim();
    };
    stage.Capture(static_cast<Dia::DeferredStep::StepKey>(7));
    stage.Run(probe);
    EXPECT_TRUE(stage.HasPending());
    EXPECT_NE(nullptr, during);
    EXPECT_NE(before, during);   // the stepped sim was not swapped for the new one
    stage.Commit();
    EXPECT_TRUE(published.empty());   // discarded: revision changed
    EXPECT_TRUE(stage.HasPending());  // still not promoted
    stage.Capture(static_cast<Dia::DeferredStep::StepKey>(14));   // promotion happens here
    EXPECT_FALSE(stage.HasPending());
    stage.Run(exec);
    stage.Commit();
    EXPECT_EQ(1u, published.size());
}

// AC 16
TEST_F(WeatherStepsTest, RunIgnoresParametersQueueAndTerrainChangedAfterCapture)
{
    SpinUp(1);
    stage.Capture(static_cast<Dia::DeferredStep::StepKey>(21));
    stage.SetParameters(Draoi::HexWorld::Weather::WeatherParameters());   // all zero
    CoW::Weather::WeatherEntry e;
    e.kind = CoW::Weather::EntryKind::kSetParam;
    strcpy_s(e.key, "cloud.decay");
    e.value = 9.0f;
    EXPECT_TRUE(stage.Enqueue(e));
    source.width = 3;
    source.height = 3;
    stage.Run(exec);
    stage.Commit();
    Reference ref;
    ref.Live(21);
    EXPECT_TRUE(Matches(ref));
}

// AC 17
TEST_F(WeatherStepsTest, RunNeverCallsTheBaselineProvider)
{
    TestBaseline provider;
    stage.SetBaselineProvider(&provider);
    SpinUp(1);
    stage.Capture(static_cast<Dia::DeferredStep::StepKey>(0));
    const uint32_t callsAfterCapture = provider.calls;
    EXPECT_GT(callsAfterCapture, 0u);
    std::thread worker([&]() { stage.Run(exec); });
    worker.join();
    EXPECT_EQ(callsAfterCapture, provider.calls);
    EXPECT_FALSE(provider.wrongThread);
    stage.Commit();
}

// AC 24
TEST_F(WeatherStepsTest, NoBaselineProviderIsDegradedWithOneInfoLogAndUsesTheTable)
{
    SpinUp(1);
    ASSERT_TRUE(HealthIs(Dia::Observation::Health::HealthStatus::kOK));
    Step(0);
    Step(7);
    EXPECT_TRUE(HealthIs(Dia::Observation::Health::HealthStatus::kDegraded));
    EXPECT_EQ(Dia::Core::StringCRC(CoW::Weather::kCauseFallbackBaseline), stage.GetHealth().reason);
    EXPECT_EQ(1u, BaselineInfos());
    EXPECT_TRUE(stage.IsReady());   // Degraded still steps
    Reference ref;
    ref.Live(0);
    ref.Live(7);
    EXPECT_TRUE(Matches(ref));
}

TEST_F(WeatherStepsTest, SettingTheBaselineProviderIsUsedNextCaptureAndHealthRecovers)
{
    SpinUp(1);
    Step(0);
    ASSERT_TRUE(HealthIs(Dia::Observation::Health::HealthStatus::kDegraded));
    TestBaseline provider;
    stage.SetBaselineProvider(&provider);
    Step(7);
    EXPECT_TRUE(HealthIs(Dia::Observation::Health::HealthStatus::kOK));
    EXPECT_GT(provider.calls, 0u);
    Reference ref;
    ref.Live(0);
    const SeasonalBaselineInput in = ref.BaselineFor(provider, SeasonOfDay(7));
    ref.Live(7, &in);
    EXPECT_TRUE(Matches(ref));
}

// AC 25
TEST_F(WeatherStepsTest, OutOfRangeBaselineIsClampedLoggedOncePerBiomeSeasonAndDegraded)
{
    TestBaseline provider;
    provider.values[2] = 1.5f;    // the terrain's biome
    provider.values[5] = -0.5f;
    stage.SetBaselineProvider(&provider);
    SpinUp(1);
    const unsigned errorsBefore = Errors();
    Step(0);
    EXPECT_EQ(errorsBefore + 2u, Errors());   // biome 2 and 5, spring
    EXPECT_TRUE(HealthIs(Dia::Observation::Health::HealthStatus::kDegraded));
    EXPECT_EQ(Dia::Core::StringCRC(CoW::Weather::kCauseBaselineClamped), stage.GetHealth().reason);
    Step(7);   // same season: no new errors
    EXPECT_EQ(errorsBefore + 2u, Errors());
    Step(CoW::kDaysPerSeason);   // summer: new pairs
    EXPECT_EQ(errorsBefore + 4u, Errors());

    Reference ref;
    SeasonalBaselineInput spring = ref.BaselineFor(provider, Season::Spring);
    ref.Live(0, &spring);
    ref.Live(7, &spring);
    SeasonalBaselineInput summer = ref.BaselineFor(provider, Season::Summer);
    ref.Live(CoW::kDaysPerSeason, &summer);
    EXPECT_TRUE(Matches(ref));
}

// AC 27
TEST_F(WeatherStepsTest, ReaderInTheHandlerSeesThatStepsCommittedState)
{
    SpinUp(1);
    Reference ref;
    ref.Live(35);
    bool checked = false;
    bool same = false;
    stage.SetPublishSeam([&](uint64_t week, Season) {
        const CoW::Weather::WeatherSnapshot snap = stage.GetSnapshot();
        same = snap.fields != nullptr && WT::SameFields(ref.sim.Fields(), *snap.fields) && snap.week == week;
        checked = true;
    });
    Step(35);
    EXPECT_TRUE(checked);
    EXPECT_TRUE(same);
}

// AC 31
TEST_F(WeatherStepsTest, ProviderArraysAreLatchedOnceAndRunIgnoresLaterProviderChanges)
{
    TestBaseline baseline;
    TestHydro hydro;
    stage.SetBaselineProvider(&baseline);
    stage.SetHydrologyProvider(&hydro);
    SpinUp(1);
    stage.Capture(static_cast<Dia::DeferredStep::StepKey>(3));
    EXPECT_EQ(1u, hydro.calls);
    const uint32_t baselineCalls = baseline.calls;
    for (uint32_t b = 0; b < 32; ++b) { baseline.values[b] = 0.99f; }   // poison after Capture
    hydro.wetness = 0.0f;
    std::thread worker([&]() { stage.Run(exec); });
    worker.join();
    stage.Commit();
    EXPECT_EQ(1u, hydro.calls);
    EXPECT_EQ(baselineCalls, baseline.calls);
    EXPECT_FALSE(hydro.wrongThread);
    EXPECT_FALSE(baseline.wrongThread);

    Reference ref;
    TestBaseline clean;
    const SeasonalBaselineInput in = ref.BaselineFor(clean, SeasonOfDay(3));
    HydrologyInputs h;
    TestHydro::Fill(h, 0.4f);
    ref.Live(3, &in, &h);
    EXPECT_TRUE(Matches(ref));
}

// AC 32
TEST_F(WeatherStepsTest, NoHydrologyProviderIsNotDegradedAndMatchesADirectNoHydrologyRun)
{
    TestBaseline baseline;   // isolates the hydrology question from the baseline fallback
    stage.SetBaselineProvider(&baseline);
    SpinUp(1);
    Step(0);
    EXPECT_TRUE(HealthIs(Dia::Observation::Health::HealthStatus::kOK));
    Reference ref;
    const SeasonalBaselineInput in = ref.BaselineFor(baseline, Season::Spring);
    ref.Live(0, &in);
    EXPECT_TRUE(Matches(ref));
}

TEST_F(WeatherStepsTest, HydrologyProviderWithNothingToGiveBehavesAsAbsent)
{
    TestBaseline baseline;
    TestHydro hydro;
    hydro.result = false;
    stage.SetBaselineProvider(&baseline);
    stage.SetHydrologyProvider(&hydro);
    SpinUp(1);
    Step(0);
    EXPECT_EQ(1u, hydro.calls);
    EXPECT_TRUE(HealthIs(Dia::Observation::Health::HealthStatus::kOK));
    Reference ref;
    const SeasonalBaselineInput in = ref.BaselineFor(baseline, Season::Spring);
    ref.Live(0, &in);
    EXPECT_TRUE(Matches(ref));
}

// AC 36
TEST_F(WeatherStepsTest, ReadyForRevisionOnlyForTheTargetOnceSpunUp)
{
    stage.OnWorldComplete(5);
    EXPECT_FALSE(stage.IsReadyForRevision(5));   // spin-up queued
    while (stage.RunNextJob()) {}
    stage.Update();
    EXPECT_TRUE(stage.IsReadyForRevision(5));
    EXPECT_FALSE(stage.IsReadyForRevision(6));
    stage.OnWorldComplete(6);
    EXPECT_FALSE(stage.IsReadyForRevision(6));   // spinning up again
    EXPECT_FALSE(stage.IsReadyForRevision(5));
}

// AC 33, A4-only half: 52 weekly steps through the real stage equal a direct Initialise + Step loop bit for bit.
TEST_F(WeatherStepsTest, FiftyTwoWeeksThroughTheStageMatchADirectSimLoop)
{
    SpinUp(1);
    Reference ref;
    for (uint32_t w = 0; w < 52; ++w)
    {
        const uint64_t day = static_cast<uint64_t>(w) * CoW::kDaysPerWeek;
        Step(day);
        ref.Live(day);
        ASSERT_TRUE(Matches(ref)) << "week " << w;
    }
    ASSERT_EQ(52u, published.size());
    EXPECT_EQ(Season::Winter, published[51].season);
    EXPECT_EQ(51u, published[51].week);
}
