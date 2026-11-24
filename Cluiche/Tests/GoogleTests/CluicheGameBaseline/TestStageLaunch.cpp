// Launch presets (Stage Catalogue F2) Task 7: catalogue preset names/descriptions (AC 11) and stages.launch (AC 9).
#include <gtest/gtest.h>

#include <Modules/SimNavigationHandlerModule.h>
#include <Modules/StageCatalogueModule.h>
#include <Modules/StageCatalogueQueries.h>
#include <Modules/StageCatalogueState.h>
#include <Testing/StageCatalogueFixture.h>
#include <Types/StageLaunchRequest.h>
#include <DiaApplicationFlow/IApplicationControl.h>

#include <cstring>
#include <memory>
#include <string>

using namespace Cluiche::AppFlow;
using Dia::Core::StringCRC;
namespace Q = Cluiche::AppFlow::StageCatalogueQueries;

namespace {

std::string DiagamePath(const char* fixture)
{
    return (Cluiche::AppFlow::Testing::FindStageCatalogueFixtures(__FILE__) / fixture / "game.diagame").string();
}

Json::Value Args(const char* json)
{
    Json::Value v;
    Json::Reader r;
    r.parse(json, v);
    return v;
}

struct PresetFixture {
    StageCatalogueState state;
    PresetFixture() { EXPECT_EQ(state.LoadCatalogue(DiagamePath("presets").c_str()), StageCatalogueResult::kOk); }
};

// Exposes the launch seams; records what would be sent.
class LaunchModule : public StageCatalogueModule {
public:
    LaunchModule() : StageCatalogueModule(StringCRC("TestLaunch")) {}
    void Configure(const std::string& diagame)
    {
        Json::Value cfg(Json::objectValue);
        cfg["diagame_path"] = diagame;
        Json::FastWriter w;
        OnConfigure(w.write(cfg).c_str());
    }
    bool Start() { return DoStart() == Dia::ApplicationFlow::StartResult::kReady; }
    Json::Value Launch(const Json::Value& args) { return HandleLaunch(args); }

    bool atBoot = true;
    bool sendOk = true;
    unsigned int sent = 0;
    StageLaunchRequest last;

protected:
    bool IsAtBoot() const override { return atBoot; }
    bool SendLaunchRequest(const StageLaunchRequest& request) override
    {
        if (!sendOk) { return false; }
        ++sent;
        last = request;
        return true;
    }
};

struct RecordingControl : Dia::ApplicationFlow::IApplicationControl {
    int plainCalls = 0;
    int presetCalls = 0;
    StringCRC lastStage;
    StringCRC lastPreset;
    void TransitionTo(const StringCRC& s) override { ++plainCalls; lastStage = s; lastPreset = StringCRC(); }
    void TransitionTo(const StringCRC& s, const StringCRC& p) override { ++presetCalls; lastStage = s; lastPreset = p; }
    void RequestShutdown() override {}
    StringCRC GetCurrentStage() const override { return current; }
    StringCRC GetCurrentPreset() const override { return StringCRC(); }
    void GetStageTransitions(const StringCRC&, Dia::Core::Containers::DynamicArrayC<StringCRC, 32>& out) const override { if (bootListsStage) { out.Add(initialStage); } }
    bool RegisterTransitionGuard(Dia::ApplicationFlow::Module*, Dia::ApplicationFlow::TransitionGuardFn) override { return true; }
    void UnregisterTransitionGuards(Dia::ApplicationFlow::Module*) override {}
    const Json::Value* GetDiagameConfig() const override { return nullptr; }

    // Initial launch support (Task 8).
    bool hasInitial = false;
    StringCRC initialStage, initialPreset;
    StringCRC current = StringCRC("Boot");
    bool bootListsStage = true;
    bool TakeInitialLaunch(StringCRC& stage, StringCRC& preset) override
    {
        if (!hasInitial) { return false; }
        hasInitial = false;
        stage = initialStage; preset = initialPreset;
        return true;
    }
};

} // namespace

// ---- AC 11: catalogue, snapshot, queries ----

