// A4 WeatherModule Task 2: world ingest and spin-up (ACs 4, 5, 6, 7, 8, 9, 10, 26). Terrain is hand-built through a fake
// world source, so no WorldGenerationModule is involved. The stage's launch mode makes job timing deterministic.
#include <gtest/gtest.h>

#include <string.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <DiaThreading/JobSystem.h>

#include <DiaObservation/Log/ISink.h>
#include <DiaObservation/Log/LogEntry.h>
#include <DiaObservation/Log/Logger.h>
#include <DiaParallelExecutor/SerialExecutor.h>
#include <DraoiHexWorld/BiomeRegistry.h>
#include <DraoiHexWorldWeather/Testing/WeatherTestFixtures.h>
#include <Shared/CoWCalendar.h>
#include <Shared/Weather/WeatherStage.h>
#include <Shared/Weather/WeatherTerrainBuilder.h>

#include "WeatherTestWorld.h"

namespace
{
using CoW::Weather::CauseCode;
using CoW::Weather::IWeatherWorldSource;
using CoW::Weather::WeatherStage;
using Draoi::HexWorld::Weather::HydrologyInputs;
using Draoi::HexWorld::Weather::InfluenceSnapshot;
using Draoi::HexWorld::Weather::Season;
using Draoi::HexWorld::Weather::WeatherSimulation;
using Draoi::HexWorld::Weather::WeatherTerrain;
namespace WT = Draoi::HexWorld::Weather::Testing;

// Hand-built world: every announced revision gets a width x height terrain with `waterColumns` ocean columns.
class FakeWorldSource : public IWeatherWorldSource
{
public:
    bool GetWorld(CoW::WorldStep::WorldView& view, WeatherTerrain& terrain) override
    {
        ++reads;
        if (!hasWorld) { return false; }
        WT::HexTerrainOptions o;
        o.waterColumns = waterColumns;
        o.latitudeRamp = true;
        WeatherTestWorld::CopyTerrain(WT::MakeHexTerrain(width, height, o), terrain);
        view.present = true;
        view.width = width;
        view.height = height;
        return true;
    }
    uint32_t width = 8, height = 6, waterColumns = 2, reads = 0;
    bool hasWorld = true;
};

struct FakeHold : public CoW::WorldStep::IGenerationHold
{
    void Hold() override { ++holds; }
    void Release() override { ++releases; }
    uint32_t holds = 0, releases = 0;
};

class ErrorSink : public Dia::Observation::Log::ISink
{
public:
    void OnLogEntry(const Dia::Observation::Log::LogEntry& e) override
    {
        if (strstr(e.message, "WTH") == nullptr) { return; }
        if (e.level == Dia::Observation::Log::LogLevel::kError) { ++errors; }
    }
    const char* GetName() const override { return "WeatherSpinUpSink"; }
    unsigned errors = 0;
};

// The reference: a direct Initialise + Step loop with the calendar written out by hand (13 weeks a season, 52 a year).
void ReferenceSpinUp(const WeatherTerrain& terrain, uint32_t weeks, WeatherSimulation& out)
{
    Dia::ParallelExecutor::SerialExecutor exec;
    const HydrologyInputs hydro;
    const InfluenceSnapshot influences;
    const auto params = WT::MakeTestParams();
    ASSERT_TRUE(out.Initialise(terrain, params, WT::TestRegistry(), nullptr));
    for (uint32_t i = 0; i < weeks; ++i)
    {
        out.Step(params, static_cast<Season>((i / 13u) % 4u), i % 52u, WT::TestRegistry(), nullptr, hydro, influences, exec);
    }
}

class WeatherSpinUpTest : public ::testing::Test
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
        stage.SetSpinupWeeks(6);
        stage.SetGenerationHold(&hold);
        stage.SetLaunchMode(WeatherStage::LaunchMode::kManual);
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
    unsigned Errors()
    {
        Dia::Observation::Log::Logger::Instance().FlushSync();
        return sink.errors;
    }
    // Announce a revision and run its job to completion, then poll.
    void SpinUp(uint32_t revision)
    {
        stage.OnWorldComplete(revision);
        while (stage.RunNextJob()) {}
        stage.Update();
    }

    FakeWorldSource source;
    FakeHold hold;
    ErrorSink sink;
    WeatherStage stage;
};
}

