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

    MockSaveable loadParticipant;
    loadParticipant.version = 2;
    loadParticipant.value   = 0;

    bool migrationCalled = false;
    SaveRegistry loadReg;
    loadReg.Register(StringCRC("mock"), &loadParticipant);
    loadReg.RegisterMigration(StringCRC("mock"), 1, [&migrationCalled](LoadContext& ctx) {
        migrationCalled = true;
        (void)ctx;
    });

    SaveManager loadMgr;
    loadMgr.Init(cfg, loadReg);

    LoadResult lr = loadMgr.Load(StringCRC("mig_single"));
    ASSERT_TRUE(lr.Ok()) << "Load failed: " << static_cast<int>(lr.code);

    EXPECT_TRUE(migrationCalled) << "Migration v1->v2 was not called";
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
//   Save value=10 at v1.  Migration v1->v2 reads "value", doubles it, and
//   writes it back to the context via ctx.Write.  The v2 Deserialize reads
//   "value" from the context and sees 20.
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_Migration, MigrationTransformsData)
{
    EnsureTestDir();

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

    MockSaveable loadParticipant;
    loadParticipant.version = 2;
    loadParticipant.value   = 0;

    SaveRegistry loadReg;
    loadReg.Register(StringCRC("transform"), &loadParticipant);

    // Migration reads the old value and writes back the transformed value.
    // Deserialize then reads the updated key and sees the doubled result.
    loadReg.RegisterMigration(StringCRC("transform"), 1, [](LoadContext& ctx) {
        int32_t old = 0;
        ctx.Read(StringCRC("value"), old);
        ctx.Write(StringCRC("value"), old * 2);
    });

    SaveManager loadMgr;
    loadMgr.Init(cfg, loadReg);

    LoadResult lr = loadMgr.Load(StringCRC("mig_transform"));
    ASSERT_TRUE(lr.Ok()) << "Load failed: " << static_cast<int>(lr.code);

    EXPECT_EQ(20, loadParticipant.value)
        << "Expected doubled value 20 after migration, got " << loadParticipant.value;

    remove("temp/savegame_migration_tests/slot_mig_transform.sav");
}
