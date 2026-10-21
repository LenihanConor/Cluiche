// TestCellFieldGapsBgfxObservation.cpp - Task 26B gap tests for CellFieldObservation (metrics + health)
// and the GPU-free parts of the Canvas CellField wiring.
#include <gtest/gtest.h>

#include <DiaBgfx/Canvas.h>
#include <DiaBgfx/Renderers/CellFieldObservation.h>
#include <DiaBgfx/Renderers/CellFieldRenderer.h>
#include <DiaGraphics/CellField/CellFieldRegistry.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Metric/MetricRegistry.h>

#include <stdint.h>
#include <string.h>

using namespace Dia::Bgfx;
using namespace Dia::Graphics;
namespace Obs = Dia::Observation;

namespace
{
    struct Metrics
    {
        Obs::Metric::Counter c[11];
        Obs::Metric::Gauge   g[5];
        CellFieldMetrics Bind()
        {
            CellFieldMetrics m;
            m.uploads = &c[0]; m.uploadedBytes = &c[1]; m.geometryBuilds = &c[2]; m.droppedCommands = &c[3];
            m.coalescedCommands = &c[4]; m.staleHandles = &c[5]; m.unknownColourMaps = &c[6];
            m.outOfRangeCells = &c[7]; m.unbuiltLayerRequests = &c[8]; m.rejectedRequests = &c[9]; m.drawCalls = &c[10];
            m.queueDepth = &g[0]; m.queueBytes = &g[1]; m.layersLive = &g[2]; m.geometryBytes = &g[3]; m.texturesResident = &g[4];
            return m;
        }
    };

    CellFieldObservationInput Input(const CellFieldStats& r, const CellFieldRendererStats& s, bool programValid = true, bool registrySet = true)
    {
        CellFieldObservationInput in;
        in.registry = r;
        in.renderer = s;
        in.registrySet = registrySet;
        in.programValid = programValid;
        return in;
    }

    const Obs::Health::HealthStatus kOK = Obs::Health::HealthStatus::kOK;
    const Obs::Health::HealthStatus kDegraded = Obs::Health::HealthStatus::kDegraded;
    const Obs::Health::HealthStatus kFailing = Obs::Health::HealthStatus::kFailing;

    Obs::Health::Health Status(CellFieldObservation& o) { return o.GetHealthReporter().Report(); }

    // Feeds `n` condition-free updates.
    void Clear(CellFieldObservation& o, const CellFieldStats& r, const CellFieldRendererStats& s, uint32_t n, bool programValid = true)
    {
        for (uint32_t i = 0; i < n; ++i)
            o.Update(Input(r, s, programValid));
    }
}

// ===========================================================================
// Metrics publishing
// ===========================================================================

TEST(CellFieldGapsObservation, CounterDeltaWhenStatsDecreasePublishesTheNewValueWhole)
{
    Metrics m;
    CellFieldObservation obs;
    obs.SetMetrics(m.Bind());
    CellFieldStats r;
    CellFieldRendererStats s;
    r.uploads = 10; s.drawCalls = 7; r.droppedCommands = 4;
    obs.Update(Input(r, s));
    EXPECT_EQ(10u, m.c[0].Value());
    EXPECT_EQ(7u, m.c[10].Value());

    r.uploads = 3; s.drawCalls = 2; r.droppedCommands = 1;   // source restarted (new registry/renderer)
    obs.Update(Input(r, s));
    EXPECT_EQ(13u, m.c[0].Value());
    EXPECT_EQ(9u, m.c[10].Value());
    EXPECT_EQ(5u, m.c[3].Value());

    r.uploads = 0; s.drawCalls = 0;                          // reset to zero publishes nothing and re-baselines
    obs.Update(Input(r, s));
    EXPECT_EQ(13u, m.c[0].Value());
    r.uploads = 5;
    obs.Update(Input(r, s));
    EXPECT_EQ(18u, m.c[0].Value());
}

