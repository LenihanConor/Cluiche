// Integration tests for CoW::WorldGenerationModule inside a real Application:
// MessageBusHostModule + RandomStreamModule + WorldGenerationModule on a 30 Hz SimPU.
#include <gtest/gtest.h>
#include <Modules/WorldGenerationModule.h>   // first: RandomStream.h must precede windows.h (min/max macros)
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <Modules/MessageBusHostModule.h>
#include <Modules/RandomStreamModule.h>
#include <Shared/WorldGeneration/Testing/TestGuarantees.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace CoW;

namespace {

Cluiche::AppFlow::MessageBusHostModule* g_bus = nullptr;
RandomStreamModule*                     g_rng = nullptr;
WorldGenerationModule*                  g_wg  = nullptr;

Module* CreateBus(const StringCRC& id) { g_bus = new Cluiche::AppFlow::MessageBusHostModule(id); return g_bus; }
Module* CreateRng(const StringCRC& id) { g_rng = new RandomStreamModule(id); return g_rng; }
Module* CreateWg(const StringCRC& id)  { g_wg  = new WorldGenerationModule(id); return g_wg; }

constexpr float kDt = 1.0f / 30.0f;

struct Fixture
{
    TypeRegistry reg;
    ApplicationManifestV3 manifest;
    std::unique_ptr<Application> app;
    Cluiche::AppFlow::MessageBusHostModule* bus = nullptr;
    WorldGenerationModule* wg = nullptr;
    Dia::MessageBus::BusSubscriptionHandle sub;
    std::vector<WorldGenerationComplete> msgs;
    std::string config;

    explicit Fixture(const char* configJson = nullptr)
    {
        if (configJson)
            config = configJson;

        reg.Register(StringCRC("MessageBusHostModule"), &CreateBus);
        reg.Register(StringCRC("RandomStreamModule"), &CreateRng);
        reg.Register(StringCRC("WorldGenerationModule"), &CreateWg);

        manifest.version = 3;
        StageDeclaration run; run.name = StringCRC("Run");
        manifest.stages.Add(run);
        manifest.initialStage = StringCRC("Run");

        ProcessingUnitDeclaration pu;
        pu.instanceId = StringCRC("SimPU");
        pu.frequencyHz = 30.0f;
        pu.dedicatedThread = false;

        ModuleDeclaration busDecl;
        busDecl.instanceId = StringCRC("MessageBusHostModule");
        busDecl.typeId = StringCRC("MessageBusHostModule");
        busDecl.stages.Add(StringCRC("all"));
        pu.modules.Add(busDecl);

        ModuleDeclaration rngDecl;
        rngDecl.instanceId = StringCRC("RandomStreamModule");
        rngDecl.typeId = StringCRC("RandomStreamModule");
        rngDecl.stages.Add(StringCRC("all"));
        pu.modules.Add(rngDecl);

        ModuleDeclaration wgDecl;
        wgDecl.instanceId = StringCRC("WorldGenerationModule");
        wgDecl.typeId = StringCRC("WorldGenerationModule");
        wgDecl.stages.Add(StringCRC("all"));
        wgDecl.dependencies.Add(StringCRC("MessageBusHostModule"));
        wgDecl.dependencies.Add(StringCRC("RandomStreamModule"));
        if (!config.empty())
            wgDecl.configJson = config.c_str();
        pu.modules.Add(wgDecl);

        manifest.processingUnits.Add(pu);
    }

    bool Boot()
    {
        g_bus = nullptr; g_rng = nullptr; g_wg = nullptr;
        app = std::make_unique<Application>(manifest, reg);
        if (!app->Start())
            return false;
        bus = g_bus;
        wg = g_wg;
        if (!bus || !wg)
            return false;
        // Register the type up front (idempotent with the module's own registration) so a subscriber
        // can exist before the first generation.
        bus->GetBus().RegisterType<WorldGenerationComplete, 4>();
        sub = bus->GetBus().Subscribe<WorldGenerationComplete>(StringCRC("TestWgSink"),
            [this](const WorldGenerationComplete& m) { msgs.push_back(m); });
        for (int i = 0; i < 20; ++i)
        {
            app->Update(kDt);
            if (wg->GetState() == ModuleState::kActive)
                return true;
        }
        return false;
    }

