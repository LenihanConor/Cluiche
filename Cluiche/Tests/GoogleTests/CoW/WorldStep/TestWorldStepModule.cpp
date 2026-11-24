// World Step Feature 2: module metadata, config, the key source and the ordering-hazard gate, stage registration,
// the IDeferredStep surface, sim-hosted names and observability (module-and-cadence.md AC 1-5, 17, 23-26;
// plan tasks 1-3, 5, 6, 16, 17).
#include "WorldStepTestRig.h"

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/MainModule.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaDebugServer/SimHostedTypes.h>
#include <Modules/DeferredStepModule.h>
#include <DiaDeferredStep/Testing/RecordingStep.h>
#include <DiaStreams/EventStreamReader.h>
#include <DiaStreams/EventStreamWriter.h>
#include <DiaStreams/ServiceStreamWriter.h>
#include <DiaThreading/JobSystem.h>
#include <Modules/MessageBusHostModule.h>

#include <chrono>
#include <cstring>
#include <memory>
#include <thread>

using namespace WsTest;
using Dia::Observation::Health::HealthStatus;
using Dia::DeferredStep::Testing::LogCapture;
using Level = Dia::Observation::Log::LogLevel;

// ---- Module metadata (AC 2) -------------------------------------------------------------------------------------

TEST(WorldStepModule, RegisteredWithTheGlobalTypeRegistryAsSimOnly)
{
    const Dia::ApplicationFlow::TypeRegistry& reg = Dia::ApplicationFlow::TypeRegistry::Global();
    EXPECT_TRUE(reg.Contains(StringCRC("WorldStepModule")));
    EXPECT_EQ(CoW::WorldStepModule::kTypeId, StringCRC("WorldStepModule"));
    EXPECT_EQ(CoW::WorldStepModule::kAllowedPUs, Dia::ApplicationFlow::PUAffinity::kSim);
    // The framework refuses to place a Sim-only type on MainPU (ProcessingUnit::AddModule checks this value).
    EXPECT_EQ(reg.GetAllowedPUs(CoW::WorldStepModule::kTypeId), Dia::ApplicationFlow::PUAffinity::kSim);
    const char* description = reg.GetDescription(CoW::WorldStepModule::kTypeId);
    ASSERT_NE(description, nullptr);
    EXPECT_GT(std::strlen(description), 0u);
}

// ---- Config (AC 1, module half) ---------------------------------------------------------------------------------

TEST(WorldStepModule, ValidConfigStartsAndIsReadBack)
{
    Harness h(Config("weather,ecology", 9, false));
    ASSERT_TRUE(h.started);
    EXPECT_EQ(h.module.GetConfig().applyAfterSteps, 9u);
    EXPECT_FALSE(h.module.GetConfig().deferred);
    EXPECT_EQ(h.module.GetConfig().order.Size(), 2u);
    EXPECT_TRUE(h.module.IsRunning());
}

TEST(WorldStepModule, MissingApplyAfterDaysAndDeferredTakeTheirDefaults)
{
    Harness h("{\"order\":[\"weather\"]}");
    ASSERT_TRUE(h.started);
    EXPECT_EQ(h.module.GetConfig().applyAfterSteps, 7u);
    EXPECT_TRUE(h.module.GetConfig().deferred);
}

TEST(WorldStepModule, InvalidConfigRefusesToStartAndNamesTheKey)
{
    struct Case { const char* json; const char* key; };
    const Case cases[] = {
        { "",                                                              "config" },
        { "{not json",                                                     "config" },
        { "{}",                                                            "order" },
        { "{\"order\":[]}",                                                "order" },
        { "{\"order\":\"weather\"}",                                       "order" },
        { "{\"order\":[\"a\",\"a\"]}",                                     "order" },
        { "{\"order\":[\"a\"],\"apply_after_steps\":0}",                    "apply_after_steps" },
        { "{\"order\":[\"a\"],\"apply_after_steps\":366}",                  "apply_after_steps" },
        { "{\"order\":[\"a\"],\"apply_after_steps\":7.5}",                  "apply_after_steps" },
        { "{\"order\":[\"a\"],\"deferred\":\"true\"}",                     "deferred" },
        { "{\"order\":[\"a\"],\"bogus\":1}",                               "bogus" },
    };
    for (const Case& c : cases)
    {
        SCOPED_TRACE(c.json);
        LogCapture logs("WST");
        Harness h(c.json, false);
        EXPECT_EQ(h.module.CallStart(), Dia::ApplicationFlow::StartResult::kFailed);
        EXPECT_FALSE(h.module.IsRunning());
        EXPECT_STREQ(h.module.GetConfigError().key.AsCStr(), c.key);
        EXPECT_GE(logs.Count(Level::kError, c.key), 1u);
        EXPECT_EQ(h.scheduler.GetChannelCount(), 0u);                       // nothing acquired
        EXPECT_FALSE(h.host.GetHost().Has(StringCRC("cow.worldstep.status")));
    }
}

// ---- Key source and the key-source ordering gate (AC 4, 5; plan tasks 1 and 3) ----------------------------------

