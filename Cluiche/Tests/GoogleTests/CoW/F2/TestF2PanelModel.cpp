// Tests for CoW::F2PanelModel: the pure view model behind the F-2 TestRunnerPanel.
// States mirror the approved mockup (docs/mockups/f2-core-runtime-panel-mockup.html).
#include <gtest/gtest.h>
#include <Modules/F2PanelModel.h>
#include <Modules/F2TestRunnerModule.h>

using namespace CoW;

namespace {

constexpr uint8_t kNotRun = static_cast<uint8_t>(F2CheckStatus::NotRun);
constexpr uint8_t kRunning = static_cast<uint8_t>(F2CheckStatus::Running);
constexpr uint8_t kPass = static_cast<uint8_t>(F2CheckStatus::Pass);
constexpr uint8_t kFail = static_cast<uint8_t>(F2CheckStatus::Fail);

F2Fingerprint MakeFingerprint(uint64_t day, SpeedTier tier)
{
    F2Fingerprint f;
    for (int i = 0; i < kF2FingerprintStreamCount; ++i)
    {
        f.streams[i].seed = static_cast<uint32_t>((i + 1) * 10000);
        f.streams[i].drawCount = 100 + i;
    }
    f.dayCounter = day;
    f.speedTier = tier;
    return f;
}

F2PanelInput IdleInput()
{
    F2PanelInput in;
    in.hasClock = true;
    in.day = 5;
    in.speedTier = SpeedTier::Paused;
    return in;
}

F2PanelInput MidRunInput()
{
    F2PanelInput in = IdleInput();
    in.running = true;
    in.phase = static_cast<uint8_t>(F2PhaseId::ReferenceFuture);
    in.scenarioTick = 712;
    in.checkStatus[0] = kPass;
    in.checkResult[0] = "8 / 8 streams independent over 64 draws";
    in.checkStatus[3] = kPass;
    in.checkStatus[4] = kRunning;
    in.checkStatus[5] = kPass;
    in.hasF500 = true;
    in.f500 = MakeFingerprint(5, SpeedTier::Paused);
    return in;
}

F2PanelInput CompleteInput(bool c3Fails)
{
    F2PanelInput in = IdleInput();
    in.running = false;
    in.phase = 0xFF;
    for (int i = 0; i < kF2PanelCheckCount; ++i)
    {
        in.checkStatus[i] = kPass;
        in.checkResult[i] = "ok";
    }
    in.hasF500 = in.hasF1000 = in.hasReplay1000 = in.hasRerun1000 = true;
    in.f500 = MakeFingerprint(5, SpeedTier::Paused);
    in.f1000 = MakeFingerprint(16, SpeedTier::Fastest);
    in.rerun1000 = in.f1000;
    in.replay1000 = in.f1000;
    if (c3Fails)
    {
        in.checkStatus[2] = kFail;
        in.checkResult[2] = "state differs: 2 fields differ";
        in.replay1000.dayCounter = 15;
        in.replay1000.preModalSpeedTier = SpeedTier::Fast;
    }
    return in;
}

} // namespace

TEST(F2PanelModel, IdleStateHasAllChecksNotRunAndControlsEnabled)
{
    const F2PanelModel m = F2PanelModel::Build(IdleInput());

    ASSERT_EQ(m.phases.size(), 8u);
    for (const F2PanelPhase& p : m.phases)
    {
        EXPECT_FALSE(p.done);
        EXPECT_FALSE(p.current);
        EXPECT_FALSE(p.failed);
    }
    ASSERT_EQ(m.checks.size(), 6u);
    EXPECT_EQ(m.checks[0].id, "C1");
    EXPECT_EQ(m.checks[5].id, "C6");
    for (const F2PanelCheckRow& c : m.checks)
        EXPECT_EQ(c.status, "NotRun");
    EXPECT_EQ(m.runLabel, "Run Scenario");
    EXPECT_TRUE(m.runEnabled);
    EXPECT_TRUE(m.controlsEnabled);
    EXPECT_EQ(m.tickText, "Idle");
    EXPECT_EQ(m.footer, "Ready.");
    EXPECT_EQ(m.clockText, "Day 5 | Paused | modals 0");
    EXPECT_FALSE(m.complete);
    ASSERT_EQ(m.fingerprints.size(), 4u);
    EXPECT_EQ(m.fingerprints[0].label, "F500");
    EXPECT_EQ(m.fingerprints[0].state, "none");
}

