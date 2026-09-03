////////////////////////////////////////////////////////////////////////////////
// Filename: TestSimTimeSaveState.cpp
// GoogleTest suite — DiaSimTime Task 4.7: SimTimeSaveState (save/load) plus the
// two additive integration gaps it needed:
//   * SimTimeScheduler::GetPendingEntries (Gap A) — read-only pending peek.
//   * SimTimeRegistry::IsRegistered       (Gap B) — registered-vs-unknown query.
//
// SimTimeSaveState is exercised directly against standalone SimTimeDomain /
// SimTimeScheduler / SimTimeRegistry instances — no Application / Module needed.
// Round-trip goes through the real SaveContext -> Flush -> jsoncpp parse ->
// LoadContext path (the same shape as TestSaveLoadContext.cpp).
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <DiaSimTime/SimTimeSaveState.h>
#include <DiaSimTime/SimTimeScheduler.h>
#include <DiaSimTime/SimTimeRegistry.h>
#include <DiaSimTime/SimTimeState.h>
#include <DiaSimTime/SimTimePolicy.h>
#include <DiaSimTime/SimTimeTier.h>
#include <DiaSimTime/ISimTimeBudgetedSystem.h>
#include <DiaSimTime/SimTimeSchedulerFire.h>

#include <DiaSaveGame/SaveContext.h>
#include <DiaSaveGame/LoadContext.h>

#include <DiaCore/SimTime/SimTimeDomain.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Time/TimeRelative.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::Core;
using namespace Dia::Core::Containers;
using namespace Dia::SimTime;

namespace {

    TimeAbsolute Ms(int ms)  { return TimeAbsolute::CreateFromMilliseconds(ms); }
    TimeRelative RMs(int ms) { return TimeRelative::CreateFromMilliseconds(ms); }

    class FakeSystem : public ISimTimeBudgetedSystem
    {
    public:
        explicit FakeSystem(const char* id) : mId(id) {}
        StringCRC GetSystemId() const override { return mId; }
        void UpdateBudgeted(float /*budgetMs*/) override {}
    private:
        StringCRC mId;
    };

    // Serialize `state` through the real save path and hand back the parsed JSON
    // tree (ready to construct a LoadContext from).
    Json::Value SaveToJson(const SimTimeSaveState& state)
    {
        Dia::SaveGame::SaveContext save;
        state.Serialize(save);

        static char buf[Dia::SaveGame::SaveContext::kBufferSize];
        EXPECT_TRUE(save.Flush(buf, sizeof(buf)));

        Json::Value  root;
        Json::Reader reader;
        EXPECT_TRUE(reader.parse(buf, root));
        return root;
    }

    template<unsigned int N>
    bool FiredContains(const DynamicArrayC<SimTimeSchedulerFire, N>& fired, const StringCRC& eventType)
    {
        for (unsigned int i = 0; i < fired.Size(); ++i)
        {
            if (fired[i].eventType == eventType) return true;
        }
        return false;
    }

} // anonymous namespace

// ---------------------------------------------------------------------------
// 1. World gameTime round-trips exactly.
// ---------------------------------------------------------------------------
TEST(SimTimeSaveStateTest, GameTimeRoundTrips)
{
    SimTimeDomain    worldA(StringCRC("world"), 60.0f);
    SimTimeScheduler schedA;
    SimTimeRegistry  regA(schedA);
    worldA.AdvanceTo(Ms(5000));
    ASSERT_EQ(worldA.Now(), Ms(5000));

    SimTimeSaveState saveA(worldA, schedA, regA);
    const Json::Value root = SaveToJson(saveA);

    // Fresh objects (world starts at Zero).
    SimTimeDomain    worldB(StringCRC("world"), 60.0f);
    SimTimeScheduler schedB;
    SimTimeRegistry  regB(schedB);
    ASSERT_EQ(worldB.Now(), TimeAbsolute::Zero());

    SimTimeSaveState saveB(worldB, schedB, regB);
    Dia::SaveGame::LoadContext load(root);
    saveB.Deserialize(load);

    EXPECT_EQ(worldB.Now(), Ms(5000)) << "restored world clock must match the saved gameTime";
}

