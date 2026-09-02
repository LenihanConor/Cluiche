////////////////////////////////////////////////////////////////////////////////
// Filename: TestSimTimeRegistry.cpp
// GoogleTest suite — DiaSimTime Task 4.3: SimTimeRegistry.
//
// Covers:
//   - tier LOD throttling (DueThisTick / MarkRan) including the maxInterval
//     override and the kImmediate / kDormant edge tiers,
//   - the sleep/wake state machine (Sleep / Wake / GetState),
//   - wake-on-time: RegisterWakeOnTime + a real EventStreamStore round-trip
//     (scheduler.Connect -> Tick -> registry.ProcessWakeEvents -> Wake),
//   - wake-on-message: RegisterWakeOnMessage + NotifyMessage fan-out,
//   - safe no-op on unregistered systemIds.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaSimTime/SimTimeRegistry.h>
#include <DiaSimTime/SimTimeScheduler.h>
#include <DiaSimTime/SimTimeSchedulerFire.h>
#include <DiaSimTime/ISimTimeBudgetedSystem.h>
#include <DiaSimTime/SimTimePolicy.h>
#include <DiaSimTime/SimTimeState.h>
#include <DiaSimTime/SimTimeTier.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Memory/UniquePtr.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Time/TimeRelative.h>
#include <DiaStreams/IStreamConnector.h>
#include <DiaStreams/IStreamStore.h>

#include <vector>

using namespace Dia::Core;
using namespace Dia::SimTime;

namespace {

    TimeAbsolute Ms(int ms) { return TimeAbsolute::CreateFromMilliseconds(ms); }

    // Minimal concrete budgeted system: identity + a no-op budgeted update.
    class FakeSystem : public ISimTimeBudgetedSystem
    {
    public:
        explicit FakeSystem(const char* id) : mId(id) {}
        Dia::Core::StringCRC GetSystemId() const override { return mId; }
        void UpdateBudgeted(float /*budgetMs*/) override {}
    private:
        Dia::Core::StringCRC mId;
    };

    // Minimal test-only IStreamConnector — same shape as the one in
    // TestSimTimeScheduler.cpp (no reusable connector exists in the test tree;
    // both files integration-test through the real register-or-find contract).
    class TestStreamConnector : public Dia::ApplicationFlow::IStreamConnector
    {
    public:
        Dia::ApplicationFlow::IStreamStore* RegisterOrFindStreamStore(
            Dia::Core::UniquePtr<Dia::ApplicationFlow::IStreamStore> newStore) override
        {
            for (auto& existing : mStores)
            {
                if (existing->GetId() == newStore->GetId())
                {
                    return existing.Get();
                }
            }
            Dia::ApplicationFlow::IStreamStore* raw = newStore.Get();
            mStores.push_back(std::move(newStore));
            return raw;
        }
    private:
        std::vector<Dia::Core::UniquePtr<Dia::ApplicationFlow::IStreamStore>> mStores;
    };

} // anonymous namespace

// ---------------------------------------------------------------------------
// Register / Unregister accounting.
// ---------------------------------------------------------------------------
TEST(SimTimeRegistryTest, RegisterAndUnregisterTracksCount)
{
    SimTimeScheduler sched;
    SimTimeRegistry  registry(sched);
    EXPECT_EQ(registry.GetRegisteredCount(), 0);

    FakeSystem sys("Sys");
    registry.Register(&sys, SimTimePolicy{});
    EXPECT_EQ(registry.GetRegisteredCount(), 1);
    EXPECT_EQ(registry.GetState(sys.GetSystemId()), SimTimeState::kAwake)
        << "state defaults to kAwake on registration";

    registry.Unregister(&sys);
    EXPECT_EQ(registry.GetRegisteredCount(), 0);
}

