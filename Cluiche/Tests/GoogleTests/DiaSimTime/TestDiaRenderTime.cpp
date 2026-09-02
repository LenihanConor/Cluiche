////////////////////////////////////////////////////////////////////////////////
// Filename: TestDiaRenderTime.cpp
// GoogleTest suite — DiaSimTime Task C.1 / 1.7: DiaMainTime + DiaRenderTime modules.
//
// Covers:
//   - DiaRenderTime::DoUpdate with no data published on "SimTime" yet: the
//     owning ProcessingUnit's RenderTimeContext stays at its default — no crash.
//   - Once a SimTimeContext is written to the "SimTime" FrameStream, DiaRenderTime
//     reads it and pushes a RenderTimeContext (carrying the written simTime and an
//     incrementing renderFrame) into the owning ProcessingUnit.
//   - DiaMainTime starts/stops/ticks cleanly through a real Application (it does
//     no real work — MainTimeContext is already computed directly by ProcessingUnit).
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaSimTime/DiaMainTime.h>
#include <DiaSimTime/DiaRenderTime.h>
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/RenderModule.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaStreams/StreamWriter.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Time/TimeRelative.h>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace Dia::Core::Containers;
using Dia::SimTime::SimTimeContext;
using Dia::SimTime::RenderTimeContext;
using Dia::SimTime::DiaMainTime;
using Dia::SimTime::DiaRenderTime;

namespace {

    // ---------------------------------------------------------------------------
    // Test-local helper: writes a SimTimeContext to the "SimTime" FrameStream on
    // command. Stands in for the (not-yet-built, later-phase) DiaSimTimeModule.
    // No kAllowedPUs declared, so TypeRegistry defaults it to kAny and it can sit
    // on any named PU (including RenderPU) alongside DiaRenderTime.
    // ---------------------------------------------------------------------------
    struct SimTimeWriterModule : SimModule
    {
        using SimModule::SimModule;
        static const StringCRC kTypeId;

        StreamWriter<SimTimeContext> mWriter{this, StringCRC("SimTime")};
        bool           pendingWrite = false;
        SimTimeContext pendingCtx{ Dia::Core::TimeAbsolute::Zero(), Dia::Core::TimeRelative::Zero() };

        void OnConnectStreams(Application& app) override { mWriter.Connect(app); }
        StartResult DoStart() override { return StartResult::kReady; }
        void DoUpdate(const Dia::SimTime::SimTimeContext&) override
        {
            if (pendingWrite)
            {
                mWriter.Write(pendingCtx, pendingCtx.gameTime);
                pendingWrite = false;
            }
        }
        StopResult DoStop() override { return StopResult::kDone; }
    };
    const StringCRC SimTimeWriterModule::kTypeId("RT_SimTimeWriterModule");

    // ---------------------------------------------------------------------------
    // Test-local helper: a sibling RenderModule placed AFTER DiaRenderTime in the
    // RenderPU's module array. Records whatever GetProcessingUnit()->GetRenderTimeContext()
    // returns each tick — this is how a real sibling RenderModule would observe the
    // context DiaRenderTime just pushed (Application does not expose ProcessingUnit*
    // directly to tests, so a module recording its own reads is the established pattern;
    // see TestPUTypedModules.cpp's RecordingSimModule).
    // ---------------------------------------------------------------------------
    struct RecordingSiblingModule : RenderModule
    {
        using RenderModule::RenderModule;
        static const StringCRC kTypeId;

        int               updateCalls = 0;
        RenderTimeContext lastCtx{ 0.0f, Dia::Core::TimeAbsolute::Zero(), 0 };

        StartResult DoStart() override { return StartResult::kReady; }
        StopResult  DoStop()  override { return StopResult::kDone; }
        void        DoUpdate(const RenderTimeContext& ctx) override
        {
            ++updateCalls;
            lastCtx = ctx;
        }
    };
    const StringCRC RecordingSiblingModule::kTypeId("RT_RecordingSiblingModule");

    SimTimeWriterModule*      g_writer   = nullptr;
    RecordingSiblingModule*   g_sibling  = nullptr;
    DiaRenderTime*            g_renderTime = nullptr;
    DiaMainTime*              g_mainTime   = nullptr;

