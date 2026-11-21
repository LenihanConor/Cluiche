// World Step Feature 3: save and load of the pending step (save-and-history.md AC 1-10, 12, 13, 16 restored half,
// 21, 22; plan tasks 7-10, 12, 13). Real SaveManager/SaveRegistry, real jsoncpp text, real DeferredStepScheduler.
#include "WorldStepSaveRig.h"

using namespace WsTest;
using CoW::WorldStep::ChainPhase;
using CoW::WorldStep::WorldStepApplied;
using Dia::DeferredStep::StepState;
using Dia::DeferredStep::Testing::LogCapture;
using Dia::Observation::Health::HealthStatus;
using Dia::SaveGame::LoadContext;
using Dia::SaveGame::LoadResultCode;
using Dia::SaveGame::SaveContext;
using Level = Dia::Observation::Log::LogLevel;

namespace
{
    const StringCRC kWorld("world");

    // The record a module writes after driving a rig to `driveTo` (the step kicked that day is complete, chain Ran).
    Json::Value MakeRecord(const std::string& config, uint64_t driveTo, bool registerBoth = true)
    {
        Rig rig(config, registerBoth);
        if (!registerBoth)
            EXPECT_TRUE(rig.module.RegisterStage(StringCRC("weather"), rig.weather));
        Drive(rig, 1, driveTo);
        SaveContext sc;
        rig.module.SerializePending(sc);
        return sc.Root();
    }

    std::string IdText(const char* stage) { return std::to_string(StringCRC(stage).Value()); }

    struct RunResult
    {
        StageSnapshot weather{};
        StageSnapshot ecology{};
        std::vector<uint64_t> commitDays;
        std::vector<StepKey> captureKeys;
        std::vector<WorldStepApplied> signals;
    };

    // Uninterrupted reference run.
    RunResult Reference(const std::string& config, uint64_t last)
    {
        RunResult out;
        SaveRig rig(UniqueSlot("ref"), config);
        FakeWorldStepListener l;
        rig.module.RegisterListener(l);
        Drive(rig, 1, last);
        out.weather = Snap(rig.weather);
        out.ecology = Snap(rig.ecology);
        out.commitDays = rig.commitDays;
        out.captureKeys = rig.CaptureKeys("weather");
        for (unsigned i = 0; i < l.received.Size(); ++i)
            out.signals.push_back(l.received[i]);
        rig.module.UnregisterListener(l);
        return out;
    }
}

// ---- AC 1-3: the pending record --------------------------------------------------------------------------------------------

TEST(WorldStepSaveRecord, WritesEveryFieldAndOneObjectPerEnabledStageInDeclaredOrder)
{
    const Json::Value r = MakeRecord(kDefaultConfig, 14);
    EXPECT_EQ(r["world_version"].asInt(), 1);
    EXPECT_EQ(r["step_day"].asInt64(), 14);
    EXPECT_EQ(r["pending_day"].asInt64(), -1);
    EXPECT_EQ(r["latest_day"].asInt64(), 14);
    EXPECT_EQ(r["stage_count"].asInt(), 2);
    ASSERT_TRUE(r["stage_0"].isObject());
    ASSERT_TRUE(r["stage_1"].isObject());
    EXPECT_FALSE(r.isMember("stage_2"));
    EXPECT_EQ(r["stage_0"]["id"].asInt64(), static_cast<int64_t>(StringCRC("weather").Value()));
    EXPECT_EQ(r["stage_1"]["id"].asInt64(), static_cast<int64_t>(StringCRC("ecology").Value()));
    EXPECT_GT(r["stage_0"]["id"].asInt64(), 0);   // a CRC with the high bit set is still positive as int64
    EXPECT_EQ(r["stage_0"]["working"].asInt(), 14);   // each stage's own data sits in its object
    EXPECT_EQ(r["stage_0"]["result"].asInt(), 42);
    EXPECT_EQ(r["stage_1"]["working"].asInt(), 28);
    EXPECT_EQ(r["stage_1"]["result"].asInt(), 84);
}

