// Integration tests for CoW::WorldGenerationModule inside a real Application:
// MessageBusHostModule + RandomStreamModule + WorldGenerationModule on a 30 Hz SimPU.
#include <gtest/gtest.h>
#include <Modules/WorldGen/WorldGenerationModule.h>   // first: RandomStream.h must precede windows.h (min/max macros)
#include <Shared/WorldGeneration/Testing/TerrainRulesTestHelpers.h>
#include <Shared/World/BiomeRegistry.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <Modules/MessageBusHostModule.h>
#include <Modules/Plumbing/RandomStreamModule.h>
#include <Shared/WorldGeneration/Testing/TestGuarantees.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaCore/FilePath/Path.h>
#include <DiaCore/FilePath/PathStore.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
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
std::string g_rulesTextOverride;   // non-empty: the sidecar text the next module is created with (task 26 tests)
bool        g_noSidecarText = false;      // alias tests: do not inject text, so the module's own load path runs
std::string g_defaultPathOverride;        // non-empty: the default sidecar path the next module is created with
std::string g_biomesTextOverride;         // non-empty: the biomes text the next module is created with
std::string g_biomesPathOverride;         // non-empty: the biomes path the next module is created with (else the shipped file)

std::string FindShippedBiomes()
{
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::absolute(std::filesystem::path(__FILE__), ec).parent_path();
    while (!dir.empty())
    {
        const std::filesystem::path candidate = dir / "Cluiche/Assets/CoW/Global/World/biomes.json";
        if (std::filesystem::is_regular_file(candidate, ec)) return candidate.string();
        const std::filesystem::path parent = dir.parent_path();
        if (parent == dir) break;
        dir = parent;
    }
    return {};
}

Module* CreateWg(const StringCRC& id)
{
    g_wg = new WorldGenerationModule(id);
    if (!g_biomesTextOverride.empty())
        g_wg->SetBiomesText(g_biomesTextOverride);
    else
        g_wg->SetBiomesPath(g_biomesPathOverride.empty() ? FindShippedBiomes() : g_biomesPathOverride);
    if (!g_noSidecarText)
        g_wg->SetRulesSidecarText(g_rulesTextOverride.empty() ? CoW::WorldGeneration::TestSidecarText() : g_rulesTextOverride);
    if (!g_defaultPathOverride.empty())
        g_wg->SetRulesSidecarPath(g_defaultPathOverride);
    return g_wg;
}

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

// ---- Timeout and failure handling (Feature 5): Failed event, cancel, timeout, error, health ------------------------------

#include <Shared/WorldGeneration/GenerationGuard.h>
#include <stdexcept>

namespace {

struct FailedTap
{
    Dia::MessageBus::BusSubscriptionHandle sub;
    std::vector<WorldGenerationFailed> msgs;
    explicit FailedTap(Fixture& f)
    {
        f.bus->GetBus().RegisterType<WorldGenerationFailed, 4>();
        sub = f.bus->GetBus().Subscribe<WorldGenerationFailed>(StringCRC("TestFailedSink"),
            [this](const WorldGenerationFailed& m) { msgs.push_back(m); });
    }
};

struct StepClock : WorldGeneration::IGenerationClock
{
    double now = 10.0;
    double NowSeconds() const override { return now; }
};

// Fails every validation, advances the clock, optionally throws.
class DragGuarantee : public WorldGeneration::IWorldGuarantee
{
public:
    DragGuarantee(StepClock& clock, double step, bool throws) : step(step), mClock(clock), mThrows(throws) {}
    StringCRC GetId() const override { return StringCRC("Test.Drag"); }
    WorldGeneration::GuaranteeResult Check(const WorldGeneration::ValidationContext&) const override
    {
        mClock.now += step;
        if (mThrows) { throw std::runtime_error("Test error"); }
        WorldGeneration::GuaranteeResult r;
        r.idCrc = GetId().Value();
        r.satisfied = !fail;
        if (fail) { r.AddDiagnostic("drag"); }
        return r;
    }
    double step;
    bool fail = true;
private:
    StepClock& mClock;
    bool mThrows;
};

} // namespace

