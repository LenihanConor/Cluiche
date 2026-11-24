// World Step Feature 3: gap audit and edge cases (plan task 15). AC to test map lives in the plan notes; this file holds
// the cases the AC list does not name: no step in flight, load twice, load over a live world, an unready stage at load,
// high-bit stage ids, a re-save of a restored step, and malformed capacity values.
#include "WorldStepSaveRig.h"

using namespace WsTest;
using Dia::DeferredStep::ChainPhase;
using Dia::DeferredStep::StepState;
using Dia::DeferredStep::Testing::LogCapture;
using Dia::SaveGame::LoadContext;
using Dia::SaveGame::SaveContext;
using Level = Dia::Observation::Log::LogLevel;

namespace
{
    const StringCRC kWorld("world");

    // The first generated stage names whose CRC has the high bit set (so int32 would turn them negative).
    bool FindHighBitNames(std::string& first, std::string& second)
    {
        first.clear();
        second.clear();
        for (int i = 0; i < 5000; ++i)
        {
            const std::string name = "hb" + std::to_string(i);
            if (StringCRC{ name.c_str() }.Value() >= 0x80000000u)
            {
                if (first.empty()) first = name; else { second = name; return true; }
            }
        }
        return false;
    }

    void SaveWorldAt(const std::string& slot, uint64_t day, const std::string& config = kDefaultConfig)
    {
        SaveRig a(slot, config);
        Drive(a, 1, day);
        ASSERT_TRUE(a.Save().Ok());
    }
}

TEST(WorldStepSaveEdge, ASaveWithNoStepInFlightWritesNothingForTheChannelAndLoadsClean)
{
    const std::string slot = UniqueSlot();
    SlotFile file(slot);
    {
        SaveRig a(slot);
        Drive(a, 1, 6);   // no week reached
        ASSERT_TRUE(a.Save().Ok());
        EXPECT_EQ(a.weather.serializeCalls, 0u);
        EXPECT_EQ(a.ecology.serializeCalls, 0u);
    }
    SaveRig b(slot);
    FakeStepChainListener l;
    ASSERT_TRUE(b.module.RegisterListener(l));
    LogCapture logs("WST");
    ASSERT_TRUE(b.Load().Ok());
    EXPECT_EQ(b.weather.deserializeCalls, 0u);
    EXPECT_EQ(b.module.GetChain().Phase(), ChainPhase::Idle);
    EXPECT_EQ(b.scheduler.GetState(kWorld), StepState::kIdle);
    EXPECT_EQ(logs.Count(Level::kError), 0u);
    Drive(b, 1, 14);
    EXPECT_EQ(l.received.Size(), 1u);   // the loaded world kicks and applies normally
    b.module.UnregisterListener(l);
}

TEST(WorldStepSaveEdge, ASynchronousWorldNeverWritesAPendingRecord)
{
    const std::string slot = UniqueSlot();
    SlotFile file(slot);
    const std::string sync = Config("weather,ecology", 7, false);
    {
        SaveRig a(slot, sync);
        Drive(a, 1, 14);
        ASSERT_TRUE(a.Save().Ok());
        EXPECT_EQ(a.weather.serializeCalls, 0u);
    }
    SaveRig b(slot, sync);
    ASSERT_TRUE(b.Load().Ok());
    EXPECT_EQ(b.module.GetChain().Phase(), ChainPhase::Idle);
}

TEST(WorldStepSaveEdge, LoadingTwiceInARowAppliesTheRestoredStepOnce)
{
    const std::string slot = UniqueSlot();
    SlotFile file(slot);
    SaveWorldAt(slot, 17);

    SaveRig b(slot);
    FakeStepChainListener l;
    ASSERT_TRUE(b.module.RegisterListener(l));
    ASSERT_TRUE(b.Load().Ok());
    ASSERT_TRUE(b.Load().Ok());
    EXPECT_EQ(b.module.GetChain().Phase(), ChainPhase::Ran);
    EXPECT_EQ(b.weather.deserializeCalls, 2u);
    Drive(b, 18, 21);
    EXPECT_EQ(b.module.ApplyCount(), 1u);
    ASSERT_EQ(l.received.Size(), 1u);
    EXPECT_EQ(l.received[0].stepKey, 14u);
    EXPECT_EQ(b.weather.committed, 42);
    b.module.UnregisterListener(l);
}