// AC 4
TEST_F(WeatherSpinUpTest, ReadyForRevisionOnlyOnceItsSpinUpCompleted)
{
    stage.OnWorldComplete(3);
    EXPECT_FALSE(stage.IsReadyForRevision(3));   // job queued, not run
    EXPECT_EQ(1u, stage.QueuedJobCount());
    ASSERT_TRUE(stage.RunNextJob());
    EXPECT_FALSE(stage.IsReadyForRevision(3));   // result not yet taken on SimPU
    stage.Update();
    EXPECT_TRUE(stage.IsReadyForRevision(3));
    EXPECT_FALSE(stage.IsReadyForRevision(4));
    EXPECT_FALSE(stage.IsReadyForRevision(2));
}

// AC 5
TEST_F(WeatherSpinUpTest, NoWaterSourceIsFailingWithCauseAndNoResult)
{
    source.waterColumns = 0;
    SpinUp(1);
    EXPECT_FALSE(stage.IsReady());
    EXPECT_FALSE(stage.IsReadyForRevision(1));
    EXPECT_EQ(Dia::Observation::Health::HealthStatus::kFailing, stage.GetHealth().status);
    EXPECT_EQ(static_cast<uint32_t>(CauseCode::kNoWaterSource), stage.GetCauseCode());
    EXPECT_FALSE(stage.HasPending());
    EXPECT_FALSE(stage.HasActive());
    EXPECT_GE(Errors(), 1u);
    EXPECT_EQ(0u, stage.GetSnapshot().cellCount);
}

TEST_F(WeatherSpinUpTest, NoWaterSourceThenWorldWithWaterRecovers)
{
    source.waterColumns = 0;
    SpinUp(1);
    ASSERT_FALSE(stage.IsReady());
    source.waterColumns = 2;
    SpinUp(2);
    EXPECT_TRUE(stage.IsReadyForRevision(2));
    EXPECT_EQ(Dia::Observation::Health::HealthStatus::kOK, stage.GetHealth().status);
}

// AC 6
TEST_F(WeatherSpinUpTest, SecondCompletionBeforeJobRunsOnlyNewestBecomesReady)
{
    stage.OnWorldComplete(1);
    source.width = 5;
    source.height = 4;
    stage.OnWorldComplete(2);
    while (stage.RunNextJob()) {}
    stage.Update();
    EXPECT_TRUE(stage.IsReadyForRevision(2));
    EXPECT_FALSE(stage.IsReadyForRevision(1));
    EXPECT_EQ(20u, stage.GetSnapshot().cellCount);
}

TEST_F(WeatherSpinUpTest, FinishedResultOfOlderRevisionIsDroppedWhenNewerIsAnnounced)
{
    stage.OnWorldComplete(1);
    ASSERT_TRUE(stage.RunNextJob());   // revision 1's result sits in the slot, untaken
    source.width = 5;
    source.height = 4;
    stage.OnWorldComplete(2);
    while (stage.RunNextJob()) {}
    stage.Update();
    EXPECT_TRUE(stage.IsReadyForRevision(2));
    EXPECT_FALSE(stage.IsReadyForRevision(1));
    EXPECT_EQ(20u, stage.GetSnapshot().cellCount);
}

TEST_F(WeatherSpinUpTest, SupersededMidSpinUpStopsBetweenWeeks)
{
    uint32_t weeksRun = 0;
    bool announced = false;
    stage.SetSpinupWeeks(40);
    stage.SetSpinUpWeekObserver([&](uint32_t) {
        ++weeksRun;
        if (!announced)
        {
            announced = true;
            source.width = 5;
            source.height = 4;
            stage.OnWorldComplete(2);   // queues job 2; job 1 must notice before its next week
        }
    });
    stage.OnWorldComplete(1);
    ASSERT_TRUE(stage.RunNextJob());
    EXPECT_EQ(1u, weeksRun);             // stopped after the week in which the newer world arrived
    ASSERT_TRUE(stage.RunNextJob());     // job 2 runs all its weeks
    EXPECT_EQ(41u, weeksRun);
    stage.Update();
    EXPECT_TRUE(stage.IsReadyForRevision(2));
    EXPECT_FALSE(stage.IsReadyForRevision(1));
    EXPECT_EQ(20u, stage.GetSnapshot().cellCount);
}

// AC 7: spin-up is a plain Initialise + Step loop from calendar offset 0 (week i: season (i/13)%4, week-of-year i%52).
TEST_F(WeatherSpinUpTest, SpunUpFieldsAreBitIdenticalToDirectStepLoop)
{
    stage.SetSpinupWeeks(60);   // past one year: pins the week-of-year wrap and the season rollover
    SpinUp(1);
    ASSERT_TRUE(stage.IsReadyForRevision(1));

    WeatherTerrain terrain;
    CoW::WorldStep::WorldView view;
    ASSERT_TRUE(source.GetWorld(view, terrain));
    WeatherSimulation reference;
    ReferenceSpinUp(terrain, 60, reference);

    const CoW::Weather::WeatherSnapshot snap = stage.GetSnapshot();
    ASSERT_NE(nullptr, snap.fields);
    EXPECT_TRUE(WT::SameFields(reference.Fields(), *snap.fields));
    EXPECT_EQ(reference.GetCommittedWeek(), snap.week);
}