TEST(WorldGenModuleTimeout, FailedEventIsTriviallyCopyableWithAStableTypeId)
{
    EXPECT_TRUE(std::is_trivially_copyable_v<WorldGenerationFailed>);
    EXPECT_EQ(WorldGenerationFailed::kTypeId, StringCRC("CoW.WorldGenerationFailed"));
    const WorldGenerationFailed m{};
    EXPECT_EQ(m.reason, FailureReason::Unsatisfiable);
    EXPECT_EQ(m.seed, 0u);
    EXPECT_EQ(m.attempts, 0u);
    EXPECT_EQ(m.message[0], 0);
    EXPECT_EQ(sizeof(m.message), 128u);
}

TEST(WorldGenModuleTimeout, ARegenerateRunsOneAttemptPerFrame)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.Pump(3);
    WorldGeneration::Testing::FixedGuarantee never("Test.Never", false);
    ASSERT_TRUE(f.wg->GetSiteRegistry().RegisterGuarantee(&never));
    Json::Value a(Json::objectValue); a["maxRetryAttempts"] = 4;
    f.wg->GetSession().Regenerate(a);
    f.wg->NotifyAction();
    f.Pump(1);
    EXPECT_TRUE(f.wg->GetSession().IsGenerating());
    EXPECT_EQ(f.wg->GetSession().Status()["progress"]["attempts"].asUInt(), 1u);
    f.Pump(2);
    EXPECT_TRUE(f.wg->GetSession().IsGenerating());
    f.Pump(2);
    EXPECT_FALSE(f.wg->GetSession().IsGenerating());
    EXPECT_EQ(f.wg->GetLastReport()->attempts.Size(), 4u);
}

TEST(WorldGenModuleTimeout, ATimeoutPublishesFailedKeepsTheWorldFailsHealthAndRecovers)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    FailedTap tap(f);
    f.Pump(3);
    f.msgs.clear();
    const uint32_t revision = f.wg->GetWorldRevision();
    const uint64_t timeoutsBefore = CounterValue("cow.worldgen.timeouts");

    StepClock clock;
    DragGuarantee drag(clock, 4.0, false);
    f.wg->GetSession().SetGenerationClock(&clock);
    ASSERT_TRUE(f.wg->GetSiteRegistry().RegisterGuarantee(&drag));
    Json::Value a(Json::objectValue); a["timeoutSeconds"] = 5;
    f.wg->GetSession().Regenerate(a);
    f.wg->NotifyAction();
    f.Pump(6);

    ASSERT_EQ(tap.msgs.size(), 1u);
    EXPECT_EQ(tap.msgs[0].reason, FailureReason::Timeout);
    EXPECT_EQ(tap.msgs[0].attempts, 2u);
    EXPECT_STREQ(tap.msgs[0].message, "TIMEOUT: World generation exceeded 5.0 seconds");
    EXPECT_TRUE(f.msgs.empty());                                   // no WorldGenerationComplete
    EXPECT_EQ(f.wg->GetWorldRevision(), revision);                 // the previous world stands
    EXPECT_EQ(HealthOf(f), Dia::Observation::Health::HealthStatus::kFailing);
    EXPECT_EQ(CounterValue("cow.worldgen.timeouts") - timeoutsBefore, 1u);

    // Recover: the guarantee passes now; the next Regenerate succeeds and health recovers.
    drag.fail = false;
    drag.step = 0.0;
    f.wg->GetSession().Regenerate(Json::Value(Json::objectValue));
    f.wg->NotifyAction();
    f.Pump(3);
    ASSERT_EQ(f.msgs.size(), 1u);
    EXPECT_EQ(f.wg->GetWorldRevision(), revision + 1);
    EXPECT_EQ(HealthOf(f), Dia::Observation::Health::HealthStatus::kOK);
}

TEST(WorldGenModuleTimeout, AnErrorPublishesFailedWithTheExceptionMessageAndFailsHealth)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    FailedTap tap(f);
    f.Pump(3);
    f.msgs.clear();
    const uint32_t revision = f.wg->GetWorldRevision();
    const uint64_t errorsBefore = CounterValue("cow.worldgen.errors");

    StepClock clock;
    DragGuarantee boom(clock, 0.0, true);
    ASSERT_TRUE(f.wg->GetSiteRegistry().RegisterGuarantee(&boom));
    f.wg->GetSession().Regenerate(Json::Value(Json::objectValue));
    f.wg->NotifyAction();
    f.Pump(3);

    ASSERT_EQ(tap.msgs.size(), 1u);
    EXPECT_EQ(tap.msgs[0].reason, FailureReason::Error);
    EXPECT_STREQ(tap.msgs[0].message, "FAILURE: World generation error: Test error");
    EXPECT_TRUE(f.msgs.empty());
    EXPECT_EQ(f.wg->GetWorldRevision(), revision);
    EXPECT_EQ(HealthOf(f), Dia::Observation::Health::HealthStatus::kFailing);
    EXPECT_EQ(CounterValue("cow.worldgen.errors") - errorsBefore, 1u);
}

