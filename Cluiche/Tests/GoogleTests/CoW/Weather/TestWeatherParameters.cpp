// A4 WeatherModule Task 4: parameters at start (AC 18 amended, AC 23) and the sidecar behind queued reloads / packs
// (AC 20). Strict load against the registry's biome ids, Failing on any fault, no fallback.
#include <Modules/Weather/WeatherModule.h>   // first: pulls WorldGenerationModule.h before windows.h
#include "WeatherParamTestKit.h"
#include "../WorldStep/WorldStepTestRig.h"

namespace
{
using namespace WeatherParamKit;
using Dia::Core::StringCRC;
using Dia::Observation::Health::HealthStatus;
using Dia::Observation::Log::LogLevel;

Json::Value Sidecar() { return ParseJson(SidecarText()); }

class WeatherParamsTest : public ParamStageTest {};

class WeatherProbe : public CoW::WeatherModule
{
public:
    explicit WeatherProbe(const StringCRC& id) : CoW::WeatherModule(id) {}
    Dia::ApplicationFlow::StartResult CallStart() { return DoStart(); }
    Dia::ApplicationFlow::StopResult CallStop() { return DoStop(); }
};
}

// ---- AC 18 (amended): every fault is Failing naming the dotted key, never ready, never steps ---------------------

TEST_F(WeatherParamsTest, MissingSidecarFileFailsTheStageNamingTheSidecar)
{
    source.fail = true;
    EXPECT_FALSE(Load());
    EXPECT_STREQ("sidecar", err.key);
    EXPECT_TRUE(Health(HealthStatus::kFailing));
    EXPECT_EQ(StringCRC("sidecar"), stage.GetHealth().reason);
    EXPECT_EQ(static_cast<uint32_t>(CoW::Weather::CauseCode::kParameterFault), stage.GetCauseCode());
    EXPECT_FALSE(stage.Parameters().IsLoaded());
}

TEST_F(WeatherParamsTest, FaultedStageNeverBecomesReadyAndNeverSteps)
{
    source.fail = true;
    ASSERT_FALSE(Load());
    SpinUp(1);   // the world is announced, the faulted stage refuses to spin up
    EXPECT_FALSE(stage.IsReady());
    EXPECT_FALSE(stage.IsReadyForRevision(1));
    EXPECT_EQ(0u, stage.GetSnapshot().cellCount);
    unsigned published = 0;
    stage.SetPublishSeam([&](uint64_t, Draoi::HexWorld::Weather::Season) { ++published; });
    Step(0);
    EXPECT_EQ(0u, published);
    EXPECT_TRUE(Health(HealthStatus::kFailing));   // a new world did not clear it
    EXPECT_GE(Log().Count(LogLevel::kError, "parameter fault at 'sidecar'"), 1u);
}

TEST_F(WeatherParamsTest, BadValueInTheSidecarNamesItsDottedKey)
{
    Json::Value doc = Sidecar();
    doc["entries"][1]["default"] = -1.0;   // solar.cloud_albedo, below its range
    source.text = DumpJson(doc);
    EXPECT_FALSE(Load());
    EXPECT_STREQ("solar.cloud_albedo", err.key);
    EXPECT_TRUE(Health(HealthStatus::kFailing));
    EXPECT_EQ(StringCRC("solar.cloud_albedo"), stage.GetHealth().reason);
    SpinUp(1);
    EXPECT_FALSE(stage.IsReady());
}

TEST_F(WeatherParamsTest, BoundKeyMissingFromTheSidecarNamesTheKey)
{
    Json::Value doc = Sidecar();
    Json::Value kept(Json::arrayValue);
    for (const Json::Value& e : doc["entries"])
    {
        if (e["key"].asString() != "solar.cloud_decay") { kept.append(e); }
    }
    doc["entries"] = kept;
    source.text = DumpJson(doc);
    EXPECT_FALSE(Load());
    EXPECT_STREQ("solar.cloud_decay", err.key);
    EXPECT_TRUE(Health(HealthStatus::kFailing));
    EXPECT_FALSE(stage.IsReady());
}