TEST(WorldStepSaveEdge, LoadingOverALiveStepDiscardsItAndRestoresTheSavedOne)
{
    const std::string slot = UniqueSlot();
    SlotFile file(slot);
    SaveWorldAt(slot, 17);

    SaveRig b(slot);
    FakeStepChainListener l;
    ASSERT_TRUE(b.module.RegisterListener(l));
    Drive(b, 1, 10);   // week 7 of a different timeline is in flight
    ASSERT_NE(b.scheduler.GetState(kWorld), StepState::kIdle);
    ASSERT_TRUE(b.Load().Ok());

    EXPECT_EQ(b.module.GetChain().Phase(), ChainPhase::Ran);
    EXPECT_EQ(b.module.LastStepDay(), 14u);   // the saved step, not the live one
    EXPECT_EQ(b.module.GetCurrentKey(), 17u);
    EXPECT_EQ(l.received.Size(), 0u);          // the discarded live step never applies
    Drive(b, 18, 21);
    EXPECT_EQ(b.module.ApplyCount(), 1u);
    ASSERT_EQ(l.received.Size(), 1u);
    EXPECT_EQ(l.received[0].stepKey, 14u);
    EXPECT_EQ(l.received[0].applyKey, 21u);
    b.module.UnregisterListener(l);
}

TEST(WorldStepSaveEdge, AStageNotReadyAtLoadMakesTheRestoredStepAnIgnoredNoOp)
{
    const std::string slot = UniqueSlot();
    SlotFile file(slot);
    SaveWorldAt(slot, 17);

    SaveRig b(slot);
    FakeStepChainListener l;
    ASSERT_TRUE(b.module.RegisterListener(l));
    b.ecology.SetReady(false);
    LogCapture logs("WST");
    ASSERT_TRUE(b.Load().Ok());
    EXPECT_EQ(b.module.GetChain().Phase(), ChainPhase::Idle);   // RestoreRan refused without calling a stage
    EXPECT_EQ(logs.Count(Level::kError, "ecology"), 1u);
    EXPECT_EQ(logs.Count(Level::kError, "could not restore step day"), 1u);

    logs.Clear();
    Drive(b, 18, 21);
    EXPECT_EQ(logs.Warnings("nothing to commit"), 1u);
    EXPECT_EQ(b.Count(CallPhase::Commit), 0u);
    EXPECT_EQ(l.received.Size(), 0u);
    EXPECT_EQ(b.weather.committed, 21);   // the committed state from the stage's own participant is untouched

    b.ecology.SetReady(true);
    Drive(b, 22, 35);
    EXPECT_EQ(b.module.ApplyCount(), 1u);   // the next kick (week 28) was accepted and applied at 35
    b.module.UnregisterListener(l);
}