TEST(WorldGenModuleTimeout, UnsatisfiableAlsoPublishesFailedWithItsOwnReason)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    FailedTap tap(f);
    f.Pump(3);
    WorldGeneration::Testing::FixedGuarantee never("Test.Never", false);
    ASSERT_TRUE(f.wg->GetSiteRegistry().RegisterGuarantee(&never));
    Json::Value a(Json::objectValue); a["maxRetryAttempts"] = 2;
    f.wg->GetSession().Regenerate(a);
    f.wg->NotifyAction();
    f.Pump(5);
    ASSERT_EQ(tap.msgs.size(), 1u);
    EXPECT_EQ(tap.msgs[0].reason, FailureReason::Unsatisfiable);
    EXPECT_EQ(tap.msgs[0].attempts, 2u);
    EXPECT_EQ(std::strncmp(tap.msgs[0].message, "FAILURE", 7), 0);
}

TEST(WorldGenModuleTimeout, ACancelMidRunPublishesFailedKeepsTheWorldAndLeavesHealthAlone)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    FailedTap tap(f);
    f.Pump(3);
    f.msgs.clear();
    const uint32_t revision = f.wg->GetWorldRevision();
    const uint64_t cancelsBefore = CounterValue("cow.worldgen.cancels");
    ASSERT_EQ(HealthOf(f), Dia::Observation::Health::HealthStatus::kOK);

    WorldGeneration::Testing::FixedGuarantee never("Test.Never", false);
    ASSERT_TRUE(f.wg->GetSiteRegistry().RegisterGuarantee(&never));
    Json::Value a(Json::objectValue); a["maxRetryAttempts"] = 10;
    f.wg->GetSession().Regenerate(a);
    f.wg->NotifyAction();
    f.Pump(2);
    ASSERT_TRUE(f.wg->GetSession().IsGenerating());

    const Json::Value r = f.wg->GetSession().Cancel(Json::Value(Json::objectValue));
    ASSERT_TRUE(r["cancelled"].asBool());
    f.Pump(2);

    ASSERT_EQ(tap.msgs.size(), 1u);
    EXPECT_EQ(tap.msgs[0].reason, FailureReason::ManualCancel);
    EXPECT_EQ(tap.msgs[0].attempts, 2u);
    EXPECT_STREQ(tap.msgs[0].message, "CANCEL: World generation cancelled by user");
    EXPECT_TRUE(f.msgs.empty());
    EXPECT_EQ(f.wg->GetWorldRevision(), revision);
    EXPECT_EQ(HealthOf(f), Dia::Observation::Health::HealthStatus::kOK);   // a user action is not a failure
    EXPECT_EQ(CounterValue("cow.worldgen.cancels") - cancelsBefore, 1u);
    EXPECT_FALSE(f.wg->GetSession().IsGenerating());
}

TEST(WorldGenModuleTimeout, ASuccessfulRegeneratePublishesNoFailedEvent)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    FailedTap tap(f);
    f.Pump(3);
    f.wg->GetSession().Regenerate(Json::Value(Json::objectValue));
    f.wg->NotifyAction();
    f.Pump(3);
    EXPECT_TRUE(tap.msgs.empty());
}

TEST(WorldGenModuleTimeout, ConfigSetsTheTimeoutAndBadValuesKeepTheDefault)
{
    Fixture f("{\"timeout_seconds\":12.5}");
    ASSERT_TRUE(f.Boot());
    f.Pump(3);
    EXPECT_FLOAT_EQ(f.wg->GetSession().TimeoutSeconds(), 12.5f);

    Fixture g("{\"timeout_seconds\":\"soon\"}");
    ASSERT_TRUE(g.Boot());
    g.Pump(3);
    EXPECT_FLOAT_EQ(g.wg->GetSession().TimeoutSeconds(), WorldGeneration::kDefaultTimeoutSeconds);

    Fixture h("{\"timeout_seconds\":-3}");
    ASSERT_TRUE(h.Boot());
    h.Pump(3);
    EXPECT_FLOAT_EQ(h.wg->GetSession().TimeoutSeconds(), WorldGeneration::kDefaultTimeoutSeconds);
}