    void Pump(int frames) { for (int i = 0; i < frames; ++i) app->Update(kDt); }
};

bool SameCells(const WorldGeneration::WorldStructure& a, const WorldGeneration::WorldStructure& b)
{
    return a.CellCount() == b.CellCount()
        && std::memcmp(a.Cells(), b.Cells(), sizeof(WorldGeneration::Cell) * a.CellCount()) == 0;
}

} // namespace

TEST(WorldGenModule, BootsAndGeneratesFirstWorld)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.Pump(3);
    EXPECT_EQ(f.wg->GetWorldRevision(), 1u);
    ASSERT_NE(f.wg->GetCurrentWorld(), nullptr);
    EXPECT_EQ(f.wg->GetCurrentWorld()->dimensions.width, 66);
    EXPECT_EQ(f.wg->GetCurrentWorld()->dimensions.height, 42);
    EXPECT_TRUE(f.wg->IsHealthRegistered());
}

TEST(WorldGenModule, PublishesCompleteMessageWithSeedAndRevision)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.Pump(3);
    ASSERT_EQ(f.msgs.size(), 1u);
    EXPECT_EQ(f.msgs[0].seed, f.wg->GetSession().Seed());
    EXPECT_EQ(f.msgs[0].worldRevision, f.wg->GetWorldRevision());
    EXPECT_EQ(f.msgs[0].retryAttempts, 0u);
}

TEST(WorldGenModule, LivePreviewChangeBumpsRevisionOnceAndIdleIsQuiet)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.Pump(3);
    ASSERT_EQ(f.msgs.size(), 1u);

    Json::Value args(Json::objectValue);
    args["rivers"] = 0.9;
    f.wg->GetSession().Set(args);
    f.wg->NotifyAction();
    f.Pump(1);
    EXPECT_EQ(f.wg->GetWorldRevision(), 2u);
    f.Pump(1);   // the bus delivers on the following frame
    EXPECT_EQ(f.msgs.size(), 2u);
    f.Pump(10);
    EXPECT_EQ(f.msgs.size(), 2u);
    EXPECT_EQ(f.wg->GetWorldRevision(), 2u);
}

TEST(WorldGenModule, SeveralSetsInOneFrameCoalesce)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.Pump(3);
    const uint32_t before = f.wg->GetWorldRevision();
    const size_t msgsBefore = f.msgs.size();

    Json::Value a(Json::objectValue); a["rivers"] = 0.8;
    Json::Value b(Json::objectValue); b["rivers"] = 0.3;
    Json::Value c(Json::objectValue); c["seed"] = 77;
    f.wg->GetSession().Set(a);
    f.wg->GetSession().Set(b);
    f.wg->GetSession().Set(c);
    f.Pump(3);
    EXPECT_EQ(f.wg->GetWorldRevision(), before + 1);
    EXPECT_EQ(f.msgs.size(), msgsBefore + 1);
}

TEST(WorldGenModule, RegenerateBumpsRevisionAndSetSizeResizes)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.Pump(3);
    ASSERT_EQ(f.wg->GetWorldRevision(), 1u);

    f.wg->GetSession().Regenerate(Json::Value(Json::objectValue));
    f.Pump(2);
    EXPECT_EQ(f.wg->GetWorldRevision(), 2u);

    Json::Value size(Json::objectValue); size["size"] = "Standard";
    f.wg->GetSession().SetSize(size);
    f.Pump(2);
    EXPECT_EQ(f.wg->GetWorldRevision(), 3u);
    ASSERT_NE(f.wg->GetCurrentWorld(), nullptr);
    EXPECT_EQ(f.wg->GetCurrentWorld()->dimensions.width, 80);
    EXPECT_EQ(f.wg->GetCurrentWorld()->dimensions.height, 52);
}

