#include <gtest/gtest.h>

#include <DiaSaveGame/SaveManager.h>
#include <DiaSaveGame/SaveRegistry.h>
#include <DiaSaveGame/ISaveable.h>
#include <DiaSaveGame/SaveContext.h>
#include <DiaSaveGame/LoadContext.h>
#include <DiaSaveGame/SaveConfig.h>
#include <DiaSaveGame/SaveFormat.h>
#include <DiaSaveGame/SaveResult.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstdio>
#include <cstring>
#include <direct.h>

using namespace Dia::SaveGame;
using Dia::Core::StringCRC;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void EnsureTestDir()
{
    _mkdir("temp");
    _mkdir("temp/savegame_migration_tests");
}

static SaveConfig MakeConfig()
{
    return SaveConfig{ "temp/savegame_migration_tests/", "slot_{id}.sav", 0, SaveFormat::Json };
}

// ---------------------------------------------------------------------------
// MockSaveable — versioned; carries a single integer field
// ---------------------------------------------------------------------------

struct MockSaveable : ISaveable
{
    int      value   = 0;     // written/read via Serialize/Deserialize
    uint32_t version = 1;     // callers set this before registering

    void Serialize(SaveContext& ctx) const override
    {
        ctx.Write(StringCRC("value"), value);
    }
    void Deserialize(LoadContext& ctx) override
    {
        ctx.Read(StringCRC("value"), value);
    }
    uint32_t GetVersion() const override { return version; }
};

// ---------------------------------------------------------------------------
// TEST 1 — SingleMigration
//   Save at v1, reload with v2 registry that has a v1→v2 migration.
//   Migration sets value = 42.
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_Migration, SingleMigration)
{
    EnsureTestDir();

    // -- SAVE at version 1 --
    MockSaveable saveParticipant;
    saveParticipant.version = 1;
    saveParticipant.value   = 10;

    SaveRegistry saveReg;
    saveReg.Register(StringCRC("mock"), &saveParticipant);

    SaveManager saveMgr;
    SaveConfig  cfg = MakeConfig();
    saveMgr.Init(cfg, saveReg);

    SaveResult sr = saveMgr.Save(StringCRC("mig_single"));
    ASSERT_TRUE(sr.Ok()) << "Save failed: " << static_cast<int>(sr.code);

    // -- LOAD with version 2 + migration v1->v2 --
    MockSaveable loadParticipant;
    loadParticipant.version = 2;  // live registry says v2
    loadParticipant.value   = 0;

    SaveRegistry loadReg;
    loadReg.Register(StringCRC("mock"), &loadParticipant);

    // Migration v1->v2: sets value field to 42
    loadReg.RegisterMigration(StringCRC("mock"), 1, [](LoadContext& ctx) {
        // Overwrite the value key with a migrated value
        // (In practice a migration would transform old fields to new ones;
        //  here we simply write a known sentinel into the context's backing
        //  store isn't possible directly, so we rely on Deserialize being called
        //  after migration.  Instead, we verify the migration ran by a side-effect
        //  flag captured in the lambda below.)
        (void)ctx; // migration body — real migrations would transform ctx data
    });

    // Use a separate flag to verify migration was invoked
    bool migrationCalled = false;
    // Re-register migration with a capture
    SaveRegistry loadReg2;
    loadReg2.Register(StringCRC("mock"), &loadParticipant);
    loadReg2.RegisterMigration(StringCRC("mock"), 1, [&migrationCalled](LoadContext& ctx) {
        migrationCalled = true;
        (void)ctx;
    });

    SaveManager loadMgr;
    loadMgr.Init(cfg, loadReg2);

    LoadResult lr = loadMgr.Load(StringCRC("mig_single"));
    ASSERT_TRUE(lr.Ok()) << "Load failed: " << static_cast<int>(lr.code);

    EXPECT_TRUE(migrationCalled) << "Migration v1->v2 was not called";
    // Deserialize should still have run and restored the saved value
    EXPECT_EQ(10, loadParticipant.value);

    remove("temp/savegame_migration_tests/slot_mig_single.sav");
}

// ---------------------------------------------------------------------------
// TEST 2 — ChainedMigration
//   Save at v1, reload with v3 registry that has migrations v1→v2 and v2→v3.
//   Both migration callbacks must fire in order.
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_Migration, ChainedMigration)
{
    EnsureTestDir();

    // -- SAVE at version 1 --
    MockSaveable saveParticipant;
    saveParticipant.version = 1;
    saveParticipant.value   = 7;

    SaveRegistry saveReg;
    saveReg.Register(StringCRC("chained"), &saveParticipant);

    SaveManager saveMgr;
    SaveConfig  cfg = MakeConfig();
    saveMgr.Init(cfg, saveReg);

    SaveResult sr = saveMgr.Save(StringCRC("mig_chain"));
    ASSERT_TRUE(sr.Ok()) << "Save failed: " << static_cast<int>(sr.code);

    // -- LOAD with version 3 + migrations v1->v2 and v2->v3 --
    MockSaveable loadParticipant;
    loadParticipant.version = 3;
    loadParticipant.value   = 0;

    int migrationOrder = 0;
    int firstStepOrder  = -1;
    int secondStepOrder = -1;

    SaveRegistry loadReg;
    loadReg.Register(StringCRC("chained"), &loadParticipant);
    loadReg.RegisterMigration(StringCRC("chained"), 1, [&](LoadContext& ctx) {
        firstStepOrder = ++migrationOrder;
        (void)ctx;
    });
    loadReg.RegisterMigration(StringCRC("chained"), 2, [&](LoadContext& ctx) {
        secondStepOrder = ++migrationOrder;
        (void)ctx;
    });

    SaveManager loadMgr;
    loadMgr.Init(cfg, loadReg);

    LoadResult lr = loadMgr.Load(StringCRC("mig_chain"));
    ASSERT_TRUE(lr.Ok()) << "Load failed: " << static_cast<int>(lr.code);

    EXPECT_EQ(1, firstStepOrder)  << "v1->v2 migration did not fire first";
    EXPECT_EQ(2, secondStepOrder) << "v2->v3 migration did not fire second";
    // Deserialize should still have run
    EXPECT_EQ(7, loadParticipant.value);

    remove("temp/savegame_migration_tests/slot_mig_chain.sav");
}