    Module* CreateWriter(const StringCRC& id)
    {
        auto* m = new SimTimeWriterModule(id);
        g_writer = m;
        return m;
    }

    Module* CreateSibling(const StringCRC& id)
    {
        auto* m = new RecordingSiblingModule(id);
        g_sibling = m;
        return m;
    }

    Module* CreateRenderTime(const StringCRC& id)
    {
        auto* m = new DiaRenderTime(id);
        g_renderTime = m;
        return m;
    }

    Module* CreateMainTime(const StringCRC& id)
    {
        auto* m = new DiaMainTime(id);
        g_mainTime = m;
        return m;
    }

    // Pump Update() until every module on puId is kActive.
    bool PumpUntilAllActive(Application& app, const StringCRC& puId, int limit = 100)
    {
        for (int i = 0; i < limit; ++i)
        {
            DynamicArrayC<ModuleStateInfo, 64> infos;
            app.GetActiveModules(puId, infos);
            if (infos.Size() > 0)
            {
                bool allActive = true;
                for (unsigned int m = 0; m < infos.Size(); ++m)
                {
                    if (infos[m].state != ModuleState::kActive)
                    {
                        allActive = false;
                        break;
                    }
                }
                if (allActive) return true;
            }
            app.Update(1.0f / 60.0f);
        }
        return false;
    }

    ModuleDeclaration MakeModule(const StringCRC& typeId, const StringCRC& instanceId, const StringCRC& stageName)
    {
        ModuleDeclaration mod;
        mod.instanceId     = instanceId;
        mod.typeId         = typeId;
        mod.startTimeoutMs = 10000.0f;
        mod.stopTimeoutMs  = 5000.0f;
        mod.stages.Add(stageName);
        return mod;
    }

} // anonymous namespace