TEST(CellFieldGapsObservation, DecreasingStatsDoNotRaiseHealthConditions)
{
    CellFieldObservation obs;
    CellFieldStats r;
    CellFieldRendererStats s;
    r.droppedCommands = 5; s.unknownColourMaps = 5; s.unbuiltLayerRequests = 5;
    obs.Update(Input(r, s));
    EXPECT_EQ(kDegraded, Status(obs).status);
    Clear(obs, r, s, CellFieldObservation::kRecoveryFrames);
    EXPECT_EQ(kOK, Status(obs).status);
    r.droppedCommands = 0; s.unknownColourMaps = 0; s.unbuiltLayerRequests = 0;   // a reset is not a new drop
    obs.Update(Input(r, s));
    EXPECT_EQ(kOK, Status(obs).status);
}

TEST(CellFieldGapsObservation, FirstUpdateAfterConstructionWithNoMetricsBoundIsSafe)
{
    CellFieldObservation obs;   // never SetMetrics: every pointer null
    CellFieldStats r;
    CellFieldRendererStats s;
    r.uploads = 99; r.uploadedBytes = 1000; s.drawCalls = 3; s.geometryBytes = 5; s.colourMapsResident = 2;
    obs.Update(Input(r, s));
    EXPECT_EQ(kOK, Status(obs).status);
    obs.Update(Input(r, s));
    SUCCEED();
}

TEST(CellFieldGapsObservation, PartiallyBoundMetricsPublishOnlyTheBoundOnes)
{
    Metrics m;
    CellFieldMetrics partial;
    partial.uploads = &m.c[0];
    partial.layersLive = &m.g[2];
    EXPECT_FALSE(partial.IsBound());
    CellFieldObservation obs;
    obs.SetMetrics(partial);
    CellFieldStats r;
    CellFieldRendererStats s;
    r.uploads = 4; r.layersLive = 3; r.droppedCommands = 2; s.drawCalls = 8;
    obs.Update(Input(r, s));
    EXPECT_EQ(4u, m.c[0].Value());
    EXPECT_EQ(3.0, m.g[2].Value());
    EXPECT_EQ(0u, m.c[3].Value());    // unbound counters untouched
    EXPECT_EQ(0u, m.c[10].Value());
    EXPECT_EQ(0.0, m.g[0].Value());
    EXPECT_EQ(kDegraded, Status(obs).status);   // health does not need bound metrics
}

TEST(CellFieldGapsObservation, IsBoundRequiresEverySixteenPointers)
{
    Metrics m;
    EXPECT_TRUE(m.Bind().IsBound());
    EXPECT_FALSE(CellFieldMetrics().IsBound());
    Obs::Metric::Counter* CellFieldMetrics::* counters[] = {
        &CellFieldMetrics::uploads, &CellFieldMetrics::uploadedBytes, &CellFieldMetrics::geometryBuilds,
        &CellFieldMetrics::droppedCommands, &CellFieldMetrics::coalescedCommands, &CellFieldMetrics::staleHandles,
        &CellFieldMetrics::unknownColourMaps, &CellFieldMetrics::outOfRangeCells, &CellFieldMetrics::unbuiltLayerRequests,
        &CellFieldMetrics::rejectedRequests, &CellFieldMetrics::drawCalls };
    Obs::Metric::Gauge* CellFieldMetrics::* gauges[] = {
        &CellFieldMetrics::queueDepth, &CellFieldMetrics::queueBytes, &CellFieldMetrics::layersLive,
        &CellFieldMetrics::geometryBytes, &CellFieldMetrics::texturesResident };
    int missing = 0;
    for (auto member : counters)
    {
        CellFieldMetrics b = m.Bind();
        b.*member = nullptr;
        EXPECT_FALSE(b.IsBound());
        ++missing;
    }
    for (auto member : gauges)
    {
        CellFieldMetrics b = m.Bind();
        b.*member = nullptr;
        EXPECT_FALSE(b.IsBound());
        ++missing;
    }
    EXPECT_EQ(16, missing);
}

TEST(CellFieldGapsObservation, HugeCounterDeltasPublishWithoutOverflowAndKeepAccumulating)
{
    Metrics m;
    CellFieldObservation obs;
    obs.SetMetrics(m.Bind());
    CellFieldStats r;
    CellFieldRendererStats s;
    r.uploadedBytes = 1ull << 62;
    obs.Update(Input(r, s));
    EXPECT_EQ(1ull << 62, m.c[1].Value());
    r.uploadedBytes += 1;
    obs.Update(Input(r, s));
    EXPECT_EQ((1ull << 62) + 1ull, m.c[1].Value());
    r.uploadedBytes = (1ull << 63);
    obs.Update(Input(r, s));
    EXPECT_EQ((1ull << 63), m.c[1].Value());
    obs.Update(Input(r, s));   // unchanged: no double count
    EXPECT_EQ((1ull << 63), m.c[1].Value());
}

