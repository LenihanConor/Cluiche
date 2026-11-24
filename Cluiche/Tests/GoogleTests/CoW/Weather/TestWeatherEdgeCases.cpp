// A4 WeatherModule Task 6: exhaustive edge pass over the stage (regeneration races, spin-up lengths, failure then
// success, queue bounds, rapid regenerations, tiny worlds, stepping after Stop, sidecar fault recovery, hostile
// providers). Each test names the AC it hardens. See "Task 6 gap audit" in weather-module.plan.md.
#include "WeatherParamTestKit.h"

#include <cmath>
#include <limits>

namespace
{
using namespace WeatherParamKit;
using Dia::Observation::Health::HealthStatus;
using Dia::Observation::Log::LogLevel;
using Draoi::HexWorld::Weather::HydrologyInputs;
using Draoi::HexWorld::Weather::SeasonalBaselineInput;
using Draoi::HexWorld::Weather::WeatherFields;

// A world source whose size and water can change between announcements.
class DimsSource : public CoW::Weather::IWeatherWorldSource
{
public:
    bool GetWorld(CoW::WorldStep::WorldView& view, WeatherTerrain& terrain) override
    {
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
    uint32_t width = 8, height = 6, waterColumns = 2;
    bool hasWorld = true;
};

Season SeasonOfDay(uint64_t day) { return static_cast<Season>((day / (7u * 13u)) % 4u); }
uint32_t WeekOfYear(uint64_t day) { return static_cast<uint32_t>((day / 7u) % 52u); }

// Direct Initialise + Step loop over a hand-built world of any size, calendar written out by hand.
struct DirectRef
{
    DirectRef(uint32_t w, uint32_t h, uint32_t waterColumns, const Draoi::HexWorld::Weather::WeatherParameters& p, uint32_t weeks)
        : params(p)
    {
        WT::HexTerrainOptions o;
        o.waterColumns = waterColumns;
        o.latitudeRamp = true;
        WeatherTestWorld::CopyTerrain(WT::MakeHexTerrain(w, h, o), terrain);
        ok = sim.Initialise(terrain, params, WT::TestRegistry(), nullptr);
        for (uint32_t i = 0; ok && i < weeks; ++i)
        {
            sim.Step(params, static_cast<Season>((i / 13u) % 4u), i % 52u, WT::TestRegistry(), nullptr, noHydro, noInfl, exec);
        }
    }
    void Live(uint64_t day, const SeasonalBaselineInput* baseline = nullptr)
    {
        sim.Step(params, SeasonOfDay(day), WeekOfYear(day), WT::TestRegistry(), baseline, noHydro, noInfl, exec);
        sim.SetCommittedWeek(day / 7u);
    }
    bool ok = false;
    WeatherTerrain terrain;
    Draoi::HexWorld::Weather::WeatherParameters params;
    WeatherSimulation sim;
    HydrologyInputs noHydro;
    Draoi::HexWorld::Weather::InfluenceSnapshot noInfl;
    Dia::ParallelExecutor::SerialExecutor exec;
};

SeasonalBaselineInput Flat(uint32_t cells, float v)
{
    SeasonalBaselineInput in;
    in.temperature.Reserve(cells);
    for (uint32_t i = 0; i < cells; ++i) { in.temperature.Add(v); }
    return in;
}

bool AllFinite(const Dia::Core::Containers::DynamicArray<float>& a)
{
    for (uint32_t i = 0; i < a.Size(); ++i) { if (!std::isfinite(a[i])) { return false; } }
    return true;
}

bool AllFinite(const WeatherFields& f)
{
    return AllFinite(f.moisture) && AllFinite(f.temperature) && AllFinite(f.pressure) && AllFinite(f.windDirection)
        && AllFinite(f.windSpeed) && AllFinite(f.precipitation) && AllFinite(f.cloudCover) && AllFinite(f.waterToGround)
        && AllFinite(f.snowPack) && AllFinite(f.pressureAnomaly);
}

uint64_t DiscardedCount()
{
    auto* c = Dia::Observation::Metric::MetricRegistry::Instance().FindCounter(
        Dia::Core::StringCRC(CoW::World::MetricName(CoW::World::kWeather, "discarded_steps").c_str()));
    return c != nullptr ? c->Value() : 0;
}

class ConstBaseline : public CoW::Weather::IBiomeSeasonalBaseline
{
public:
    explicit ConstBaseline(float v) : value(v) {}
    float GetBaseline(uint32_t, Season) const override { ++calls; return value; }
    float value;
    mutable uint32_t calls = 0;
};

class OddHydro : public CoW::Weather::IHydrologyInputsProvider
{
public:
    bool GetInputs(HydrologyInputs& out) override
    {
        ++calls;
        out.Resize(10);   // the world has 48 cells
        out.present = true;
        return true;
    }
    uint32_t calls = 0;
};

class EdgeTest : public ParamStageTest
{
protected:
    void SetUp() override
    {
        ParamStageTest::SetUp();
        stage.SetWorldSource(&dims);
        stage.SetPublishSeam([this](uint64_t, Season) { ++published; });
    }
    bool SameAs(const DirectRef& ref) const
    {
        const CoW::Weather::WeatherSnapshot snap = stage.GetSnapshot();
        return ref.ok && snap.fields != nullptr && WT::SameFields(ref.sim.Fields(), *snap.fields)
            && ref.sim.GetCommittedWeek() == snap.week;
    }
    void RunAllJobs() { while (stage.RunNextJob()) {} stage.Update(); }

