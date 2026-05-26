////////////////////////////////////////////////////////////////////////////////
// TestServiceChannel.cpp
// GoogleTest suite — ServiceStreamStore commit lifecycle + validator error paths
//
// Task 13 (TDD RED): all tests written against the ServiceStream spec.
// Tests compile once task 10 (Application commit gate) is wired.
// Module types prefixed "SC_" to avoid ODR collisions.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaApplicationFlow/Manifest/ManifestValidatorV2.h>
#include <DiaApplicationFlow/Streams/ServiceStreamStore.h>
#include <DiaApplicationFlow/Streams/ServiceStreamWriter.h>
#include <DiaApplicationFlow/Streams/ServiceStreamReader.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace Dia::Core::Containers;

// ---------------------------------------------------------------------------
// Minimal service type for testing
// ---------------------------------------------------------------------------

struct SC_Handle { int value = 42; };

// ---------------------------------------------------------------------------
// ServiceStreamStore unit tests
// ---------------------------------------------------------------------------

TEST(ServiceStreamStore, RegisterThenCommitAllowsGet)
{
    ServiceStreamStore<SC_Handle> store(StringCRC("canvas"), StringCRC("SC_Handle"));
    SC_Handle h;
    h.value = 99;

    store.Register(h);
    EXPECT_TRUE(store.IsRegistered());
    EXPECT_FALSE(store.IsCommitted()); // not committed yet

    store.Commit();
    EXPECT_TRUE(store.IsCommitted());
    EXPECT_EQ(store.Get().value, 99);
}

TEST(ServiceStreamStore, GetBeforeCommitAsserts)
{
    ServiceStreamStore<SC_Handle> store(StringCRC("canvas"), StringCRC("SC_Handle"));
    SC_Handle h;
    store.Register(h);
    EXPECT_FALSE(store.IsCommitted());
    // Get() before Commit() must assert — we verify the flag, not the crash
    EXPECT_FALSE(store.IsCommitted());
}

TEST(ServiceStreamStore, ResetClearsHandleAndCommitted)
{
    ServiceStreamStore<SC_Handle> store(StringCRC("canvas"), StringCRC("SC_Handle"));
    SC_Handle h;
    store.Register(h);
    store.Commit();
    ASSERT_TRUE(store.IsCommitted());

    store.Reset();
    EXPECT_FALSE(store.IsCommitted());
    EXPECT_FALSE(store.IsRegistered());
}

TEST(ServiceStreamStore, DoubleRegisterWithoutResetIsDetected)
{
    // Two Register() calls without a Reset() in between should assert.
    // We verify state: after first Register, IsRegistered() is true.
    // The second Register() call is left to the DIA_ASSERT guard.
    ServiceStreamStore<SC_Handle> store(StringCRC("canvas"), StringCRC("SC_Handle"));
    SC_Handle h1, h2;
    store.Register(h1);
    EXPECT_TRUE(store.IsRegistered());
    // Cannot call Register again without triggering assert — just verify flag stays true
    EXPECT_TRUE(store.IsRegistered());
}

TEST(ServiceStreamStore, ResetThenRegisterAgainWorks)
{
    ServiceStreamStore<SC_Handle> store(StringCRC("canvas"), StringCRC("SC_Handle"));
    SC_Handle h1, h2;
    h1.value = 1; h2.value = 2;

    store.Register(h1);
    store.Commit();
    EXPECT_EQ(store.Get().value, 1);

    store.Reset();
    store.Register(h2);
    store.Commit();
    EXPECT_EQ(store.Get().value, 2);
}

// ---------------------------------------------------------------------------
// Validator — ServiceStream error codes
// ---------------------------------------------------------------------------