TEST(CellFieldGapsObservation, SetMetricsRebaselinesSoTheFirstUpdatePublishesTheCumulativeValue)
{
    // Characterisation: SetMetrics zeroes the baselines, so counters bound AFTER stats accumulated
    // receive the full cumulative value on the first Update (and also raise the matching condition).
    Metrics first, second;
    CellFieldObservation obs;
    obs.SetMetrics(first.Bind());
    CellFieldStats r;
    CellFieldRendererStats s;
    r.uploads = 6;
    obs.Update(Input(r, s));
    EXPECT_EQ(6u, first.c[0].Value());
    obs.SetMetrics(second.Bind());
    obs.Update(Input(r, s));
    EXPECT_EQ(6u, second.c[0].Value());
    EXPECT_EQ(6u, first.c[0].Value());   // the old primitives are no longer touched
}

TEST(CellFieldGapsObservation, GaugesFollowCurrentValuesUpAndDown)
{
    Metrics m;
    CellFieldObservation obs;
    obs.SetMetrics(m.Bind());
    CellFieldStats r;
    CellFieldRendererStats s;
    r.commandsQueued = 9; r.queuedBytes = 1234; r.layersLive = 5;
    s.geometryBytes = 777; s.colourMapsResident = 2; s.valueTexturesResident = 6;
    obs.Update(Input(r, s));
    EXPECT_EQ(9.0, m.g[0].Value());
    EXPECT_EQ(1234.0, m.g[1].Value());
    EXPECT_EQ(5.0, m.g[2].Value());
    EXPECT_EQ(777.0, m.g[3].Value());
    EXPECT_EQ(8.0, m.g[4].Value());
    r = CellFieldStats(); s = CellFieldRendererStats();
    obs.Update(Input(r, s));
    for (int i = 0; i < 5; ++i)
        EXPECT_EQ(0.0, m.g[i].Value()) << i;
}

TEST(CellFieldGapsObservation, EachCounterMapsToItsOwnStatField)
{
    Metrics m;
    CellFieldObservation obs;
    obs.SetMetrics(m.Bind());
    CellFieldStats r;
    CellFieldRendererStats s;
    r.uploads = 1; r.uploadedBytes = 2; r.geometryBuilds = 3; r.droppedCommands = 4; r.coalescedCommands = 5;
    r.staleHandles = 6; s.unknownColourMaps = 7; s.outOfRangeCategoryCells = 8; s.unbuiltLayerRequests = 9;
    r.rejectedRequests = 10; s.drawCalls = 11;
    obs.Update(Input(r, s));
    for (int i = 0; i < 11; ++i)
        EXPECT_EQ(static_cast<uint64_t>(i + 1), m.c[i].Value()) << i;
}

TEST(CellFieldGapsObservation, RegisterMetricsIsIdempotentAndReturnsTheSamePrimitives)
{
    Obs::Metric::MetricRegistry& reg = Obs::Metric::MetricRegistry::Instance();
    const CellFieldMetrics a = RegisterCellFieldMetrics(reg);
    const CellFieldMetrics b = RegisterCellFieldMetrics(reg);
    EXPECT_TRUE(a.IsBound());
    EXPECT_EQ(a.uploads, b.uploads);
    EXPECT_EQ(a.drawCalls, b.drawCalls);
    EXPECT_EQ(a.queueDepth, b.queueDepth);
    EXPECT_EQ(a.texturesResident, b.texturesResident);
}

// ===========================================================================
// Health state machine
// ===========================================================================

TEST(CellFieldGapsObservation, InitialReportIsOkWithZeroCountsAndReporterName)
{
    CellFieldObservation obs;
    const Obs::Health::Health h = Status(obs);
    EXPECT_EQ(kOK, h.status);
    EXPECT_EQ(0u, h.errors);
    EXPECT_EQ(0u, h.warnings);
    EXPECT_EQ(Dia::Core::StringCRC("DiaBgfx.CellField"), obs.GetHealthReporter().GetReporterName());
}