    // Spins up and steps a w x h world; either it comes up and every field stays finite, or it fails cleanly.
    void ExpectCleanOutcome(uint32_t w, uint32_t h, uint32_t waterColumns, bool* cameUp)
    {
        dims.width = w;
        dims.height = h;
        dims.waterColumns = waterColumns;
        SpinUp(1);
        if (stage.IsReadyForRevision(1))
        {
            *cameUp = true;
            EXPECT_EQ(w * h, stage.GetSnapshot().cellCount);
            for (uint32_t s = 0; s < 4; ++s) { Step(s * CoW::kDaysPerSeason); }
            EXPECT_EQ(4u, published);
            const CoW::Weather::WeatherSnapshot snap = stage.GetSnapshot();
            ASSERT_NE(nullptr, snap.fields);
            EXPECT_TRUE(AllFinite(*snap.fields));
            EXPECT_EQ(w * h, snap.fields->CellCount());
        }
        else
        {
            *cameUp = false;
            EXPECT_TRUE(Health(HealthStatus::kFailing));
            EXPECT_NE(0u, stage.GetCauseCode());
            EXPECT_FALSE(stage.HasPending());
            EXPECT_EQ(0u, stage.GetSnapshot().cellCount);
            Step(0);   // a failed stage never steps
            EXPECT_EQ(0u, published);
        }
    }