TEST(WorldStepModule, KeyIsTheDayOfTheLatestTick)
{
    Harness h;
    EXPECT_EQ(h.module.GetCurrentKey(), 0u);
    h.module.OnSimulationTick(MakeTick(5));
    EXPECT_EQ(h.module.GetCurrentKey(), 5u);   // visible immediately, before the module's own DoUpdate
    h.module.OnSimulationTick(MakeTick(6));
    EXPECT_EQ(h.module.GetCurrentKey(), 6u);
}

TEST(WorldStepModule, AnOlderTickNeverLowersTheKey)
{
    Harness h;
    h.module.OnSimulationTick(MakeTick(10));
    h.module.OnSimulationTick(MakeTick(4));
    EXPECT_EQ(h.module.GetCurrentKey(), 10u);
}

TEST(WorldStepModule, KeyIsMonotonicOver400DaysWithSeveralTicksPerFrame)
{
    LogCapture schedulerLogs("DeferredStep");
    Harness h;
    uint64_t day = 0;
    uint64_t previous = 0;
    for (int frame = 0; day < 400; ++frame)
    {
        const int ticksThisFrame = 1 + (frame % 4);   // 1, 2, 3, 4, 1, ...
        for (int i = 0; i < ticksThisFrame && day < 400; ++i)
        {
            h.module.OnSimulationTick(MakeTick(++day));
            EXPECT_GE(h.module.GetCurrentKey(), previous);
            previous = h.module.GetCurrentKey();
        }
        h.scheduler.Tick(h.module.GetCurrentKey());   // would warn and assert if the key ever fell
        h.module.CallUpdate();
    }
    EXPECT_EQ(h.module.GetCurrentKey(), 400u);
    EXPECT_EQ(schedulerLogs.Warnings("went backwards"), 0u);
}

TEST(WorldStepModule, KeyDoesNotFallAfterStopAndStartsAtZeroOnReEntry)
{
    Harness h;
    h.module.OnSimulationTick(MakeTick(50));
    ASSERT_EQ(h.module.GetCurrentKey(), 50u);
    h.Stop();
    EXPECT_EQ(h.module.GetCurrentKey(), 50u);   // a residual DeferredStepModule update must not see the key fall
    ASSERT_TRUE(h.Start());
    EXPECT_EQ(h.module.GetCurrentKey(), 0u);    // no stale key survives re-entry
}

TEST(WorldStepModule, ACadenceStepRunsAfterTheKeyWasAlreadyPublishedForThatSameStep)
{
    // Spec order: OnSimulationTick (bus dispatch) -> DeferredStepModule::DoUpdate -> WorldStepModule::DoUpdate.
    // Between the first two, the key already equals the new day.
    Harness h;
    h.module.OnSimulationTick(MakeTick(3));
    EXPECT_EQ(h.module.GetCurrentKey(), 3u);
    EXPECT_EQ(h.module.TicksProcessed(), 0u);   // the cadence has not run yet
    h.module.CallUpdate();
    EXPECT_EQ(h.module.TicksProcessed(), 1u);
}

// The lazy-read half of the gate: SetKeySource called after DeferredStepModule::DoStart and before its first DoUpdate
// is honoured on that first DoUpdate. Uses the real DeferredStepModule, standalone.
namespace
{
    struct DsmProbe : Dia::DeferredStep::DeferredStepModule
    {
        explicit DsmProbe(const StringCRC& id) : DeferredStepModule(id) {}
        Dia::ApplicationFlow::StartResult CallStart() { return DoStart(); }
        Dia::ApplicationFlow::StopResult CallStop() { return DoStop(); }
        void CallUpdate()
        {
            Dia::SimTime::SimTimeContext ctx{ Dia::Core::TimeAbsolute::Zero(), Dia::Core::TimeRelative::Zero(), 0, 1.0f, false };
            DoUpdate(ctx);
        }
    };
}

TEST(WorldStepModule, KeySourceSetAfterDeferredStepStartIsHonouredOnTheFirstUpdate)
{
    DsmProbe dsm{ StringCRC("DeferredStepModule") };
    ASSERT_EQ(dsm.CallStart(), Dia::ApplicationFlow::StartResult::kReady);

    Probe module{ StringCRC("WorldStepModule") };
    module.CallConfigure(kDefaultConfig);
    CoW::WorldStepModule::TestHosts hosts;
    hosts.scheduler = &dsm.GetScheduler();
    module.UseTestHosts(hosts);
    ASSERT_EQ(module.CallStart(), Dia::ApplicationFlow::StartResult::kReady);
    dsm.SetKeySource(module);   // what WorldStepModule::DoStart does in production, after DeferredStepModule::DoStart

    Dia::DeferredStep::Testing::RecordingStep probeStep;
    ASSERT_TRUE(dsm.GetScheduler().Register(StringCRC("WsGateProbe"), probeStep));
    ASSERT_EQ(dsm.GetScheduler().Kick(StringCRC("WsGateProbe"), 4, 5), Dia::DeferredStep::KickResult::kLaunched);

    module.OnSimulationTick(MakeTick(5));   // the bus handler runs first
    dsm.CallUpdate();                       // the very first DeferredStepModule update sees day 5 and applies the step due at 5
    EXPECT_EQ(probeStep.GetApplyCount(), 1u);

    dsm.GetScheduler().Unregister(StringCRC("WsGateProbe"));
    module.CallStop();
    dsm.CallStop();
}