TEST_F(WeatherParamsTest, SidecarKeyWithNoBindingNamesTheKey)
{
    Json::Value doc = Sidecar();
    Json::Value extra = doc["entries"][0];
    extra["key"] = "solar.invented_rate";
    doc["entries"].append(extra);
    source.text = DumpJson(doc);
    EXPECT_FALSE(Load());
    EXPECT_STREQ("solar.invented_rate", err.key);
    EXPECT_TRUE(Health(HealthStatus::kFailing));
}

TEST_F(WeatherParamsTest, PackThatBreaksAConstraintFailsTheLoad)
{
    std::vector<Json::Value> packs;
    packs.push_back(ParseJson("{\"format\":\"cow.weather_params\",\"version\":1,\"precipitation.snow_threshold\":0.4}"));
    EXPECT_FALSE(stage.LoadParameters(source, packs, err));
    EXPECT_TRUE(Health(HealthStatus::kFailing));
    EXPECT_FALSE(stage.Parameters().IsLoaded());   // no fallback to the sidecar defaults
}

TEST_F(WeatherParamsTest, ValidSidecarLoadsAndSnapshotsTheDefaults)
{
    ASSERT_TRUE(Load()) << err.key << ": " << err.reason;
    EXPECT_TRUE(Health(HealthStatus::kOK));
    EXPECT_EQ(1u, source.reads);
    const Draoi::HexWorld::Weather::WeatherParameters p = Snapshot();
    EXPECT_FLOAT_EQ(0.4f, p.cloudAlbedo);
    EXPECT_FLOAT_EQ(0.35f, p.meltThreshold);
    EXPECT_TRUE(p.edgeInflow);
}

TEST_F(WeatherParamsTest, ActivePackIsAppliedAtLoad)
{
    std::vector<Json::Value> packs;
    packs.push_back(ParseJson("{\"format\":\"cow.weather_params\",\"version\":1,\"solar.cloud_albedo\":0.55}"));
    ASSERT_TRUE(stage.LoadParameters(source, packs, err)) << err.key << ": " << err.reason;
    EXPECT_FLOAT_EQ(0.55f, Snapshot().cloudAlbedo);
}

TEST_F(WeatherParamsTest, SpinUpRunsOnTheSidecarParameters)
{
    ASSERT_TRUE(Load());
    SpinUp(1);
    ASSERT_TRUE(stage.IsReadyForRevision(1));
    RefSim ref(Snapshot());
    const CoW::Weather::WeatherSnapshot snap = stage.GetSnapshot();
    ASSERT_NE(nullptr, snap.fields);
    EXPECT_TRUE(WT::SameFields(ref.sim.Fields(), *snap.fields));
}

TEST_F(WeatherParamsTest, ParamsHashIsStableAcrossLoadsAndNonZero)
{
    ASSERT_TRUE(Load());
    CountingSource other;
    CoW::Weather::WeatherParameterStore second;
    CoW::Weather::ParamError e2;
    ASSERT_TRUE(second.Load(other, WT::TestRegistry(), e2));
    EXPECT_NE(0u, stage.ParametersHash());
    EXPECT_EQ(stage.ParametersHash(), second.Hash());
}

// ---- AC 23: no sidecar read after init except through a queued reload ---------------------------------------------

TEST_F(WeatherParamsTest, NoSidecarReadAfterInitExceptThroughAQueuedReload)
{
    ASSERT_TRUE(Load());
    ASSERT_EQ(1u, source.reads);
    SpinUp(1);
    ASSERT_TRUE(stage.QueueParameterSet("solar.cloud_albedo", 0.5f, err));
    for (uint32_t i = 0; i < 4; ++i) { Step(i * CoW::kDaysPerWeek); }
    EXPECT_EQ(1u, source.reads);

    ASSERT_TRUE(stage.QueueReload());
    EXPECT_EQ(1u, source.reads);   // queued only: the read happens at Capture
    Step(5 * CoW::kDaysPerWeek);
    EXPECT_EQ(2u, source.reads);
    Step(6 * CoW::kDaysPerWeek);
    EXPECT_EQ(2u, source.reads);
}