TEST(WorldGenModule, ConfigHonoursSizeAndSeed)
{
    Fixture f("{\"size\":\"Standard\",\"seed\":4242}");
    ASSERT_TRUE(f.Boot());
    f.Pump(3);
    ASSERT_NE(f.wg->GetCurrentWorld(), nullptr);
    EXPECT_EQ(f.wg->GetCurrentWorld()->dimensions.width, 80);
    EXPECT_EQ(f.wg->GetSession().Seed(), 4242u);
    ASSERT_FALSE(f.msgs.empty());
    EXPECT_EQ(f.msgs[0].seed, 4242u);
}

TEST(WorldGenModule, InvalidConfigKeepsDefaults)
{
    {
        Fixture f("{ this is not json");
        ASSERT_TRUE(f.Boot());
        f.Pump(3);
        ASSERT_NE(f.wg->GetCurrentWorld(), nullptr);
        EXPECT_EQ(f.wg->GetCurrentWorld()->dimensions.width, 66);
        EXPECT_EQ(f.wg->GetSession().Seed(), 1u);
    }
    {
        Fixture f("{\"size\":\"Gigantic\",\"seed\":-5,\"auto_generate\":\"yes\"}");
        ASSERT_TRUE(f.Boot());
        f.Pump(3);
        ASSERT_NE(f.wg->GetCurrentWorld(), nullptr);
        EXPECT_EQ(f.wg->GetCurrentWorld()->dimensions.width, 66);
        EXPECT_EQ(f.wg->GetSession().Seed(), 1u);
    }
}

TEST(WorldGenModule, AutoGenerateFalseWaitsForNotifyAction)
{
    Fixture f("{\"auto_generate\":false}");
    ASSERT_TRUE(f.Boot());
    f.Pump(5);
    EXPECT_EQ(f.wg->GetWorldRevision(), 0u);
    EXPECT_EQ(f.wg->GetCurrentWorld(), nullptr);
    EXPECT_TRUE(f.msgs.empty());

    f.wg->NotifyAction();
    f.Pump(2);
    EXPECT_EQ(f.wg->GetWorldRevision(), 1u);
    EXPECT_NE(f.wg->GetCurrentWorld(), nullptr);
    EXPECT_EQ(f.msgs.size(), 1u);
}

TEST(WorldGenModule, SameSeedIsDeterministic)
{
    Fixture a("{\"seed\":9001}");
    ASSERT_TRUE(a.Boot());
    a.Pump(3);
    // Copy before the second fixture replaces the file-scope pointers.
    const WorldGeneration::WorldStructure worldA = *a.wg->GetCurrentWorld();

    Fixture b("{\"seed\":9001}");
    ASSERT_TRUE(b.Boot());
    b.Pump(3);
    ASSERT_NE(b.wg->GetCurrentWorld(), nullptr);
    EXPECT_TRUE(SameCells(worldA, *b.wg->GetCurrentWorld()));
}

TEST(WorldGenModule, StopUnregistersHealthReporter)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.Pump(3);
    ASSERT_TRUE(f.wg->IsHealthRegistered());
    f.app->RequestShutdown();
    f.Pump(20);
    EXPECT_FALSE(f.wg->IsHealthRegistered());
}

// ---- Retry mechanism (Feature 3): events, Complete.retryAttempts, counters, health --------------------------------------

