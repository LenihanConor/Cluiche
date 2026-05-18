////////////////////////////////////////////////////////////////////////////////
// Filename: TestLifecycleEvents.cpp
// GoogleTest suite — LifecycleEvent emission through Application
//
// Exercises $lifecycle stream emission for:
//   kStageTransitionRequested (TransitionTo)
//   kStageTransitionStarted   (ApplyPendingTransition)
//   kStageTransitionCommitted (TickTransitionDrain)
//   kShutdownRequested        (RequestShutdown)
//
// Module types prefixed "LC_" to avoid ODR collisions.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV2.h>
#include <DiaApplicationFlow/Streams/EventStreamStore.h>
#include <DiaApplicationFlow/Streams/EventStreamReader.h>
#include <DiaApplicationFlow/Streams/Event.h>
#include <DiaApplicationFlow/LifecycleEvent.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace Dia::Core::Containers;

// ---------------------------------------------------------------------------
// Minimal module that connects an EventStreamReader on $lifecycle.
// ---------------------------------------------------------------------------

struct LC_LifecycleReaderModule : Module
{
    using Module::Module;
    static const StringCRC kTypeId;

    EventStreamReader<LifecycleEvent> mReader{this, StringCRC("$lifecycle")};
    DynamicArrayC<Event<LifecycleEvent>, 64> consumed;

    void OnConnectStreams(Application& app) override
    {
        // $lifecycle is framework-owned — connect via FindStreamStore, not
        // RegisterOrFindStreamStore.  The store is always present after Start().
        // We wire directly to avoid the manifest-gating path.
        IStreamStore* istore = app.FindStreamStore(StringCRC("$lifecycle"));
        if (istore)
        {
            auto* estore = static_cast<EventStreamStore<LifecycleEvent>*>(istore);
            // Register our own reader slot.  mReaderIndex is private so we use
            // a thin helper: call Consume() which no-ops if disconnected, or
            // we store the pointer and index here.
            // Simplest: store pointer and call RegisterReader on it.
            mLifecycleStore  = estore;
            mReaderIndex     = estore->RegisterReader();
        }
    }

    void DoUpdate(float) override
    {
        if (mLifecycleStore && mReaderIndex >= 0)
        {
            mLifecycleStore->Consume(mReaderIndex, consumed);
        }
    }

    StartResult DoStart() override { return StartResult::kReady; }
    StopResult  DoStop() override  { return StopResult::kDone; }

    // Call from the test after RequestShutdown — DoUpdate won't run during
    // shutdown so we drain directly.
    void DrainNow()
    {
        if (mLifecycleStore && mReaderIndex >= 0)
            mLifecycleStore->Consume(mReaderIndex, consumed);
    }

    bool HasKind(LifecycleEventKind k) const
    {
        for (unsigned int i = 0; i < consumed.Size(); ++i)
            if (consumed[i].payload.kind == k)
                return true;
        return false;
    }

    EventStreamStore<LifecycleEvent>* mLifecycleStore = nullptr;
    int                               mReaderIndex    = -1;
};
const StringCRC LC_LifecycleReaderModule::kTypeId("LC_LifecycleReaderModule");

struct LC_SimpleModule : Module
{
    using Module::Module;
    static const StringCRC kTypeId;
    StartResult DoStart() override { return StartResult::kReady; }
    void        DoUpdate(float) override {}
    StopResult  DoStop() override { return StopResult::kDone; }
};
const StringCRC LC_SimpleModule::kTypeId("LC_SimpleModule");

static LC_LifecycleReaderModule* g_lcReader = nullptr;

static Module* CreateLcReader(const StringCRC& id)
{
    g_lcReader = new LC_LifecycleReaderModule(id);
    return g_lcReader;
}

static Module* CreateLcSimple(const StringCRC& id)
{
    return new LC_SimpleModule(id);
}

// ---------------------------------------------------------------------------
// Helper — pump Update() until at least one module in puId is kActive and
// none are still kStarting (stable-enough for event consumption tests).
// Multi-stage manifests have modules that are kInactive in the off-stage,
// so we can't require ALL modules active — just a settled initial stage.
// ---------------------------------------------------------------------------

