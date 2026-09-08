#include <gtest/gtest.h>

#include <DiaSaveGame/SaveManager.h>
#include <DiaSaveGame/SaveRegistry.h>
#include <DiaSaveGame/SaveConfig.h>
#include <DiaSaveGame/SaveFormat.h>
#include <DiaSaveGame/Testing/SaveTestHelpers.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Architecture/Observer.h>

#include <cstdio>
#include <direct.h>

using namespace Dia::SaveGame;
using namespace Dia::SaveGame::Testing;
using Dia::Core::StringCRC;

static void EnsureEventsTestDir()
{
    _mkdir("temp");
    _mkdir("temp/savegame_event_tests");
}

static SaveConfig MakeEventsConfig()
{
    return SaveConfig{ "temp/savegame_event_tests/", "slot_{id}.sav", 0, SaveFormat::Json };
}

// ---------------------------------------------------------------------------
// SaveEventCountingObserver — unique name to avoid ODR with other test TUs
// ---------------------------------------------------------------------------
struct SaveEventCountingObserver : Dia::Core::Observer
{
    int saveStartedCount      = 0;
    int saveCompletedCount    = 0;
    int loadStartedCount      = 0;
    int loadCompletedCount    = 0;
    int migrationAppliedCount = 0;

    void ObserverNotification(const Dia::Core::ObserverSubject*, int message) override
    {
        switch (static_cast<SaveEvent>(message))
        {
        case SaveEvent::SaveStarted:      ++saveStartedCount;      break;
        case SaveEvent::SaveCompleted:    ++saveCompletedCount;    break;
        case SaveEvent::LoadStarted:      ++loadStartedCount;      break;
        case SaveEvent::LoadCompleted:    ++loadCompletedCount;    break;
        case SaveEvent::MigrationApplied: ++migrationAppliedCount; break;
        default: break;
        }
    }
};

// ---------------------------------------------------------------------------
// SaveEvents_FireOnSave
//   Save fires SaveStarted and SaveCompleted; no Load events.
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_Events, SaveEvents_FireOnSave)
{
    EnsureEventsTestDir();

    MockSaveable participant;
    participant.valueToWrite = 5;
    participant.version      = 1;

    SaveRegistry reg;
    reg.Register(StringCRC("sev_p"), &participant);

    SaveConfig  cfg = MakeEventsConfig();
    SaveManager mgr;
    mgr.Init(cfg, reg);

    SaveEventCountingObserver obs;
    mgr.OnSaveStarted().AttachToObserver(&obs);
    mgr.OnSaveCompleted().AttachToObserver(&obs);
    mgr.OnLoadStarted().AttachToObserver(&obs);
    mgr.OnLoadCompleted().AttachToObserver(&obs);
    mgr.OnMigrationApplied().AttachToObserver(&obs);

    ASSERT_TRUE(mgr.Save(StringCRC("ev_save")).Ok());

    EXPECT_EQ(1, obs.saveStartedCount)      << "OnSaveStarted should fire once";
    EXPECT_EQ(1, obs.saveCompletedCount)    << "OnSaveCompleted should fire once";
    EXPECT_EQ(0, obs.loadStartedCount)      << "OnLoadStarted should not fire during Save";
    EXPECT_EQ(0, obs.loadCompletedCount)    << "OnLoadCompleted should not fire during Save";
    EXPECT_EQ(0, obs.migrationAppliedCount) << "OnMigrationApplied should not fire during Save";

    mgr.OnSaveStarted().DetachFromObserver(&obs);
    mgr.OnSaveCompleted().DetachFromObserver(&obs);
    mgr.OnLoadStarted().DetachFromObserver(&obs);
    mgr.OnLoadCompleted().DetachFromObserver(&obs);
    mgr.OnMigrationApplied().DetachFromObserver(&obs);

    remove("temp/savegame_event_tests/slot_ev_save.sav");
}