TEST(CellFieldGapsObservation, RecoveryCompletesExactlyOnTheLastClearFrame)
{
    CellFieldObservation obs;
    CellFieldStats r;
    CellFieldRendererStats s;
    r.droppedCommands = 1;
    obs.Update(Input(r, s));
    ASSERT_EQ(kDegraded, Status(obs).status);
    Clear(obs, r, s, CellFieldObservation::kRecoveryFrames - 1);
    EXPECT_EQ(kDegraded, Status(obs).status);
    obs.Update(Input(r, s));   // the 120th clear frame
    EXPECT_EQ(kOK, Status(obs).status);
    Clear(obs, r, s, 500);
    EXPECT_EQ(kOK, Status(obs).status);
}

TEST(CellFieldGapsObservation, ConditionOnTheVeryFrameRecoveryWouldCompleteKeepsDegradedAndRestartsTheWindow)
{
    CellFieldObservation obs;
    CellFieldStats r;
    CellFieldRendererStats s;
    r.droppedCommands = 1;
    obs.Update(Input(r, s));
    Clear(obs, r, s, CellFieldObservation::kRecoveryFrames - 1);
    r.droppedCommands = 2;   // new drop exactly when the 120th clear frame would have arrived
    obs.Update(Input(r, s));
    EXPECT_EQ(kDegraded, Status(obs).status);
    EXPECT_EQ(1u, Status(obs).warnings);   // same reason: not logged/counted again
    Clear(obs, r, s, CellFieldObservation::kRecoveryFrames - 1);
    EXPECT_EQ(kDegraded, Status(obs).status);   // the window really restarted
    obs.Update(Input(r, s));
    EXPECT_EQ(kOK, Status(obs).status);
}

TEST(CellFieldGapsObservation, ConditionRightAfterRecoveryDegradesAgainAndCountsAnotherWarning)
{
    CellFieldObservation obs;
    CellFieldStats r;
    CellFieldRendererStats s;
    r.droppedCommands = 1;
    obs.Update(Input(r, s));
    Clear(obs, r, s, CellFieldObservation::kRecoveryFrames);
    ASSERT_EQ(kOK, Status(obs).status);
    r.droppedCommands = 2;
    obs.Update(Input(r, s));
    EXPECT_EQ(kDegraded, Status(obs).status);
    EXPECT_EQ(2u, Status(obs).warnings);
}

TEST(CellFieldGapsObservation, ConditionPriorityAndReasonSwitching)
{
    CellFieldObservation obs;
    CellFieldStats r;
    CellFieldRendererStats s;
    // All three degraded conditions in one update: dropped commands win.
    r.droppedCommands = 1; s.unbuiltLayerRequests = 1; s.unknownColourMaps = 1;
    obs.Update(Input(r, s));
    EXPECT_EQ(Dia::Core::StringCRC("cellfield.dropped_commands"), Status(obs).reason);
    EXPECT_EQ(1u, Status(obs).warnings);
    // Dropped stops; unbuilt continues to rise: reason switches, warning counted.
    s.unbuiltLayerRequests = 2;
    obs.Update(Input(r, s));
    EXPECT_EQ(Dia::Core::StringCRC("cellfield.unbuilt_layers"), Status(obs).reason);
    EXPECT_EQ(2u, Status(obs).warnings);
    // Same reason repeating does not add warnings.
    s.unbuiltLayerRequests = 3;
    obs.Update(Input(r, s));
    EXPECT_EQ(2u, Status(obs).warnings);
    // Unknown colour map rising alone.
    s.unknownColourMaps = 2;
    obs.Update(Input(r, s));
    EXPECT_EQ(Dia::Core::StringCRC("cellfield.unknown_colour_maps"), Status(obs).reason);
    EXPECT_EQ(3u, Status(obs).warnings);
    EXPECT_EQ(kDegraded, Status(obs).status);
}