TEST(WorldStepSaveRecord, PendingDayHoldsTheRetriedWeekOtherwiseMinusOne)
{
    const Json::Value retried = MakeRecord(Config("weather,ecology", 10, true), 15);   // week 7 in flight, week 14 waiting
    EXPECT_EQ(retried["step_day"].asInt64(), 7);
    EXPECT_EQ(retried["pending_day"].asInt64(), 14);
    EXPECT_EQ(retried["latest_day"].asInt64(), 15);

    const Json::Value plain = MakeRecord(kDefaultConfig, 14);
    EXPECT_EQ(plain["pending_day"].asInt64(), -1);
}

TEST(WorldStepSaveRecord, OnlyEnabledStagesWriteABlob)
{
    const Json::Value r = MakeRecord(Config("weather", 7, true), 14);   // ecology is registered but not in the order
    EXPECT_EQ(r["stage_count"].asInt(), 1);
    EXPECT_TRUE(r.isMember("stage_0"));
    EXPECT_FALSE(r.isMember("stage_1"));
    EXPECT_EQ(r["stage_0"]["id"].asInt64(), static_cast<int64_t>(StringCRC("weather").Value()));
}

TEST(WorldStepSaveRecord, ARetiredStageWritesNoBlob)
{
    Rig rig;
    Drive(rig, 1, 14);
    rig.module.UnregisterStage(StringCRC("ecology"));   // discards the step in flight; kick again with one stage in the chain
    EXPECT_EQ(rig.module.GetChain().EnabledCount(), 1u);
    EXPECT_FALSE(rig.module.GetChain().Phase() == ChainPhase::Ran);   // nothing complete to write
    LogCapture logs("WST");
    SaveContext sc;
    rig.module.SerializePending(sc);
    EXPECT_FALSE(sc.Root().isMember("stage_1"));
    EXPECT_FALSE(sc.Root().isMember("world_version"));
    EXPECT_EQ(logs.Count(Level::kError, "SerializePending"), 1u);
    ASSERT_TRUE(rig.module.RegisterStage(StringCRC("ecology"), rig.ecology));
}

// ---- AC 8-10: Validate ---------------------------------------------------------------------------------------------------------

namespace
{
    struct Fault
    {
        const char* name;
        const char* keyInLog;
        void (*mutate)(Json::Value&);
    };

    const Fault kFaults[] = {
        { "world_version 2",              "world_version", [](Json::Value& r) { r["world_version"] = 2; } },
        { "world_version not an integer", "world_version", [](Json::Value& r) { r["world_version"] = "one"; } },
        { "missing step_day",             "step_day",      [](Json::Value& r) { r.removeMember("step_day"); } },
        { "missing pending_day",          "pending_day",   [](Json::Value& r) { r.removeMember("pending_day"); } },
        { "missing latest_day",           "latest_day",    [](Json::Value& r) { r.removeMember("latest_day"); } },
        { "non-integer stage_count",      "stage_count",   [](Json::Value& r) { r["stage_count"] = "two"; } },
        { "stage_count too high",         "stage_count",   [](Json::Value& r) { r["stage_count"] = 3; } },
        { "stage_count too low",          "stage_count",   [](Json::Value& r) { r["stage_count"] = 1; } },
        { "stage object missing",         "stage_count",   [](Json::Value& r) { r.removeMember("stage_1"); } },
        { "stage_N gap",                  "stage_1",       [](Json::Value& r) { r["stage_2"] = r["stage_1"]; r.removeMember("stage_1"); } },
        { "stage id not an integer",      "stage_0",       [](Json::Value& r) { r["stage_0"]["id"] = "weather"; } },
        { "duplicate stage id",           "stage_1",       [](Json::Value& r) { r["stage_1"]["id"] = r["stage_0"]["id"]; } },
    };
}