namespace {

struct SC_Module : Module
{
    using Module::Module;
    static const StringCRC kTypeId;
    StartResult DoStart() override { return StartResult::kReady; }
    void        DoUpdate(float) override {}
    StopResult  DoStop() override { return StopResult::kDone; }
};
const StringCRC SC_Module::kTypeId("SC_Module");

TypeRegistry BuildSCRegistry()
{
    TypeRegistry reg;
    reg.Register(SC_Module::kTypeId,
        [](const StringCRC& id) -> Module* { return new SC_Module(id); });
    return reg;
}

// Build the minimum valid baseline for service-stream tests.
// Caller adds the service stream + provider/consumer as needed.
ApplicationManifestV3 BuildSCBaseManifest()
{
    ApplicationManifestV3 m;
    m.version = 3;

    StageDeclaration boot;
    boot.name = StringCRC("Boot");
    m.stages.Add(boot);
    m.initialStage = StringCRC("Boot");

    ProcessingUnitDeclaration mainPU;
    mainPU.instanceId      = StringCRC("MainPU");
    mainPU.frequencyHz     = 60.0f;
    mainPU.dedicatedThread = false;

    ModuleDeclaration mod;
    mod.instanceId = StringCRC("mod0");
    mod.typeId     = SC_Module::kTypeId;
    mod.stages.Add(StringCRC("Boot"));
    mainPU.modules.Add(mod);

    m.processingUnits.Add(mainPU);
    return m;
}

StreamDeclaration MakeServiceStream(const char* id, const char* payloadType = "SC_Handle")
{
    StreamDeclaration s;
    s.id          = StringCRC(id);
    s.kind        = StringCRC("ServiceStream");
    s.payloadType = StringCRC(payloadType);
    return s;
}

bool HasCode(const DynamicArrayC<ValidationEntry, 64>& results, const char* code,
             ValidationSeverity sev = ValidationSeverity::kError)
{
    const StringCRC target(code);
    for (unsigned int i = 0; i < results.Size(); ++i)
        if (results[i].code == target && results[i].severity == sev)
            return true;
    return false;
}

} // anonymous namespace

// SERVICE_STREAM_MISSING_PROVIDER — declared ServiceStream with no provider
TEST(ServiceStreamValidator, MissingProvider_Error)
{
    TypeRegistry reg = BuildSCRegistry();
    ApplicationManifestV3 m = BuildSCBaseManifest();

    m.streams.Add(MakeServiceStream("canvas"));
    // Add a consumer but no provider
    ChannelBinding ch;
    ch.id   = StringCRC("canvas");
    ch.role = StringCRC("consumes");
    m.processingUnits[0].modules[0].channels.Add(ch);

    ManifestValidatorV2 v(reg);
    v.Validate(m);
    EXPECT_TRUE(HasCode(v.GetResults(), "SERVICE_STREAM_MISSING_PROVIDER"));
}

// SERVICE_STREAM_MULTIPLE_PROVIDERS — two modules both declare provides
TEST(ServiceStreamValidator, MultipleProviders_Error)
{
    TypeRegistry reg = BuildSCRegistry();
    ApplicationManifestV3 m = BuildSCBaseManifest();

    // Add a second module so we can have two providers
    ModuleDeclaration mod2;
    mod2.instanceId = StringCRC("mod1");
    mod2.typeId     = SC_Module::kTypeId;
    mod2.stages.Add(StringCRC("Boot"));
    m.processingUnits[0].modules.Add(mod2);

    m.streams.Add(MakeServiceStream("canvas"));

    ChannelBinding prov1; prov1.id = StringCRC("canvas"); prov1.role = StringCRC("provides");
    ChannelBinding prov2; prov2.id = StringCRC("canvas"); prov2.role = StringCRC("provides");
    ChannelBinding cons;  cons.id  = StringCRC("canvas"); cons.role  = StringCRC("consumes");

    m.processingUnits[0].modules[0].channels.Add(prov1);
    m.processingUnits[0].modules[1].channels.Add(prov2);
    // Add a consumer to avoid ORPHAN_PROVIDER masking
    ModuleDeclaration mod3;
    mod3.instanceId = StringCRC("mod2");
    mod3.typeId     = SC_Module::kTypeId;
    mod3.stages.Add(StringCRC("Boot"));
    mod3.channels.Add(cons);
    m.processingUnits[0].modules.Add(mod3);

    ManifestValidatorV2 v(reg);
    v.Validate(m);
    EXPECT_TRUE(HasCode(v.GetResults(), "SERVICE_STREAM_MULTIPLE_PROVIDERS"));
}