// ---- Parameter Files task 26 (AC 18, AC 19): the module loads the terrain sidecar; a bad one refuses generation ----

TEST(WorldGenModuleRules, GoodSidecarGeneratesAndHealthStaysOk)
{
    g_rulesTextOverride.clear();
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.Pump(3);
    EXPECT_TRUE(f.wg->AreRulesLoaded());
    EXPECT_TRUE(f.wg->GetRulesErrorKey().empty());
    EXPECT_EQ(HealthOf(f), Dia::Observation::Health::HealthStatus::kOK);
    EXPECT_EQ(f.wg->GetWorldRevision(), 1u);
}

TEST(WorldGenModuleRules, BrokenSidecarPutsHealthInErrorNamingTheKeyAndRefusesGeneration)
{
    std::string text = CoW::WorldGeneration::TestSidecarText();
    const std::string needle = "\"default\": 0.02";
    const size_t pos = text.find(needle);
    ASSERT_NE(pos, std::string::npos);
    text.replace(pos, needle.size(), "\"default\": 99.0");   // kCoastElev default out of range
    g_rulesTextOverride = text;

    Fixture f;
    const bool booted = f.Boot();
    g_rulesTextOverride.clear();
    ASSERT_TRUE(booted);
    f.Pump(5);

    EXPECT_FALSE(f.wg->AreRulesLoaded());
    EXPECT_EQ(f.wg->GetRulesErrorKey(), "kCoastElev");
    const Dia::Observation::Health::Health health = f.wg->GetHealthReporter().Report();
    EXPECT_EQ(health.status, Dia::Observation::Health::HealthStatus::kFailing);
    EXPECT_NE(std::string(health.reason.AsChar()).find("kCoastElev"), std::string::npos) << health.reason.AsChar();

    // Generation is refused: no world, and the commands' shared entry point answers rules_not_loaded.
    EXPECT_EQ(f.wg->GetWorldRevision(), 0u);
    EXPECT_EQ(f.wg->GetCurrentWorld(), nullptr);
    const Json::Value reply = f.wg->GetSession().Regenerate(Json::Value(Json::objectValue));
    EXPECT_FALSE(reply["success"].asBool());
    EXPECT_EQ(reply["error"].asString(), "rules_not_loaded");
    f.Pump(5);
    EXPECT_EQ(f.wg->GetWorldRevision(), 0u);
}

// ---- Biome Registry task 10: the module owns the registry, loads it at start, refuses generation on a bad file ----