TEST(StageLaunchCatalogue, EntryCarriesPresetNamesAndDescriptions)
{
    PresetFixture f;
    const int i = f.state.Catalogue().Find(StringCRC("WithPresets"));
    ASSERT_GE(i, 0);
    const StageCatalogueEntry& e = f.state.Catalogue()[static_cast<unsigned int>(i)];
    ASSERT_EQ(e.presets.Size(), 2u);
    EXPECT_STREQ(e.presets[0].name.AsCStr(), "alpine42");
    EXPECT_STREQ(e.presets[0].description.AsCStr(), "Alpine, seed 42");
    EXPECT_STREQ(e.presets[1].name.AsCStr(), "bare");
    EXPECT_STREQ(e.presets[1].description.AsCStr(), "");

    const int n = f.state.Catalogue().Find(StringCRC("NoPresets"));
    ASSERT_GE(n, 0);
    EXPECT_EQ(f.state.Catalogue()[static_cast<unsigned int>(n)].presets.Size(), 0u);
}

TEST(StageLaunchCatalogue, SnapshotCarriesPresets)
{
    PresetFixture f;
    std::unique_ptr<StageCatalogueSnapshot> snap(new StageCatalogueSnapshot());
    f.state.BuildSnapshot(*snap);
    ASSERT_EQ(snap->entryCount, 2u);
    const StageCatalogueSnapshotEntry& e = snap->entries[0];
    ASSERT_EQ(e.presetCount, 2u);
    EXPECT_STREQ(e.presets[0].name, "alpine42");
    EXPECT_STREQ(e.presets[0].description, "Alpine, seed 42");
    EXPECT_STREQ(e.presets[1].name, "bare");
    EXPECT_EQ(snap->entries[1].presetCount, 0u);
    EXPECT_LE(sizeof(StageCatalogueSnapshot), kMaxStageCatalogueSnapshotBytes);
}

TEST(StageLaunchCatalogue, ListAndGetReturnPresetsWithoutOverrides)
{
    PresetFixture f;
    const Json::Value list = Q::List(f.state, Json::Value(Json::objectValue))["stages"];
    ASSERT_EQ(list.size(), 2u);
    const Json::Value& presets = list[0]["presets"];
    ASSERT_TRUE(presets.isArray());
    ASSERT_EQ(presets.size(), 2u);
    EXPECT_EQ(presets[0]["name"].asString(), "alpine42");
    EXPECT_EQ(presets[0]["description"].asString(), "Alpine, seed 42");
    EXPECT_EQ(presets[1]["name"].asString(), "bare");
    EXPECT_EQ(presets[0].getMemberNames().size(), 2u);   // name + description only
    EXPECT_EQ(list[1]["presets"].size(), 0u);

    const Json::Value got = Q::Get(f.state, Args("{\"stage\":\"WithPresets\"}"));
    ASSERT_EQ(got["presets"].size(), 2u);
    EXPECT_EQ(got["presets"][0]["name"].asString(), "alpine42");
    Json::FastWriter w;
    const std::string text = w.write(got);
    EXPECT_EQ(text.find("modules"), std::string::npos);
    EXPECT_EQ(text.find("WorldGen"), std::string::npos);
}

// ---- AC 9: validation ----

TEST(StageLaunchValidate, OkWithPreset)
{
    PresetFixture f;
    StageLaunchRequest req;
    std::string err;
    EXPECT_TRUE(Q::ValidateLaunch(f.state, Args("{\"stage\":\"WithPresets\",\"preset\":\"alpine42\"}"), true, req, err));
    EXPECT_EQ(req.stage, StringCRC("WithPresets"));
    EXPECT_EQ(req.preset, StringCRC("alpine42"));
}

TEST(StageLaunchValidate, OkWithoutPreset)
{
    PresetFixture f;
    StageLaunchRequest req;
    std::string err;
    EXPECT_TRUE(Q::ValidateLaunch(f.state, Args("{\"stage\":\"NoPresets\"}"), true, req, err));
    EXPECT_EQ(req.stage, StringCRC("NoPresets"));
    EXPECT_EQ(req.preset.Value(), 0u);
}

TEST(StageLaunchValidate, RefusedOutsideBoot)
{
    PresetFixture f;
    StageLaunchRequest req;
    std::string err;
    EXPECT_FALSE(Q::ValidateLaunch(f.state, Args("{\"stage\":\"WithPresets\"}"), false, req, err));
    EXPECT_NE(err.find("Boot"), std::string::npos);
}

TEST(StageLaunchValidate, UnknownStageRefused)
{
    PresetFixture f;
    StageLaunchRequest req;
    std::string err;
    EXPECT_FALSE(Q::ValidateLaunch(f.state, Args("{\"stage\":\"Nope\"}"), true, req, err));
    EXPECT_NE(err.find("Nope"), std::string::npos);
}