TEST_F(WeatherSpinUpTest, SpinUpStartsAtCalendarOffsetZeroRegardlessOfAnyLiveDay)
{
    stage.SetSpinupWeeks(1);
    SpinUp(1);
    WeatherTerrain terrain;
    CoW::WorldStep::WorldView view;
    ASSERT_TRUE(source.GetWorld(view, terrain));
    WeatherSimulation reference;
    ReferenceSpinUp(terrain, 1, reference);   // one Step: Spring, week-of-year 0
    EXPECT_TRUE(WT::SameFields(reference.Fields(), *stage.GetSnapshot().fields));
    EXPECT_EQ(CoW::kWeeksPerSeason, 13u);
    EXPECT_EQ(CoW::kSeasonsPerYear, 4u);
}

TEST_F(WeatherSpinUpTest, ZeroSpinUpWeeksOnlyInitialises)
{
    stage.SetSpinupWeeks(0);
    SpinUp(1);
    ASSERT_TRUE(stage.IsReadyForRevision(1));
    WeatherTerrain terrain;
    CoW::WorldStep::WorldView view;
    ASSERT_TRUE(source.GetWorld(view, terrain));
    WeatherSimulation reference;
    ReferenceSpinUp(terrain, 0, reference);
    EXPECT_TRUE(WT::SameFields(reference.Fields(), *stage.GetSnapshot().fields));
    EXPECT_EQ(0u, stage.GetSnapshot().week);
}

// AC 8
TEST_F(WeatherSpinUpTest, SynchronousLauncherCompletesBeforeAnnouncementReturns)
{
    stage.SetLaunchMode(WeatherStage::LaunchMode::kSynchronous);
    stage.OnWorldComplete(9);
    EXPECT_EQ(0u, stage.QueuedJobCount());
    EXPECT_FALSE(stage.SpinUpRunning());
    stage.Update();
    EXPECT_TRUE(stage.IsReadyForRevision(9));
}

// AC 9
TEST_F(WeatherSpinUpTest, WorldGenerationFailedIsFailingAndNotReadyUntilNextSuccess)
{
    SpinUp(1);
    ASSERT_TRUE(stage.IsReadyForRevision(1));
    stage.OnWorldFailed();
    EXPECT_FALSE(stage.IsReady());
    EXPECT_FALSE(stage.IsReadyForRevision(1));
    EXPECT_EQ(Dia::Observation::Health::HealthStatus::kFailing, stage.GetHealth().status);
    EXPECT_EQ(0u, stage.GetSnapshot().cellCount);
    SpinUp(2);
    EXPECT_TRUE(stage.IsReadyForRevision(2));
    EXPECT_EQ(Dia::Observation::Health::HealthStatus::kOK, stage.GetHealth().status);
}

TEST_F(WeatherSpinUpTest, FailureWhileSpinningUpDropsTheQueuedJobsResult)
{
    stage.OnWorldComplete(1);
    stage.OnWorldFailed();
    while (stage.RunNextJob()) {}
    stage.Update();
    EXPECT_FALSE(stage.IsReady());
    EXPECT_EQ(Dia::Observation::Health::HealthStatus::kFailing, stage.GetHealth().status);
    EXPECT_FALSE(stage.HasPending());
}

TEST_F(WeatherSpinUpTest, AnnouncementWithoutAWorldIsIgnored)
{
    source.hasWorld = false;
    stage.OnWorldComplete(1);
    EXPECT_EQ(0u, stage.QueuedJobCount());
    EXPECT_FALSE(stage.IsReady());
}

TEST_F(WeatherSpinUpTest, MissingBiomeRegistryFailsTheSpinUpWithoutCrashing)
{
    stage.SetRegistry(nullptr);
    SpinUp(1);
    EXPECT_FALSE(stage.IsReady());
    EXPECT_EQ(Dia::Observation::Health::HealthStatus::kFailing, stage.GetHealth().status);
}