TEST(WorldStepSaveValidate, AGoodRecordPasses)
{
    const Json::Value good = MakeRecord(kDefaultConfig, 14);
    Rig rig;
    LogCapture logs("WST");
    LoadContext lc(good);
    EXPECT_TRUE(rig.module.ValidatePending(lc));
    EXPECT_EQ(logs.Count(Level::kError), 0u);
}

TEST(WorldStepSaveValidate, EachFaultFailsAndNamesTheKeyWithExactlyOneError)
{
    const Json::Value good = MakeRecord(kDefaultConfig, 14);
    Rig rig;
    LogCapture logs("WST");
    for (const Fault& f : kFaults)
    {
        SCOPED_TRACE(f.name);
        Json::Value bad = good;
        f.mutate(bad);
        logs.Clear();
        LoadContext lc(bad);
        EXPECT_FALSE(rig.module.ValidatePending(lc));
        EXPECT_EQ(logs.Count(Level::kError), 1u);
        EXPECT_EQ(logs.Count(Level::kError, f.keyInLog), 1u);
    }
}

TEST(WorldStepSaveValidate, AMissingBlobForAnEnabledStageFailsAndNamesTheStage)
{
    Json::Value bad = MakeRecord(kDefaultConfig, 14);
    bad.removeMember("stage_1");
    bad["stage_count"] = 1;   // internally consistent, but ecology is in the order
    Rig rig;
    LogCapture logs("WST");
    LoadContext lc(bad);
    EXPECT_FALSE(rig.module.ValidatePending(lc));
    EXPECT_EQ(logs.Count(Level::kError, "ecology"), 1u);
}

TEST(WorldStepSaveValidate, ABlobForAStageNotInOrderPasses)
{
    const Json::Value record = MakeRecord(kDefaultConfig, 14);   // weather and ecology
    Rig rig(Config("weather", 7, true), false);
    ASSERT_TRUE(rig.module.RegisterStage(StringCRC("weather"), rig.weather));
    LogCapture logs("WST");
    LoadContext lc(record);
    EXPECT_TRUE(rig.module.ValidatePending(lc));
    EXPECT_EQ(logs.Count(Level::kError), 0u);
}

TEST(WorldStepSaveValidate, AConfiguredButUnregisteredStageNeedsNoBlob)
{
    const Json::Value record = MakeRecord(Config("weather", 7, true), 14);   // weather only
    Rig rig(kDefaultConfig, false);                                          // ecology is configured, never registered
    ASSERT_TRUE(rig.module.RegisterStage(StringCRC("weather"), rig.weather));
    LoadContext lc(record);
    EXPECT_TRUE(rig.module.ValidatePending(lc));
}

TEST(WorldStepSaveValidate, AStageThatRejectsItsOwnBlobRefusesTheRecordAndIsNamed)
{
    const Json::Value good = MakeRecord(kDefaultConfig, 14);
    Rig rig;
    LogCapture logs("WST");

    rig.ecology.failValidate = true;
    {
        LoadContext lc(good);
        EXPECT_FALSE(rig.module.ValidatePending(lc));
    }
    EXPECT_EQ(logs.Count(Level::kError, "ecology"), 1u);
    EXPECT_GE(rig.ecology.validateCalls, 1u);
    rig.ecology.failValidate = false;

    Json::Value corrupt = good;
    corrupt["stage_0"]["checksum"] = 12345;   // the fake's own checksum no longer matches its body
    logs.Clear();
    LoadContext lc(corrupt);
    EXPECT_FALSE(rig.module.ValidatePending(lc));
    EXPECT_EQ(logs.Count(Level::kError, "weather"), 1u);
}