namespace {
std::string DuplicateIdBiomes()
{
    // The shipped file with the Lake row's id changed to the Ocean row's: the loader names the "id" field.
    std::ifstream in(FindShippedBiomes(), std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const std::string needle = "\"id\": \"Biome.Lake\"";
    const size_t pos = text.find(needle);
    if (pos != std::string::npos)
        text.replace(pos, needle.size(), "\"id\": \"Biome.Ocean\"");
    return text;
}
}

TEST(WorldGenModuleBiomes, ShippedFileLoadsThirteenBiomesAndHealthStaysOk)
{
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.Pump(3);
    EXPECT_TRUE(f.wg->AreBiomesLoaded());
    EXPECT_EQ(f.wg->GetBiomeRegistry().Count(), 13u);
    EXPECT_EQ(HealthOf(f), Dia::Observation::Health::HealthStatus::kOK);
    EXPECT_EQ(f.wg->GetWorldRevision(), 1u);
}

TEST(WorldGenModuleBiomes, DuplicateIdFailsHealthNamingTheFieldAndRefusesGeneration)
{
    g_biomesTextOverride = DuplicateIdBiomes();
    Fixture f;
    const bool booted = f.Boot();
    g_biomesTextOverride.clear();
    ASSERT_TRUE(booted);
    f.Pump(5);

    EXPECT_FALSE(f.wg->AreBiomesLoaded());
    EXPECT_EQ(f.wg->GetBiomeRegistry().Count(), 0u);
    EXPECT_EQ(f.wg->GetBiomesErrorField(), "id");
    const auto health = f.wg->GetHealthReporter().Report();
    EXPECT_EQ(health.status, Dia::Observation::Health::HealthStatus::kFailing);
    EXPECT_NE(std::string(health.reason.AsChar()).find("id"), std::string::npos) << health.reason.AsChar();
    EXPECT_NE(std::string(health.reason.AsChar()).find("biomes_load_failed"), std::string::npos) << health.reason.AsChar();

    EXPECT_EQ(f.wg->GetWorldRevision(), 0u);
    EXPECT_EQ(f.wg->GetCurrentWorld(), nullptr);
    const Json::Value reply = f.wg->GetSession().Regenerate(Json::Value(Json::objectValue));
    EXPECT_FALSE(reply["success"].asBool());
    EXPECT_EQ(reply["error"].asString(), "biomes_not_loaded");
    f.Pump(5);
    EXPECT_EQ(f.wg->GetWorldRevision(), 0u);
    EXPECT_TRUE(f.msgs.empty());
}

TEST(WorldGenModuleBiomes, MissingFileFailsHealthAndRefusesGeneration)
{
    g_biomesPathOverride = "Z:/definitely/not/here/biomes.json";
    Fixture f;
    const bool booted = f.Boot();
    g_biomesPathOverride.clear();
    ASSERT_TRUE(booted);
    f.Pump(5);

    EXPECT_FALSE(f.wg->AreBiomesLoaded());
    EXPECT_NE(f.wg->GetBiomesErrorReason().find("Z:/definitely/not/here/biomes.json"), std::string::npos) << f.wg->GetBiomesErrorReason();
    EXPECT_EQ(f.wg->GetHealthReporter().Report().status, Dia::Observation::Health::HealthStatus::kFailing);
    EXPECT_EQ(f.wg->GetWorldRevision(), 0u);
    EXPECT_EQ(f.wg->GetSession().Regenerate(Json::Value(Json::objectValue))["error"].asString(), "biomes_not_loaded");
}

// ---- Terrain sidecar through its catalogue alias (config key rules_sidecar_alias, PathStore read, 2026-11-20) ----

namespace {

// A sidecar file in the temp directory plus a PathStore alias for it; both removed on scope exit.
struct AliasedSidecar
{
    std::string alias;
    std::string path;
    bool registered = false;