TEST(StageLaunchValidate, MissingStageRefused)
{
    PresetFixture f;
    StageLaunchRequest req;
    std::string err;
    EXPECT_FALSE(Q::ValidateLaunch(f.state, Json::Value(Json::objectValue), true, req, err));
    EXPECT_FALSE(err.empty());
}

TEST(StageLaunchValidate, UnknownPresetRefused)
{
    PresetFixture f;
    StageLaunchRequest req;
    std::string err;
    EXPECT_FALSE(Q::ValidateLaunch(f.state, Args("{\"stage\":\"WithPresets\",\"preset\":\"nope\"}"), true, req, err));
    EXPECT_NE(err.find("nope"), std::string::npos);
    EXPECT_FALSE(Q::ValidateLaunch(f.state, Args("{\"stage\":\"NoPresets\",\"preset\":\"alpine42\"}"), true, req, err));
}

TEST(StageLaunchValidate, NonStringPresetRefused)
{
    PresetFixture f;
    StageLaunchRequest req;
    std::string err;
    EXPECT_FALSE(Q::ValidateLaunch(f.state, Args("{\"stage\":\"WithPresets\",\"preset\":5}"), true, req, err));
}

// ---- AC 9: module command ----

TEST(StageLaunchModule, QueuesRequestAndRepliesOk)
{
    LaunchModule m;
    m.Configure(DiagamePath("presets"));
    ASSERT_TRUE(m.Start());
    const Json::Value reply = m.Launch(Args("{\"stage\":\"WithPresets\",\"preset\":\"alpine42\"}"));
    EXPECT_TRUE(reply["ok"].asBool());
    EXPECT_EQ(m.sent, 1u);
    EXPECT_EQ(m.last.stage, StringCRC("WithPresets"));
    EXPECT_EQ(m.last.preset, StringCRC("alpine42"));
}

TEST(StageLaunchModule, RefusesOutsideBootWithoutSending)
{
    LaunchModule m;
    m.Configure(DiagamePath("presets"));
    ASSERT_TRUE(m.Start());
    m.atBoot = false;
    const Json::Value reply = m.Launch(Args("{\"stage\":\"WithPresets\"}"));
    EXPECT_TRUE(reply.isMember("error"));
    EXPECT_EQ(m.sent, 0u);
}

TEST(StageLaunchModule, UnknownStageAndPresetRefusedWithoutSending)
{
    LaunchModule m;
    m.Configure(DiagamePath("presets"));
    ASSERT_TRUE(m.Start());
    EXPECT_TRUE(m.Launch(Args("{\"stage\":\"Nope\"}")).isMember("error"));
    EXPECT_TRUE(m.Launch(Args("{\"stage\":\"WithPresets\",\"preset\":\"nope\"}")).isMember("error"));
    EXPECT_EQ(m.sent, 0u);
}

TEST(StageLaunchModule, SendFailureRepliesError)
{
    LaunchModule m;
    m.Configure(DiagamePath("presets"));
    ASSERT_TRUE(m.Start());
    m.sendOk = false;
    EXPECT_TRUE(m.Launch(Args("{\"stage\":\"NoPresets\"}")).isMember("error"));
}

TEST(StageLaunchModule, SecondLaunchWhileFirstPendingRefused)
{
    LaunchModule m;
    m.Configure(DiagamePath("presets"));
    ASSERT_TRUE(m.Start());
    EXPECT_TRUE(m.Launch(Args("{\"stage\":\"NoPresets\"}"))["ok"].asBool());
    const Json::Value second = m.Launch(Args("{\"stage\":\"WithPresets\"}"));
    EXPECT_TRUE(second.isMember("error"));
    EXPECT_EQ(m.sent, 1u);
}

TEST(StageLaunchModule, LaunchAllowedAgainAfterLeavingBoot)
{
    LaunchModule m;
    m.Configure(DiagamePath("presets"));
    ASSERT_TRUE(m.Start());
    EXPECT_TRUE(m.Launch(Args("{\"stage\":\"NoPresets\"}"))["ok"].asBool());
    m.atBoot = false;
    EXPECT_TRUE(m.Launch(Args("{\"stage\":\"NoPresets\"}")).isMember("error"));
    m.atBoot = true;
    EXPECT_TRUE(m.Launch(Args("{\"stage\":\"NoPresets\"}"))["ok"].asBool());
    EXPECT_EQ(m.sent, 2u);
}