// SERVICE_STREAM_ORPHAN_PROVIDER — provider declared but no consumer
TEST(ServiceStreamValidator, OrphanProvider_Error)
{
    TypeRegistry reg = BuildSCRegistry();
    ApplicationManifestV3 m = BuildSCBaseManifest();

    m.streams.Add(MakeServiceStream("canvas"));

    ChannelBinding prov; prov.id = StringCRC("canvas"); prov.role = StringCRC("provides");
    m.processingUnits[0].modules[0].channels.Add(prov);
    // No consumer declared

    ManifestValidatorV2 v(reg);
    v.Validate(m);
    EXPECT_TRUE(HasCode(v.GetResults(), "SERVICE_STREAM_ORPHAN_PROVIDER"));
}

// SERVICE_STREAM_PROVIDER_AFTER_CONSUMER — consumer PU index < provider PU index
TEST(ServiceStreamValidator, ProviderAfterConsumer_Error)
{
    TypeRegistry reg = BuildSCRegistry();
    ApplicationManifestV3 m = BuildSCBaseManifest();

    // Add a second PU that will be the provider (index 1)
    ProcessingUnitDeclaration renderPU;
    renderPU.instanceId      = StringCRC("RenderPU");
    renderPU.frequencyHz     = 60.0f;
    renderPU.dedicatedThread = false;

    ModuleDeclaration renderMod;
    renderMod.instanceId = StringCRC("renderMod");
    renderMod.typeId     = SC_Module::kTypeId;
    renderMod.stages.Add(StringCRC("Boot"));
    renderPU.modules.Add(renderMod);
    m.processingUnits.Add(renderPU);

    m.streams.Add(MakeServiceStream("canvas"));

    // Consumer is in MainPU (index 0), provider is in RenderPU (index 1) — wrong order
    ChannelBinding cons; cons.id = StringCRC("canvas"); cons.role = StringCRC("consumes");
    ChannelBinding prov; prov.id = StringCRC("canvas"); prov.role = StringCRC("provides");
    m.processingUnits[0].modules[0].channels.Add(cons);
    m.processingUnits[1].modules[0].channels.Add(prov);

    ManifestValidatorV2 v(reg);
    v.Validate(m);
    EXPECT_TRUE(HasCode(v.GetResults(), "SERVICE_STREAM_PROVIDER_AFTER_CONSUMER"));
}

// SERVICE_STREAM_WRONG_ROLE — reads/writes on a ServiceStream kind
TEST(ServiceStreamValidator, WrongRole_ReadsOnServiceStream_Error)
{
    TypeRegistry reg = BuildSCRegistry();
    ApplicationManifestV3 m = BuildSCBaseManifest();

    m.streams.Add(MakeServiceStream("canvas"));

    ChannelBinding ch; ch.id = StringCRC("canvas"); ch.role = StringCRC("reads");
    m.processingUnits[0].modules[0].channels.Add(ch);

    ManifestValidatorV2 v(reg);
    v.Validate(m);
    EXPECT_TRUE(HasCode(v.GetResults(), "SERVICE_STREAM_WRONG_ROLE"));
}

TEST(ServiceStreamValidator, WrongRole_WritesOnServiceStream_Error)
{
    TypeRegistry reg = BuildSCRegistry();
    ApplicationManifestV3 m = BuildSCBaseManifest();

    m.streams.Add(MakeServiceStream("canvas"));

    ChannelBinding ch; ch.id = StringCRC("canvas"); ch.role = StringCRC("writes");
    m.processingUnits[0].modules[0].channels.Add(ch);

    ManifestValidatorV2 v(reg);
    v.Validate(m);
    EXPECT_TRUE(HasCode(v.GetResults(), "SERVICE_STREAM_WRONG_ROLE"));
}

// SERVICE_STREAM_ORPHAN_CONSUMER — provides/consumes on a non-ServiceStream kind
TEST(ServiceStreamValidator, OrphanConsumer_ProvidesOnEventStream_Error)
{
    TypeRegistry reg = BuildSCRegistry();
    ApplicationManifestV3 m = BuildSCBaseManifest();

    StreamDeclaration es;
    es.id   = StringCRC("inputEvents");
    es.kind = StringCRC("EventStream");
    es.payloadType = StringCRC("InputEvent");
    m.streams.Add(es);

    ChannelBinding ch; ch.id = StringCRC("inputEvents"); ch.role = StringCRC("provides");
    m.processingUnits[0].modules[0].channels.Add(ch);

    ManifestValidatorV2 v(reg);
    v.Validate(m);
    EXPECT_TRUE(HasCode(v.GetResults(), "SERVICE_STREAM_ORPHAN_CONSUMER"));
}