TEST(CellFieldGapsObservation, FailingBeatsEveryDegradedConditionAndCountsOneError)
{
    CellFieldObservation obs;
    CellFieldStats r;
    CellFieldRendererStats s;
    r.droppedCommands = 1; s.unbuiltLayerRequests = 1; s.unknownColourMaps = 1;
    obs.Update(Input(r, s, false));
    const Obs::Health::Health h = Status(obs);
    EXPECT_EQ(kFailing, h.status);
    EXPECT_EQ(Dia::Core::StringCRC("cellfield.program_missing"), h.reason);
    EXPECT_EQ(1u, h.errors);
    EXPECT_EQ(0u, h.warnings);
    // Staying failing does not add errors.
    r.droppedCommands = 2;
    obs.Update(Input(r, s, false));
    EXPECT_EQ(1u, Status(obs).errors);
    EXPECT_EQ(kFailing, Status(obs).status);
}

TEST(CellFieldGapsObservation, FailingThenProgramReturnsWithNewDropDegradesImmediately)
{
    CellFieldObservation obs;
    CellFieldStats r;
    CellFieldRendererStats s;
    obs.Update(Input(r, s, false));
    ASSERT_EQ(kFailing, Status(obs).status);
    r.droppedCommands = 1;
    obs.Update(Input(r, s, true));
    EXPECT_EQ(kDegraded, Status(obs).status);
    EXPECT_EQ(Dia::Core::StringCRC("cellfield.dropped_commands"), Status(obs).reason);
    EXPECT_EQ(1u, Status(obs).errors);     // cumulative error count survives
    EXPECT_EQ(1u, Status(obs).warnings);
}

TEST(CellFieldGapsObservation, ProgramValidFlappingStaysFailingUntilAFullCleanWindow)
{
    CellFieldObservation obs;
    CellFieldStats r;
    CellFieldRendererStats s;
    for (int i = 0; i < 301; ++i)
    {
        obs.Update(Input(r, s, (i % 2) == 1));   // false, true, false, true ...
        EXPECT_EQ(kFailing, Status(obs).status) << i;
    }
    EXPECT_EQ(1u, Status(obs).errors);           // one Failing episode, not one per flap
    Clear(obs, r, s, CellFieldObservation::kRecoveryFrames - 1, true);
    EXPECT_EQ(kFailing, Status(obs).status);
    obs.Update(Input(r, s, true));
    EXPECT_EQ(kOK, Status(obs).status);

    obs.Update(Input(r, s, false));              // second episode counts again
    EXPECT_EQ(kFailing, Status(obs).status);
    EXPECT_EQ(2u, Status(obs).errors);
}

TEST(CellFieldGapsObservation, ProgramMissingNeedsARegistry)
{
    CellFieldObservation obs;
    CellFieldStats r;
    CellFieldRendererStats s;
    for (int i = 0; i < 10; ++i)
        obs.Update(Input(r, s, false, false));   // no registry set: program state is irrelevant
    EXPECT_EQ(kOK, Status(obs).status);
    EXPECT_EQ(0u, Status(obs).errors);
}

TEST(CellFieldGapsObservation, ReportReasonsAreTheDocumentedStrings)
{
    const char* expected[] = { "cellfield.dropped_commands", "cellfield.unbuilt_layers", "cellfield.unknown_colour_maps" };
    for (int which = 0; which < 3; ++which)
    {
        CellFieldObservation obs;
        CellFieldStats r;
        CellFieldRendererStats s;
        if (which == 0) r.droppedCommands = 1;
        if (which == 1) s.unbuiltLayerRequests = 1;
        if (which == 2) s.unknownColourMaps = 1;
        obs.Update(Input(r, s));
        EXPECT_EQ(Dia::Core::StringCRC(expected[which]), Status(obs).reason) << which;
        EXPECT_EQ(kDegraded, Status(obs).status) << which;
    }
}

TEST(CellFieldGapsObservation, OutOfRangeCellsAndStaleHandlesAreMetricsOnlyNotHealthConditions)
{
    CellFieldObservation obs;
    CellFieldStats r;
    CellFieldRendererStats s;
    r.staleHandles = 50; r.rejectedRequests = 9; r.coalescedCommands = 4; s.outOfRangeCategoryCells = 1000;
    obs.Update(Input(r, s));
    EXPECT_EQ(kOK, Status(obs).status);
}