TEST(WorldStepSaveValidate, ValidateMutatesNothingWhetherItPassesOrFails)
{
    const Json::Value good = MakeRecord(kDefaultConfig, 14);
    Rig rig;
    Drive(rig, 1, 17);   // a live world with committed state and a step in flight
    rig.module.OnSimulationTick(MakeTick(17));

    const uint64_t stepDay = rig.module.LastStepDay();
    const int64_t pending = rig.module.PendingWeekDay();
    const ChainPhase phase = rig.module.GetChain().Phase();
    const StageSnapshot w = Snap(rig.weather), e = Snap(rig.ecology);
    const uint64_t key = rig.module.GetCurrentKey();
    const uint64_t applies = rig.module.ApplyCount();
    const uint32_t deserializes = rig.weather.deserializeCalls + rig.ecology.deserializeCalls;
    const StepState state = rig.scheduler.GetState(kWorld);

    auto unchanged = [&]
    {
        EXPECT_EQ(rig.module.LastStepDay(), stepDay);
        EXPECT_EQ(rig.module.PendingWeekDay(), pending);
        EXPECT_EQ(rig.module.GetChain().Phase(), phase);
        EXPECT_TRUE(Snap(rig.weather) == w);
        EXPECT_TRUE(Snap(rig.ecology) == e);
        EXPECT_EQ(rig.module.GetCurrentKey(), key);
        EXPECT_EQ(rig.module.ApplyCount(), applies);
        EXPECT_EQ(rig.weather.deserializeCalls + rig.ecology.deserializeCalls, deserializes);
        EXPECT_EQ(rig.scheduler.GetState(kWorld), state);
    };

    {
        LoadContext lc(good);
        EXPECT_TRUE(rig.module.ValidatePending(lc));
    }
    unchanged();
    for (const Fault& f : kFaults)
    {
        Json::Value bad = good;
        f.mutate(bad);
        LoadContext lc(bad);
        EXPECT_FALSE(rig.module.ValidatePending(lc));
    }
    unchanged();
}

// ---- AC 4, 5, 7, 16 (restored), 21: real round trips -------------------------------------------------------------------------

TEST(WorldStepSaveLoad, MidStepSaveLoadContinuesBitIdenticallyAndAppliesAtItsOriginalKey)
{
    const std::string slot = UniqueSlot();
    SlotFile file(slot);
    const RunResult ref = Reference(kDefaultConfig, 52);

    {
        SaveRig a(slot);
        Drive(a, 1, 17);   // week 7 applied at 14; week 14 is complete and applies at 21
        ASSERT_EQ(a.module.GetChain().Phase(), ChainPhase::Ran);
        ASSERT_TRUE(a.Save().Ok());
    }

    SaveRig b(slot);
    FakeWorldStepListener l;
    ASSERT_TRUE(b.module.RegisterListener(l));
    ASSERT_TRUE(b.Load().Ok());
    EXPECT_EQ(b.module.GetChain().Phase(), ChainPhase::Ran);          // AC 7
    EXPECT_EQ(b.module.LastStepDay(), 14u);
    EXPECT_EQ(b.module.GetCurrentKey(), 17u);                         // AC 21: the key is right before any tick
    EXPECT_EQ(b.scheduler.GetState(kWorld), StepState::kComplete);
    EXPECT_EQ(b.weather.committed, 21);                               // the stages' own state came back first
    EXPECT_EQ(l.received.Size(), 0u);                                 // loading raises nothing

    Drive(b, 18, 52);

    ASSERT_FALSE(b.commitDays.empty());
    EXPECT_EQ(b.commitDays.front(), 21u);                             // original apply key
    std::vector<uint64_t> refTail;
    for (uint64_t d : ref.commitDays) if (d > 17) refTail.push_back(d);
    EXPECT_EQ(b.commitDays, refTail);
    EXPECT_TRUE(Snap(b.weather) == ref.weather);
    EXPECT_TRUE(Snap(b.ecology) == ref.ecology);

    // AC 16 (restored half) and 14: the restored step raised exactly one signal, labelled with its own day.
    std::vector<WorldStepApplied> refSignals;
    for (const WorldStepApplied& s : ref.signals) if (s.applyKey > 17) refSignals.push_back(s);
    ASSERT_EQ(l.received.Size(), refSignals.size());
    EXPECT_EQ(l.received[0].stepDay, 14u);
    EXPECT_EQ(l.received[0].applyKey, 21u);
    for (unsigned i = 0; i < l.received.Size(); ++i)
    {
        EXPECT_EQ(l.received[i].stepDay, refSignals[i].stepDay);
        EXPECT_EQ(l.received[i].applyKey, refSignals[i].applyKey);
    }
    b.module.UnregisterListener(l);
}

