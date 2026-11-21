// World Step Feature 3: a load drops the old timeline even when the save has no pending record. A save made with no
// step in flight writes no 'world' record, so DeserializePending never runs; SaveManager::OnLoadStarted is the only
// hook that can clear the retried week, the key and the chain phase of the world being loaded over.
#include "WorldStepSaveRig.h"

using namespace WsTest;
using CoW::WorldStep::ChainPhase;
using Dia::DeferredStep::StepState;
using Dia::DeferredStep::Testing::LogCapture;

namespace
{
    const StringCRC kWorld("world");
    const std::string kCfg = Config("weather,ecology", 10, true);   // week 7 applies at 17; week 14 finds the channel busy

    // A save at day 20 with no step in flight and so no pending record (a synchronous world never has one). The
    // DeferredStep scheduler's tick key never goes backwards, so a load over a live world goes to the same day or later.
    void SaveQuietWorld(const std::string& slot)
    {
        SaveRig a(slot, Config("weather,ecology", 10, false));
        Drive(a, 1, 20);
        ASSERT_TRUE(a.Save().Ok());
        ASSERT_EQ(a.weather.serializeCalls, 0u);
    }
}

TEST(WorldStepLoadReset, ALoadFromAQuietSaveDropsTheRetriedWeekAndTheStepInFlight)
{
    const std::string slot = UniqueSlot();
    SlotFile file(slot);
    SaveQuietWorld(slot);

    SaveRig b(slot, kCfg);
    FakeWorldStepListener l;
    ASSERT_TRUE(b.module.RegisterListener(l));
    Drive(b, 1, 14);
    ASSERT_TRUE(b.module.HasPendingWeek());
    ASSERT_EQ(b.module.PendingWeekDay(), 14);
    ASSERT_EQ(b.module.KickCount(), 1u);

    ASSERT_TRUE(b.Load().Ok());

    EXPECT_FALSE(b.module.HasPendingWeek());
    EXPECT_EQ(b.module.GetChain().Phase(), ChainPhase::Idle);
    EXPECT_EQ(b.scheduler.GetState(kWorld), StepState::kIdle);
    EXPECT_TRUE(b.module.IsChannelRegistered());
    EXPECT_EQ(b.module.KickCount(), 1u);          // nothing was kicked from the old timeline
    EXPECT_EQ(b.module.ApplyCount(), 0u);
    EXPECT_EQ(l.received.Size(), 0u);
    b.module.UnregisterListener(l);
}

TEST(WorldStepLoadReset, NormalCadenceResumesOnTheLoadedTimeline)
{
    const std::string slot = UniqueSlot();
    SlotFile file(slot);
    SaveQuietWorld(slot);

    SaveRig b(slot, kCfg);
    FakeWorldStepListener l;
    ASSERT_TRUE(b.module.RegisterListener(l));
    Drive(b, 1, 14);
    ASSERT_TRUE(b.Load().Ok());

    Drive(b, 21, 24);                             // the loaded timeline's next week is 21
    EXPECT_FALSE(b.module.HasPendingWeek());      // the old week 14 was not retried in
    EXPECT_EQ(b.module.LastStepDay(), 21u);
    EXPECT_EQ(b.module.KickCount(), 2u);          // exactly one new kick: week 21
    EXPECT_EQ(b.module.GetCurrentKey(), 24u);

    Drive(b, 25, 31);
    ASSERT_EQ(l.received.Size(), 1u);
    EXPECT_EQ(l.received[0].stepDay, 21u);
    EXPECT_EQ(l.received[0].applyKey, 31u);
    b.module.UnregisterListener(l);
}

TEST(WorldStepLoadReset, TicksQueuedBeforeALoadAreNotReplayedAgainstTheLoadedWorld)
{
    const std::string slot = UniqueSlot();
    SlotFile file(slot);
    SaveQuietWorld(slot);

    SaveRig b(slot, kCfg);
    Drive(b, 1, 13);
    b.module.OnSimulationTick(MakeTick(14));      // delivered, cadence not run yet
    ASSERT_TRUE(b.Load().Ok());
    b.module.CallUpdate();
    EXPECT_FALSE(b.module.HasPendingWeek());      // week 14 of the old timeline never reached the cadence
    EXPECT_EQ(b.module.KickCount(), 1u);
}

// A load of an EARLIER day over a live world that has advanced: the scheduler's tick-key floor and the module's key
// both go back, and the loaded timeline's cadence runs from there.
TEST(WorldStepLoadReset, ALoadOfAnEarlierDayOverAnAdvancedWorldResetsTheKeyAndTheCadence)
{
    const std::string slot = UniqueSlot();
    SlotFile file(slot);
    {
        SaveRig a(slot, Config("weather,ecology", 10, false));
        Drive(a, 1, 10);
        ASSERT_TRUE(a.Save().Ok());
        ASSERT_EQ(a.weather.serializeCalls, 0u);
    }
    SaveRig b(slot, kCfg);
    FakeWorldStepListener l;
    ASSERT_TRUE(b.module.RegisterListener(l));
    Drive(b, 1, 20);
    ASSERT_EQ(b.module.GetCurrentKey(), 20u);
    const uint32_t before = l.received.Size();    // week 7 of the old timeline applied at 17

    ASSERT_TRUE(b.Load().Ok());
    EXPECT_EQ(b.module.GetCurrentKey(), 0u);      // no pending record: the old timeline's day is gone
    EXPECT_FALSE(b.module.HasPendingWeek());

    Drive(b, 11, 14);                              // a lower key than the old timeline reached; must not trip the scheduler
    EXPECT_EQ(b.module.GetCurrentKey(), 14u);
    EXPECT_EQ(b.module.LastStepDay(), 14u);
    Drive(b, 15, 24);
    ASSERT_EQ(l.received.Size(), before + 1u);
    EXPECT_EQ(l.received[before].stepDay, 14u);
    EXPECT_EQ(l.received[before].applyKey, 24u);
    b.module.UnregisterListener(l);
}

TEST(WorldStepLoadReset, ALoadWithAPendingRecordStillRestoresOverTheReset)
{
    const std::string slot = UniqueSlot();
    SlotFile file(slot);
    {
        SaveRig a(slot, kCfg);
        Drive(a, 1, 14);   // week 7 in flight, week 14 pending
        ASSERT_TRUE(a.Save().Ok());
    }
    SaveRig b(slot, kCfg);
    Drive(b, 1, 14);
    ASSERT_TRUE(b.Load().Ok());
    EXPECT_EQ(b.module.GetChain().Phase(), ChainPhase::Ran);
    EXPECT_EQ(b.module.LastStepDay(), 7u);
    EXPECT_EQ(b.module.PendingWeekDay(), 14);
    EXPECT_EQ(b.module.GetCurrentKey(), 14u);
}