// ---------------------------------------------------------------------------
// TEST 3 — MissingMigrationReturnsError
//   Manifest says v1, live registry says v3, but only v1->v2 is registered.
//   Load must return LoadResultCode::MigrationError.
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_Migration, MissingMigrationReturnsError)
{
    EnsureTestDir();

    // -- SAVE at version 1 --
    MockSaveable saveParticipant;
    saveParticipant.version = 1;
    saveParticipant.value   = 99;

    SaveRegistry saveReg;
    saveReg.Register(StringCRC("gapped"), &saveParticipant);

    SaveManager saveMgr;
    SaveConfig  cfg = MakeConfig();
    saveMgr.Init(cfg, saveReg);

    SaveResult sr = saveMgr.Save(StringCRC("mig_gap"));
    ASSERT_TRUE(sr.Ok()) << "Save failed: " << static_cast<int>(sr.code);

    // -- LOAD with version 3 but only v1->v2 registered (gap at v2->v3) --
    MockSaveable loadParticipant;
    loadParticipant.version = 3;
    loadParticipant.value   = 0;

    SaveRegistry loadReg;
    loadReg.Register(StringCRC("gapped"), &loadParticipant);
    // Only register v1->v2, deliberately omit v2->v3
    loadReg.RegisterMigration(StringCRC("gapped"), 1, [](LoadContext& ctx) {
        (void)ctx;
    });

    SaveManager loadMgr;
    loadMgr.Init(cfg, loadReg);

    LoadResult lr = loadMgr.Load(StringCRC("mig_gap"));
    EXPECT_FALSE(lr.Ok());
    EXPECT_EQ(LoadResultCode::MigrationError, lr.code)
        << "Expected MigrationError but got: " << static_cast<int>(lr.code);

    remove("temp/savegame_migration_tests/slot_mig_gap.sav");
}

// ---------------------------------------------------------------------------
// TEST 4 — MigrationTransformsData
//   LoadContext is read-only (wraps const Json::Value&), so a migration
//   cannot push new data into it.  The correct pattern is a lambda capture
//   side-channel: the migration callback reads the old value into a captured
//   variable; Deserialize reads from both the context (for v2 keys) and the
//   capture (for migrated keys).
//
//   Here: save value=10 at v1.  Migration v1->v2 reads "value" and doubles
//   it into a captured int.  The v2 Deserialize reads from the capture.
//   Assert that the deserialized result is 20.
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_Migration, MigrationTransformsData)
{
    EnsureTestDir();

    // -- SAVE at version 1, value = 10 --
    MockSaveable saveParticipant;
    saveParticipant.version = 1;
    saveParticipant.value   = 10;

    SaveRegistry saveReg;
    saveReg.Register(StringCRC("transform"), &saveParticipant);

    SaveManager saveMgr;
    SaveConfig  cfg = MakeConfig();
    saveMgr.Init(cfg, saveReg);

    SaveResult sr = saveMgr.Save(StringCRC("mig_transform"));
    ASSERT_TRUE(sr.Ok()) << "Save failed: " << static_cast<int>(sr.code);

    // -- LOAD at version 2 with a migration that doubles the value via capture --
    int32_t migratedValue = 0;  // side-channel filled by migration callback

    struct V2Saveable : ISaveable
    {
        int32_t  value   = 0;
        uint32_t version = 2;
        int32_t* migrated;  // points to the capture set by the migration

        void Serialize(SaveContext& ctx) const override
        {
            ctx.Write(StringCRC("value"), value);
        }
        void Deserialize(LoadContext&) override
        {
            // Migration already decoded the old data into *migrated
            value = *migrated;
        }
        uint32_t GetVersion() const override { return version; }
    };

    V2Saveable loadParticipant;
    loadParticipant.migrated = &migratedValue;

    SaveRegistry loadReg;
    loadReg.Register(StringCRC("transform"), &loadParticipant);

    // Migration v1->v2: read old "value", double it, store in side-channel
    loadReg.RegisterMigration(StringCRC("transform"), 1, [&migratedValue](LoadContext& ctx) {
        int32_t oldValue = 0;
        ctx.Read(StringCRC("value"), oldValue);
        migratedValue = oldValue * 2;
    });

    SaveManager loadMgr;
    loadMgr.Init(cfg, loadReg);

    LoadResult lr = loadMgr.Load(StringCRC("mig_transform"));
    ASSERT_TRUE(lr.Ok()) << "Load failed: " << static_cast<int>(lr.code);

    EXPECT_EQ(20, loadParticipant.value)
        << "Expected doubled value 20 after migration, got " << loadParticipant.value;

    remove("temp/savegame_migration_tests/slot_mig_transform.sav");
}