// ---------------------------------------------------------------------------
// Tier throttling: a kHigh (~30Hz, ~33.3ms) system is due immediately (never
// ran), not due right after MarkRan, and due again once >= the tier interval
// of game time has elapsed.
// ---------------------------------------------------------------------------
TEST(SimTimeRegistryTest, TierThrottleGatesCallRate)
{
    SimTimeScheduler sched;
    SimTimeRegistry  registry(sched);

    FakeSystem sys("HighSys");
    SimTimePolicy policy;
    policy.tier = SimTimeTier::kHigh;
    registry.Register(&sys, policy);
    const StringCRC id = sys.GetSystemId();

    // Fresh system: due immediately, before any run.
    EXPECT_TRUE(registry.DueThisTick(id, Ms(0)));

    // After running at t=0, not due at the same instant.
    registry.MarkRan(id, Ms(0));
    EXPECT_FALSE(registry.DueThisTick(id, Ms(0)))
        << "just ran — throttle should block the same tick";
    EXPECT_FALSE(registry.DueThisTick(id, Ms(20)))
        << "20ms < ~33ms tier interval — still throttled";

    // Once the tier interval has elapsed it becomes due again.
    EXPECT_TRUE(registry.DueThisTick(id, Ms(50)))
        << "50ms >= ~33ms tier interval — due again";
}

// ---------------------------------------------------------------------------
// kImmediate is always due; kDormant is never due via the throttle path (even
// while kAwake — the two axes are independent).
// ---------------------------------------------------------------------------
TEST(SimTimeRegistryTest, ImmediateAlwaysDueDormantNeverDue)
{
    SimTimeScheduler sched;
    SimTimeRegistry  registry(sched);

    FakeSystem immediate("ImmSys");
    FakeSystem dormant("DormSys");
    SimTimePolicy immPolicy;  immPolicy.tier  = SimTimeTier::kImmediate;
    SimTimePolicy dormPolicy; dormPolicy.tier = SimTimeTier::kDormant;
    registry.Register(&immediate, immPolicy);
    registry.Register(&dormant,   dormPolicy);

    // kImmediate: due even right after a run.
    registry.MarkRan(immediate.GetSystemId(), Ms(0));
    EXPECT_TRUE(registry.DueThisTick(immediate.GetSystemId(), Ms(0)));

    // kDormant: never due via the tier throttle, even fresh and awake.
    EXPECT_EQ(registry.GetState(dormant.GetSystemId()), SimTimeState::kAwake);
    EXPECT_FALSE(registry.DueThisTick(dormant.GetSystemId(), Ms(0)));
    EXPECT_FALSE(registry.DueThisTick(dormant.GetSystemId(), Ms(100000)));
}

// ---------------------------------------------------------------------------
// A non-zero policy.maxInterval overrides the tier default.
// ---------------------------------------------------------------------------
TEST(SimTimeRegistryTest, MaxIntervalOverridesTierDefault)
{
    SimTimeScheduler sched;
    SimTimeRegistry  registry(sched);

    FakeSystem sys("OverrideSys");
    SimTimePolicy policy;
    policy.tier        = SimTimeTier::kHigh;                       // default ~33ms
    policy.maxInterval = TimeRelative::CreateFromMilliseconds(200); // override -> 200ms
    registry.Register(&sys, policy);
    const StringCRC id = sys.GetSystemId();

    registry.MarkRan(id, Ms(0));
    EXPECT_FALSE(registry.DueThisTick(id, Ms(100)))
        << "100ms < 200ms override interval — not due";
    EXPECT_TRUE(registry.DueThisTick(id, Ms(200)))
        << "200ms >= 200ms override interval — due";
}

// ---------------------------------------------------------------------------
// Sleep/wake state machine: a sleeping system is observably distinct from an
// awake one via GetState. (The actual per-tick "skip" decision is Task 4.4's
// gate loop — here we prove the state machine itself.)
// ---------------------------------------------------------------------------
TEST(SimTimeRegistryTest, SleepAndWakeTransitionState)
{
    SimTimeScheduler sched;
    SimTimeRegistry  registry(sched);

    FakeSystem sys("SleeperSys");
    registry.Register(&sys, SimTimePolicy{});
    const StringCRC id = sys.GetSystemId();

    EXPECT_EQ(registry.GetState(id), SimTimeState::kAwake);
    registry.Sleep(id);
    EXPECT_EQ(registry.GetState(id), SimTimeState::kSleeping);
    registry.Wake(id);
    EXPECT_EQ(registry.GetState(id), SimTimeState::kAwake);
}