TEST(F2PanelModel, MidRunFlagsPhasesLocksControlsAndShowsRunning)
{
    const F2PanelModel m = F2PanelModel::Build(MidRunInput());

    ASSERT_EQ(m.phases.size(), 8u);
    for (int i = 0; i < 3; ++i)
    {
        EXPECT_TRUE(m.phases[i].done) << i;
        EXPECT_FALSE(m.phases[i].current) << i;
    }
    EXPECT_TRUE(m.phases[3].current);
    EXPECT_FALSE(m.phases[3].done);
    for (int i = 4; i < 8; ++i)
    {
        EXPECT_FALSE(m.phases[i].done) << i;
        EXPECT_FALSE(m.phases[i].current) << i;
    }
    EXPECT_EQ(m.runLabel, "Running...");
    EXPECT_FALSE(m.runEnabled);
    EXPECT_FALSE(m.controlsEnabled);
    EXPECT_EQ(m.tickText, "Phase 3 Reference future  -  tick 712 / 1000");
    EXPECT_EQ(m.footer, "Scenario running. Manual controls locked.");
    EXPECT_EQ(m.checks[0].status, "Pass");
    EXPECT_EQ(m.checks[0].result, "8 / 8 streams independent over 64 draws");
    EXPECT_EQ(m.checks[1].status, "NotRun");
    EXPECT_EQ(m.checks[4].status, "Running");
    EXPECT_EQ(m.fingerprints[0].state, "ok");
    EXPECT_EQ(m.fingerprints[1].label, "F1000");
    EXPECT_EQ(m.fingerprints[1].state, "pending");
}

TEST(F2PanelModel, CompleteAllPassOffersRunAgainAndUnlocksControls)
{
    const F2PanelModel m = F2PanelModel::Build(CompleteInput(false));

    EXPECT_TRUE(m.complete);
    EXPECT_EQ(m.passCount, 6);
    EXPECT_EQ(m.failCount, 0);
    EXPECT_EQ(m.runLabel, "Run Again");
    EXPECT_TRUE(m.runEnabled);
    EXPECT_TRUE(m.controlsEnabled);
    EXPECT_EQ(m.tickText, "Scenario complete  -  6 pass, 0 fail");
    EXPECT_EQ(m.footer, "All checks passed.");
    for (const F2PanelPhase& p : m.phases)
    {
        EXPECT_TRUE(p.done);
        EXPECT_FALSE(p.failed);
    }
    ASSERT_EQ(m.fingerprints.size(), 4u);
    for (const F2PanelFingerprintRow& r : m.fingerprints)
    {
        EXPECT_EQ(r.state, "ok") << r.label;
        EXPECT_TRUE(r.diffFields.empty()) << r.label;
    }
    EXPECT_EQ(m.fingerprints[2].text, "identical to F1000 (all fields)");
}

TEST(F2PanelModel, C3FailureShowsFailingRowFailedPhaseAndFlaggedDiffFields)
{
    const F2PanelModel m = F2PanelModel::Build(CompleteInput(true));

    EXPECT_EQ(m.passCount, 5);
    EXPECT_EQ(m.failCount, 1);
    EXPECT_EQ(m.checks[2].status, "Fail");
    EXPECT_EQ(m.checks[2].result, "state differs: 2 fields differ");
    EXPECT_EQ(m.tickText, "Scenario complete  -  5 pass, 1 fail");
    EXPECT_EQ(m.footer, "1 check failed. See C3.");
    EXPECT_TRUE(m.phases[5].failed) << "C3 is raised in the Replay phase";
    EXPECT_FALSE(m.phases[4].failed);

    const F2PanelFingerprintRow& replay = m.fingerprints[2];
    EXPECT_EQ(replay.label, "Replay F1000");
    EXPECT_EQ(replay.state, "diff");
    ASSERT_EQ(replay.diffFields.size(), 2u);
    EXPECT_EQ(replay.diffFields[0], "dayCounter");
    EXPECT_EQ(replay.diffFields[1], "preModalSpeedTier");
    EXPECT_EQ(m.fingerprints[3].state, "ok");
}

TEST(F2PanelModel, JsonRoundTripPreservesTheModel)
{
    const F2PanelModel original = F2PanelModel::Build(CompleteInput(true));
    const Json::Value json = original.ToJson();

    EXPECT_TRUE(json["phases"].isArray());
    EXPECT_TRUE(json["checks"].isArray());
    EXPECT_TRUE(json["fingerprints"].isArray());

    F2PanelModel copy;
    ASSERT_TRUE(F2PanelModel::FromJson(json, copy));
    EXPECT_EQ(copy.ToJson(), json);
    EXPECT_EQ(copy.runLabel, original.runLabel);
    EXPECT_EQ(copy.footer, original.footer);
    ASSERT_EQ(copy.fingerprints.size(), original.fingerprints.size());
    EXPECT_EQ(copy.fingerprints[2].diffFields, original.fingerprints[2].diffFields);
    EXPECT_TRUE(copy.phases[5].failed);
    EXPECT_EQ(copy.checks[2].status, "Fail");
}

TEST(F2PanelModel, FromJsonRejectsMalformedInput)
{
    F2PanelModel copy;
    EXPECT_FALSE(F2PanelModel::FromJson(Json::Value(Json::objectValue), copy));
    EXPECT_FALSE(F2PanelModel::FromJson(Json::Value(42), copy));
}