// ===========================================================================
// Canvas wiring (GPU-free parts)
// ===========================================================================

TEST(CellFieldGapsCanvas, NullRegistryIsAcceptedAndClearsAPreviousOne)
{
    Canvas canvas;
    CellFieldRegistry registry;
    canvas.SetCellFieldRegistry(&registry);
    EXPECT_EQ(&registry, canvas.GetCellFieldRegistry());
    canvas.SetCellFieldRegistry(nullptr);
    EXPECT_EQ(nullptr, canvas.GetCellFieldRegistry());
    EXPECT_FALSE(canvas.HasCellFieldRenderer());
}

TEST(CellFieldGapsCanvas, LastRegistrySetBeforeInitWins)
{
    Canvas canvas;
    CellFieldRegistry a, b;
    canvas.SetCellFieldRegistry(&a);
    canvas.SetCellFieldRegistry(&b);
    EXPECT_EQ(&b, canvas.GetCellFieldRegistry());
}

TEST(CellFieldGapsCanvas, RendererStatsWithNoRendererAreZeroedFieldByField)
{
    Canvas canvas;
    CellFieldRendererStats stats;
    stats.unknownColourMaps = 1; stats.outOfRangeCategoryCells = 2; stats.colourMapUploads = 3;
    stats.colourMapsResident = 4; stats.drawCalls = 5; stats.backdropDraws = 6; stats.unbuiltLayerRequests = 7;
    stats.valueTexturesResident = 8; stats.geometryBytes = 9; stats.geometryEntries = 10;
    EXPECT_FALSE(canvas.GetCellFieldRendererStats(stats));
    EXPECT_EQ(0u, stats.unknownColourMaps);
    EXPECT_EQ(0u, stats.outOfRangeCategoryCells);
    EXPECT_EQ(0u, stats.colourMapUploads);
    EXPECT_EQ(0u, stats.colourMapsResident);
    EXPECT_EQ(0u, stats.drawCalls);
    EXPECT_EQ(0u, stats.backdropDraws);
    EXPECT_EQ(0u, stats.unbuiltLayerRequests);
    EXPECT_EQ(0u, stats.valueTexturesResident);
    EXPECT_EQ(0u, stats.geometryBytes);
    EXPECT_EQ(0u, stats.geometryEntries);

    // Same with a registry set but no GPU init (renderer is created at DeferredInit only).
    CellFieldRegistry registry;
    canvas.SetCellFieldRegistry(&registry);
    stats.drawCalls = 99;
    EXPECT_FALSE(canvas.GetCellFieldRendererStats(stats));
    EXPECT_EQ(0u, stats.drawCalls);
}

TEST(CellFieldGapsCanvas, MetricsAndHealthAccessibleBeforeInit)
{
    Canvas canvas;
    Metrics m;
    canvas.SetCellFieldMetrics(m.Bind());
    canvas.SetCellFieldMetrics(CellFieldMetrics());   // unbinding is also fine
    EXPECT_EQ(kOK, canvas.GetCellFieldHealthReporter().Report().status);
    EXPECT_EQ(Dia::Core::StringCRC("DiaBgfx.CellField"), canvas.GetCellFieldHealthReporter().GetReporterName());
    EXPECT_EQ(0u, canvas.GetCellFieldHealthReporter().Report().errors);
}

TEST(CellFieldGapsCanvas, ShutdownBeforeInitTwiceWithRegistrySetIsSafe)
{
    Canvas canvas;
    CellFieldRegistry registry;
    canvas.SetCellFieldRegistry(&registry);
    canvas.Shutdown();
    canvas.Shutdown();
    EXPECT_FALSE(canvas.HasCellFieldRenderer());
    EXPECT_EQ(&registry, canvas.GetCellFieldRegistry());
}

TEST(CellFieldGapsCanvas, NotInitialisedUntilGpuInit)
{
    Canvas canvas;
    CellFieldRegistry registry;
    canvas.SetCellFieldRegistry(&registry);   // legal only before init (DIA_ASSERT after; needs a GPU to test)
    EXPECT_FALSE(canvas.IsInitialised());
    EXPECT_EQ(7, canvas.GetImGuiViewId());    // CellField view 8 is ordered before ImGui's 7 via setViewOrder
}