namespace {

struct RetryTap
{
    Dia::MessageBus::BusSubscriptionHandle sub;
    std::vector<WorldGenerationRetry> msgs;
    explicit RetryTap(Fixture& f)
    {
        f.bus->GetBus().RegisterType<WorldGenerationRetry, 16>();
        sub = f.bus->GetBus().Subscribe<WorldGenerationRetry>(StringCRC("TestRetrySink"),
            [this](const WorldGenerationRetry& m) { msgs.push_back(m); });
    }
};

uint64_t CounterValue(const char* name)
{
    auto* c = Dia::Observation::Metric::MetricRegistry::Instance().FindCounter(StringCRC(name));
    return c ? c->Value() : 0u;
}

Dia::Observation::Health::HealthStatus HealthOf(Fixture& f) { return f.wg->GetHealthReporter().Report().status; }

} // namespace

TEST(WorldGenModuleRetry, FirstAttemptSuccessPublishesNoRetryEventAndZeroRetryAttempts)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    RetryTap tap(f);
    f.Pump(3);
    f.msgs.clear();
    f.wg->GetSession().Regenerate(Json::Value(Json::objectValue));
    f.Pump(2);
    EXPECT_TRUE(tap.msgs.empty());
    ASSERT_EQ(f.msgs.size(), 1u);
    EXPECT_EQ(f.msgs[0].retryAttempts, 0u);
    ASSERT_NE(f.wg->GetLastReport(), nullptr);
    EXPECT_EQ(f.wg->GetLastReport()->attempts.Size(), 1u);
    EXPECT_EQ(f.wg->GetReportRevision(), 1u);
}

TEST(WorldGenModuleRetry, EachFailedAttemptPublishesARetryEventAndCompleteCarriesTheRetryCount)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    RetryTap tap(f);
    f.Pump(3);
    f.msgs.clear();
    WorldGeneration::Testing::FailFirstNGuarantee flaky("Test.Flaky", 2);
    ASSERT_TRUE(f.wg->GetSiteRegistry().RegisterGuarantee(&flaky));

    f.wg->GetSession().Regenerate(Json::Value(Json::objectValue));
    f.Pump(5);   // one attempt per frame

    ASSERT_EQ(tap.msgs.size(), 2u);
    EXPECT_EQ(tap.msgs[0].attemptNumber, 1u);
    EXPECT_EQ(tap.msgs[1].attemptNumber, 2u);
    for (const WorldGenerationRetry& m : tap.msgs)
    {
        EXPECT_EQ(m.maxAttempts, WorldGeneration::kDefaultMaxRetryAttempts);
        EXPECT_EQ(m.failedGuaranteeCount, 1u);
    }
    ASSERT_EQ(f.msgs.size(), 1u);
    EXPECT_EQ(f.msgs[0].retryAttempts, 2u);
    EXPECT_EQ(f.msgs[0].worldRevision, f.wg->GetWorldRevision());
    EXPECT_EQ(f.wg->GetLastReport()->attempts.Size(), 3u);
}

TEST(WorldGenModuleRetry, UnsatisfiableGenerationPublishesNoCompleteFailsHealthAndRecoversOnTheNextSuccess)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    RetryTap tap(f);
    f.Pump(3);
    f.msgs.clear();
    EXPECT_EQ(HealthOf(f), Dia::Observation::Health::HealthStatus::kOK);
    const uint64_t attemptsBefore = CounterValue("cow.worldgen.attempts");
    const uint64_t unsatBefore = CounterValue("cow.worldgen.unsatisfiable");

    WorldGeneration::Testing::FailFirstNGuarantee flaky("Test.Flaky", 3);   // fails the first three validations
    ASSERT_TRUE(f.wg->GetSiteRegistry().RegisterGuarantee(&flaky));
    Json::Value a(Json::objectValue); a["maxRetryAttempts"] = 3;
    f.wg->GetSession().Regenerate(a);
    f.Pump(5);   // one attempt per frame

    EXPECT_TRUE(f.msgs.empty());                 // no world accepted: no Complete
    ASSERT_EQ(tap.msgs.size(), 3u);              // every failed attempt, including the last
    EXPECT_EQ(f.wg->GetLastReport()->outcome, WorldGeneration::GenerationOutcome::Unsatisfiable);
    EXPECT_EQ(HealthOf(f), Dia::Observation::Health::HealthStatus::kFailing);
    EXPECT_EQ(CounterValue("cow.worldgen.attempts") - attemptsBefore, 3u);
    EXPECT_EQ(CounterValue("cow.worldgen.unsatisfiable") - unsatBefore, 1u);

    // The guarantee now passes (its three failing calls are used up): the next Regenerate succeeds and health recovers.
    f.wg->GetSession().Regenerate(Json::Value(Json::objectValue));
    f.Pump(5);
    ASSERT_EQ(f.msgs.size(), 1u);
    EXPECT_EQ(f.msgs[0].retryAttempts, 0u);
    EXPECT_EQ(HealthOf(f), Dia::Observation::Health::HealthStatus::kOK);
    EXPECT_EQ(CounterValue("cow.worldgen.unsatisfiable") - unsatBefore, 1u);   // not incremented again
}