TEST(WorldStepSaveLoad, ARetriedWeekPendingAtSaveKicksOnTheFirstFreeTickWithItsOwnDay)
{
    const std::string slot = UniqueSlot();
    SlotFile file(slot);
    const std::string config = Config("weather,ecology", 10, true);
    const RunResult ref = Reference(config, 70);

    {
        SaveRig a(slot, config);
        Drive(a, 1, 15);   // week 7 in flight (applies 17); week 14 waiting for a free channel
        ASSERT_EQ(a.module.PendingWeekDay(), 14);
        ASSERT_TRUE(a.Save().Ok());
    }

    SaveRig b(slot, config);
    FakeWorldStepListener l;
    ASSERT_TRUE(b.module.RegisterListener(l));
    ASSERT_TRUE(b.Load().Ok());
    EXPECT_EQ(b.module.PendingWeekDay(), 14);   // AC 5: restored, not yet kicked (the restored step still holds the channel)
    EXPECT_EQ(b.module.LastStepDay(), 7u);

    Drive(b, 16, 16);
    EXPECT_TRUE(b.module.HasPendingWeek());
    EXPECT_TRUE(b.kickDays.empty());
    Drive(b, 17, 70);
    ASSERT_FALSE(b.kickDays.empty());
    EXPECT_EQ(b.kickDays.front(), 17u);          // the first free tick: the restored step applied, then the week kicks
    ASSERT_FALSE(b.CaptureKeys("weather").empty());
    EXPECT_EQ(b.CaptureKeys("weather").front(), 14u);   // with the represented day it had

    std::vector<StepKey> refKeys(ref.captureKeys.begin() + 1, ref.captureKeys.end());   // minus week 7, kicked before the save
    EXPECT_EQ(b.CaptureKeys("weather"), refKeys);
    EXPECT_TRUE(Snap(b.weather) == ref.weather);
    EXPECT_TRUE(Snap(b.ecology) == ref.ecology);

    ASSERT_GE(l.received.Size(), 2u);
    EXPECT_EQ(l.received[0].stepDay, 7u);
    EXPECT_EQ(l.received[0].applyKey, 17u);
    EXPECT_EQ(l.received[1].stepDay, 14u);       // the retried week keeps its missed day across the save
    EXPECT_EQ(l.received[1].applyKey, 27u);
    b.module.UnregisterListener(l);
}

TEST(WorldStepSaveLoad, ASaveFromTwoStagesLoadsOnOneStageWithExactlyOneWarningNamingTheOther)
{
    const std::string slot = UniqueSlot();
    SlotFile file(slot);
    {
        SaveRig a(slot);
        Drive(a, 1, 14);
        ASSERT_TRUE(a.Save().Ok());
    }

    SaveRig b(slot, Config("weather", 7, true), false);
    ASSERT_TRUE(b.module.RegisterStage(StringCRC("weather"), b.weather));
    LogCapture logs("WST");
    ASSERT_TRUE(b.Load().Ok());

    EXPECT_EQ(logs.Warnings(IdText("ecology").c_str()), 1u);
    EXPECT_EQ(logs.Warnings("skipped"), 1u);
    EXPECT_EQ(logs.Warnings(), 1u);
    EXPECT_EQ(logs.Count(Level::kError), 0u);
    EXPECT_EQ(b.weather.deserializeCalls, 1u);
    EXPECT_EQ(b.ecology.deserializeCalls, 0u);
    EXPECT_EQ(b.module.GetChain().Phase(), ChainPhase::Ran);

    Drive(b, 15, 21);
    EXPECT_EQ(b.weather.committed, 42);   // the restored result (14 * 3) committed at its original key
    EXPECT_EQ(b.module.ApplyCount(), 1u);
}