// ---- The ordering gate in a real Application (AC 4; plan task 1) ------------------------------------------------
// MessageBusHostModule, DeferredStepModule and WorldStepModule on a 30 Hz SimPU, in manifest order, with a probe
// channel on the real scheduler. A probe step due on day D must apply during the same sim step in which the module
// processes day D's SimulationTick. If the bus delivered into each module's own DoUpdate the key would be one step
// stale and the probe would apply a step later.
namespace
{
    using namespace Dia::ApplicationFlow;

    Cluiche::AppFlow::MessageBusHostModule*  gBus = nullptr;
    Dia::DeferredStep::DeferredStepModule*   gDeferred = nullptr;
    Cluiche::AppFlow::SimQueryHostModule*    gQueryHost = nullptr;
    CoW::WorldStepModule*                    gWorldStep = nullptr;
    std::string                              gWorldStepConfig;

    Module* MakeBus(const StringCRC& id)       { gBus = new Cluiche::AppFlow::MessageBusHostModule(id); return gBus; }
    Module* MakeDeferred(const StringCRC& id)  { gDeferred = new Dia::DeferredStep::DeferredStepModule(id); return gDeferred; }
    Module* MakeQueryHost(const StringCRC& id) { gQueryHost = new Cluiche::AppFlow::SimQueryHostModule(id); return gQueryHost; }
    Module* MakeWorldStep(const StringCRC& id) { gWorldStep = new CoW::WorldStepModule(id); return gWorldStep; }

    // Main-side stand-in: connects the two SimQueryHost streams so they exist; it never sends.
    class WsMainShim : public MainModule
    {
    public:
        static const StringCRC kTypeId;
        explicit WsMainShim(const StringCRC& id) : MainModule(id) {}
        void OnConnectStreams(Application& app) override { mReq.Connect(app); mReps.Connect(app); }
    protected:
        StartResult DoStart() override { return StartResult::kReady; }
        StopResult DoStop() override { return StopResult::kDone; }
        void DoUpdate(const Dia::SimTime::MainTimeContext&) override {}
    private:
        EventStreamWriter<Dia::DebugServer::SimRequestEvent> mReq{ this, "SimQueryRequest" };
        EventStreamReader<Dia::DebugServer::SimReplyEvent> mReps{ this, "SimQueryReply" };
    };
    const StringCRC WsMainShim::kTypeId("WsMainShim");
    Module* MakeShim(const StringCRC& id) { return new WsMainShim(id); }

    // Declared so the manifest validates (DeferredStepModule consumes it); only active in "Run", which is never entered,
    // so the scheduler runs steps inline.
    class WsProvider : public Module
    {
    public:
        static const StringCRC kTypeId;
        explicit WsProvider(const StringCRC& id) : Module(id) {}
        void OnConnectStreams(Application& app) override { mWriter.Connect(app); }
        StartResult DoStart() override { return StartResult::kReady; }
        void DoUpdate(float) override {}
        StopResult DoStop() override { return StopResult::kDone; }
    private:
        ServiceStreamWriter<Dia::Threading::JobSystem> mWriter{ this, "JobSystemService" };
    };
    const StringCRC WsProvider::kTypeId("WsProvider");
    Module* MakeProvider(const StringCRC& id) { return new WsProvider(id); }

    ModuleDeclaration Mod(const StringCRC& typeId, const char* instance, const char* stage,
        std::initializer_list<std::pair<const char*, const char*>> channels, std::initializer_list<const char*> deps = {})
    {
        ModuleDeclaration m;
        m.instanceId = StringCRC(instance);
        m.typeId = typeId;
        m.startTimeoutMs = 10000.0f;
        m.stopTimeoutMs = 5000.0f;
        m.stages.Add(StringCRC(stage));
        for (const auto& c : channels)
        {
            ChannelBinding b;
            b.id = StringCRC(c.first);
            b.role = StringCRC(c.second);
            m.channels.Add(b);
        }
        for (const char* d : deps) m.dependencies.Add(StringCRC(d));
        return m;
    }

    void AddEventStream(ApplicationManifestV3& m, const char* id, const char* from, const char* to, unsigned capacity)
    {
        StreamDeclaration s;
        s.id = StringCRC(id);
        s.kind = StringCRC("EventStream");
        s.fromPU = StringCRC(from);
        s.toPU = StringCRC(to);
        s.capacity = capacity;
        m.streams.Add(s);
    }

    struct AppHarness
    {
        static constexpr float kDt = 1.0f / 30.0f;
        TypeRegistry reg;
        ApplicationManifestV3 manifest;
        std::unique_ptr<Application> app;