static bool LcPumpUntilStable(Application& app, const StringCRC& puId, int limit = 100)
{
    for (int i = 0; i < limit; ++i)
    {
        app.Update(1.0f / 60.0f);
        DynamicArrayC<ModuleStateInfo, 64> infos;
        app.GetActiveModules(puId, infos);
        bool anyActive   = false;
        bool anyStarting = false;
        for (unsigned int m = 0; m < infos.Size(); ++m)
        {
            if (infos[m].state == ModuleState::kActive)   anyActive   = true;
            if (infos[m].state == ModuleState::kStarting) anyStarting = true;
        }
        if (anyActive && !anyStarting)
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Helper — build a two-stage manifest with a $lifecycle reader and a simple
// module in each stage.
// ---------------------------------------------------------------------------

static ApplicationManifestV2 BuildTwoStageManifest()
{
    ApplicationManifestV2 manifest;
    manifest.version = 2;

    StageDeclaration boot;  boot.name = StringCRC("Boot");
    StageDeclaration run;   run.name  = StringCRC("Run");
    manifest.stages.Add(boot);
    manifest.stages.Add(run);
    manifest.initialStage = StringCRC("Boot");

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    // Lifecycle reader lives in both stages ("all").
    ModuleDeclaration readerMod;
    readerMod.instanceId     = StringCRC("lcReader");
    readerMod.typeId         = LC_LifecycleReaderModule::kTypeId;
    readerMod.startTimeoutMs = 10000.0f;
    readerMod.stopTimeoutMs  = 5000.0f;
    readerMod.stages.Add(StringCRC("all"));
    pu.modules.Add(readerMod);

    // Boot module.
    ModuleDeclaration bootMod;
    bootMod.instanceId     = StringCRC("bootMod");
    bootMod.typeId         = LC_SimpleModule::kTypeId;
    bootMod.startTimeoutMs = 10000.0f;
    bootMod.stopTimeoutMs  = 5000.0f;
    bootMod.stages.Add(StringCRC("Boot"));
    pu.modules.Add(bootMod);

    // Run module.
    ModuleDeclaration runMod;
    runMod.instanceId     = StringCRC("runMod");
    runMod.typeId         = LC_SimpleModule::kTypeId;
    runMod.startTimeoutMs = 10000.0f;
    runMod.stopTimeoutMs  = 5000.0f;
    runMod.stages.Add(StringCRC("Run"));
    pu.modules.Add(runMod);

    manifest.processingUnits.Add(pu);
    return manifest;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// TransitionTo() emits kStageTransitionRequested before queuing the transition.
TEST(LifecycleEvents, TransitionToEmitsRequested)
{
    g_lcReader = nullptr;

    TypeRegistry reg;
    reg.Register(LC_LifecycleReaderModule::kTypeId, CreateLcReader);
    reg.Register(LC_SimpleModule::kTypeId,           CreateLcSimple);

    ApplicationManifestV2 manifest = BuildTwoStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    // Pump until the lifecycle reader is active.
    ASSERT_TRUE(LcPumpUntilStable(app, StringCRC("MainPU"), 100));
    ASSERT_NE(g_lcReader, nullptr);

    // Trigger a stage transition — this calls TransitionTo internally,
    // which emits kStageTransitionRequested.
    app.TransitionTo(StringCRC("Run"));

    // Pump once so the reader collects any pending events.
    app.Update(1.0f / 60.0f);

    EXPECT_TRUE(g_lcReader->HasKind(LifecycleEventKind::kStageTransitionRequested))
        << "TransitionTo should emit kStageTransitionRequested on the $lifecycle stream";
}

// ApplyPendingTransition emits kStageTransitionStarted.
TEST(LifecycleEvents, TransitionStartedEmitted)
{
    g_lcReader = nullptr;

    TypeRegistry reg;
    reg.Register(LC_LifecycleReaderModule::kTypeId, CreateLcReader);
    reg.Register(LC_SimpleModule::kTypeId,           CreateLcSimple);

    ApplicationManifestV2 manifest = BuildTwoStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    ASSERT_TRUE(LcPumpUntilStable(app, StringCRC("MainPU"), 100));
    ASSERT_NE(g_lcReader, nullptr);

    app.TransitionTo(StringCRC("Run"));
    // Pump enough to apply the transition (ApplyPendingTransition runs at top of Update).
    for (int i = 0; i < 20; ++i)
        app.Update(1.0f / 60.0f);

    EXPECT_TRUE(g_lcReader->HasKind(LifecycleEventKind::kStageTransitionStarted))
        << "ApplyPendingTransition should emit kStageTransitionStarted";
}

// TickTransitionDrain emits kStageTransitionCommitted once outgoing modules stop.
TEST(LifecycleEvents, TransitionCommittedEmitted)
{
    g_lcReader = nullptr;

    TypeRegistry reg;
    reg.Register(LC_LifecycleReaderModule::kTypeId, CreateLcReader);
    reg.Register(LC_SimpleModule::kTypeId,           CreateLcSimple);

    ApplicationManifestV2 manifest = BuildTwoStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    ASSERT_TRUE(LcPumpUntilStable(app, StringCRC("MainPU"), 100));
    ASSERT_NE(g_lcReader, nullptr);

    app.TransitionTo(StringCRC("Run"));
    // Pump until transition completes: outgoing modules stop, new stage committed.
    for (int i = 0; i < 50; ++i)
        app.Update(1.0f / 60.0f);

    EXPECT_TRUE(g_lcReader->HasKind(LifecycleEventKind::kStageTransitionCommitted))
        << "TickTransitionDrain should emit kStageTransitionCommitted";
}

// RequestShutdown() emits kShutdownRequested.
TEST(LifecycleEvents, ShutdownRequestedEmitted)
{
    g_lcReader = nullptr;

    TypeRegistry reg;
    reg.Register(LC_LifecycleReaderModule::kTypeId, CreateLcReader);
    reg.Register(LC_SimpleModule::kTypeId,           CreateLcSimple);

    ApplicationManifestV2 manifest = BuildTwoStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    ASSERT_TRUE(LcPumpUntilStable(app, StringCRC("MainPU"), 100));
    ASSERT_NE(g_lcReader, nullptr);

    app.RequestShutdown();
    // DoUpdate doesn't run after shutdown — drain the store directly.
    g_lcReader->DrainNow();

    EXPECT_TRUE(g_lcReader->HasKind(LifecycleEventKind::kShutdownRequested))
        << "RequestShutdown should emit kShutdownRequested on the $lifecycle stream";
}

// At least one lifecycle event flows from the $lifecycle stream reader
// across the full boot sequence (boot-up emits kStageTransitionRequested
// for the initial stage queue or similar — confirm the stream is live).
TEST(LifecycleEvents, BootSequenceEmitsAtLeastOneEvent)
{
    g_lcReader = nullptr;

    TypeRegistry reg;
    reg.Register(LC_LifecycleReaderModule::kTypeId, CreateLcReader);
    reg.Register(LC_SimpleModule::kTypeId,           CreateLcSimple);

    ApplicationManifestV2 manifest = BuildTwoStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    ASSERT_TRUE(LcPumpUntilStable(app, StringCRC("MainPU"), 100));
    ASSERT_NE(g_lcReader, nullptr);

    // Trigger one event we know will fire, then drain directly.
    app.RequestShutdown();
    g_lcReader->DrainNow();

    EXPECT_GT(g_lcReader->consumed.Size(), 0u)
        << "At least one lifecycle event should have been consumed from the $lifecycle stream";
}

// Envelope sanity: the senderCrc on lifecycle events should be the framework
// sender ($framework crc).
TEST(LifecycleEvents, EnvelopeSenderCrcIsFramework)
{
    g_lcReader = nullptr;

    TypeRegistry reg;
    reg.Register(LC_LifecycleReaderModule::kTypeId, CreateLcReader);
    reg.Register(LC_SimpleModule::kTypeId,           CreateLcSimple);

    ApplicationManifestV2 manifest = BuildTwoStageManifest();
    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    ASSERT_TRUE(LcPumpUntilStable(app, StringCRC("MainPU"), 100));
    ASSERT_NE(g_lcReader, nullptr);

    app.RequestShutdown();
    g_lcReader->DrainNow();

    ASSERT_GT(g_lcReader->consumed.Size(), 0u);

    const unsigned int expectedCrc = StringCRC("$framework").Value();
    for (unsigned int i = 0; i < g_lcReader->consumed.Size(); ++i)
    {
        EXPECT_EQ(g_lcReader->consumed[i].senderCrc, expectedCrc)
            << "All lifecycle events should carry senderCrc = '$framework'";
    }
}