TEST(WorldStepSaveLoad, AMissingBlobForAnEnabledStageRefusesTheLoadAndChangesNothing)
{
    const std::string slot = UniqueSlot();
    SlotFile file(slot);
    {
        SaveRig a(slot, Config("weather", 7, true), false);   // saved without ecology
        ASSERT_TRUE(a.module.RegisterStage(StringCRC("weather"), a.weather));
        Drive(a, 1, 14);
        ASSERT_TRUE(a.Save().Ok());
    }

    SaveRig b(slot);   // ecology is in order here
    FakeWorldStepListener l;
    ASSERT_TRUE(b.module.RegisterListener(l));
    b.weather.committed = 77;
    b.ecology.committed = 88;
    LogCapture logs("WST");
    const auto result = b.Load();

    EXPECT_FALSE(result.Ok());
    EXPECT_EQ(result.code, LoadResultCode::DeserializeError);
    EXPECT_EQ(logs.Count(Level::kError, "ecology"), 1u);
    EXPECT_EQ(b.state.deserializeCalls, 0u);          // no participant ran
    EXPECT_EQ(b.weather.deserializeCalls, 0u);
    EXPECT_EQ(b.weather.committed, 77);
    EXPECT_EQ(b.ecology.committed, 88);
    EXPECT_EQ(b.module.GetChain().Phase(), ChainPhase::Idle);
    EXPECT_EQ(b.scheduler.GetState(kWorld), StepState::kIdle);
    EXPECT_EQ(b.module.LastStepDay(), 0u);
    EXPECT_EQ(b.module.GetCurrentKey(), 0u);
    Drive(b, 1, 6);
    EXPECT_EQ(l.received.Size(), 0u);                 // a failed load raises nothing
    b.module.UnregisterListener(l);
}

TEST(WorldStepSaveLoad, ARestoredPendingStepWhoseApplyKeyIsAlreadyReachedAppliesOnTheFirstTick)
{
    const std::string slot = UniqueSlot();
    SlotFile file(slot);
    const std::string config = Config("weather,ecology", 3, true);
    {
        SaveRig a(slot, config);
        Drive(a, 1, 7);                              // kicked at 7, applies at 10
        a.module.OnSimulationTick(MakeTick(10));     // the key reaches 10 before the scheduler ticks it
        ASSERT_EQ(a.module.GetCurrentKey(), 10u);
        ASSERT_TRUE(a.Save().Ok());
    }

    SaveRig b(slot, config);
    ASSERT_TRUE(b.Load().Ok());
    EXPECT_EQ(b.module.GetCurrentKey(), 10u);        // AC 21: restored before the first GetCurrentKey()
    EXPECT_EQ(b.module.ApplyCount(), 0u);

    b.scheduler.Tick(b.module.GetCurrentKey());      // AC 22: the first tick after the load applies it
    EXPECT_EQ(b.module.ApplyCount(), 1u);
    EXPECT_EQ(b.weather.committed, 21);
    EXPECT_EQ(b.module.GetChain().Phase(), ChainPhase::Idle);
}