        explicit AppHarness(const std::string& config)
        {
            gBus = nullptr; gDeferred = nullptr; gQueryHost = nullptr; gWorldStep = nullptr;
            gWorldStepConfig = config;
            reg.Register(Cluiche::AppFlow::MessageBusHostModule::kTypeId, MakeBus);
            reg.Register(Dia::DeferredStep::DeferredStepModule::kTypeId, MakeDeferred);
            reg.Register(Cluiche::AppFlow::SimQueryHostModule::kTypeId, MakeQueryHost);
            reg.Register(CoW::WorldStepModule::kTypeId, MakeWorldStep);
            reg.Register(WsMainShim::kTypeId, MakeShim);
            reg.Register(WsProvider::kTypeId, MakeProvider);

            manifest.version = 3;
            StageDeclaration boot; boot.name = StringCRC("Boot"); manifest.stages.Add(boot);
            StageDeclaration run;  run.name = StringCRC("Run");   manifest.stages.Add(run);
            manifest.initialStage = StringCRC("Boot");
            AddEventStream(manifest, "SimQueryRequest", "MainPU", "SimPU", 16);
            AddEventStream(manifest, "SimQueryReply", "SimPU", "MainPU", 48);
            StreamDeclaration svc;
            svc.id = StringCRC("JobSystemService");
            svc.kind = StringCRC("ServiceStream");
            svc.payloadType = StringCRC("JobSystem");
            manifest.streams.Add(svc);

            ProcessingUnitDeclaration mainPu;
            mainPu.instanceId = StringCRC("MainPU");
            mainPu.frequencyHz = 30.0f;
            mainPu.dedicatedThread = false;
            mainPu.modules.Add(Mod(WsMainShim::kTypeId, "shim", "Boot", { { "SimQueryRequest", "writes" }, { "SimQueryReply", "reads" } }));
            mainPu.modules.Add(Mod(WsProvider::kTypeId, "provider", "Run", { { "JobSystemService", "provides" } }));
            manifest.processingUnits.Add(mainPu);

            ProcessingUnitDeclaration simPu;
            simPu.instanceId = StringCRC("SimPU");
            simPu.frequencyHz = 30.0f;
            simPu.dedicatedThread = false;
            simPu.modules.Add(Mod(Cluiche::AppFlow::SimQueryHostModule::kTypeId, "SimQueryHostModule", "Boot",
                { { "SimQueryRequest", "reads" }, { "SimQueryReply", "writes" } }));
            simPu.modules.Add(Mod(Cluiche::AppFlow::MessageBusHostModule::kTypeId, "MessageBusHostModule", "Boot", {}));
            simPu.modules.Add(Mod(Dia::DeferredStep::DeferredStepModule::kTypeId, "DeferredStepModule", "Boot",
                { { "JobSystemService", "consumes" } }));
            ModuleDeclaration ws = Mod(CoW::WorldStepModule::kTypeId, "WorldStepModule", "Boot", {},
                { "MessageBusHostModule", "DeferredStepModule", "SimQueryHostModule" });
            ws.configJson = gWorldStepConfig.c_str();
            simPu.modules.Add(ws);
            manifest.processingUnits.Add(simPu);

            app.reset(new Application(manifest, reg));
        }

        bool Start()
        {
            if (!app->Start()) return false;
            for (int i = 0; i < 60; ++i)
            {
                app->Update(kDt);
                if (gWorldStep && gWorldStep->GetState() == ModuleState::kActive) break;
            }
            return gWorldStep && gDeferred && gBus && gQueryHost && gWorldStep->GetState() == ModuleState::kActive;
        }

        ~AppHarness()
        {
            if (!app) return;
            app->RequestShutdown();
            for (int guard = 0; guard < 1000 && app->Update(kDt); ++guard) {}
            app.reset();
        }
    };
}

TEST(WorldStepModule, GateTheKeyIsNotStaleInTheSameSimStepAsTheDeferredStepUpdate)
{
    AppHarness h(kDefaultConfig);
    ASSERT_TRUE(h.Start());
    EXPECT_TRUE(gWorldStep->IsRunning());
    EXPECT_TRUE(gWorldStep->IsChannelRegistered());
    EXPECT_TRUE(gWorldStep->NamesRegistered());
    EXPECT_TRUE(gQueryHost->GetHost().Has(StringCRC("cow.worldstep.status")));
    EXPECT_TRUE(gQueryHost->GetHost().Has(StringCRC("cow.worldstep.step")));
    EXPECT_EQ(gDeferred->GetScheduler().GetChannelCount(), 1u);   // the "world" channel

    Dia::DeferredStep::Testing::RecordingStep probe;
    const StringCRC probeChannel("WsGateProbe");
    ASSERT_TRUE(gDeferred->GetScheduler().Register(probeChannel, probe));
    // The bus type is registered by the module's DoStart; ticks are posted exactly as GameSpeedControllerModule does.
    for (uint64_t day = 10; day <= 40; ++day)
    {
        ASSERT_EQ(gDeferred->GetScheduler().Kick(probeChannel, day - 1, day), Dia::DeferredStep::KickResult::kLaunched);
        const uint64_t processedBefore = gWorldStep->TicksProcessed();
        const uint32_t appliesBefore = probe.GetApplyCount();
        ASSERT_TRUE(gBus->GetBus().Broadcast(MakeTick(day)));
        int frames = 0;
        while (gWorldStep->TicksProcessed() == processedBefore && frames++ < 30)
            h.app->Update(AppHarness::kDt);
        ASSERT_GT(gWorldStep->TicksProcessed(), processedBefore) << "day " << day << " never reached the module";
        // The cadence of day D has run, so DeferredStepModule::DoUpdate already ticked with key D in this step.
        EXPECT_EQ(gWorldStep->GetCurrentKey(), day);
        EXPECT_EQ(probe.GetApplyCount(), appliesBefore + 1) << "key was stale on day " << day;
    }
    RecordProperty("key_source_verdict", "not stale: SimulationTick reaches WorldStepModule before DeferredStepModule::DoUpdate");
    gDeferred->GetScheduler().Unregister(probeChannel);
}

