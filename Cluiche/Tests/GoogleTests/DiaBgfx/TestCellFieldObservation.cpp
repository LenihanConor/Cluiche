// TestCellFieldObservation.cpp - CellField metrics mapping, health transitions and rate-limited
// lifecycle logging. GPU-free: fake factories, directly-constructed Counter/Gauge primitives.
#include <gtest/gtest.h>

#include <DiaBgfx/Renderers/CellFieldObservation.h>
#include <DiaBgfx/Renderers/CellFieldRenderer.h>
#include <DiaGraphics/Frame/CellFieldFrameData.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaPathfinding/HexTopology.h>
#include "../DiaGraphics/HexTopologyToCellGridDesc.h"

#include <stdio.h>

using namespace Dia::Bgfx;
using namespace Dia::Graphics;
namespace Obs = Dia::Observation;

namespace
{
    class FakeGeo : public ICellFieldGpuGeometryFactory
    {
    public:
        bool Create(const CellGridGeometry&, CellFieldGpuHandle& out) override { out = nextHandle++; return true; }
        void Destroy(CellFieldGpuHandle) override {}
        uint32_t nextHandle = 1;
    };

    class FakeTex : public ICellFieldGpuTextureFactory
    {
    public:
        bool Create(uint16_t, uint16_t, CellFieldGpuTextureHandle& out) override { out = nextHandle++; return true; }
        void Update(CellFieldGpuTextureHandle, const float*, uint32_t) override {}
        void Destroy(CellFieldGpuTextureHandle) override {}
        uint32_t nextHandle = 1;
    };

    class FakeColourMap : public ICellFieldGpuColourMapFactory
    {
    public:
        bool CreateRgba8(uint32_t, const uint32_t*, uint32_t, bool, CellFieldGpuColourMapHandle& out) override { out = nextHandle++; return true; }
        void UpdateRgba8(CellFieldGpuColourMapHandle, const uint32_t*, uint32_t) override {}
        void Destroy(CellFieldGpuColourMapHandle) override {}
        uint32_t nextHandle = 1;
    };

    struct Fixture
    {
        FakeGeo geo;
        FakeTex tex;
        FakeColourMap cmap;
        CellFieldRegistry registry;
        CellFieldRenderer renderer{ 0, nullptr, &registry, geo, tex, cmap };
        HexDescStorage storage;

        Dia::Core::CellFieldLayerHandle Make(uint32_t w, uint32_t h, CellFieldValueKind kind)
        {
            Dia::Pathfinding::HexTopology topo(w, h);
            CellGridDesc d = HexTopologyToCellGridDesc(topo, 1.0f, storage);
            Dia::Core::CellFieldLayerHandle handle;
            EXPECT_TRUE(registry.CreateLayer(d, kind, handle).ok);
            return handle;
        }
    };

    struct TestMetrics
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

    CellFieldObservationInput Input(const CellFieldStats& r, const CellFieldRendererStats& s, bool programValid = true)
    {
        CellFieldObservationInput in;
        in.registry = r;
        in.renderer = s;
        in.registrySet = true;
        in.programValid = programValid;
        return in;
    }

    const Obs::Health::HealthStatus kOK = Obs::Health::HealthStatus::kOK;
    const Obs::Health::HealthStatus kDegraded = Obs::Health::HealthStatus::kDegraded;
    const Obs::Health::HealthStatus kFailing = Obs::Health::HealthStatus::kFailing;
}