// ---- AC 20: pack atomic; reload only at Capture; failed reload keeps the previous values and logs -----------------

TEST_F(WeatherParamsTest, ReloadDiscardsSetsAndRestoresTheSidecarValuesAtCapture)
{
    ASSERT_TRUE(Load());
    SpinUp(1);
    ASSERT_TRUE(stage.QueueParameterSet("solar.cloud_albedo", 0.5f, err));
    Step(0);
    EXPECT_FLOAT_EQ(0.5f, Snapshot().cloudAlbedo);
    const uint64_t changedHash = stage.ParametersHash();

    ASSERT_TRUE(stage.QueueReload());
    EXPECT_FLOAT_EQ(0.5f, Snapshot().cloudAlbedo);   // not before Capture
    Step(CoW::kDaysPerWeek);
    EXPECT_FLOAT_EQ(0.4f, Snapshot().cloudAlbedo);
    EXPECT_NE(changedHash, stage.ParametersHash());
}

TEST_F(WeatherParamsTest, FailedReloadKeepsThePreviousValuesAndLogsAnError)
{
    ASSERT_TRUE(Load());
    SpinUp(1);
    ASSERT_TRUE(stage.QueueParameterSet("solar.cloud_albedo", 0.5f, err));
    Step(0);
    const uint64_t hash = stage.ParametersHash();

    source.fail = true;
    ASSERT_TRUE(stage.QueueReload());
    Step(CoW::kDaysPerWeek);
    EXPECT_FLOAT_EQ(0.5f, Snapshot().cloudAlbedo);
    EXPECT_EQ(hash, stage.ParametersHash());
    EXPECT_GE(Log().Count(LogLevel::kError, "parameter reload failed"), 1u);
    EXPECT_FALSE(Health(HealthStatus::kFailing));   // a failed reload is not a stage fault
    EXPECT_TRUE(stage.IsReady());

    // A reload that parses but breaks the binding keeps the previous values too.
    Json::Value doc = Sidecar();
    doc["entries"][1]["default"] = -1.0;
    source.fail = false;
    source.text = DumpJson(doc);
    ASSERT_TRUE(stage.QueueReload());
    Step(2 * CoW::kDaysPerWeek);
    EXPECT_FLOAT_EQ(0.5f, Snapshot().cloudAlbedo);
    EXPECT_GE(Log().Count(LogLevel::kError, "solar.cloud_albedo"), 1u);

    // And a good file works again afterwards.
    source.text = SidecarText();
    ASSERT_TRUE(stage.QueueReload());
    Step(3 * CoW::kDaysPerWeek);
    EXPECT_FLOAT_EQ(0.4f, Snapshot().cloudAlbedo);
}

TEST_F(WeatherParamsTest, PackIsAtomicRejectedWholeWhenAKeyBreaksAConstraint)
{
    ASSERT_TRUE(Load());
    SpinUp(1);
    ASSERT_TRUE(stage.QueueParameterOverrides(
        ParseJson("{\"format\":\"cow.weather_params\",\"version\":1,\"precipitation.snow_threshold\":0.4,\"solar.cloud_albedo\":0.5}"), err));
    EXPECT_FLOAT_EQ(0.4f, Snapshot().cloudAlbedo);   // nothing before Capture
    Step(0);
    EXPECT_FLOAT_EQ(0.4f, Snapshot().cloudAlbedo);   // the good key did not apply either
    EXPECT_FLOAT_EQ(0.3f, Snapshot().snowThreshold);
    EXPECT_GE(Log().Count(LogLevel::kError, "parameter pack rejected"), 1u);
}

TEST_F(WeatherParamsTest, ValidPackAppliesAtCaptureAndIsVisibleToThatStep)
{
    ASSERT_TRUE(Load());
    SpinUp(1);
    RefSim ref(Snapshot());
    ASSERT_TRUE(stage.QueueParameterOverrides(
        ParseJson("{\"format\":\"cow.weather_params\",\"version\":1,\"solar.cloud_albedo\":0.5}"), err));
    EXPECT_FLOAT_EQ(0.4f, Snapshot().cloudAlbedo);
    Step(0);
    EXPECT_FLOAT_EQ(0.5f, Snapshot().cloudAlbedo);
    ref.params.cloudAlbedo = 0.5f;
    ref.Live(0);
    EXPECT_TRUE(Matches(ref));
}