// ---- Stage registration (AC 17; plan task 5) ---------------------------------------------------------------------

TEST(WorldStepModule, DuplicateRegistrationReturnsFalseAndWarnsOnce)
{
    LogCapture logs("WST");
    Rig rig(kDefaultConfig, false);
    EXPECT_TRUE(rig.module.RegisterStage(StringCRC("weather"), rig.weather));
    EXPECT_FALSE(rig.module.RegisterStage(StringCRC("weather"), rig.weather));
    EXPECT_FALSE(rig.module.RegisterStage(StringCRC("weather"), rig.weather));
    EXPECT_FALSE(rig.module.RegisterStage(StringCRC("weather"), rig.weather));
    EXPECT_EQ(logs.Warnings("rejected"), 1u);   // logged once per id, not per attempt
    EXPECT_EQ(rig.module.GetChain().RegisteredCount(), 1u);
}

TEST(WorldStepModule, ARegisteredStageNotInTheOrderIsSkippedWithExactlyOneWarning)
{
    LogCapture logs("WST");
    Rig rig(Config("weather", 7, true), false);
    EXPECT_TRUE(rig.module.RegisterStage(StringCRC("weather"), rig.weather));
    EXPECT_TRUE(rig.module.RegisterStage(StringCRC("ecology"), rig.ecology));
    rig.Steps(1, 30);
    EXPECT_EQ(logs.Warnings("not in the configured order"), 1u);
    EXPECT_EQ(rig.Count(CallPhase::Capture, "ecology"), 0u);   // never called
    EXPECT_GT(rig.Count(CallPhase::Capture, "weather"), 0u);
}

TEST(WorldStepModule, MissingNeverRegisteredAndRetiredRegisteredThenLeftAreTrackedApart)
{
    Rig rig(kDefaultConfig, false);
    EXPECT_TRUE(rig.module.RegisterStage(StringCRC("weather"), rig.weather));
    rig.Step(1);
    EXPECT_EQ(rig.module.MissingStageCount(), 1u);   // ecology never registered
    EXPECT_EQ(rig.module.RetiredStageCount(), 0u);
    Json::Value status = rig.module.BuildStatusPayload();
    ASSERT_EQ(status["missing"].size(), 1u);
    EXPECT_EQ(status["missing"][0u].asString(), "ecology");
    EXPECT_EQ(status["retired"].size(), 0u);

    EXPECT_TRUE(rig.module.RegisterStage(StringCRC("ecology"), rig.ecology));
    rig.Step(2);
    EXPECT_EQ(rig.module.MissingStageCount(), 0u);

    rig.module.UnregisterStage(StringCRC("weather"));   // it was registered once, so this is a retirement
    rig.Step(3);
    EXPECT_EQ(rig.module.MissingStageCount(), 0u);
    EXPECT_EQ(rig.module.RetiredStageCount(), 1u);
    status = rig.module.BuildStatusPayload();
    ASSERT_EQ(status["retired"].size(), 1u);
    EXPECT_EQ(status["retired"][0u].asString(), "weather");
    EXPECT_EQ(status["missing"].size(), 0u);
}

// ---- IDeferredStep surface and channel registration (plan task 6) -----------------------------------------------

TEST(WorldStepModule, PendingRecordVersionIsOne)
{
    Harness h;
    EXPECT_EQ(h.module.GetPendingVersion(), 1u);
}

TEST(WorldStepModule, TheWorldChannelIsRegisteredOnlyInDeferredMode)
{
    {
        Harness deferred(Config("weather,ecology", 7, true));
        EXPECT_EQ(deferred.scheduler.GetChannelCount(), 1u);
        EXPECT_TRUE(deferred.module.IsChannelRegistered());
    }
    {
        Harness sync(Config("weather,ecology", 7, false));
        EXPECT_EQ(sync.scheduler.GetChannelCount(), 0u);
        EXPECT_FALSE(sync.module.IsChannelRegistered());
    }
}