TEST(CellFieldObservation, MetricsMatchStatsAfterScriptedSequence)
{
    Fixture f;
    const uint32_t colours[3] = { 0xFF0000FFu, 0x00FF00FFu, 0x0000FFFFu };
    ASSERT_TRUE(f.registry.RegisterPalette(Dia::Core::StringCRC("terrain"), colours, 3, 0x808080FFu).ok);
    auto h = f.Make(4, 3, CellFieldValueKind::Category);
    float values[12] = { 0, 1, 2, 9, 9, 1, 0, 2, 1, 1, 7, 2 };   // 3 cells outside a 3-colour palette
    ASSERT_TRUE(f.registry.Update(h, values, 12).ok);
    ASSERT_TRUE(f.registry.Update(h, values, 12).ok);              // coalesced
    f.registry.Update(Dia::Core::CellFieldLayerHandle(), values, 12);   // stale handle
    CellGridDesc bad;
    Dia::Core::CellFieldLayerHandle none;
    EXPECT_FALSE(f.registry.CreateLayer(bad, CellFieldValueKind::Float, none).ok);   // rejected

    auto h2 = f.Make(4, 3, CellFieldValueKind::Category);
    f.renderer.DrainRegistry();
    CellFieldFrameData frame;
    Dia::Core::CellFieldDrawRequest ok;
    ok.layer = h;
    ok.colourMap = Dia::Core::StringCRC("terrain").Value();
    frame.RequestDrawCellFieldData(ok);
    f.renderer.Draw(frame);
    CellFieldFrameData frame2;   // a frame accepts one request (Step 1 limit)
    Dia::Core::CellFieldDrawRequest unknown = ok;
    unknown.layer = h2;
    unknown.colourMap = Dia::Core::StringCRC("nope").Value();
    frame2.RequestDrawCellFieldData(unknown);
    f.renderer.Draw(frame2);

    TestMetrics tm;
    CellFieldObservation obs;
    obs.SetMetrics(tm.Bind());
    const CellFieldStats rs = f.registry.GetStats();
    const CellFieldRendererStats ss = f.renderer.GetRendererStats();
    obs.Update(Input(rs, ss));

    EXPECT_EQ(rs.uploads, tm.c[0].Value());
    EXPECT_EQ(rs.uploadedBytes, tm.c[1].Value());
    EXPECT_EQ(rs.geometryBuilds, tm.c[2].Value());
    EXPECT_EQ(rs.droppedCommands, tm.c[3].Value());
    EXPECT_EQ(rs.coalescedCommands, tm.c[4].Value());
    EXPECT_EQ(1u, tm.c[4].Value());
    EXPECT_EQ(rs.staleHandles, tm.c[5].Value());
    EXPECT_GE(tm.c[5].Value(), 1u);
    EXPECT_EQ(1u, tm.c[6].Value());                      // unknown colour map
    EXPECT_EQ(3u, tm.c[7].Value());                      // out-of-range cells
    EXPECT_EQ(rs.rejectedRequests, tm.c[9].Value());
    EXPECT_EQ(1u, tm.c[9].Value());
    EXPECT_EQ(ss.drawCalls, tm.c[10].Value());
    EXPECT_EQ(1u, tm.c[10].Value());
    EXPECT_EQ(2.0, tm.g[2].Value());                     // layers_live
    EXPECT_EQ((double)ss.geometryBytes, tm.g[3].Value());
    EXPECT_GT(tm.g[3].Value(), 0.0);
    EXPECT_EQ(3u, ss.valueTexturesResident);             // two layers + dummy elevation
    EXPECT_EQ((double)(ss.colourMapsResident + ss.valueTexturesResident), tm.g[4].Value());

    // Publishing identical stats again must not double count.
    obs.Update(Input(rs, ss));
    EXPECT_EQ(3u, tm.c[7].Value());
    EXPECT_EQ(rs.uploads, tm.c[0].Value());
}

TEST(CellFieldObservation, QueueGaugesTrackPendingCommands)
{
    Fixture f;
    f.Make(4, 3, CellFieldValueKind::Float);
    TestMetrics tm;
    CellFieldObservation obs;
    obs.SetMetrics(tm.Bind());
    obs.Update(Input(f.registry.GetStats(), f.renderer.GetRendererStats()));
    EXPECT_EQ(1.0, tm.g[0].Value());
    EXPECT_GT(tm.g[1].Value(), 0.0);
    f.renderer.DrainRegistry();
    obs.Update(Input(f.registry.GetStats(), f.renderer.GetRendererStats()));
    EXPECT_EQ(0.0, tm.g[0].Value());
    EXPECT_EQ(0.0, tm.g[1].Value());
}

TEST(CellFieldObservation, RegisterMetricsRegistersAllSixteenNames)
{
    const CellFieldMetrics m = RegisterCellFieldMetrics(Obs::Metric::MetricRegistry::Instance());
    EXPECT_TRUE(m.IsBound());
    auto& reg = Obs::Metric::MetricRegistry::Instance();
    const char* counters[] = { "cellfield.uploads", "cellfield.uploaded_bytes", "cellfield.geometry_builds",
        "cellfield.dropped_commands", "cellfield.coalesced_commands", "cellfield.stale_handles",
        "cellfield.unknown_colour_maps", "cellfield.out_of_range_cells", "cellfield.unbuilt_layer_requests",
        "cellfield.rejected_requests", "cellfield.draw_calls" };
    const char* gauges[] = { "cellfield.queue_depth", "cellfield.queue_bytes", "cellfield.layers_live",
        "cellfield.geometry_bytes", "cellfield.textures_resident" };
    for (const char* n : counters) EXPECT_NE(nullptr, reg.FindCounter(Dia::Core::StringCRC(n))) << n;
    for (const char* n : gauges)   EXPECT_NE(nullptr, reg.FindGauge(Dia::Core::StringCRC(n))) << n;
}