TEST(WorldStepSaveLoad, ALoadedWorldAcceptsTheNextKickAfterTheRestoredStepApplied)
{
    const std::string slot = UniqueSlot();
    SlotFile file(slot);
    {
        SaveRig a(slot);
        Drive(a, 1, 17);
        ASSERT_TRUE(a.Save().Ok());
    }
    SaveRig b(slot);
    ASSERT_TRUE(b.Load().Ok());
    Drive(b, 18, 29);
    EXPECT_EQ(b.module.ApplyCount(), 2u);   // week 14 at 21, week 21 at 28
    EXPECT_EQ(b.module.KickCount(), 2u);    // weeks 21 and 28 kicked after the load
    EXPECT_EQ(b.module.LostWeeks(), 0u);
    EXPECT_EQ(b.module.GetHealthReporter().Report().status, HealthStatus::kOK);
}

// ---- Post-Validate stage failure and the unready chain (AC 13 through the module, AC 20a end to end) ---------------------------

TEST(WorldStepSaveLoad, AStageThatCannotRestoreAfterValidateLogsAnErrorDegradesHealthAndTheStepIsIgnoredAtItsKey)
{
    const std::string slot = UniqueSlot();
    SlotFile file(slot);
    {
        SaveRig a(slot);
        Drive(a, 1, 17);
        ASSERT_TRUE(a.Save().Ok());
    }

    SaveRig b(slot);
    FakeWorldStepListener l;
    ASSERT_TRUE(b.module.RegisterListener(l));
    b.weather.failDeserialize = true;   // validates, then cannot restore: goes not-ready through its own health
    LogCapture logs("WST");
    ASSERT_TRUE(b.Load().Ok());         // past Validate nothing is rolled back

    EXPECT_GE(logs.Count(Level::kError, "weather"), 1u);
    EXPECT_FALSE(b.weather.IsReady());
    EXPECT_EQ(b.module.GetChain().Phase(), ChainPhase::Idle);   // RestoreRan refused, no stage method called
    EXPECT_EQ(b.module.GetHealthReporter().Report().status, HealthStatus::kDegraded);

    logs.Clear();
    Drive(b, 18, 21);                   // the channel still reaches its apply key
    EXPECT_EQ(logs.Warnings("nothing to commit"), 1u);
    EXPECT_EQ(l.received.Size(), 0u);
    EXPECT_EQ(b.module.ApplyCount(), 0u);
    EXPECT_EQ(b.Count(CallPhase::Commit), 0u);
    EXPECT_EQ(b.scheduler.GetState(kWorld), StepState::kIdle);
    EXPECT_EQ(b.module.KickCount(), 0u);   // no kicks while a stage is not ready

    b.weather.SetReady(true);           // the next kick is accepted once the stage recovers
    b.weather.failDeserialize = false;
    Drive(b, 22, 35);
    EXPECT_GE(b.module.KickCount(), 1u);
    EXPECT_GE(b.module.ApplyCount(), 1u);
    EXPECT_GE(l.received.Size(), 1u);
    EXPECT_EQ(b.module.GetHealthReporter().Report().status, HealthStatus::kOK);
    b.module.UnregisterListener(l);
}

// ---- AC 12: participant ordering ------------------------------------------------------------------------------------------------

TEST(WorldStepSaveOrder, TheDeferredStepParticipantMustDeserializeAfterTheStagesOwnState)
{
    const std::string slot = UniqueSlot();
    SlotFile file(slot);
    {
        SaveRig a(slot);
        Drive(a, 1, 17);   // weather committed 21 (week 7), pending result for week 14
        ASSERT_TRUE(a.Save().Ok());
    }
    {
        SaveRig right(slot, kDefaultConfig, true, true);
        ASSERT_TRUE(right.Load().Ok());
        EXPECT_EQ(right.weather.committedAtDeserialize, 21);   // committed state existed when the pending result landed
        EXPECT_EQ(right.weather.committed, 21);
    }
    {
        SaveRig wrong(slot, kDefaultConfig, true, false);
        ASSERT_TRUE(wrong.Load().Ok());
        EXPECT_NE(wrong.weather.committedAtDeserialize, 21);   // the stage's state had not been restored yet: the check fails
        EXPECT_EQ(wrong.weather.committedAtDeserialize, 0);
    }
}