// ---- AC 9: SimPU handler seam ----

TEST(StageLaunchHandler, RequestRoutesToTransitionWithPreset)
{
    RecordingControl ctl;
    StageLaunchRequest req;
    req.stage = StringCRC("WithPresets");
    req.preset = StringCRC("alpine42");
    SimNavigationHandlerModule::HandleLaunchRequest(ctl, nullptr, req);
    EXPECT_EQ(ctl.lastStage, StringCRC("WithPresets"));
    EXPECT_EQ(ctl.lastPreset, StringCRC("alpine42"));
    EXPECT_EQ(ctl.presetCalls, 1);
    EXPECT_EQ(ctl.plainCalls, 0);
}

TEST(StageLaunchHandler, RequestWithoutPresetIsPlainTransition)
{
    RecordingControl ctl;
    StageLaunchRequest req;
    req.stage = StringCRC("NoPresets");
    SimNavigationHandlerModule::HandleLaunchRequest(ctl, nullptr, req);
    EXPECT_EQ(ctl.plainCalls, 1);
    EXPECT_EQ(ctl.lastPreset.Value(), 0u);
}

TEST(StageLaunchHandler, EmptyStageIgnored)
{
    RecordingControl ctl;
    SimNavigationHandlerModule::HandleLaunchRequest(ctl, nullptr, StageLaunchRequest());
    EXPECT_EQ(ctl.plainCalls + ctl.presetCalls, 0);
}

// ---- AC 13: initial launch (--stage/--preset) ----

TEST(StageInitialLaunch, IssuedOnceAtBoot)
{
    RecordingControl ctl;
    ctl.hasInitial = true; ctl.initialStage = StringCRC("WithPresets"); ctl.initialPreset = StringCRC("alpine42");
    EXPECT_EQ(SimNavigationHandlerModule::IssueInitialLaunch(ctl, nullptr), SimNavigationHandlerModule::InitialLaunchResult::kIssued);
    EXPECT_EQ(ctl.presetCalls, 1);
    EXPECT_EQ(ctl.lastStage, StringCRC("WithPresets"));
    EXPECT_EQ(ctl.lastPreset, StringCRC("alpine42"));
    EXPECT_EQ(SimNavigationHandlerModule::IssueInitialLaunch(ctl, nullptr), SimNavigationHandlerModule::InitialLaunchResult::kNone);
    EXPECT_EQ(ctl.presetCalls, 1);
}

TEST(StageInitialLaunch, NothingToDoWhenNoneSet)
{
    RecordingControl ctl;
    EXPECT_EQ(SimNavigationHandlerModule::IssueInitialLaunch(ctl, nullptr), SimNavigationHandlerModule::InitialLaunchResult::kNone);
    EXPECT_EQ(ctl.plainCalls + ctl.presetCalls, 0);
}

TEST(StageInitialLaunch, StageWithoutPresetIsPlainTransition)
{
    RecordingControl ctl;
    ctl.hasInitial = true; ctl.initialStage = StringCRC("NoPresets");
    EXPECT_EQ(SimNavigationHandlerModule::IssueInitialLaunch(ctl, nullptr), SimNavigationHandlerModule::InitialLaunchResult::kIssued);
    EXPECT_EQ(ctl.plainCalls, 1);
    EXPECT_EQ(ctl.presetCalls, 0);
}

TEST(StageInitialLaunch, UnknownStageStaysAtBoot)
{
    RecordingControl ctl;
    ctl.hasInitial = true; ctl.initialStage = StringCRC("Nope"); ctl.bootListsStage = false;
    EXPECT_EQ(SimNavigationHandlerModule::IssueInitialLaunch(ctl, nullptr), SimNavigationHandlerModule::InitialLaunchResult::kUnknownStage);
    EXPECT_EQ(ctl.plainCalls + ctl.presetCalls, 0);
}

TEST(StageInitialLaunch, NotAtBootWaits)
{
    RecordingControl ctl;
    ctl.current = StringCRC("Other");
    ctl.hasInitial = true; ctl.initialStage = StringCRC("WithPresets");
    EXPECT_EQ(SimNavigationHandlerModule::IssueInitialLaunch(ctl, nullptr), SimNavigationHandlerModule::InitialLaunchResult::kNone);
    EXPECT_TRUE(ctl.hasInitial); // not consumed
    EXPECT_EQ(ctl.plainCalls + ctl.presetCalls, 0);
}