// ---------------------------------------------------------------------------
// 2. A pending one-shot fires at the same absolute game time post-load, under a
//    fresh handle (ST-016) — verified via the fire payload, not handle equality.
// ---------------------------------------------------------------------------
TEST(SimTimeSaveStateTest, PendingOneShotFiresAtSameTimeAfterLoad)
{
    SimTimeDomain    worldA(StringCRC("world"), 60.0f);
    SimTimeScheduler schedA;
    SimTimeRegistry  regA(schedA);

    const StringCRC evt("oneshot.event");
    const StringCRC tgt("target.system");
    schedA.ScheduleAt(Ms(200), evt, tgt);   // saved before it fires

    SimTimeSaveState saveA(worldA, schedA, regA);
    const Json::Value root = SaveToJson(saveA);

    // Restore into a fresh scheduler.
    SimTimeDomain    worldB(StringCRC("world"), 60.0f);
    SimTimeScheduler schedB;
    SimTimeRegistry  regB(schedB);
    SimTimeSaveState saveB(worldB, schedB, regB);
    Dia::SaveGame::LoadContext load(root);
    saveB.Deserialize(load);

    EXPECT_EQ(schedB.GetQueueDepth(), 1);

    // Not due yet at t=100ms.
    DynamicArrayC<SimTimeSchedulerFire, 16> firedEarly;
    schedB.Tick(Ms(100), firedEarly);
    EXPECT_FALSE(FiredContains(firedEarly, evt)) << "must not fire before 200ms";

    // Fires at t=200ms.
    DynamicArrayC<SimTimeSchedulerFire, 16> fired;
    schedB.Tick(Ms(200), fired);
    EXPECT_TRUE(FiredContains(fired, evt)) << "restored one-shot must fire at its saved time";
}

// ---------------------------------------------------------------------------
// 3. A recurring entry keeps recurring after restore (fires on multiple ticks).
// ---------------------------------------------------------------------------
TEST(SimTimeSaveStateTest, RecurringEntryKeepsRecurringAfterLoad)
{
    SimTimeDomain    worldA(StringCRC("world"), 60.0f);
    SimTimeScheduler schedA;
    SimTimeRegistry  regA(schedA);

    const StringCRC evt("recurring.event");
    const StringCRC tgt("target.system");
    schedA.ScheduleRecurring(RMs(100), evt, tgt);   // first fire at 100ms

    SimTimeSaveState saveA(worldA, schedA, regA);
    const Json::Value root = SaveToJson(saveA);

    SimTimeDomain    worldB(StringCRC("world"), 60.0f);
    SimTimeScheduler schedB;
    SimTimeRegistry  regB(schedB);
    SimTimeSaveState saveB(worldB, schedB, regB);
    Dia::SaveGame::LoadContext load(root);
    saveB.Deserialize(load);

    // First fire at 100ms.
    DynamicArrayC<SimTimeSchedulerFire, 16> fired1;
    schedB.Tick(Ms(100), fired1);
    EXPECT_TRUE(FiredContains(fired1, evt)) << "recurring entry fires at its saved first-fire time";

    // Re-armed to 200ms — fires again.
    DynamicArrayC<SimTimeSchedulerFire, 16> fired2;
    schedB.Tick(Ms(200), fired2);
    EXPECT_TRUE(FiredContains(fired2, evt)) << "recurring entry re-armed and fired again after restore";
}

// ---------------------------------------------------------------------------
// 4. A sleeping system remains asleep across save/load; an awake one stays awake.
// ---------------------------------------------------------------------------
TEST(SimTimeSaveStateTest, SleepStateRoundTrips)
{
    SimTimeScheduler schedA;
    SimTimeRegistry  regA(schedA);
    SimTimeDomain    worldA(StringCRC("world"), 60.0f);

    FakeSystem sleeper("Sleeper");
    FakeSystem waker("Waker");
    regA.Register(&sleeper, SimTimePolicy{});
    regA.Register(&waker,   SimTimePolicy{});
    regA.Sleep(sleeper.GetSystemId());

    SimTimeSaveState saveA(worldA, schedA, regA);
    const Json::Value root = SaveToJson(saveA);

    // Restore into a registry where BOTH systems are still registered (awake).
    SimTimeScheduler schedB;
    SimTimeRegistry  regB(schedB);
    SimTimeDomain    worldB(StringCRC("world"), 60.0f);
    FakeSystem sleeperB("Sleeper");
    FakeSystem wakerB("Waker");
    regB.Register(&sleeperB, SimTimePolicy{});
    regB.Register(&wakerB,   SimTimePolicy{});
    ASSERT_EQ(regB.GetState(sleeperB.GetSystemId()), SimTimeState::kAwake);

    SimTimeSaveState saveB(worldB, schedB, regB);
    Dia::SaveGame::LoadContext load(root);
    saveB.Deserialize(load);

    EXPECT_EQ(regB.GetState(sleeperB.GetSystemId()), SimTimeState::kSleeping)
        << "slept system must remain asleep after restore";
    EXPECT_EQ(regB.GetState(wakerB.GetSystemId()), SimTimeState::kAwake)
        << "awake system must remain awake after restore";
}