TEST(CellFieldObservation, HealthDegradesOnDroppedCommandsAndRecoversAfterClearFrames)
{
    CellFieldObservation obs;
    CellFieldStats rs;
    CellFieldRendererStats ss;
    obs.Update(Input(rs, ss));
    EXPECT_EQ(kOK, obs.GetHealthReporter().Report().status);

    rs.droppedCommands = 3;
    obs.Update(Input(rs, ss));
    Obs::Health::Health h = obs.GetHealthReporter().Report();
    EXPECT_EQ(kDegraded, h.status);
    EXPECT_EQ(Dia::Core::StringCRC("cellfield.dropped_commands"), h.reason);
    EXPECT_EQ(1u, h.warnings);

    for (uint32_t i = 0; i < CellFieldObservation::kRecoveryFrames - 1; ++i)
        obs.Update(Input(rs, ss));
    EXPECT_EQ(kDegraded, obs.GetHealthReporter().Report().status);
    obs.Update(Input(rs, ss));
    EXPECT_EQ(kOK, obs.GetHealthReporter().Report().status);
}

TEST(CellFieldObservation, NewConditionRestartsRecoveryWindow)
{
    CellFieldObservation obs;
    CellFieldStats rs;
    CellFieldRendererStats ss;
    ss.unknownColourMaps = 1;
    obs.Update(Input(rs, ss));
    EXPECT_EQ(kDegraded, obs.GetHealthReporter().Report().status);
    EXPECT_EQ(Dia::Core::StringCRC("cellfield.unknown_colour_maps"), obs.GetHealthReporter().Report().reason);
    for (uint32_t i = 0; i < CellFieldObservation::kRecoveryFrames - 1; ++i)
        obs.Update(Input(rs, ss));
    ss.unbuiltLayerRequests = 1;   // fresh condition just before recovery
    obs.Update(Input(rs, ss));
    EXPECT_EQ(kDegraded, obs.GetHealthReporter().Report().status);
    EXPECT_EQ(Dia::Core::StringCRC("cellfield.unbuilt_layers"), obs.GetHealthReporter().Report().reason);
    for (uint32_t i = 0; i < CellFieldObservation::kRecoveryFrames - 1; ++i)
        obs.Update(Input(rs, ss));
    EXPECT_EQ(kDegraded, obs.GetHealthReporter().Report().status);
    obs.Update(Input(rs, ss));
    EXPECT_EQ(kOK, obs.GetHealthReporter().Report().status);
}

TEST(CellFieldObservation, HealthFailingWhenProgramMissingThenRecovers)
{
    CellFieldObservation obs;
    CellFieldStats rs;
    CellFieldRendererStats ss;
    obs.Update(Input(rs, ss, false));
    Obs::Health::Health h = obs.GetHealthReporter().Report();
    EXPECT_EQ(kFailing, h.status);
    EXPECT_EQ(Dia::Core::StringCRC("cellfield.program_missing"), h.reason);
    EXPECT_EQ(1u, h.errors);

    for (uint32_t i = 0; i < CellFieldObservation::kRecoveryFrames - 1; ++i)
        obs.Update(Input(rs, ss, true));
    EXPECT_NE(kOK, obs.GetHealthReporter().Report().status);
    obs.Update(Input(rs, ss, true));
    EXPECT_EQ(kOK, obs.GetHealthReporter().Report().status);
}

TEST(CellFieldObservation, NoRegistryMeansNoHealthCondition)
{
    CellFieldObservation obs;
    CellFieldObservationInput in;
    in.registrySet = false;
    in.programValid = false;
    obs.Update(in);
    EXPECT_EQ(kOK, obs.GetHealthReporter().Report().status);
}

TEST(CellFieldObservation, LifecycleLogsAreRateLimitedPerDrainCycle)
{
    CellFieldRegistry registry;
    const uint32_t colour = 0xFFFFFFFFu;
    const uint32_t events = 28;
    for (uint32_t i = 0; i < events; ++i)
    {
        char name[32];
        snprintf(name, sizeof(name), "pal%u", i);
        ASSERT_TRUE(registry.RegisterPalette(Dia::Core::StringCRC(name), &colour, 1, 0).ok);
    }
    CellFieldStats s = registry.GetStats();
    EXPECT_EQ(CellFieldRegistry::kLifecycleLogBudget, s.lifecycleLogsEmitted);
    EXPECT_EQ(events - CellFieldRegistry::kLifecycleLogBudget, s.lifecycleLogsSuppressed);

    struct NullSink : ICellFieldCommandSink
    {
        void OnCreate(Dia::Core::CellFieldLayerHandle, const CellGridDesc&, CellFieldValueKind) override {}
        void OnUpdate(Dia::Core::CellFieldLayerHandle, const float*, uint32_t) override {}
        void OnDestroy(Dia::Core::CellFieldLayerHandle) override {}
    } sink;
    registry.Drain(sink);                        // new cycle: budget refilled
    ASSERT_TRUE(registry.RegisterPalette(Dia::Core::StringCRC("after"), &colour, 1, 0).ok);
    s = registry.GetStats();
    EXPECT_EQ(CellFieldRegistry::kLifecycleLogBudget + 1u, s.lifecycleLogsEmitted);
    EXPECT_EQ(events - CellFieldRegistry::kLifecycleLogBudget, s.lifecycleLogsSuppressed);
}