// AC 10 (stage half; the module half is in TestWeatherModule.cpp)
TEST_F(WeatherSpinUpTest, GenerationHoldTakenInStartAndReleasedByNotifySubscribed)
{
    EXPECT_EQ(1u, hold.holds);
    EXPECT_EQ(0u, hold.releases);
    EXPECT_TRUE(stage.GenerationHeld());
    stage.NotifySubscribed();
    EXPECT_EQ(1u, hold.releases);
    EXPECT_FALSE(stage.GenerationHeld());
    stage.NotifySubscribed();
    EXPECT_EQ(1u, hold.releases);   // idempotent
}

// AC 26
TEST_F(WeatherSpinUpTest, SnapshotCellCountIsZeroBeforeFirstWorldAndWorldSizeAfter)
{
    EXPECT_EQ(0u, stage.GetSnapshot().cellCount);
    EXPECT_EQ(nullptr, stage.GetSnapshot().fields);
    SpinUp(1);
    EXPECT_EQ(48u, stage.GetSnapshot().cellCount);
    EXPECT_NE(nullptr, stage.GetSnapshot().fields);
}

TEST_F(WeatherSpinUpTest, SnapshotIsZeroWhileANewerWorldIsStillSpinningUp)
{
    SpinUp(1);
    ASSERT_EQ(48u, stage.GetSnapshot().cellCount);
    stage.OnWorldComplete(2);   // target moves to 2; no sim for it yet
    EXPECT_EQ(0u, stage.GetSnapshot().cellCount);
}

// The S-1 world to weather terrain copy.
TEST(WeatherSpinUpTerrain, CopiesElevationLatitudeWaterAndBiomePerCell)
{
    using namespace Draoi::HexWorld;
    WorldStructure world;
    world.Resize(GridDimensions{ 3, 2 });
    for (int i = 0; i < 6; ++i)
    {
        Cell& c = world.Cells()[i];
        c.elevation = 0.1f * i;
        c.latitude = 0.05f * i;
        c.waterSource = (i == 4) ? WaterSource::Lake : (i == 0 ? WaterSource::Ocean : WaterSource::None);
        c.biome = static_cast<BiomeIndex>(i + 2);
    }
    WeatherTerrain t;
    ASSERT_TRUE(CoW::Weather::BuildWeatherTerrain(world, t));
    EXPECT_TRUE(t.IsConsistent());
    EXPECT_EQ(3u, t.width);
    EXPECT_EQ(2u, t.height);
    EXPECT_FLOAT_EQ(0.3f, t.elevation[3]);
    EXPECT_FLOAT_EQ(0.25f, t.latitude[5]);
    EXPECT_EQ(WaterSource::Lake, t.waterSource[4]);
    EXPECT_EQ(WaterSource::Ocean, t.waterSource[0]);
    EXPECT_EQ(static_cast<BiomeIndex>(7), t.biome[5]);
}

TEST(WeatherSpinUpTerrain, EmptyWorldIsRefused)
{
    Draoi::HexWorld::WorldStructure world;
    WeatherTerrain t;
    EXPECT_FALSE(CoW::Weather::BuildWeatherTerrain(world, t));
    EXPECT_EQ(0u, t.width);
}

// ---- Task 5b: JobSystem launcher (pool submit, inline fallback, join on stop) --------------------------------------

namespace
{
class WarnSink : public Dia::Observation::Log::ISink
{
public:
    void OnLogEntry(const Dia::Observation::Log::LogEntry& e) override
    {
        if (strstr(e.message, "WTH") != nullptr && e.level == Dia::Observation::Log::LogLevel::kWarning) { ++warnings; }
    }
    const char* GetName() const override { return "WeatherPoolWarnSink"; }
    std::atomic<unsigned> warnings{ 0 };
};

// The production submitter shape over a real JobSystem (tag = the 'world' channel id).
CoW::Weather::WeatherStage::JobSubmitter PoolSubmitter(Dia::Threading::JobSystem& js)
{
    return [&js](std::function<void()> fn) {
        Dia::Threading::JobTag tag;
        tag.owner = Dia::Core::StringCRC("world").Value();
        return js.TrySubmit(std::move(fn), tag).IsValid();
    };
}

bool WaitReady(WeatherStage& stage, uint32_t revision)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (std::chrono::steady_clock::now() < deadline)
    {
        stage.Update();
        if (stage.IsReadyForRevision(revision)) { return true; }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}
}

