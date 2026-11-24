// A4 WeatherModule Task 4: the Queue* API and ApplyEntry for the nine entry kinds (AC 19, 21, 22 and the forcing,
// region forcing, biome change, preset, state-replace and wind-clear kinds). Entries apply in arrival order at Capture,
// before the step latches its inputs. Biome change and clear-wind are drained and logged only until Draoi grows the
// WeatherSimulation methods; preset until Feature 6; state replace until Feature 4.
#include "WeatherParamTestKit.h"

namespace
{
using namespace WeatherParamKit;
using Dia::Observation::Health::HealthStatus;
using Dia::Observation::Log::LogLevel;
using Draoi::HexWorld::Weather::ForcingKind;
using Draoi::HexWorld::Weather::WeatherForcing;

class WeatherQueueTest : public ParamStageTest
{
protected:
    void SetUp() override
    {
        ParamStageTest::SetUp();
        ASSERT_TRUE(Load()) << err.key << ": " << err.reason;
        SpinUp(1);
        ASSERT_TRUE(stage.IsReadyForRevision(1));
    }
    static WeatherForcing Pressure(uint32_t cell, float value, uint16_t weeks = 2)
    {
        WeatherForcing f;
        f.kind = ForcingKind::PressureHold;
        f.cell = cell;
        f.value = value;
        f.weeksRemaining = weeks;
        return f;
    }
    uint32_t Forcings() const { const auto* s = stage.ReadableSim(); return s != nullptr ? s->ForcingCount() : 0; }
};
}

// ---- AC 19 ---------------------------------------------------------------------------------------------------------

TEST_F(WeatherQueueTest, ValidSetAppliesAtNextCaptureAndNotBefore)
{
    RefSim ref(Snapshot());
    ASSERT_TRUE(stage.QueueParameterSet("solar.cloud_albedo", 0.5f, err));
    EXPECT_EQ(1u, stage.QueueCount());
    EXPECT_FLOAT_EQ(0.4f, Snapshot().cloudAlbedo);   // not before Capture
    Step(0);
    EXPECT_EQ(0u, stage.QueueCount());
    EXPECT_FLOAT_EQ(0.5f, Snapshot().cloudAlbedo);
    ref.params.cloudAlbedo = 0.5f;   // and that same step already used it (the queue drains before LatchInputs)
    ref.Live(0);
    EXPECT_TRUE(Matches(ref));
}

TEST_F(WeatherQueueTest, ValidationDoesNotChangeTheParametersOrTheHash)
{
    const uint64_t hash = stage.ParametersHash();
    ASSERT_TRUE(stage.QueueParameterSet("solar.cloud_albedo", 0.5f, err));
    ASSERT_TRUE(stage.QueueParameterSet("solar.cloud_albedo", 0.55f, err));   // the probe was reverted, both validate
    EXPECT_EQ(hash, stage.ParametersHash());
    EXPECT_FLOAT_EQ(0.4f, Snapshot().cloudAlbedo);
}

TEST_F(WeatherQueueTest, InvalidSetReturnsFalseAtOnceNamingTheKeyAndQueuesNothing)
{
    EXPECT_FALSE(stage.QueueParameterSet("solar.cloud_albedo", 9.0f, err));   // out of range
    EXPECT_STREQ("solar.cloud_albedo", err.key);
    EXPECT_GT(strlen(err.reason), 0u);
    EXPECT_EQ(0u, stage.QueueCount());

    EXPECT_FALSE(stage.QueueParameterSet("no.such_key", 0.5f, err));          // unknown key
    EXPECT_STREQ("no.such_key", err.key);
    EXPECT_EQ(0u, stage.QueueCount());

    EXPECT_FALSE(stage.QueueParameterSet("influence.max_active", 64.5f, err));   // non-whole int
    EXPECT_STREQ("influence.max_active", err.key);

    EXPECT_FALSE(stage.QueueParameterSet("precipitation.snow_threshold", 0.4f, err));   // breaks a constraint
    EXPECT_STREQ("precipitation.snow_threshold", err.key);
    EXPECT_EQ(0u, stage.QueueCount());
    EXPECT_TRUE(Health(HealthStatus::kOK));
}