// ---------------------------------------------------------------------------
// 5. A saved systemId no longer registered in the target is logged-and-skipped
//    (no assert / crash); the still-registered system is restored normally.
// ---------------------------------------------------------------------------
TEST(SimTimeSaveStateTest, UnregisteredSavedSystemIsSkippedNotAsserted)
{
    SimTimeScheduler schedA;
    SimTimeRegistry  regA(schedA);
    SimTimeDomain    worldA(StringCRC("world"), 60.0f);

    FakeSystem kept("Kept");
    FakeSystem gone("Gone");
    regA.Register(&kept, SimTimePolicy{});
    regA.Register(&gone, SimTimePolicy{});
    regA.Sleep(kept.GetSystemId());
    regA.Sleep(gone.GetSystemId());

    SimTimeSaveState saveA(worldA, schedA, regA);
    const Json::Value root = SaveToJson(saveA);

    // Only "Kept" is registered in the restore target — "Gone" is absent.
    SimTimeScheduler schedB;
    SimTimeRegistry  regB(schedB);
    SimTimeDomain    worldB(StringCRC("world"), 60.0f);
    FakeSystem keptB("Kept");
    regB.Register(&keptB, SimTimePolicy{});

    SimTimeSaveState saveB(worldB, schedB, regB);
    Dia::SaveGame::LoadContext load(root);
    saveB.Deserialize(load);   // must not assert/crash on the missing "Gone"

    EXPECT_EQ(regB.GetRegisteredCount(), 1) << "no phantom system should be created";
    EXPECT_EQ(regB.GetState(keptB.GetSystemId()), SimTimeState::kSleeping)
        << "the still-registered system's sleep state is restored";
}

// ---------------------------------------------------------------------------
// 6. Combined round trip: gameTime + a pending scheduler entry + sleep state
//    are all persisted and restored together through ONE Serialize/Deserialize
//    pass (each was previously only proven in isolation). The world domain is
//    additionally paused/scaled on the source to prove that has no bearing on
//    the pieces SimTimeSaveState actually owns (ST-015 does not persist
//    pause/scale — only gameTime).
// ---------------------------------------------------------------------------
TEST(SimTimeSaveStateTest, CombinedGameTimeSchedulerAndSleepStateRoundTripTogether)
{
    SimTimeDomain    worldA(StringCRC("world"), 60.0f);
    SimTimeScheduler schedA;
    SimTimeRegistry  regA(schedA);

    worldA.AdvanceTo(Ms(3000));
    worldA.Pause();
    worldA.SetScale(0.5f);

    const StringCRC evt("combo.event");
    const StringCRC tgt("combo.target");
    schedA.ScheduleAt(Ms(3100), evt, tgt);

    FakeSystem sleeper("ComboSleeper");
    FakeSystem waker("ComboWaker");
    regA.Register(&sleeper, SimTimePolicy{});
    regA.Register(&waker,   SimTimePolicy{});
    regA.Sleep(sleeper.GetSystemId());

    SimTimeSaveState saveA(worldA, schedA, regA);
    const Json::Value root = SaveToJson(saveA);

    SimTimeDomain    worldB(StringCRC("world"), 60.0f);
    SimTimeScheduler schedB;
    SimTimeRegistry  regB(schedB);
    FakeSystem sleeperB("ComboSleeper");
    FakeSystem wakerB("ComboWaker");
    regB.Register(&sleeperB, SimTimePolicy{});
    regB.Register(&wakerB,   SimTimePolicy{});

    SimTimeSaveState saveB(worldB, schedB, regB);
    Dia::SaveGame::LoadContext load(root);
    saveB.Deserialize(load);

    EXPECT_EQ(worldB.Now(), Ms(3000))
        << "world clock restored correctly alongside the scheduler + sleep state";
    EXPECT_EQ(schedB.GetQueueDepth(), 1);
    EXPECT_EQ(regB.GetState(sleeperB.GetSystemId()), SimTimeState::kSleeping);
    EXPECT_EQ(regB.GetState(wakerB.GetSystemId()), SimTimeState::kAwake);

    DynamicArrayC<SimTimeSchedulerFire, 16> fired;
    schedB.Tick(Ms(3100), fired);
    EXPECT_TRUE(FiredContains(fired, evt))
        << "the restored scheduler entry must still fire at its saved time";
}