TEST(WorldStepModule, EveryStageGetsTheSameKeyThroughTheSchedulerCapturePath)
{
    Rig rig;
    rig.Steps(1, 7);
    ASSERT_EQ(rig.CaptureKeys("weather").size(), 1u);
    EXPECT_EQ(rig.CaptureKeys("weather"), rig.CaptureKeys("ecology"));
    EXPECT_EQ(rig.module.LastStepDay(), 7u);
}

// ---- Sim-hosted names (AC 23, 24; plan task 16) -----------------------------------------------------------------

TEST(WorldStepModule, BothNamesRegisterWithNonEmptyHelp)
{
    Rig rig;
    for (const char* name : { "cow.worldstep.status", "cow.worldstep.step" })
    {
        SCOPED_TRACE(name);
        ASSERT_TRUE(rig.host.GetHost().Has(StringCRC(name)));
        const Dia::DebugServer::SimHelpStored* help = rig.host.GetHost().GetHelp(StringCRC(name));
        ASSERT_NE(help, nullptr);
        EXPECT_GT(std::strlen(help->description), 0u);
        EXPECT_STREQ(help->category, "worldstep");
    }
    EXPECT_TRUE(rig.module.NamesRegistered());
}

TEST(WorldStepModule, ForcedFailureOfTheSecondRegistrationLeavesNoNamesBehind)
{
    LogCapture logs("WST");
    Harness h(kDefaultConfig, false);
    // Another module already owns the command name, so the second registration must fail.
    ASSERT_TRUE(h.host.RegisterQuery("cow.worldstep.step", Dia::DebugServer::SimHelpInfo{ "squatter" },
        [](const Json::Value&) { Json::Value v(Json::objectValue); v["success"] = true; return v; }));
    EXPECT_EQ(h.module.CallStart(), Dia::ApplicationFlow::StartResult::kFailed);
    EXPECT_FALSE(h.module.NamesRegistered());
    EXPECT_FALSE(h.host.GetHost().Has(StringCRC("cow.worldstep.status")));   // the first was rolled back in the same call
    EXPECT_TRUE(h.host.GetHost().Has(StringCRC("cow.worldstep.step")));      // the squatter's name is untouched
    EXPECT_EQ(h.scheduler.GetChannelCount(), 0u);                            // nothing else was acquired
    EXPECT_FALSE(h.module.IsRunning());
    EXPECT_GE(logs.Count(Level::kError, "cow.worldstep.step"), 1u);
}

TEST(WorldStepModule, StopRemovesBothNames)
{
    Rig rig;
    ASSERT_TRUE(rig.host.GetHost().Has(StringCRC("cow.worldstep.status")));
    rig.UnregisterBoth();
    rig.Stop();
    EXPECT_FALSE(rig.host.GetHost().Has(StringCRC("cow.worldstep.status")));
    EXPECT_FALSE(rig.host.GetHost().Has(StringCRC("cow.worldstep.step")));
    EXPECT_FALSE(rig.module.NamesRegistered());
}

TEST(WorldStepModule, StatusPayloadCarriesTheSpecifiedFields)
{
    Rig rig;
    rig.Steps(1, 8);
    const Json::Value s = rig.module.BuildStatusPayload();
    EXPECT_TRUE(s["success"].asBool());
    EXPECT_EQ(s["mode"].asString(), "deferred");
    EXPECT_EQ(s["applyAfterDays"].asUInt(), 7u);
    ASSERT_EQ(s["stages"].size(), 2u);
    EXPECT_EQ(s["stages"][0u]["id"].asString(), "weather");
    EXPECT_EQ(s["stages"][1u]["id"].asString(), "ecology");
    EXPECT_TRUE(s["stages"][0u]["ready"].asBool());
    EXPECT_EQ(s["retired"].size(), 0u);
    EXPECT_EQ(s["missing"].size(), 0u);
    EXPECT_EQ(s["latestDay"].asUInt64(), 8u);
    EXPECT_EQ(s["lastKickDay"].asUInt64(), 7u);
    EXPECT_TRUE(s["lastApplyKey"].isNull());   // nothing applied yet: the first step applies at 14
    EXPECT_TRUE(s["pendingWeek"].isNull());
    EXPECT_TRUE(s["inFlight"].asBool());
    EXPECT_TRUE(s.isMember("lastStallMs"));

    rig.ecology.SetReady(false);
    EXPECT_FALSE(rig.module.BuildStatusPayload()["stages"][1u]["ready"].asBool());
    rig.Steps(9, 14);
    const Json::Value after = rig.module.BuildStatusPayload();
    EXPECT_EQ(after["lastApplyKey"].asUInt64(), 14u);
}

TEST(WorldStepModule, StepCommandForcesAKickRegardlessOfTheWeekGate)
{
    Rig rig;
    rig.Steps(1, 3);   // day 3 is not a week
    const Json::Value r = rig.module.RunStepCommand(Json::Value(Json::objectValue));
    EXPECT_TRUE(r["success"].asBool()) << r.toStyledString();
    EXPECT_EQ(rig.module.KickCount(), 1u);
    rig.Harvest();
    ASSERT_EQ(rig.CaptureKeys("weather").size(), 1u);
    EXPECT_EQ(rig.CaptureKeys("weather")[0], 3u);   // the step stands for the current day
    EXPECT_EQ(r["applyKey"].asUInt64(), 10u);
}