TEST_F(WeatherQueueTest, SetOnAStageWithoutParametersIsRefusedNamingTheKey)
{
    CoW::Weather::WeatherStage bare;
    CoW::Weather::ParamError e;
    EXPECT_FALSE(bare.QueueParameterSet("solar.cloud_albedo", 0.5f, e));
    EXPECT_STREQ("solar.cloud_albedo", e.key);
}

TEST_F(WeatherQueueTest, TooLongKeyIsRefused)
{
    const std::string longKey(80, 'a');
    EXPECT_FALSE(stage.QueueParameterSet(longKey.c_str(), 0.5f, err));
    EXPECT_EQ(0u, stage.QueueCount());
}

// ---- AC 21 ---------------------------------------------------------------------------------------------------------

TEST_F(WeatherQueueTest, EntriesApplyInArrivalOrderBeforeTheStepCaptures)
{
    RefSim ref(Snapshot());
    ASSERT_TRUE(stage.QueueParameterSet("solar.cloud_albedo", 0.5f, err));
    ASSERT_TRUE(stage.QueueParameterSet("solar.cloud_albedo", 0.45f, err));
    ASSERT_TRUE(stage.QueueForcing(Pressure(5, 0.9f)));
    ASSERT_TRUE(stage.QueueParameterSet("solar.cloud_albedo", 0.55f, err));
    EXPECT_EQ(4u, stage.QueueCount());
    Step(0);
    EXPECT_FLOAT_EQ(0.55f, Snapshot().cloudAlbedo);   // the last set wins
    ref.params.cloudAlbedo = 0.55f;
    ASSERT_TRUE(ref.sim.AddForcing(Pressure(5, 0.9f)));
    ref.Live(0);
    EXPECT_TRUE(Matches(ref));
}

TEST_F(WeatherQueueTest, FullQueueRefusesTheNextEntryWarnsOnceAndDegrades)
{
    for (uint32_t i = 0; i < 128; ++i) { ASSERT_TRUE(stage.QueueClearWindOverrides()) << i; }
    EXPECT_EQ(128u, stage.QueueCount());
    EXPECT_FALSE(stage.QueueClearWindOverrides());
    EXPECT_EQ(1u, Log().Count(LogLevel::kWarning, "mutation queue full"));
    EXPECT_TRUE(Health(HealthStatus::kDegraded));
    EXPECT_EQ(Dia::Core::StringCRC("queue_full"), stage.GetHealth().reason);
    EXPECT_FALSE(stage.QueueParameterSet("solar.cloud_albedo", 0.5f, err));   // names the key, reason says full
    EXPECT_STREQ("solar.cloud_albedo", err.key);
    Step(0);
    EXPECT_EQ(0u, stage.QueueCount());
    EXPECT_NE(Dia::Core::StringCRC("queue_full"), stage.GetHealth().reason);   // the drain recovers (a no-provider
                                                                               // baseline Degraded may replace it)
    EXPECT_TRUE(stage.QueueClearWindOverrides());
}

TEST_F(WeatherQueueTest, SideStorePayloadsSurviveAFullQueueOfSlotEntries)
{
    // 128 slot-backed entries fill the queue; the 129th is refused and hands its slot back, so the ring never
    // overwrites a queued payload.
    for (uint32_t i = 0; i < 128; ++i)
    {
        CoW::Weather::WeatherPreset p;
        snprintf(p.id, sizeof(p.id), "preset_%u", i);
        ASSERT_TRUE(stage.QueuePreset(p)) << i;
    }
    CoW::Weather::WeatherPreset extra;
    snprintf(extra.id, sizeof(extra.id), "refused");
    EXPECT_FALSE(stage.QueuePreset(extra));
    Step(0);
    EXPECT_GE(Log().IndexOf("preset 'preset_0'"), 0);
    EXPECT_GE(Log().IndexOf("preset 'preset_127'"), 0);
    EXPECT_EQ(-1, Log().IndexOf("preset 'refused'"));
}

// ---- forcing, region forcing --------------------------------------------------------------------------------------

TEST_F(WeatherQueueTest, ForcingTakesEffectAtTheNextCapture)
{
    RefSim ref(Snapshot());
    ASSERT_TRUE(stage.QueueForcing(Pressure(9, 0.95f, 3)));
    EXPECT_EQ(0u, Forcings());   // not before Capture
    Step(0);
    ASSERT_TRUE(ref.sim.AddForcing(Pressure(9, 0.95f, 3)));
    ref.Live(0);
    EXPECT_TRUE(Matches(ref));
    EXPECT_EQ(1u, Forcings());   // 3 weeks, one committed
}