// ---------------------------------------------------------------------------
// LoadEvents_FireOnLoad
//   Load (no migration needed) fires LoadStarted and LoadCompleted.
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_Events, LoadEvents_FireOnLoad)
{
    EnsureEventsTestDir();

    MockSaveable participant;
    participant.valueToWrite = 7;
    participant.version      = 1;

    SaveRegistry reg;
    reg.Register(StringCRC("lev_p"), &participant);

    SaveConfig  cfg = MakeEventsConfig();
    SaveManager mgr;
    mgr.Init(cfg, reg);

    // Save without an observer, then check Load events
    ASSERT_TRUE(mgr.Save(StringCRC("ev_load")).Ok());

    SaveEventCountingObserver obs;
    mgr.OnSaveStarted().AttachToObserver(&obs);
    mgr.OnSaveCompleted().AttachToObserver(&obs);
    mgr.OnLoadStarted().AttachToObserver(&obs);
    mgr.OnLoadCompleted().AttachToObserver(&obs);
    mgr.OnMigrationApplied().AttachToObserver(&obs);

    ASSERT_TRUE(mgr.Load(StringCRC("ev_load")).Ok());

    EXPECT_EQ(0, obs.saveStartedCount)      << "OnSaveStarted should not fire during Load";
    EXPECT_EQ(0, obs.saveCompletedCount)    << "OnSaveCompleted should not fire during Load";
    EXPECT_EQ(1, obs.loadStartedCount)      << "OnLoadStarted should fire once";
    EXPECT_EQ(1, obs.loadCompletedCount)    << "OnLoadCompleted should fire once";
    EXPECT_EQ(0, obs.migrationAppliedCount) << "OnMigrationApplied should not fire (no migration)";

    mgr.OnSaveStarted().DetachFromObserver(&obs);
    mgr.OnSaveCompleted().DetachFromObserver(&obs);
    mgr.OnLoadStarted().DetachFromObserver(&obs);
    mgr.OnLoadCompleted().DetachFromObserver(&obs);
    mgr.OnMigrationApplied().DetachFromObserver(&obs);

    remove("temp/savegame_event_tests/slot_ev_load.sav");
}

// ---------------------------------------------------------------------------
// MigrationApplied_FiresPerStep
//   Save at v1 with one manager; load with a v3 registry (v1->v2 and v2->v3).
//   MigrationApplied must fire exactly twice.
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_Events, MigrationApplied_FiresPerStep)
{
    EnsureEventsTestDir();

    // Save at v1
    MockSaveable saveP;
    saveP.valueToWrite = 3;
    saveP.version      = 1;

    SaveRegistry saveReg;
    saveReg.Register(StringCRC("mev_p"), &saveP);

    SaveConfig  cfg = MakeEventsConfig();
    {
        SaveManager saveMgr;
        saveMgr.Init(cfg, saveReg);
        ASSERT_TRUE(saveMgr.Save(StringCRC("ev_mig")).Ok());
    }

    // Load with v3 + 2 migrations
    MockSaveable loadP;
    loadP.version = 3;

    SaveRegistry loadReg;
    loadReg.Register(StringCRC("mev_p"), &loadP);
    loadReg.RegisterMigration(StringCRC("mev_p"), 1, [](LoadContext& ctx) { (void)ctx; });
    loadReg.RegisterMigration(StringCRC("mev_p"), 2, [](LoadContext& ctx) { (void)ctx; });

    SaveManager loadMgr;
    loadMgr.Init(cfg, loadReg);

    SaveEventCountingObserver obs;
    loadMgr.OnSaveStarted().AttachToObserver(&obs);
    loadMgr.OnSaveCompleted().AttachToObserver(&obs);
    loadMgr.OnLoadStarted().AttachToObserver(&obs);
    loadMgr.OnLoadCompleted().AttachToObserver(&obs);
    loadMgr.OnMigrationApplied().AttachToObserver(&obs);

    ASSERT_TRUE(loadMgr.Load(StringCRC("ev_mig")).Ok());

    EXPECT_EQ(0, obs.saveStartedCount)      << "OnSaveStarted should not fire during Load";
    EXPECT_EQ(0, obs.saveCompletedCount)    << "OnSaveCompleted should not fire during Load";
    EXPECT_EQ(1, obs.loadStartedCount)      << "OnLoadStarted should fire once";
    EXPECT_EQ(1, obs.loadCompletedCount)    << "OnLoadCompleted should fire once";
    EXPECT_EQ(2, obs.migrationAppliedCount) << "OnMigrationApplied should fire once per step (2 steps)";

    loadMgr.OnSaveStarted().DetachFromObserver(&obs);
    loadMgr.OnSaveCompleted().DetachFromObserver(&obs);
    loadMgr.OnLoadStarted().DetachFromObserver(&obs);
    loadMgr.OnLoadCompleted().DetachFromObserver(&obs);
    loadMgr.OnMigrationApplied().DetachFromObserver(&obs);

    remove("temp/savegame_event_tests/slot_ev_mig.sav");
}