TEST(WorldStepSaveEdge, StageIdsWithTheHighBitSetRoundTripAsPositiveInt64)
{
    std::string hiA, hiB;
    ASSERT_TRUE(FindHighBitNames(hiA, hiB));
    const std::string config = Config((hiA + "," + hiB).c_str(), 7, true);
    const std::string slot = UniqueSlot();
    SlotFile file(slot);

    {
        SaveRig a(slot, config, false);
        ASSERT_TRUE(a.module.RegisterStage(StringCRC{ hiA.c_str() }, a.weather));
        ASSERT_TRUE(a.module.RegisterStage(StringCRC{ hiB.c_str() }, a.ecology));
        Drive(a, 1, 14);
        SaveContext sc;
        a.module.SerializePending(sc);
        EXPECT_EQ(sc.Root()["stage_0"]["id"].asInt64(), static_cast<int64_t>(StringCRC{ hiA.c_str() }.Value()));
        EXPECT_GT(sc.Root()["stage_0"]["id"].asInt64(), 0x7FFFFFFFll);
        EXPECT_GT(sc.Root()["stage_1"]["id"].asInt64(), 0x7FFFFFFFll);
        ASSERT_TRUE(a.Save().Ok());
        a.module.UnregisterStage(StringCRC{ hiA.c_str() });
        a.module.UnregisterStage(StringCRC{ hiB.c_str() });
    }

    SaveRig b(slot, config, false);
    ASSERT_TRUE(b.module.RegisterStage(StringCRC{ hiA.c_str() }, b.weather));
    ASSERT_TRUE(b.module.RegisterStage(StringCRC{ hiB.c_str() }, b.ecology));
    LogCapture logs("WST");
    ASSERT_TRUE(b.Load().Ok());
    EXPECT_EQ(logs.Count(Level::kError), 0u);
    EXPECT_EQ(logs.Warnings(), 0u);
    EXPECT_EQ(b.weather.deserializeCalls, 1u);
    EXPECT_EQ(b.ecology.deserializeCalls, 1u);
    EXPECT_EQ(b.module.GetChain().Phase(), ChainPhase::Ran);
    Drive(b, 15, 21);
    EXPECT_EQ(b.weather.committed, 42);
    EXPECT_EQ(b.ecology.committed, 84);
    b.module.UnregisterStage(StringCRC{ hiA.c_str() });
    b.module.UnregisterStage(StringCRC{ hiB.c_str() });
}

TEST(WorldStepSaveEdge, ReSavingARestoredStepWritesTheSameRecord)
{
    const std::string slot = UniqueSlot();
    SlotFile file(slot);
    Json::Value original;
    {
        SaveRig a(slot);
        Drive(a, 1, 17);
        SaveContext sc;
        a.module.SerializePending(sc);
        original = sc.Root();
        ASSERT_TRUE(a.Save().Ok());
    }
    SaveRig b(slot);
    ASSERT_TRUE(b.Load().Ok());
    SaveContext again;
    b.module.SerializePending(again);
    EXPECT_EQ(again.Root(), original);
}

TEST(WorldStepSaveEdge, CapacityAndRangeFaultsInTheRecordAreRefused)
{
    Json::Value record;
    {
        Rig rig;
        Drive(rig, 1, 14);
        SaveContext sc;
        rig.module.SerializePending(sc);
        record = sc.Root();
    }
    Rig rig;
    LogCapture logs("WST");

    Json::Value tooMany = record;   // nine consistent stage objects: more than the chain can hold
    tooMany["stage_count"] = 9;
    for (int i = 2; i < 9; ++i)
    {
        Json::Value blob = record["stage_0"];
        blob["id"] = 1000 + i;
        tooMany["stage_" + std::to_string(i)] = blob;
    }
    {
        LoadContext lc(tooMany);
        EXPECT_FALSE(rig.module.ValidatePending(lc));
    }
    EXPECT_EQ(logs.Count(Level::kError, "stage_count"), 1u);

    Json::Value negative = record;
    negative["pending_day"] = -5;
    logs.Clear();
    {
        LoadContext lc(negative);
        EXPECT_FALSE(rig.module.ValidatePending(lc));
    }
    EXPECT_EQ(logs.Count(Level::kError, "pending_day"), 1u);

    Json::Value zero = record;
    zero["world_version"] = 0;
    logs.Clear();
    {
        LoadContext lc(zero);
        EXPECT_FALSE(rig.module.ValidatePending(lc));
    }
    EXPECT_EQ(logs.Count(Level::kError, "world_version"), 1u);
}

TEST(WorldStepSaveEdge, AListenerThatUnregistersAfterTheRaiseGetsNoMoreSignals)
{
    Rig rig;
    FakeStepChainListener l;
    ASSERT_TRUE(rig.module.RegisterListener(l));
    rig.Steps(1, 14);
    ASSERT_EQ(l.received.Size(), 1u);
    rig.module.UnregisterListener(l);   // outside the signal: allowed
    rig.Steps(15, 28);
    EXPECT_EQ(l.received.Size(), 1u);
    EXPECT_TRUE(rig.module.RegisterListener(l));   // and it can come back
    rig.Steps(29, 35);
    EXPECT_EQ(l.received.Size(), 2u);
    rig.module.UnregisterListener(l);
}