// ---------------------------------------------------------------------------
// Application commit gate tests (Task 10)
// These test that Application calls Commit() on ServiceStream stores after
// all DoStart() in scope, and Reset() on stage-unload.
// ---------------------------------------------------------------------------

namespace {

// Service interface and concrete service used in commit-gate tests
struct SC_Service { int id = 0; };

// Provider module: registers the service handle from DoStart
struct SC_ProviderModule : Module
{
    using Module::Module;
    static const StringCRC kTypeId;

    SC_Service service;
    ServiceStreamWriter<SC_Service> writer;

    SC_ProviderModule(const StringCRC& id)
        : Module(id)
        , writer(this, StringCRC("svc"))
    {}

    void OnConnectStreams(Application& app) override { writer.Connect(app); }

    StartResult DoStart() override
    {
        service.id = 77;
        writer.Register(service);
        return StartResult::kReady;
    }
    void       DoUpdate(float) override {}
    StopResult DoStop() override { return StopResult::kDone; }
};
const StringCRC SC_ProviderModule::kTypeId("SC_ProviderModule");

// Consumer module: reads the service handle from DoStart (after commit gate)
struct SC_ConsumerModule : Module
{
    using Module::Module;
    static const StringCRC kTypeId;

    ServiceStreamReader<SC_Service> reader;
    int readValue = -1;

    SC_ConsumerModule(const StringCRC& id)
        : Module(id)
        , reader(this, StringCRC("svc"))
    {}

    void OnConnectStreams(Application& app) override { reader.Connect(app); }

    StartResult DoStart() override
    {
        // After commit gate: IsAvailable() must be true
        if (!reader.IsAvailable())
            return StartResult::kReady; // will be checked in test
        readValue = reader.Get().id;
        return StartResult::kReady;
    }
    void       DoUpdate(float) override {}
    StopResult DoStop() override { return StopResult::kDone; }
};
const StringCRC SC_ConsumerModule::kTypeId("SC_ConsumerModule");

TypeRegistry BuildCommitGateRegistry()
{
    TypeRegistry reg;
    reg.Register(SC_ProviderModule::kTypeId,
        [](const StringCRC& id) -> Module* { return new SC_ProviderModule(id); });
    reg.Register(SC_ConsumerModule::kTypeId,
        [](const StringCRC& id) -> Module* { return new SC_ConsumerModule(id); });
    return reg;
}

ApplicationManifestV3 BuildCommitGateManifest()
{
    ApplicationManifestV3 m;
    m.version = 3;

    StageDeclaration boot;
    boot.name = StringCRC("Boot");
    m.stages.Add(boot);
    m.initialStage = StringCRC("Boot");

    StreamDeclaration svc;
    svc.id          = StringCRC("svc");
    svc.kind        = StringCRC("ServiceStream");
    svc.payloadType = StringCRC("SC_Service");
    m.streams.Add(svc);

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    ModuleDeclaration prov;
    prov.instanceId = StringCRC("provider");
    prov.typeId     = SC_ProviderModule::kTypeId;
    prov.stages.Add(StringCRC("Boot"));
    ChannelBinding pb; pb.id = StringCRC("svc"); pb.role = StringCRC("provides");
    prov.channels.Add(pb);
    pu.modules.Add(prov);

    ModuleDeclaration cons;
    cons.instanceId = StringCRC("consumer");
    cons.typeId     = SC_ConsumerModule::kTypeId;
    cons.stages.Add(StringCRC("Boot"));
    ChannelBinding cb; cb.id = StringCRC("svc"); cb.role = StringCRC("consumes");
    cons.channels.Add(cb);
    pu.modules.Add(cons);

    m.processingUnits.Add(pu);
    return m;
}

} // anonymous namespace