TEST_F(WeatherParamsTest, NonObjectPackIsRefusedAtQueueTime)
{
    ASSERT_TRUE(Load());
    EXPECT_FALSE(stage.QueueParameterOverrides(Json::Value(3), err));
    EXPECT_STREQ("pack", err.key);
    EXPECT_EQ(0u, stage.QueueCount());
}

// ---- the module: parameters load at DoStart -----------------------------------------------------------------------

namespace
{
struct ModuleParamRig
{
    ModuleParamRig() : harness(WsTest::Config("weather", 7, true)), module(StringCRC("WeatherModule"))
    {
        auto& l = Dia::Observation::Log::Logger::Instance();
        l.RegisterThreadBuffer();
        CoW::WeatherModule::TestHosts hosts;
        hosts.worldStep = &harness.module;
        hosts.worldSource = &world;
        hosts.generationHold = &hold;
        module.UseTestHosts(hosts);
        module.GetStage().SetRegistry(&WT::TestRegistry());
        module.GetStage().SetSpinupWeeks(kSpinUpWeeks);
        module.GetStage().SetLaunchMode(CoW::Weather::WeatherStage::LaunchMode::kSynchronous);
    }
    ~ModuleParamRig()
    {
        module.CallStop();
        Dia::Observation::Log::Logger::Instance().UnregisterThreadBuffer();
    }
    WsTest::Harness harness;
    WeatherProbe module;
    CountingSource source;
    FakeWorldSource world;
    FakeHold hold;
};
}

TEST(WeatherParamsModule, MissingSidecarAtDoStartLeavesTheStageFailingAndNeverReady)
{
    ModuleParamRig rig;
    rig.source.fail = true;
    rig.module.SetSidecarSource(&rig.source);
    EXPECT_EQ(Dia::ApplicationFlow::StartResult::kReady, rig.module.CallStart());   // the module starts; the stage is Failing
    EXPECT_EQ(HealthStatus::kFailing, rig.module.GetStage().GetHealth().status);
    EXPECT_EQ(StringCRC("sidecar"), rig.module.GetStage().GetHealth().reason);
    rig.module.GetStage().OnWorldComplete(1);
    rig.module.GetStage().Update();
    EXPECT_FALSE(rig.module.GetStage().IsReady());
}

TEST(WeatherParamsModule, ValidSidecarAtDoStartLoadsOnceAndTheStageSpinsUp)
{
    ModuleParamRig rig;
    rig.module.SetSidecarSource(&rig.source);
    ASSERT_EQ(Dia::ApplicationFlow::StartResult::kReady, rig.module.CallStart());
    EXPECT_EQ(1u, rig.source.reads);
    EXPECT_TRUE(rig.module.GetStage().Parameters().IsLoaded());
    EXPECT_EQ(HealthStatus::kOK, rig.module.GetStage().GetHealth().status);
    rig.module.GetStage().OnWorldComplete(1);
    rig.module.GetStage().Update();
    EXPECT_TRUE(rig.module.GetStage().IsReadyForRevision(1));
    EXPECT_EQ(1u, rig.source.reads);
}

TEST(WeatherParamsModule, FaultIsClearedByStopAndAStartLoadsAgain)
{
    ModuleParamRig rig;
    rig.source.fail = true;
    rig.module.SetSidecarSource(&rig.source);
    ASSERT_EQ(Dia::ApplicationFlow::StartResult::kReady, rig.module.CallStart());
    ASSERT_EQ(HealthStatus::kFailing, rig.module.GetStage().GetHealth().status);
    rig.module.CallStop();
    rig.source.fail = false;
    ASSERT_EQ(Dia::ApplicationFlow::StartResult::kReady, rig.module.CallStart());
    EXPECT_NE(HealthStatus::kFailing, rig.module.GetStage().GetHealth().status);
    EXPECT_TRUE(rig.module.GetStage().Parameters().IsLoaded());
}