// ---------------------------------------------------------------------------
// DiaRenderTime: no publisher yet — FetchLatest() is nullptr, no crash, default stays.
// ---------------------------------------------------------------------------
TEST(DiaRenderTimeTest, NoPublisherYetLeavesDefaultRenderTimeContext)
{
    g_sibling = nullptr;
    g_renderTime = nullptr;

    TypeRegistry reg;
    reg.Register(DiaRenderTime::kTypeId, CreateRenderTime);
    reg.Register(RecordingSiblingModule::kTypeId, CreateSibling);

    ApplicationManifestV3 manifest;
    manifest.version = 3;

    StageDeclaration boot;
    boot.name = StringCRC("Boot");
    manifest.stages.Add(boot);
    manifest.initialStage = StringCRC("Boot");

    StreamDeclaration sd;
    sd.id   = StringCRC("SimTime");
    sd.kind = StringCRC("FrameStream");
    manifest.streams.Add(sd);

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("RenderPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;
    pu.modules.Add(MakeModule(DiaRenderTime::kTypeId, StringCRC("renderTime"), StringCRC("Boot")));
    pu.modules.Add(MakeModule(RecordingSiblingModule::kTypeId, StringCRC("sibling"), StringCRC("Boot")));
    manifest.processingUnits.Add(pu);

    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    ASSERT_TRUE(PumpUntilAllActive(app, StringCRC("RenderPU"), 50));
    ASSERT_NE(g_renderTime, nullptr);
    ASSERT_NE(g_sibling, nullptr);

    app.Update(1.0f / 60.0f);

    EXPECT_EQ(g_sibling->lastCtx.renderFrame, 0u)
        << "With no publisher, DiaRenderTime must not push a RenderTimeContext — sibling should still see the default";
    EXPECT_TRUE(g_sibling->lastCtx.simTime == Dia::Core::TimeAbsolute::Zero())
        << "Default RenderTimeContext.simTime should remain Zero() with no publisher";
}

// ---------------------------------------------------------------------------
// DiaRenderTime: once a SimTimeContext is written, it is read and pushed.
// ---------------------------------------------------------------------------
TEST(DiaRenderTimeTest, ReadsPublishedSimTimeAndPushesRenderTimeContext)
{
    g_writer = nullptr;
    g_renderTime = nullptr;
    g_sibling = nullptr;

    TypeRegistry reg;
    reg.Register(SimTimeWriterModule::kTypeId, CreateWriter);
    reg.Register(DiaRenderTime::kTypeId, CreateRenderTime);
    reg.Register(RecordingSiblingModule::kTypeId, CreateSibling);

    ApplicationManifestV3 manifest;
    manifest.version = 3;

    StageDeclaration boot;
    boot.name = StringCRC("Boot");
    manifest.stages.Add(boot);
    manifest.initialStage = StringCRC("Boot");

    StreamDeclaration sd;
    sd.id   = StringCRC("SimTime");
    sd.kind = StringCRC("FrameStream");
    manifest.streams.Add(sd);

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("RenderPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;
    // Order matters: writer runs first so DiaRenderTime observes the write this same tick,
    // then the recording sibling runs after DiaRenderTime to observe the pushed context.
    pu.modules.Add(MakeModule(SimTimeWriterModule::kTypeId, StringCRC("writer"), StringCRC("Boot")));
    pu.modules.Add(MakeModule(DiaRenderTime::kTypeId, StringCRC("renderTime"), StringCRC("Boot")));
    pu.modules.Add(MakeModule(RecordingSiblingModule::kTypeId, StringCRC("sibling"), StringCRC("Boot")));
    manifest.processingUnits.Add(pu);

    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    ASSERT_TRUE(PumpUntilAllActive(app, StringCRC("RenderPU"), 50));
    ASSERT_NE(g_writer, nullptr);
    ASSERT_NE(g_renderTime, nullptr);
    ASSERT_NE(g_sibling, nullptr);

    const Dia::Core::TimeAbsolute writtenTime = Dia::Core::TimeAbsolute::CreateFromMilliseconds(1234);
    g_writer->pendingCtx.gameTime  = writtenTime;
    g_writer->pendingCtx.gameDt    = Dia::Core::TimeRelative::CreateFromMilliseconds(16);
    g_writer->pendingCtx.tick      = 1;
    g_writer->pendingCtx.timeScale = 1.0f;
    g_writer->pendingCtx.isPaused  = false;
    g_writer->pendingWrite = true;

    app.Update(1.0f / 60.0f);

    EXPECT_EQ(g_sibling->lastCtx.renderFrame, 1u)
        << "DiaRenderTime should increment renderFrame the first time it observes a publisher";
    EXPECT_TRUE(g_sibling->lastCtx.simTime == writtenTime)
        << "DiaRenderTime should carry forward the sim-time snapshot it read from the stream";

    // A second tick with no new write should still carry the last-fetched value
    // (FrameStreamStore::FetchLatest keeps returning the latest write) and bump renderFrame again.
    app.Update(1.0f / 60.0f);
    EXPECT_EQ(g_sibling->lastCtx.renderFrame, 2u);
    EXPECT_TRUE(g_sibling->lastCtx.simTime == writtenTime);
}

// ---------------------------------------------------------------------------
// DiaMainTime: starts/stops/ticks cleanly through a real Application.
// There is nothing else to assert — it does no real work; MainTimeContext is
// already computed directly by ProcessingUnit::Update().
// ---------------------------------------------------------------------------
TEST(DiaMainTimeTest, StartsUpdatesAndStopsCleanly)
{
    g_mainTime = nullptr;

    TypeRegistry reg;
    reg.Register(DiaMainTime::kTypeId, CreateMainTime);

    ApplicationManifestV3 manifest;
    manifest.version = 3;

    StageDeclaration boot;
    boot.name = StringCRC("Boot");
    manifest.stages.Add(boot);
    manifest.initialStage = StringCRC("Boot");

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;
    pu.modules.Add(MakeModule(DiaMainTime::kTypeId, StringCRC("mainTime"), StringCRC("Boot")));
    manifest.processingUnits.Add(pu);

    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    ASSERT_TRUE(PumpUntilAllActive(app, StringCRC("MainPU"), 50));
    ASSERT_NE(g_mainTime, nullptr);

    // Several ticks should not crash or throw.
    for (int i = 0; i < 5; ++i)
    {
        app.Update(1.0f / 60.0f);
    }

    SUCCEED();
}
