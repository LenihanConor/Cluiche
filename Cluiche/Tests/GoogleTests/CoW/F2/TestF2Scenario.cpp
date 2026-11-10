// Tests for the pure F-2 scenario data: script tables, draw pattern and the clock reference model.
#include <gtest/gtest.h>
#include <Modules/F2Scenario.h>
#include <algorithm>
#include <set>

using namespace CoW;

namespace {

void ExpectSortedWithGaps(const std::vector<F2ScenarioAction>& script, uint32_t lo, uint32_t hi)
{
    ASSERT_FALSE(script.empty());
    for (size_t i = 0; i < script.size(); ++i)
    {
        EXPECT_GE(script[i].tick, lo) << i;
        EXPECT_LT(script[i].tick, hi) << i;
        if (i > 0)
            EXPECT_GE(script[i].tick, script[i - 1].tick + kF2MinActionGap) << "action " << i << " too close to the previous one";
    }
}

F2ExpectedClock RunScript(const std::vector<F2ScenarioAction>& script, F2ExpectedClock model = F2ExpectedClock{})
{
    for (const F2ScenarioAction& a : script)
        F2ApplyAction(model, a);
    return model;
}

} // namespace

TEST(F2Scenario, SessionScriptIsSortedAndInRange)
{
    ExpectSortedWithGaps(F2SessionScript(), 0, kF2SessionTicks);
}

TEST(F2Scenario, RemainderScriptIsSortedInRangeAndStartsWithExplicitSpeed)
{
    const auto& script = F2RemainderScript();
    ExpectSortedWithGaps(script, kF2SessionTicks, kF2ScenarioTicks);
    EXPECT_EQ(script.front().tick, kF2SessionTicks);
    EXPECT_EQ(script.front().kind, F2ActionKind::SetSpeed);
    EXPECT_EQ(script.front().tier, SpeedTier::Normal);
}

TEST(F2Scenario, SessionCoversEveryPresetAndActionKind)
{
    std::set<SpeedTier> tiers;
    std::set<F2ActionKind> kinds;
    for (const F2ScenarioAction& a : F2SessionScript())
    {
        kinds.insert(a.kind);
        if (a.kind == F2ActionKind::SetSpeed)
            tiers.insert(a.tier);
    }
    EXPECT_EQ(tiers.count(SpeedTier::Slow), 1u);
    EXPECT_EQ(tiers.count(SpeedTier::Normal), 1u);
    EXPECT_EQ(tiers.count(SpeedTier::Fast), 1u);
    EXPECT_EQ(tiers.count(SpeedTier::Fastest), 1u);
    EXPECT_EQ(kinds.size(), 6u) << "every F2ActionKind must appear in the session";
}

TEST(F2Scenario, SessionHasNestedModalSequence)
{
    // OpenModal, OpenModal, CloseModal (still open), ..., CloseModal (balanced).
    int depth = 0, maxDepth = 0;
    bool closedWhileStillOpen = false;
    for (const F2ScenarioAction& a : F2SessionScript())
    {
        if (a.kind == F2ActionKind::OpenModal) maxDepth = std::max(maxDepth, ++depth);
        if (a.kind == F2ActionKind::CloseModal)
        {
            --depth;
            ASSERT_GE(depth, 0);
            if (depth > 0) closedWhileStillOpen = true;
        }
    }
    EXPECT_EQ(maxDepth, 2);
    EXPECT_TRUE(closedWhileStillOpen);
    EXPECT_EQ(depth, 0);
}

TEST(F2Scenario, SessionEndsPausedWithNoModalForTheSave)
{
    const F2ExpectedClock end = RunScript(F2SessionScript());
    EXPECT_EQ(end.modalDepth, 0);
    EXPECT_EQ(end.tier, SpeedTier::Paused);
    EXPECT_EQ(end.source, PauseSource::Player);
}

TEST(F2Scenario, RemainderLeavesTheClockRunning)
{
    const F2ExpectedClock end = RunScript(F2RemainderScript(), RunScript(F2SessionScript()));
    EXPECT_NE(end.tier, SpeedTier::Paused);
}