TEST(WorldGenModuleRetry, LivePreviewNeverPublishesRetryEventsOrTouchesTheReport)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    RetryTap tap(f);
    f.Pump(3);
    Json::Value s(Json::objectValue); s["rivers"] = 0.9;
    f.wg->GetSession().Set(s);
    f.Pump(2);
    EXPECT_TRUE(tap.msgs.empty());
    EXPECT_EQ(f.wg->GetLastReport(), nullptr);
    EXPECT_EQ(f.wg->GetReportRevision(), 0u);
}

TEST(WorldGenModuleRetry, ConfigSetsTheRetryBoundAndBadValuesKeepTheDefault)
{
    Fixture f("{\"max_retry_attempts\":4}");
    ASSERT_TRUE(f.Boot());
    f.Pump(3);
    EXPECT_EQ(f.wg->GetSession().MaxRetryAttempts(), 4u);

    Fixture g("{\"max_retry_attempts\":\"many\"}");
    ASSERT_TRUE(g.Boot());
    g.Pump(3);
    EXPECT_EQ(g.wg->GetSession().MaxRetryAttempts(), WorldGeneration::kDefaultMaxRetryAttempts);
}

TEST(WorldGenModuleRetry, RetryEventIsTriviallyCopyableWithAStableTypeId)
{
    EXPECT_TRUE(std::is_trivially_copyable_v<WorldGenerationRetry>);
    EXPECT_EQ(WorldGenerationRetry::kTypeId, StringCRC("CoW.WorldGenerationRetry"));
    const WorldGenerationRetry m{};
    EXPECT_EQ(m.attemptNumber, 0u);
    EXPECT_EQ(m.maxAttempts, 0u);
    EXPECT_EQ(m.failedGuaranteeCount, 0u);
}

TEST(WorldGenModuleRetry, UnsatisfiableHealthSurvivesALivePreviewTick)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.Pump(3);
    WorldGeneration::Testing::FixedGuarantee never("Test.Never", false);
    ASSERT_TRUE(f.wg->GetSiteRegistry().RegisterGuarantee(&never));
    Json::Value a(Json::objectValue); a["maxRetryAttempts"] = 2;
    f.wg->GetSession().Regenerate(a);
    f.Pump(5);   // one attempt per frame
    ASSERT_EQ(HealthOf(f), Dia::Observation::Health::HealthStatus::kFailing);

    // A live-preview tick publishes Complete for its terrain-only world but is not a retry generation: the failure stands.
    f.msgs.clear();
    Json::Value s(Json::objectValue); s["rivers"] = 0.9;
    f.wg->GetSession().Set(s);
    f.Pump(2);
    EXPECT_EQ(f.msgs.size(), 1u);
    EXPECT_EQ(f.msgs[0].retryAttempts, 0u);
    EXPECT_EQ(HealthOf(f), Dia::Observation::Health::HealthStatus::kFailing);
}