TEST(WorldStepModule, StepCommandIsRefusedWithAReasonWhileAStepIsInFlight)
{
    Rig rig;
    rig.Steps(1, 7);   // week 7 is in flight
    const Json::Value r = rig.module.RunStepCommand(Json::Value(Json::objectValue));
    EXPECT_FALSE(r["success"].asBool());
    EXPECT_NE(r["error"].asString().find("in flight"), std::string::npos);
    EXPECT_EQ(rig.module.KickCount(), 1u);
}

TEST(WorldStepModule, StepCommandIsRefusedWithAReasonWhileStagesAreNotReady)
{
    Rig rig;
    rig.ecology.SetReady(false);
    rig.Steps(1, 3);
    const Json::Value r = rig.module.RunStepCommand(Json::Value(Json::objectValue));
    EXPECT_FALSE(r["success"].asBool());
    EXPECT_NE(r["error"].asString().find("not ready"), std::string::npos);
    EXPECT_EQ(rig.module.KickCount(), 0u);
}

TEST(WorldStepModule, StepCommandRunsOneInlineStepInSynchronousModeRegardlessOfTheWeekGate)
{
    Rig rig(Config("weather,ecology", 7, false));
    rig.Steps(1, 3);
    const Json::Value r = rig.module.RunStepCommand(Json::Value(Json::objectValue));
    EXPECT_TRUE(r["success"].asBool()) << r.toStyledString();
    EXPECT_EQ(rig.module.SyncStepCount(), 1u);
    rig.Harvest();
    EXPECT_EQ(rig.Count(CallPhase::Commit, "weather"), 1u);   // inline: committed at once
    EXPECT_EQ(rig.scheduler.GetStats().kicks, 0u);
    // Run twice: there is no in-flight state to refuse.
    EXPECT_TRUE(rig.module.RunStepCommand(Json::Value(Json::objectValue))["success"].asBool());
    EXPECT_EQ(rig.module.SyncStepCount(), 2u);
}

TEST(WorldStepModule, StepCommandSetModeSwitchesWhenIdleAndIsRefusedInFlight)
{
    Rig rig(Config("weather,ecology", 7, false));
    Json::Value toDeferred(Json::objectValue);
    toDeferred["action"] = "set_mode";
    toDeferred["mode"] = "deferred";
    EXPECT_TRUE(rig.module.RunStepCommand(toDeferred)["success"].asBool());
    EXPECT_TRUE(rig.module.IsDeferred());
    EXPECT_EQ(rig.scheduler.GetChannelCount(), 1u);

    rig.Steps(1, 7);   // a step is now in flight
    Json::Value toSync(Json::objectValue);
    toSync["action"] = "set_mode";
    toSync["mode"] = "sync";
    const Json::Value refused = rig.module.RunStepCommand(toSync);
    EXPECT_FALSE(refused["success"].asBool());
    EXPECT_NE(refused["error"].asString().find("in flight"), std::string::npos);
    EXPECT_TRUE(rig.module.IsDeferred());

    Json::Value bad(Json::objectValue);
    bad["action"] = "set_mode";
    bad["mode"] = "turbo";
    EXPECT_FALSE(rig.module.RunStepCommand(bad)["success"].asBool());
    Json::Value unknown(Json::objectValue);
    unknown["action"] = "explode";
    EXPECT_FALSE(rig.module.RunStepCommand(unknown)["success"].asBool());
}

TEST(WorldStepModule, StepCommandBeforeStartIsRefused)
{
    Harness h(kDefaultConfig, false);
    EXPECT_FALSE(h.module.RunStepCommand(Json::Value(Json::objectValue))["success"].asBool());
}

// ---- Observability (AC 25, 26; plan task 17) --------------------------------------------------------------------

TEST(WorldStepModule, TheFiveMetricsAreRegisteredWhileRunning)
{
    Rig rig;
    auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
    EXPECT_NE(metrics.FindGauge(StringCRC("cow.worldstep.step_ms")), nullptr);
    EXPECT_NE(metrics.FindGauge(StringCRC("cow.worldstep.slack_days")), nullptr);
    EXPECT_NE(metrics.FindCounter(StringCRC("cow.worldstep.stalls")), nullptr);
    EXPECT_NE(metrics.FindCounter(StringCRC("cow.worldstep.late_kicks")), nullptr);
    EXPECT_NE(metrics.FindCounter(StringCRC("cow.worldstep.lost_weeks")), nullptr);
}

TEST(WorldStepModule, SlackDaysIsTheApplyKeyMinusTheDayTheStepWasFoundComplete)
{
    Rig rig;                       // the step finishes right after the kick on day 7, found complete on day 8
    rig.Steps(1, 14);
    EXPECT_DOUBLE_EQ(GaugeValue("cow.worldstep.slack_days"), 6.0);   // apply key 14 - 8
    EXPECT_EQ(rig.module.ApplyCount(), 1u);
}