TEST(F2Scenario, ManualStepsOnlyHappenWhilePaused)
{
    F2ExpectedClock model;
    int steps = 0;
    for (const F2ScenarioAction& a : F2SessionScript())
    {
        if (a.kind == F2ActionKind::ManualStep)
        {
            EXPECT_TRUE(F2ManualStepAccepted(model)) << "tick " << a.tick;
            ++steps;
        }
        F2ApplyAction(model, a);
    }
    EXPECT_GE(steps, 2);
}

TEST(F2Scenario, ModelClosingOneOfTwoModalsDoesNotResume)
{
    F2ExpectedClock m;
    F2ScenarioAction set; set.kind = F2ActionKind::SetSpeed; set.tier = SpeedTier::Fast;
    F2ScenarioAction open; open.kind = F2ActionKind::OpenModal;
    F2ScenarioAction close; close.kind = F2ActionKind::CloseModal;
    F2ApplyAction(m, set);
    F2ApplyAction(m, open);
    F2ApplyAction(m, open);
    EXPECT_EQ(m.modalDepth, 2);
    EXPECT_EQ(m.tier, SpeedTier::Paused);
    F2ApplyAction(m, close);
    EXPECT_EQ(m.modalDepth, 1);
    EXPECT_EQ(m.tier, SpeedTier::Paused);
    F2ApplyAction(m, close);
    EXPECT_EQ(m.modalDepth, 0);
    EXPECT_EQ(m.tier, SpeedTier::Fast);
}

TEST(F2Scenario, ModelPlayerPauseSurvivesModalClose)
{
    F2ExpectedClock m;
    F2ScenarioAction pause; pause.kind = F2ActionKind::PlayerPause;
    F2ScenarioAction open; open.kind = F2ActionKind::OpenModal;
    F2ScenarioAction close; close.kind = F2ActionKind::CloseModal;
    F2ApplyAction(m, pause);
    F2ApplyAction(m, open);
    F2ApplyAction(m, close);
    EXPECT_EQ(m.tier, SpeedTier::Paused);
    EXPECT_EQ(m.source, PauseSource::Player);
}

TEST(F2Scenario, DrawPatternIsDeterministicUnevenAndInRange)
{
    std::set<int> counts;
    std::set<int> streamsSeen;
    for (uint32_t tick = 0; tick < kF2ScenarioTicks; ++tick)
    {
        uint8_t a[kF2MaxDrawsPerTick] = {}, b[kF2MaxDrawsPerTick] = {};
        const int na = F2DrawsForTick(tick, a);
        const int nb = F2DrawsForTick(tick, b);
        ASSERT_EQ(na, nb);
        ASSERT_GE(na, 1);
        ASSERT_LE(na, kF2MaxDrawsPerTick);
        counts.insert(na);
        for (int i = 0; i < na; ++i)
        {
            EXPECT_EQ(a[i], b[i]);
            ASSERT_LT(a[i], 8);
            streamsSeen.insert(a[i]);
        }
    }
    EXPECT_GE(counts.size(), 3u) << "draw counts per tick should vary";
    EXPECT_EQ(streamsSeen.size(), 8u) << "every stream should be drawn from";
}

TEST(F2Scenario, CompareClockMatchesAndNamesMismatches)
{
    const SpeedPresetConfig config = DefaultSpeedPresetConfig();
    F2ExpectedClock e;
    e.tier = SpeedTier::Fast; e.source = PauseSource::None;

    F2ObservedClock o;
    o.tier = SpeedTier::Fast; o.source = PauseSource::None; o.modalDepth = 0;
    o.controllerScale = o.domainScale = 1.0f / config.wallSecondsPerDay[2];
    EXPECT_EQ(F2CompareClock(e, o, config), "");

    F2ObservedClock bad = o; bad.modalDepth = 1;
    EXPECT_NE(F2CompareClock(e, bad, config).find("modalDepth"), std::string::npos);
    bad = o; bad.tier = SpeedTier::Paused;
    EXPECT_NE(F2CompareClock(e, bad, config).find("speed tier"), std::string::npos);
    bad = o; bad.domainScale = 0.5f;
    EXPECT_NE(F2CompareClock(e, bad, config).find("world time scale"), std::string::npos);
    bad = o; bad.controllerScale = 0.5f;
    EXPECT_NE(F2CompareClock(e, bad, config).find("controller time scale"), std::string::npos);
}