TEST_F(WeatherSpinUpTest, PoolSpinUpIsBitIdenticalToTheSynchronousPath)
{
    Dia::Threading::JobSystem js;
    js.Initialize(2);
    stage.SetJobSubmitter(PoolSubmitter(js));
    stage.SetLaunchMode(WeatherStage::LaunchMode::kJobSystem);
    stage.OnWorldComplete(1);
    ASSERT_TRUE(WaitReady(stage, 1));

    WeatherTerrain terrain;
    CoW::WorldStep::WorldView view;
    ASSERT_TRUE(source.GetWorld(view, terrain));
    WeatherSimulation reference;
    ReferenceSpinUp(terrain, 6, reference);
    const CoW::Weather::WeatherSnapshot snap = stage.GetSnapshot();
    ASSERT_NE(nullptr, snap.fields);
    EXPECT_TRUE(WT::SameFields(reference.Fields(), *snap.fields));
    EXPECT_EQ(reference.GetCommittedWeek(), snap.week);
    stage.Stop();
    js.Shutdown();
}

TEST_F(WeatherSpinUpTest, FailedSubmitRunsInlineWithExactlyOneWarning)
{
    WarnSink warn;
    Dia::Observation::Log::Logger::Instance().RegisterSink(&warn);
    stage.SetJobSubmitter([](std::function<void()>) { return false; });
    stage.SetLaunchMode(WeatherStage::LaunchMode::kJobSystem);
    stage.OnWorldComplete(1);
    stage.Update();   // the job already ran inline, so the result is waiting
    EXPECT_TRUE(stage.IsReadyForRevision(1));
    Dia::Observation::Log::Logger::Instance().FlushSync();
    EXPECT_EQ(1u, warn.warnings.load());
    EXPECT_EQ(0u, Errors());
    Dia::Observation::Log::Logger::Instance().UnregisterSink(&warn);
}

TEST_F(WeatherSpinUpTest, NoSubmitterAvailableRunsInlineWithExactlyOneWarning)
{
    WarnSink warn;
    Dia::Observation::Log::Logger::Instance().RegisterSink(&warn);
    stage.SetLaunchMode(WeatherStage::LaunchMode::kJobSystem);   // no submitter set
    stage.OnWorldComplete(1);
    stage.Update();
    EXPECT_TRUE(stage.IsReadyForRevision(1));
    Dia::Observation::Log::Logger::Instance().FlushSync();
    EXPECT_EQ(1u, warn.warnings.load());
    Dia::Observation::Log::Logger::Instance().UnregisterSink(&warn);
}

TEST_F(WeatherSpinUpTest, StopDuringAPoolJobCancelsAndJoinsBeforeReturning)
{
    Dia::Threading::JobSystem js;
    js.Initialize(2);
    stage.SetSpinupWeeks(520);
    std::atomic<uint32_t> weeksRun{ 0 };
    stage.SetSpinUpWeekObserver([&](uint32_t) {
        ++weeksRun;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    });
    stage.SetJobSubmitter(PoolSubmitter(js));
    stage.SetLaunchMode(WeatherStage::LaunchMode::kJobSystem);
    stage.OnWorldComplete(1);
    for (int i = 0; i < 2000 && weeksRun.load() == 0; ++i) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    ASSERT_GT(weeksRun.load(), 0u);

    stage.Stop();   // cancels, then waits for the pool job
    const uint32_t atStop = weeksRun.load();
    EXPECT_EQ(0, js.GetActiveJobCount());   // nothing of the stage is still running on the pool
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(atStop, weeksRun.load());
    EXPECT_LT(atStop, 520u);
    EXPECT_FALSE(stage.IsReady());
    js.Shutdown();
}

TEST_F(WeatherSpinUpTest, StaleResultOfASupersededPoolJobIsDiscarded)
{
    Dia::Threading::JobSystem js;
    js.Initialize(2);
    std::atomic<bool> firstEntered{ false };
    std::atomic<bool> release{ false };
    std::atomic<bool> first{ true };
    stage.SetSpinUpWeekObserver([&](uint32_t) {
        if (first.exchange(false))   // only revision 1's job parks
        {
            firstEntered = true;
            for (int i = 0; i < 5000 && !release.load(); ++i) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        }
    });
    stage.SetJobSubmitter(PoolSubmitter(js));
    stage.SetLaunchMode(WeatherStage::LaunchMode::kJobSystem);
    stage.OnWorldComplete(1);
    for (int i = 0; i < 2000 && !firstEntered.load(); ++i) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    ASSERT_TRUE(firstEntered.load());

    source.width = 5;
    source.height = 4;
    stage.OnWorldComplete(2);
    release = true;
    ASSERT_TRUE(WaitReady(stage, 2));
    EXPECT_FALSE(stage.IsReadyForRevision(1));
    EXPECT_EQ(20u, stage.GetSnapshot().cellCount);
    stage.Stop();
    js.Shutdown();
}