TEST_F(WeatherQueueTest, RegionForcingRegistersTheSortedCellSetAndForces)
{
    RefSim ref(Snapshot());
    Dia::Core::Containers::DynamicArray<uint32_t> cells;
    cells.Reserve(8);
    for (uint32_t c : { 9u, 3u, 1u, 3u, 2u }) { cells.Add(c);}   // unsorted with a duplicate
    ASSERT_TRUE(stage.QueueRegionForcing(Pressure(0, 0.9f, 2), cells));
    Step(0);

    Dia::Core::Containers::DynamicArray<uint32_t> sorted;
    sorted.Reserve(8);
    for (uint32_t c : { 1u, 2u, 3u, 9u }) { sorted.Add(c); }
    WeatherForcing f = Pressure(0, 0.9f, 2);
    f.cellSet = ref.sim.RegisterCellSet(sorted);
    ASSERT_NE(Draoi::HexWorld::Weather::kNoCellSet, f.cellSet);
    ASSERT_TRUE(ref.sim.AddForcing(f));
    ref.Live(0);
    EXPECT_TRUE(Matches(ref));
    EXPECT_EQ(1u, stage.ReadableSim()->CellSetCount());
}

TEST_F(WeatherQueueTest, RegionForcingWithNoCellsOrOutOfRangeCellsIsHandled)
{
    Dia::Core::Containers::DynamicArray<uint32_t> none;
    EXPECT_FALSE(stage.QueueRegionForcing(Pressure(0, 0.9f), none));
    EXPECT_EQ(0u, stage.QueueCount());

    Dia::Core::Containers::DynamicArray<uint32_t> far;
    far.Reserve(1);
    far.Add(100000u);
    ASSERT_TRUE(stage.QueueRegionForcing(Pressure(0, 0.9f), far));   // range is known only against the sim
    Step(0);
    EXPECT_GE(Log().Count(LogLevel::kWarning, "region forcing refused"), 1u);
    EXPECT_EQ(0u, Forcings());
}

// ---- the kinds that are drained and logged ------------------------------------------------------------------------

TEST_F(WeatherQueueTest, PresetStateReplaceBiomeChangeAndClearWindAreDrainedInArrivalOrder)
{
    CoW::Weather::WeatherPreset preset;
    snprintf(preset.id, sizeof(preset.id), "drought");
    ASSERT_TRUE(stage.QueuePreset(preset));
    ASSERT_TRUE(stage.QueueBiomeChange(4, 2));
    CoW::Weather::WeatherStateRecord record;
    record.cellCount = 48;
    ASSERT_TRUE(stage.QueueStateReplace(record));
    ASSERT_TRUE(stage.QueueClearWindOverrides());
    EXPECT_EQ(4u, stage.QueueCount());
    Step(0);
    EXPECT_EQ(0u, stage.QueueCount());
    const int a = Log().IndexOf("preset 'drought' drained");
    const int b = Log().IndexOf("biome change for cell 4 drained");
    const int c = Log().IndexOf("state replace drained");
    const int d = Log().IndexOf("clear-wind-overrides drained");
    EXPECT_GE(a, 0);
    EXPECT_LT(a, b);
    EXPECT_LT(b, c);
    EXPECT_LT(c, d);
    EXPECT_TRUE(stage.IsReady());   // none of them fails the stage
}

TEST_F(WeatherQueueTest, ForcingDoesNotChangeWhatAReaderSeesUntilCommit)
{
    Step(0);
    RefSim before(Snapshot(), kSpinUpWeeks);
    before.Live(0);
    ASSERT_TRUE(Matches(before));
    ASSERT_TRUE(stage.QueueForcing(Pressure(9, 0.95f, 3)));
    stage.Capture(static_cast<Dia::DeferredStep::StepKey>(CoW::kDaysPerWeek));
    stage.Run(exec);
    EXPECT_TRUE(Matches(before));   // committed fields are unchanged between Capture/Run and Commit
    stage.Commit();
    EXPECT_FALSE(Matches(before));
}