    AliasedSidecar(const char* aliasName, bool writeFile, bool registerAlias)
        : alias(aliasName)
    {
        path = (std::filesystem::temp_directory_path() / (std::string(aliasName) + ".json")).generic_string();
        if (writeFile)
        {
            std::ofstream out(path, std::ios::binary);
            out << CoW::WorldGeneration::TestSidecarText();
        }
        else
        {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
        if (registerAlias && !Dia::Core::PathStore::IsPathAliasRegistered(Dia::Core::Path::Alias(alias.c_str())))
        {
            Dia::Core::PathStore::RegisterToStore(Dia::Core::Path::Alias(alias.c_str()), Dia::Core::Path::String(path.c_str()));
            registered = true;
        }
    }
    ~AliasedSidecar()
    {
        if (registered)
            Dia::Core::PathStore::UnregisterFromStore(Dia::Core::Path::Alias(alias.c_str()));
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
};

// Boots with NO sidecar text injected, so the module's own load path runs.
struct NoTextScope
{
    NoTextScope() { g_noSidecarText = true; }
    ~NoTextScope() { g_noSidecarText = false; g_defaultPathOverride.clear(); }
};

} // namespace

TEST(WorldGenModuleRulesAlias, ConfiguredAliasLoadsTheSidecarAndAppliesTheDeferredConfig)
{
    NoTextScope noText;
    AliasedSidecar sidecar("test.wg_rules_ok", true, true);
    Fixture f("{\"rules_sidecar_alias\":\"test.wg_rules_ok\",\"seed\":4242,\"size\":\"Standard\"}");
    ASSERT_TRUE(f.Boot());
    f.Pump(3);
    EXPECT_TRUE(f.wg->AreRulesLoaded());
    EXPECT_EQ(HealthOf(f), Dia::Observation::Health::HealthStatus::kOK);
    ASSERT_NE(f.wg->GetCurrentWorld(), nullptr);
    EXPECT_EQ(f.wg->GetSession().Seed(), 4242u);
    EXPECT_EQ(f.wg->GetCurrentWorld()->dimensions.width, 80);
}

TEST(WorldGenModuleRulesAlias, RegenerateOnStartWaitsForTheAliasLoadedRules)
{
    NoTextScope noText;
    AliasedSidecar sidecar("test.wg_rules_regen", true, true);
    Fixture f("{\"rules_sidecar_alias\":\"test.wg_rules_regen\",\"regenerate_on_start\":true}");
    ASSERT_TRUE(f.Boot());
    f.Pump(4);
    EXPECT_TRUE(f.wg->AreRulesLoaded());
    EXPECT_EQ(f.wg->GetWorldRevision(), 1u);
}

TEST(WorldGenModuleRulesAlias, UnregisteredAliasFailsHealthNamingItAndRefusesGenerationWithNoFallback)
{
    NoTextScope noText;
    AliasedSidecar good("test.wg_rules_fallback_bait", true, false);
    g_defaultPathOverride = good.path;
    Fixture f("{\"rules_sidecar_alias\":\"test.wg_rules_missing_alias\"}");
    ASSERT_TRUE(f.Boot());
    f.Pump(4);
    EXPECT_FALSE(f.wg->AreRulesLoaded());
    const auto health = f.wg->GetHealthReporter().Report();
    EXPECT_EQ(health.status, Dia::Observation::Health::HealthStatus::kFailing);
    EXPECT_NE(std::string(health.reason.AsChar()).find("test.wg_rules_missing_alias"), std::string::npos) << health.reason.AsChar();
    EXPECT_EQ(f.wg->GetWorldRevision(), 0u);
    const Json::Value reply = f.wg->GetSession().Regenerate(Json::Value(Json::objectValue));
    EXPECT_FALSE(reply["success"].asBool());
    EXPECT_EQ(reply["error"].asString(), "rules_not_loaded");
}

TEST(WorldGenModuleRulesAlias, RegisteredAliasWithMissingFileFailsNamingTheAliasAndThePath)
{
    NoTextScope noText;
    AliasedSidecar sidecar("test.wg_rules_nofile", false, true);
    ASSERT_TRUE(sidecar.registered);
    Fixture f("{\"rules_sidecar_alias\":\"test.wg_rules_nofile\"}");
    ASSERT_TRUE(f.Boot());
    f.Pump(4);
    EXPECT_FALSE(f.wg->AreRulesLoaded());
    EXPECT_NE(f.wg->GetRulesErrorReason().find(sidecar.path), std::string::npos) << f.wg->GetRulesErrorReason();
    EXPECT_NE(f.wg->GetRulesErrorReason().find("test.wg_rules_nofile"), std::string::npos);
    const auto health = f.wg->GetHealthReporter().Report();
    EXPECT_EQ(health.status, Dia::Observation::Health::HealthStatus::kFailing);
    EXPECT_NE(std::string(health.reason.AsChar()).find("test.wg_rules_nofile"), std::string::npos) << health.reason.AsChar();
    EXPECT_EQ(f.wg->GetSession().Regenerate(Json::Value(Json::objectValue))["error"].asString(), "rules_not_loaded");
}

TEST(WorldGenModuleRulesAlias, NonStringAliasIsIgnoredAndTheDefaultPathIsUsed)
{
    NoTextScope noText;
    AliasedSidecar file("test.wg_rules_default_file", true, false);
    g_defaultPathOverride = file.path;
    Fixture f("{\"rules_sidecar_alias\":123}");
    ASSERT_TRUE(f.Boot());
    f.Pump(3);
    EXPECT_TRUE(f.wg->AreRulesLoaded());
    EXPECT_EQ(HealthOf(f), Dia::Observation::Health::HealthStatus::kOK);
    EXPECT_EQ(f.wg->GetWorldRevision(), 1u);
}

TEST(WorldGenModuleRulesAlias, NoKeyUsesTheDefaultPath)
{
    NoTextScope noText;
    AliasedSidecar file("test.wg_rules_default_nokey", true, false);
    g_defaultPathOverride = file.path;
    Fixture f;
    ASSERT_TRUE(f.Boot());
    f.Pump(3);
    EXPECT_TRUE(f.wg->AreRulesLoaded());
    EXPECT_EQ(f.wg->GetRulesSidecarPath(), file.path);
}

TEST(WorldGenModuleRulesAlias, InjectedTextStillWinsOverAnAlias)
{
    g_rulesTextOverride.clear();
    Fixture f("{\"rules_sidecar_alias\":\"test.wg_rules_never_registered\"}");
    ASSERT_TRUE(f.Boot());
    f.Pump(3);
    EXPECT_TRUE(f.wg->AreRulesLoaded());
    EXPECT_EQ(HealthOf(f), Dia::Observation::Health::HealthStatus::kOK);
    EXPECT_EQ(f.wg->GetWorldRevision(), 1u);
}