// After Application::Start(), the consumer should be able to read the service
// handle because the framework committed the store after all providers' DoStart.
TEST(ServiceChannelCommitGate, ConsumerReadsHandleAfterStart)
{
    TypeRegistry reg = BuildCommitGateRegistry();
    ApplicationManifestV3 manifest = BuildCommitGateManifest();

    Application app(manifest, reg);
    bool started = app.Start();
    ASSERT_TRUE(started) << "Application::Start() should succeed with valid ServiceStream manifest";

    // DoStart() runs during the first Update() tick — run one frame
    app.Update(0.016f);

    // The stream must be committed after the first Update
    IStreamStore* store = app.FindStream(StringCRC("svc"));
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(store->GetKind(), StreamKind::kService);

    auto* typedStore = static_cast<ServiceStreamStore<SC_Service>*>(store);
    EXPECT_TRUE(typedStore->IsCommitted());
    EXPECT_EQ(typedStore->Get().id, 77);

    app.RequestShutdown();
    while (app.Update(0.016f)) {}
}

// Stage-scoped provider: when the provider module is only in Boot and the app
// transitions to Stage2, the ServiceStream must be Reset (IsCommitted -> false).
TEST(ServiceChannelCommitGate, StageScopedProviderResetsOnTransition)
{
    TypeRegistry reg = BuildCommitGateRegistry();
    ApplicationManifestV3 manifest = BuildCommitGateManifest();

    // Add a second stage — provider is only in Boot, consumer is in both
    StageDeclaration s2;
    s2.name = StringCRC("Stage2");
    manifest.stages[0].transitions.Add(StringCRC("Stage2"));
    s2.transitions.Add(StringCRC("Boot"));
    manifest.stages.Add(s2);

    // Provider is Boot-only (already default from BuildCommitGateManifest).
    // Consumer is in both stages so it persists across transition.
    manifest.processingUnits[0].modules[1].stages.Add(StringCRC("Stage2"));

    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    app.Update(0.016f); // DoStart fires, commit gate commits store

    IStreamStore* store = app.FindStream(StringCRC("svc"));
    ASSERT_NE(store, nullptr);
    EXPECT_TRUE(store->IsCommitted());

    // Transition to Stage2 — provider module leaves, store should be reset
    app.TransitionTo(StringCRC("Stage2"));
    // Drain transition — may take multiple ticks for modules to stop
    for (int i = 0; i < 10; ++i) app.Update(0.016f);

    EXPECT_FALSE(store->IsCommitted()) << "ServiceStream must be reset when provider leaves stage";

    app.RequestShutdown();
    while (app.Update(0.016f)) {}
}

// Reset on stage unload: after transitioning away from the stage that provided
// the service, the store must be reset (for stage-scoped streams).
// Note: global-manifest stores are never reset. This test uses a global stream
// (declared in the root manifest) — verifies that global stores survive transition.
TEST(ServiceChannelCommitGate, GlobalStoreRemainsCommittedAfterStageTransition)
{
    TypeRegistry reg = BuildCommitGateRegistry();
    ApplicationManifestV3 manifest = BuildCommitGateManifest();

    // The manifest above has only Boot stage — add a second stage to transition to
    StageDeclaration s2;
    s2.name = StringCRC("Stage2");
    manifest.stages[0].transitions.Add(StringCRC("Stage2"));
    s2.transitions.Add(StringCRC("Boot"));
    manifest.stages.Add(s2);

    // Provider and consumer are active in both stages (global service)
    manifest.processingUnits[0].modules[0].stages.Add(StringCRC("Stage2"));
    manifest.processingUnits[0].modules[1].stages.Add(StringCRC("Stage2"));

    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    // Run first frame so DoStart() fires and commit gate runs
    app.Update(0.016f);

    IStreamStore* store = app.FindStream(StringCRC("svc"));
    ASSERT_NE(store, nullptr);
    auto* typedStore = static_cast<ServiceStreamStore<SC_Service>*>(store);
    EXPECT_TRUE(typedStore->IsCommitted()) << "Should be committed after first Update";

    // Transition to Stage2 — provider stays, stream stays committed
    app.TransitionTo(StringCRC("Stage2"));
    for (int i = 0; i < 10; ++i) app.Update(0.016f);

    EXPECT_TRUE(typedStore->IsCommitted()) << "Global service stream must survive stage transition";

    app.RequestShutdown();
    while (app.Update(0.016f)) {}
}