    DimsSource dims;
    unsigned published = 0;
};
}

// ---- AC 6 / 14 / 15: regeneration against spin-up and in-flight steps ------------------------------------------------

// A finished spin-up is already pending when the regeneration arrives in the same tick: the stale sim is never promoted.
TEST_F(EdgeTest, PendingResultThenRegenerationInTheSameTickNeverPromotesTheStaleSim)
{
    ASSERT_TRUE(Load());
    SpinUp(1);
    ASSERT_TRUE(stage.HasPending());
    dims.width = 5;
    dims.height = 4;
    stage.OnWorldComplete(2);
    EXPECT_FALSE(stage.IsReady());
    Step(0);   // Capture drops the stale pending sim; the step is invalid and publishes nothing
    EXPECT_EQ(0u, published);
    EXPECT_FALSE(stage.HasPending());
    EXPECT_FALSE(stage.HasActive());
    RunAllJobs();
    ASSERT_TRUE(stage.IsReadyForRevision(2));
    EXPECT_EQ(20u, stage.GetSnapshot().cellCount);
    Step(7);
    EXPECT_EQ(1u, published);
    DirectRef ref(5, 4, 2, Snapshot(), kSpinUpWeeks);
    ref.Live(7);
    EXPECT_TRUE(SameAs(ref));
}

// Synchronous launcher: spin-up 1 completes inside OnWorldComplete(1), regeneration 2 follows before anyone polled.
TEST_F(EdgeTest, SynchronousSpinUpCompletingInTheSameTickAsARegenerationLeavesOnlyTheNewest)
{
    ASSERT_TRUE(Load());
    stage.SetLaunchMode(CoW::Weather::WeatherStage::LaunchMode::kSynchronous);
    stage.OnWorldComplete(1);
    dims.width = 5;
    dims.height = 4;
    stage.OnWorldComplete(2);
    stage.Update();
    EXPECT_TRUE(stage.IsReadyForRevision(2));
    EXPECT_FALSE(stage.IsReadyForRevision(1));
    EXPECT_EQ(20u, stage.GetSnapshot().cellCount);
    EXPECT_FALSE(stage.SpinUpRunning());
    Step(0);
    EXPECT_EQ(1u, published);
}

// A step captured on revision 1, regeneration arrives before Run: Run still reads the old sim, Commit discards it.
TEST_F(EdgeTest, RegenerationBetweenCaptureAndRunDiscardsTheStepAndLeavesTheNewWorldClean)
{
    ASSERT_TRUE(Load());
    SpinUp(1);
    Step(0);
    ASSERT_EQ(1u, published);
    const WeatherSimulation* oldSim = stage.ReadableSim();
    ASSERT_NE(nullptr, oldSim);
    const uint64_t weekBefore = oldSim->GetCommittedWeek();
    const uint64_t discardedBefore = DiscardedCount();

    stage.Capture(static_cast<Dia::DeferredStep::StepKey>(7));
    dims.width = 5;
    dims.height = 4;
    stage.OnWorldComplete(2);   // between Capture and Run
    stage.Run(exec);
    stage.Commit();
    EXPECT_EQ(1u, published);
    EXPECT_EQ(weekBefore, oldSim->GetCommittedWeek());
    EXPECT_EQ(discardedBefore + 1, DiscardedCount());

    RunAllJobs();
    ASSERT_TRUE(stage.IsReadyForRevision(2));
    DirectRef spunUpOnly(5, 4, 2, Snapshot(), kSpinUpWeeks);
    EXPECT_TRUE(SameAs(spunUpOnly));   // the discarded step touched nothing of the new world
    Step(14);
    EXPECT_EQ(2u, published);
    spunUpOnly.Live(14);
    EXPECT_TRUE(SameAs(spunUpOnly));
}

// The world fails while a step is in flight: the worker may still be reading, so the sims are dropped at Commit.
TEST_F(EdgeTest, WorldFailureWhileAStepIsInFlightDropsTheSimsAtCommitWithoutPublishing)
{
    ASSERT_TRUE(Load());
    SpinUp(1);
    stage.Capture(static_cast<Dia::DeferredStep::StepKey>(7));
    stage.Run(exec);
    stage.OnWorldFailed();
    EXPECT_TRUE(stage.HasActive());   // still alive: the step is not finished
    stage.Commit();
    EXPECT_EQ(0u, published);
    EXPECT_FALSE(stage.HasActive());
    EXPECT_FALSE(stage.IsReady());
    EXPECT_TRUE(Health(HealthStatus::kFailing));

    dims.width = 8;
    dims.height = 6;
    SpinUp(3);
    ASSERT_TRUE(stage.IsReadyForRevision(3));
    Step(14);
    EXPECT_EQ(1u, published);
    DirectRef ref(8, 6, 2, Snapshot(), kSpinUpWeeks);
    ref.Live(14);
    EXPECT_TRUE(SameAs(ref));
}

// Entries queued while a step is in flight wait for the next Capture and then apply to the new world's step.
TEST_F(EdgeTest, EntriesQueuedDuringADiscardedStepApplyToTheNextValidStep)
{
    ASSERT_TRUE(Load());
    SpinUp(1);
    stage.Capture(static_cast<Dia::DeferredStep::StepKey>(7));
    ASSERT_TRUE(stage.QueueParameterSet("solar.cloud_albedo", 0.5f, err));
    stage.OnWorldComplete(2);
    stage.Run(exec);
    stage.Commit();   // discarded
    EXPECT_EQ(1u, stage.QueueCount());
    RunAllJobs();
    ASSERT_TRUE(stage.IsReadyForRevision(2));
    Step(14);
    EXPECT_EQ(0u, stage.QueueCount());
    EXPECT_FLOAT_EQ(0.5f, Snapshot().cloudAlbedo);
    EXPECT_EQ(1u, published);
}

// ---- AC 4 / 8: spin-up length ----------------------------------------------------------------------------------------

TEST_F(EdgeTest, ZeroSpinUpWeeksThenTheFirstLiveStepMatchesADirectRun)
{
    ASSERT_TRUE(Load());
    stage.SetSpinupWeeks(0);
    SpinUp(1);
    ASSERT_TRUE(stage.IsReadyForRevision(1));
    Step(21);
    ASSERT_EQ(1u, published);
    DirectRef ref(8, 6, 2, Snapshot(), 0);
    ref.Live(21);
    EXPECT_TRUE(SameAs(ref));
}

// 520 weeks (the config maximum) on a tiny world: every week runs, the result equals a direct loop, nothing goes non-finite.
TEST_F(EdgeTest, MaximumSpinUpOf520WeeksCompletesAndEqualsADirectLoop)
{
    ASSERT_TRUE(Load());
    dims.width = 3;
    dims.height = 2;
    dims.waterColumns = 1;
    stage.SetSpinupWeeks(520);
    uint32_t weeks = 0;
    stage.SetSpinUpWeekObserver([&](uint32_t) { ++weeks; });
    SpinUp(1);
    ASSERT_TRUE(stage.IsReadyForRevision(1));
    EXPECT_EQ(520u, weeks);
    const CoW::Weather::WeatherSnapshot snap = stage.GetSnapshot();
    ASSERT_NE(nullptr, snap.fields);
    EXPECT_TRUE(AllFinite(*snap.fields));
    DirectRef ref(3, 2, 1, Snapshot(), 520);
    EXPECT_TRUE(SameAs(ref));
}

// ---- AC 9: failure then success --------------------------------------------------------------------------------------

TEST_F(EdgeTest, FailureBeforeAnyWorldThenCompleteBecomesReadyAndHealthy)
{
    ASSERT_TRUE(Load());
    stage.OnWorldFailed();
    EXPECT_FALSE(stage.IsReady());
    EXPECT_TRUE(Health(HealthStatus::kFailing));
    stage.OnWorldFailed();   // twice is harmless
    EXPECT_TRUE(Health(HealthStatus::kFailing));
    SpinUp(1);
    EXPECT_TRUE(stage.IsReadyForRevision(1));
    EXPECT_TRUE(Health(HealthStatus::kOK) || Health(HealthStatus::kDegraded));   // Degraded: no baseline provider
    EXPECT_FALSE(Health(HealthStatus::kFailing));
}

TEST_F(EdgeTest, TheSameRevisionAnnouncedAgainAfterAFailureSpinsUpAgain)
{
    ASSERT_TRUE(Load());
    stage.OnWorldComplete(1);
    ASSERT_TRUE(stage.RunNextJob());   // its result sits in the slot, untaken
    stage.OnWorldFailed();
    EXPECT_FALSE(stage.IsReady());
    SpinUp(1);
    EXPECT_TRUE(stage.IsReadyForRevision(1));
    Step(0);
    EXPECT_EQ(1u, published);
}

TEST_F(EdgeTest, FailureWhileJobQueuedThenNewRevisionOnlyTheNewOneBecomesReady)
{
    ASSERT_TRUE(Load());
    uint32_t weeks = 0;
    stage.SetSpinUpWeekObserver([&](uint32_t) { ++weeks; });
    stage.OnWorldComplete(1);
    stage.OnWorldFailed();
    dims.width = 5;
    dims.height = 4;
    stage.OnWorldComplete(2);
    RunAllJobs();
    EXPECT_TRUE(stage.IsReadyForRevision(2));
    EXPECT_FALSE(stage.IsReadyForRevision(1));
    EXPECT_EQ(kSpinUpWeeks, weeks);   // job 1 never stepped
    EXPECT_EQ(20u, stage.GetSnapshot().cellCount);
}

// ---- AC 21: queue bound ----------------------------------------------------------------------------------------------

TEST_F(EdgeTest, ExactlyOneHundredTwentyEightEntriesAreAcceptedAndEveryExtraKindIsRefusedWithAWarning)
{
    ASSERT_TRUE(Load());
    SpinUp(1);
    for (uint32_t round = 0; round < 2; ++round)
    {
        for (uint32_t i = 0; i < 128; ++i)
        {
            ASSERT_TRUE(stage.QueueBiomeChange(0, static_cast<Draoi::HexWorld::BiomeIndex>(2))) << round << ":" << i;
        }
        EXPECT_EQ(128u, stage.QueueCount());
        const unsigned warnsBefore = Log().Count(LogLevel::kWarning, "mutation queue full");
        EXPECT_FALSE(stage.QueueReload());
        EXPECT_FALSE(stage.QueueClearWindOverrides());
        EXPECT_FALSE(stage.QueueBiomeChange(1, static_cast<Draoi::HexWorld::BiomeIndex>(2)));
        EXPECT_FALSE(stage.QueueParameterSet("solar.cloud_albedo", 0.5f, err));
        EXPECT_FALSE(stage.QueueParameterOverrides(ParseJson("{\"format\":\"cow.weather_params\",\"version\":1}"), err));
        EXPECT_EQ(128u, stage.QueueCount());
        EXPECT_EQ(warnsBefore + 5u, Log().Count(LogLevel::kWarning, "mutation queue full"));
        EXPECT_TRUE(Health(HealthStatus::kDegraded));
        Step(round * CoW::kDaysPerWeek);   // drains all 128, the refused ones left nothing behind
        EXPECT_EQ(0u, stage.QueueCount());
    }
    EXPECT_EQ(2u, published);
}

// ---- AC 6: rapid regenerations ---------------------------------------------------------------------------------------

TEST_F(EdgeTest, ThreeRapidRegenerationsLeaveOnlyTheNewestReadyAndTheOldJobsNeverStep)
{
    ASSERT_TRUE(Load());
    uint32_t weeks = 0;
    stage.SetSpinUpWeekObserver([&](uint32_t) { ++weeks; });
    stage.OnWorldComplete(1);
    dims.width = 5;
    dims.height = 4;
    stage.OnWorldComplete(2);
    dims.width = 4;
    dims.height = 4;
    stage.OnWorldComplete(3);
    EXPECT_EQ(3u, stage.QueuedJobCount());
    RunAllJobs();
    EXPECT_EQ(kSpinUpWeeks, weeks);
    EXPECT_TRUE(stage.IsReadyForRevision(3));
    EXPECT_FALSE(stage.IsReadyForRevision(1));
    EXPECT_FALSE(stage.IsReadyForRevision(2));
    EXPECT_FALSE(stage.HasActive());
    EXPECT_EQ(16u, stage.GetSnapshot().cellCount);
    DirectRef ref(4, 4, 2, Snapshot(), kSpinUpWeeks);
    EXPECT_TRUE(SameAs(ref));
    Step(0);
    EXPECT_EQ(1u, published);
}

TEST_F(EdgeTest, RapidRegenerationsAfterEarlierJobsAlreadyRanKeepOnlyTheNewestResult)
{
    ASSERT_TRUE(Load());
    stage.OnWorldComplete(1);
    ASSERT_TRUE(stage.RunNextJob());
    dims.width = 5;
    dims.height = 4;
    stage.OnWorldComplete(2);
    ASSERT_TRUE(stage.RunNextJob());
    dims.width = 4;
    dims.height = 4;
    stage.OnWorldComplete(3);
    RunAllJobs();
    EXPECT_TRUE(stage.IsReadyForRevision(3));
    EXPECT_EQ(16u, stage.GetSnapshot().cellCount);
    EXPECT_FALSE(stage.IsReadyForRevision(2));
}

// ---- Tiny worlds -----------------------------------------------------------------------------------------------------

TEST_F(EdgeTest, OneCellOceanWorldComesUpOrFailsCleanly)
{
    ASSERT_TRUE(Load());
    bool up = false;
    ExpectCleanOutcome(1, 1, 1, &up);
    RecordProperty("one_cell_ocean_came_up", up ? "yes" : "no");
}

TEST_F(EdgeTest, OneCellWorldWithoutWaterFailsWithNoWaterSource)
{
    ASSERT_TRUE(Load());
    bool up = true;
    ExpectCleanOutcome(1, 1, 0, &up);
    EXPECT_FALSE(up);
    EXPECT_EQ(static_cast<uint32_t>(CoW::Weather::CauseCode::kNoWaterSource), stage.GetCauseCode());
}

TEST_F(EdgeTest, TwoCellWorldsComeUpOrFailCleanly)
{
    ASSERT_TRUE(Load());
    bool up = false;
    ExpectCleanOutcome(2, 1, 1, &up);
    RecordProperty("two_by_one_came_up", up ? "yes" : "no");
}

TEST_F(EdgeTest, TwoCellColumnWorldComesUpOrFailsCleanly)
{
    ASSERT_TRUE(Load());
    bool up = false;
    ExpectCleanOutcome(1, 2, 1, &up);
    RecordProperty("one_by_two_came_up", up ? "yes" : "no");
}

// ---- AC 3 / 29: stepping a stopped stage -----------------------------------------------------------------------------

TEST_F(EdgeTest, CommitAfterStopWithNoStepEverCapturedIsADiscardNotACrash)
{
    ASSERT_TRUE(Load());
    SpinUp(1);
    stage.Stop();
    stage.Commit();
    EXPECT_EQ(0u, published);
    EXPECT_FALSE(stage.IsReady());
}

TEST_F(EdgeTest, CaptureRunCommitAfterStopIsAnInvalidStepAndPublishesNothing)
{
    ASSERT_TRUE(Load());
    SpinUp(1);
    Step(0);
    ASSERT_EQ(1u, published);
    stage.Stop();
    Step(7);   // a fresh Capture sees no sim, so the step is invalid
    EXPECT_EQ(1u, published);
    EXPECT_FALSE(stage.IsReady());
    EXPECT_FALSE(stage.HasActive());
}

// (Fixed in Task 5b) Teardown now clears the step state. Previously Stop() dropped the sims but leaves mStepValid set from an in-flight Capture, so a Run or a
// Commit that follows Stop dereferences the null active sim. The coordinator unregisters the stage first (DoStop order), so
// production is protected by ordering, not by the stage. Enable once the base clears mStepValid in Teardown.
TEST_F(EdgeTest, RunAndCommitAfterStopWithAStepInFlightAreHarmless)
{
    ASSERT_TRUE(Load());
    SpinUp(1);
    stage.Capture(static_cast<Dia::DeferredStep::StepKey>(7));
    stage.Stop();
    stage.Run(exec);
    stage.Commit();
    EXPECT_EQ(0u, published);
}

// ---- AC 18 (amended): sidecar fault then reload ----------------------------------------------------------------------

// Observed behaviour: FailStage is permanent for the stage object. Neither a queued reload, nor a successful second
// LoadParameters, nor a new world clears it; only Stop (module exit and re-entry) does. Recorded as a finding.
TEST_F(EdgeTest, SidecarFaultIsNotRecoveredByAQueuedReloadASecondLoadOrANewWorld)
{
    source.fail = true;
    ASSERT_FALSE(Load());
    ASSERT_TRUE(Health(HealthStatus::kFailing));
    const uint32_t readsAfterFault = source.reads;

    source.fail = false;
    EXPECT_TRUE(stage.QueueReload());   // accepted into the queue ...
    SpinUp(1);
    EXPECT_FALSE(stage.IsReady());
    Step(0);                            // ... but a faulted stage never captures a valid step, so the entry is dropped
    EXPECT_EQ(0u, published);
    EXPECT_EQ(0u, stage.QueueCount());
    EXPECT_EQ(readsAfterFault, source.reads);
    EXPECT_FALSE(stage.Parameters().IsLoaded());
    EXPECT_TRUE(Health(HealthStatus::kFailing));

    EXPECT_TRUE(Load());                // the file itself is fine now
    EXPECT_TRUE(stage.Parameters().IsLoaded());
    EXPECT_TRUE(stage.IsFaulted());     // but the stage object stays faulted
    EXPECT_TRUE(Health(HealthStatus::kFailing));
    SpinUp(2);
    EXPECT_FALSE(stage.IsReady());
}

TEST_F(EdgeTest, SidecarFaultThenStopAndStartLoadsAgainAndComesUp)
{
    source.fail = true;
    ASSERT_FALSE(Load());
    stage.Stop();
    stage.Start();
    source.fail = false;
    ASSERT_TRUE(Load());
    EXPECT_FALSE(stage.IsFaulted());
    SpinUp(1);
    EXPECT_TRUE(stage.IsReadyForRevision(1));
    Step(0);
    EXPECT_EQ(1u, published);
}

// ---- AC 25: hostile baseline provider --------------------------------------------------------------------------------

TEST_F(EdgeTest, NanBaselineIsReplacedByTheMidpointLoggedOncePerBiomeAndDegraded)
{
    ASSERT_TRUE(Load());
    ConstBaseline nan(std::numeric_limits<float>::quiet_NaN());
    stage.SetBaselineProvider(&nan);
    SpinUp(1);
    Step(0);
    EXPECT_EQ(1u, published);
    EXPECT_EQ(WT::TestRegistry().Count(), Log().Count(LogLevel::kError, "baseline for biome"));
    EXPECT_TRUE(Health(HealthStatus::kDegraded));
    EXPECT_EQ(Dia::Core::StringCRC(CoW::Weather::kCauseBaselineClamped), stage.GetHealth().reason);
    EXPECT_TRUE(stage.IsReady());
    ASSERT_NE(nullptr, stage.GetSnapshot().fields);
    EXPECT_TRUE(AllFinite(*stage.GetSnapshot().fields));
    DirectRef ref(8, 6, 2, Snapshot(), kSpinUpWeeks);
    const SeasonalBaselineInput half = Flat(48, 0.5f);
    ref.Live(0, &half);
    EXPECT_TRUE(SameAs(ref));

    Step(7);   // same season: logged once per biome and season, not again
    EXPECT_EQ(WT::TestRegistry().Count(), Log().Count(LogLevel::kError, "baseline for biome"));
}

TEST_F(EdgeTest, InfiniteBaselineValuesClampToTheRangeEnds)
{
    ASSERT_TRUE(Load());
    ConstBaseline hot(std::numeric_limits<float>::infinity());
    stage.SetBaselineProvider(&hot);
    SpinUp(1);
    Step(0);
    ASSERT_NE(nullptr, stage.GetSnapshot().fields);
    EXPECT_TRUE(AllFinite(*stage.GetSnapshot().fields));
    DirectRef ref(8, 6, 2, Snapshot(), kSpinUpWeeks);
    const SeasonalBaselineInput one = Flat(48, 1.0f);
    ref.Live(0, &one);
    EXPECT_TRUE(SameAs(ref));

    ConstBaseline cold(-std::numeric_limits<float>::infinity());
    stage.SetBaselineProvider(&cold);
    Step(7);
    const SeasonalBaselineInput zero = Flat(48, 0.0f);
    ref.Live(7, &zero);
    EXPECT_TRUE(SameAs(ref));
}

// ---- AC 31 / 32: hostile hydrology provider --------------------------------------------------------------------------

TEST_F(EdgeTest, HydrologyProviderWithWrongSizedArraysIsTreatedAsAbsent)
{
    ASSERT_TRUE(Load());
    OddHydro odd;
    stage.SetHydrologyProvider(&odd);
    SpinUp(1);
    Step(0);
    EXPECT_EQ(1u, odd.calls);
    EXPECT_EQ(1u, published);
    DirectRef ref(8, 6, 2, Snapshot(), kSpinUpWeeks);
    ref.Live(0);
    EXPECT_TRUE(SameAs(ref));
}

TEST_F(EdgeTest, HydrologyProviderRemovedAfterUseStopsBeingCalled)
{
    ASSERT_TRUE(Load());
    OddHydro odd;
    stage.SetHydrologyProvider(&odd);
    SpinUp(1);
    Step(0);
    stage.SetHydrologyProvider(nullptr);
    Step(7);
    EXPECT_EQ(1u, odd.calls);
    EXPECT_EQ(2u, published);
}