// ---------------------------------------------------------------------------
// Wake-on-time end to end: RegisterWakeOnTime schedules a wake on the real
// scheduler; after the scheduler Ticks past the time and the registry drains
// the fire stream, the (initially sleeping) system flips to kAwake.
// ---------------------------------------------------------------------------
TEST(SimTimeRegistryTest, RegisterWakeOnTimeWakesAtScheduledTime)
{
    SimTimeScheduler sched;
    SimTimeRegistry  registry(sched);
    TestStreamConnector connector;
    sched.Connect(connector);
    registry.Connect(connector);

    FakeSystem sys("TimedSys");
    registry.Register(&sys, SimTimePolicy{});
    const StringCRC id = sys.GetSystemId();

    registry.Sleep(id);
    ASSERT_EQ(registry.GetState(id), SimTimeState::kSleeping);

    registry.RegisterWakeOnTime(id, Ms(100));

    // Not yet fired: tick before the scheduled time.
    sched.Tick(Ms(50));
    registry.ProcessWakeEvents();
    EXPECT_EQ(registry.GetState(id), SimTimeState::kSleeping)
        << "wake time not reached yet";

    // Fire: tick at/after the scheduled time, then drain.
    sched.Tick(Ms(100));
    registry.ProcessWakeEvents();
    EXPECT_EQ(registry.GetState(id), SimTimeState::kAwake)
        << "wake-on-time fired -> system awakened";
}

// ---------------------------------------------------------------------------
// A fired scheduler event whose eventType is NOT the wake sentinel must be
// ignored by ProcessWakeEvents (the stream fans out every scheduled type).
// ---------------------------------------------------------------------------
TEST(SimTimeRegistryTest, ProcessWakeEventsIgnoresForeignEventTypes)
{
    SimTimeScheduler sched;
    SimTimeRegistry  registry(sched);
    TestStreamConnector connector;
    sched.Connect(connector);
    registry.Connect(connector);

    FakeSystem sys("QuietSys");
    registry.Register(&sys, SimTimePolicy{});
    const StringCRC id = sys.GetSystemId();
    registry.Sleep(id);

    // Some other system schedules an unrelated event targeting our system id.
    sched.ScheduleAt(Ms(100), StringCRC("some.other.event"), id);
    sched.Tick(Ms(100));
    registry.ProcessWakeEvents();

    EXPECT_EQ(registry.GetState(id), SimTimeState::kSleeping)
        << "foreign event type must not wake the system";
}

// ---------------------------------------------------------------------------
// Wake-on-message: NotifyMessage(subscribedType) wakes the subscriber;
// NotifyMessage(otherType) does not.
// ---------------------------------------------------------------------------
TEST(SimTimeRegistryTest, RegisterWakeOnMessageWakesOnMatchingNotify)
{
    SimTimeScheduler sched;
    SimTimeRegistry  registry(sched);

    FakeSystem sys("MsgSys");
    registry.Register(&sys, SimTimePolicy{});
    const StringCRC id = sys.GetSystemId();
    registry.Sleep(id);

    registry.RegisterWakeOnMessage(id, StringCRC("enemy.spotted"));

    // Non-matching message: no wake.
    registry.NotifyMessage(StringCRC("weather.changed"));
    EXPECT_EQ(registry.GetState(id), SimTimeState::kSleeping);

    // Matching message: wakes.
    registry.NotifyMessage(StringCRC("enemy.spotted"));
    EXPECT_EQ(registry.GetState(id), SimTimeState::kAwake);
}

// ---------------------------------------------------------------------------
// Unregistered / unknown systemId is a safe no-op on every accessor — no crash,
// no assert, sensible defaults.
// ---------------------------------------------------------------------------
TEST(SimTimeRegistryTest, UnknownSystemIdIsSafeNoOp)
{
    SimTimeScheduler sched;
    SimTimeRegistry  registry(sched);
    const StringCRC ghost("NeverRegistered");

    // None of these should crash/assert.
    registry.Sleep(ghost);
    registry.Wake(ghost);
    registry.MarkRan(ghost, Ms(100));
    EXPECT_FALSE(registry.DueThisTick(ghost, Ms(100)))
        << "unknown system is never due";
    EXPECT_EQ(registry.GetState(ghost), SimTimeState::kAwake)
        << "unknown system reports the default state";

    // NotifyMessage with no subscriptions is also a no-op.
    registry.NotifyMessage(StringCRC("anything"));
    SUCCEED();
}