TEST(WorldStepModule, SlackIsZeroWhenTheStepOnlyFinishedBecauseTheSchedulerStalled)
{
    Rig rig;
    rig.autoComplete = false;
    rig.Steps(1, 14);
    EXPECT_DOUBLE_EQ(GaugeValue("cow.worldstep.slack_days"), 0.0);
}

namespace
{
    // Burns a known amount of wall time inside every stage method and records its own measurement. The module's
    // per-stage observer wraps each call from outside, so its sum must cover (and barely exceed) these inner sums.
    class TimedStage final : public Dia::DeferredStep::IStepStage
    {
    public:
        TimedStage(int captureMs, int runMs, int commitMs) : mCaptureMs(captureMs), mRunMs(runMs), mCommitMs(commitMs) {}
        bool IsReady() const override { return true; }
        void Capture(Dia::DeferredStep::StepKey) override { Burn(mCaptureMs); }
        void Run(Dia::DeferredStep::IParallelExecutor&) override { Burn(mRunMs); }
        void Commit() override { Burn(mCommitMs); mInnerAtCommit = mInnerMs; }
        void SerializePending(Dia::SaveGame::SaveContext&) const override {}
        void DeserializePending(Dia::SaveGame::LoadContext&) override {}
        bool ValidatePending(Dia::SaveGame::LoadContext&) const override { return true; }
        double InnerAtLastCommitMs() const { return mInnerAtCommit; }
    private:
        void Burn(int ms)
        {
            const auto begin = std::chrono::steady_clock::now();
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            mInnerMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
        }
        int mCaptureMs;
        int mRunMs;
        int mCommitMs;
        double mInnerMs = 0.0;
        double mInnerAtCommit = 0.0;
    };
}

TEST(WorldStepModule, StepMsIsTheSumOfPerStageCpuForTheAppliedStep)
{
    Harness h;
    TimedStage weather(1, 3, 1);
    TimedStage ecology(2, 4, 1);
    ASSERT_TRUE(h.module.RegisterStage(StringCRC("weather"), weather));
    ASSERT_TRUE(h.module.RegisterStage(StringCRC("ecology"), ecology));
    h.StepDays(1, 14);   // one step kicked at 7 and applied at 14; the one kicked at 14 has not applied
    ASSERT_EQ(h.module.ApplyCount(), 1u);
    // Every stage's inner sum up to its commit covers exactly the applied step (the second step starts after it).
    const double innerSum = weather.InnerAtLastCommitMs() + ecology.InnerAtLastCommitMs();
    const double stepMs = h.module.LastStepMs();
    EXPECT_GE(innerSum, 12.0);                 // sleep_for never returns early: 1+3+1 and 2+4+1 ms
    EXPECT_GE(stepMs + 0.001, innerSum);       // the observer brackets every stage call from outside
    EXPECT_LE(stepMs, innerSum + 20.0);        // and adds only call overhead
    EXPECT_DOUBLE_EQ(GaugeValue("cow.worldstep.step_ms"), stepMs);
    h.module.UnregisterStage(StringCRC("weather"));
    h.module.UnregisterStage(StringCRC("ecology"));
}

TEST(WorldStepModule, FailingOnlyForAMissingConfiguredStageAndClearsWhenItArrives)
{
    Rig rig(kDefaultConfig, false);
    EXPECT_TRUE(rig.module.RegisterStage(StringCRC("weather"), rig.weather));
    rig.Step(1);
    EXPECT_EQ(rig.module.GetHealthReporter().Report().status, HealthStatus::kFailing);
    EXPECT_TRUE(rig.module.RegisterStage(StringCRC("ecology"), rig.ecology));
    rig.Step(2);
    EXPECT_EQ(rig.module.GetHealthReporter().Report().status, HealthStatus::kOK);
}

TEST(WorldStepModule, LateAndLostCountersMirrorTheModule)
{
    Rig rig(Config("weather,ecology", 20, true));
    const int64_t lateBefore = CounterValue("cow.worldstep.late_kicks");
    const int64_t lostBefore = CounterValue("cow.worldstep.lost_weeks");
    rig.Steps(1, 21);   // week 14 is late, week 21 is lost
    EXPECT_EQ(CounterValue("cow.worldstep.late_kicks"), lateBefore + 1);
    EXPECT_EQ(CounterValue("cow.worldstep.lost_weeks"), lostBefore + 1);
}

TEST(WorldStepModule, ConfigureAndReadyTransitionsAreLogged)
{
    LogCapture logs("WST");
    Rig rig;
    rig.Steps(1, 3);
    EXPECT_EQ(logs.Infos("configured"), 1u);
    EXPECT_EQ(logs.Infos("now ready"), 1u);
    EXPECT_EQ(logs.Infos("stage 'weather' registered"), 1u);
}

TEST(WorldStepModule, ModuleWorksWithoutAProcessingUnitAndHealthStartsHealthy)
{
    Rig rig;
    EXPECT_EQ(rig.module.GetHealthReporter().Report().status, HealthStatus::kOK);
    EXPECT_EQ(rig.module.GetProcessingUnit(), nullptr);
}