// ---------------------------------------------------------------------------
// 7. An empty scheduler and registry round-trip cleanly — no assert/crash on
//    zero-length arrays, and the restored gameTime is unaffected.
// ---------------------------------------------------------------------------
TEST(SimTimeSaveStateTest, EmptySchedulerAndRegistryRoundTripWithoutError)
{
    SimTimeDomain    worldA(StringCRC("world"), 60.0f);
    SimTimeScheduler schedA;
    SimTimeRegistry  regA(schedA);
    worldA.AdvanceTo(Ms(1234));

    SimTimeSaveState saveA(worldA, schedA, regA);
    const Json::Value root = SaveToJson(saveA);

    SimTimeDomain    worldB(StringCRC("world"), 60.0f);
    SimTimeScheduler schedB;
    SimTimeRegistry  regB(schedB);
    SimTimeSaveState saveB(worldB, schedB, regB);
    Dia::SaveGame::LoadContext load(root);
    saveB.Deserialize(load);   // must not assert/crash with zero scheduler/registry entries

    EXPECT_EQ(worldB.Now(), Ms(1234));
    EXPECT_EQ(schedB.GetQueueDepth(), 0);
    EXPECT_EQ(regB.GetRegisteredCount(), 0);
}

// ---------------------------------------------------------------------------
// Gap A — SimTimeScheduler::GetPendingEntries: read-only peek across wheel + heap.
// ---------------------------------------------------------------------------
TEST(SimTimeSchedulerTest, GetPendingEntriesReportsLiveWheelAndHeapEntries)
{
    SimTimeScheduler sched;
    const StringCRC evtNear("near");   // wheel (within 640ms horizon of t=0)
    const StringCRC evtFar("far");     // heap  (beyond the horizon)
    const StringCRC tgt("sys");
    sched.ScheduleAt(Ms(100),  evtNear, tgt);
    sched.ScheduleAt(Ms(5000), evtFar,  tgt);
    sched.ScheduleRecurring(RMs(50), StringCRC("rec"), tgt);

    DynamicArrayC<SimTimeScheduler::PendingEntryView, SimTimeScheduler::kMaxEntries> entries;
    sched.GetPendingEntries(entries);

    EXPECT_EQ(entries.Size(), 3u) << "all three live entries reported (wheel + heap)";
    EXPECT_EQ(sched.GetQueueDepth(), 3) << "peek is read-only — nothing removed";

    bool sawNear = false, sawFar = false, sawRec = false;
    for (unsigned int i = 0; i < entries.Size(); ++i)
    {
        const SimTimeScheduler::PendingEntryView& e = entries[i];
        if (e.eventType == evtNear)          { sawNear = true; EXPECT_FALSE(e.recurring); }
        if (e.eventType == evtFar)           { sawFar  = true; EXPECT_FALSE(e.recurring); }
        if (e.eventType == StringCRC("rec")) { sawRec  = true; EXPECT_TRUE(e.recurring); }
    }
    EXPECT_TRUE(sawNear && sawFar && sawRec);
}

TEST(SimTimeSchedulerTest, GetPendingEntriesSkipsCancelledEntries)
{
    SimTimeScheduler sched;
    const StringCRC tgt("sys");
    const ScheduleHandle h = sched.ScheduleAt(Ms(100), StringCRC("keep"), tgt);
    sched.ScheduleAt(Ms(150), StringCRC("drop"), tgt);
    sched.Cancel(h);

    DynamicArrayC<SimTimeScheduler::PendingEntryView, SimTimeScheduler::kMaxEntries> entries;
    sched.GetPendingEntries(entries);

    EXPECT_EQ(entries.Size(), 1u) << "cancelled (stale) entry must not be reported";
    if (entries.Size() == 1u)
    {
        EXPECT_EQ(entries[0].eventType, StringCRC("drop"));
    }
}

// ---------------------------------------------------------------------------
// Gap B — SimTimeRegistry::IsRegistered distinguishes registered from unknown.
// ---------------------------------------------------------------------------
TEST(SimTimeRegistryTest, IsRegisteredDistinguishesRegisteredFromUnknown)
{
    SimTimeScheduler sched;
    SimTimeRegistry  registry(sched);

    FakeSystem sys("Known");
    EXPECT_FALSE(registry.IsRegistered(sys.GetSystemId())) << "not registered yet";

    registry.Register(&sys, SimTimePolicy{});
    EXPECT_TRUE(registry.IsRegistered(sys.GetSystemId()));
    EXPECT_FALSE(registry.IsRegistered(StringCRC("NeverRegistered")));

    registry.Unregister(&sys);
    EXPECT_FALSE(registry.IsRegistered(sys.GetSystemId())) << "unregistered again";
}
