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
// Test participants
// ---------------------------------------------------------------------------

struct PlayerData {
    int   hp    = 0;
    float speed = 0.0f;
    bool  alive = false;
};

struct WorldData {
    int level = 0;
};

struct PlayerSaveable : ISaveable {
    PlayerData data;
    void Serialize(SaveContext& ctx) const override {
        ctx.Write(StringCRC("hp"),    data.hp);
        ctx.Write(StringCRC("speed"), data.speed);
        ctx.Write(StringCRC("alive"), data.alive);
    }
    void Deserialize(LoadContext& ctx) override {
        ctx.Read(StringCRC("hp"),    data.hp);
        ctx.Read(StringCRC("speed"), data.speed);
        ctx.Read(StringCRC("alive"), data.alive);
    }
    uint32_t GetVersion() const override { return 1; }
};

struct WorldSaveable : ISaveable {
    WorldData data;
    void Serialize(SaveContext& ctx) const override {
        ctx.Write(StringCRC("level"), data.level);
    }
    void Deserialize(LoadContext& ctx) override {
        ctx.Read(StringCRC("level"), data.level);
    }
    uint32_t GetVersion() const override { return 1; }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Use a temp path under the OS temp directory to avoid polluting the repo.
static const char* kTestDir = "temp/savegame_tests/";

static SaveConfig MakeConfig() {
    return SaveConfig{ kTestDir, "slot_{id}.sav", 0, SaveFormat::Json };
}

static void EnsureTestDir() {
    _mkdir("temp");
    _mkdir("temp/savegame_tests");
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_Manager, TwoParticipantRoundTrip)
{
    EnsureTestDir();

    // --- Save ---
    PlayerSaveable player;
    player.data = { 80, 3.5f, true };

    WorldSaveable world;
    world.data = { 5 };

    SaveRegistry reg;
    reg.Register(StringCRC("player"), &player);
    reg.Register(StringCRC("world"),  &world);

    SaveManager mgr;
    SaveConfig cfg = MakeConfig();
    mgr.Init(cfg, reg);

    SaveResult sr = mgr.Save(StringCRC("slot0"));
    ASSERT_TRUE(sr.Ok()) << "Save failed with code " << static_cast<int>(sr.code);

    // --- Load into fresh participants ---
    PlayerSaveable loadedPlayer;
    WorldSaveable  loadedWorld;

    SaveRegistry loadReg;
    loadReg.Register(StringCRC("player"), &loadedPlayer);
    loadReg.Register(StringCRC("world"),  &loadedWorld);

    SaveManager loadMgr;
    loadMgr.Init(cfg, loadReg);

    LoadResult lr = loadMgr.Load(StringCRC("slot0"));
    ASSERT_TRUE(lr.Ok()) << "Load failed with code " << static_cast<int>(lr.code);

    EXPECT_EQ(80,   loadedPlayer.data.hp);
    EXPECT_NEAR(3.5f, loadedPlayer.data.speed, 0.0001f);
    EXPECT_TRUE(loadedPlayer.data.alive);
    EXPECT_EQ(5, loadedWorld.data.level);

    // Cleanup
    remove("temp/savegame_tests/slot_slot0.sav");
}

TEST(DiaSaveGame_Manager, LoadMissingSlotReturnsError)
{
    EnsureTestDir();

    SaveRegistry reg;
    SaveManager  mgr;
    SaveConfig   cfg = MakeConfig();
    mgr.Init(cfg, reg);

    LoadResult lr = mgr.Load(StringCRC("no_such_slot"));
    EXPECT_FALSE(lr.Ok());
    EXPECT_EQ(LoadResultCode::SlotNotFound, lr.code);
}

TEST(DiaSaveGame_Manager, LoadSkipsParticipantAbsentFromLiveRegistry)
{
    // Save a file that contains two participants (player + world).
    // Load it with a registry that only has "player".
    // The load must succeed and "player" must deserialize correctly —
    // the absent "world" participant must be silently skipped.
    EnsureTestDir();

    PlayerSaveable player;
    player.data = { 55, 1.0f, true };

    WorldSaveable world;
    world.data = { 3 };

    SaveRegistry saveReg;
    saveReg.Register(StringCRC("player"), &player);
    saveReg.Register(StringCRC("world"),  &world);

    SaveManager saveMgr;
    SaveConfig  cfg = MakeConfig();
    saveMgr.Init(cfg, saveReg);

    SaveResult sr = saveMgr.Save(StringCRC("slot_fwd"));
    ASSERT_TRUE(sr.Ok()) << "Save failed with code " << static_cast<int>(sr.code);

    // Load registry only knows "player" — "world" is gone
    PlayerSaveable loadedPlayer;

    SaveRegistry loadReg;
    loadReg.Register(StringCRC("player"), &loadedPlayer);

    SaveManager loadMgr;
    loadMgr.Init(cfg, loadReg);

    LoadResult lr = loadMgr.Load(StringCRC("slot_fwd"));
    ASSERT_TRUE(lr.Ok()) << "Load should succeed even when a saved participant is not in the live registry";

    EXPECT_EQ(55,   loadedPlayer.data.hp);
    EXPECT_NEAR(1.0f, loadedPlayer.data.speed, 0.0001f);
    EXPECT_TRUE(loadedPlayer.data.alive);

    remove("temp/savegame_tests/slot_slot_fwd.sav");
}

TEST(DiaSaveGame_Manager, LoadEngineMismatchReturnsError)
{
    EnsureTestDir();

    // Save with current engine
    PlayerSaveable player;
    player.data = { 10, 1.0f, false };

    SaveRegistry reg;
    reg.Register(StringCRC("player"), &player);

    SaveManager mgr;
    SaveConfig  cfg = MakeConfig();
    mgr.Init(cfg, reg);

    SaveResult sr = mgr.Save(StringCRC("slot_emv"));
    ASSERT_TRUE(sr.Ok());

    // Corrupt the engine_version field in the file
    FILE* f = fopen("temp/savegame_tests/slot_slot_emv.sav", "rb");
    ASSERT_NE(nullptr, f);
    char buf[65536] = {};
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';

    // Naive replace: find "engine_version" : 1 and change to 999
    char* pos = strstr(buf, "\"engine_version\"");
    ASSERT_NE(nullptr, pos);
    // Find the colon + value and overwrite the digit
    char* colon = strchr(pos, ':');
    ASSERT_NE(nullptr, colon);
    // Find the first digit after the colon
    char* digit = colon + 1;
    while (*digit == ' ') ++digit;
    // Replace the single digit '1' with "999" by shifting (buffer has room)
    size_t tail = strlen(digit);
    memmove(digit + 3, digit + 1, tail);
    digit[0] = '9'; digit[1] = '9'; digit[2] = '9';

    FILE* w = fopen("temp/savegame_tests/slot_slot_emv.sav", "wb");
    ASSERT_NE(nullptr, w);
    fwrite(buf, 1, strlen(buf), w);
    fclose(w);

    LoadResult lr = mgr.Load(StringCRC("slot_emv"));
    EXPECT_FALSE(lr.Ok());
    EXPECT_EQ(LoadResultCode::EngineMismatch, lr.code);

    remove("temp/savegame_tests/slot_slot_emv.sav");
}

// ===========================================================================
// Atomic save / slot id validation / oversize handling
// ===========================================================================

#include <DiaSaveGame/Testing/SaveTestHelpers.h>
#include <string>

namespace {

struct ValueSaveable : ISaveable {
    int value = 0;
    void Serialize(SaveContext& ctx) const override { ctx.Write(StringCRC("value"), static_cast<int32_t>(value)); }
    void Deserialize(LoadContext& ctx) override { int32_t v = 0; ctx.Read(StringCRC("value"), v); value = v; }
    uint32_t GetVersion() const override { return 1; }
};

struct HugeSaveable : ISaveable {
    void Serialize(SaveContext& ctx) const override {
        static const char chunk[] =
            "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
            "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
            "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789";
        for (int i = 0; i < 40000; ++i)
            { char k[16]; snprintf(k, sizeof(k), "b%d", i); ctx.Write(StringCRC(k), chunk); }
    }
    void Deserialize(LoadContext&) override {}
    uint32_t GetVersion() const override { return 1; }
};

std::string ReadAll(const char* path) {
    std::string s;
    FILE* f = fopen(path, "rb");
    if (!f) return s;
    char b[4096]; size_t n;
    while ((n = fread(b, 1, sizeof(b), f)) > 0) s.append(b, n);
    fclose(f);
    return s;
}

bool Exists(const char* path) {
    FILE* f = fopen(path, "rb");
    if (f) { fclose(f); return true; }
    return false;
}

const char* kSlotA   = "temp/savegame_tests/slot_atom.sav";
const char* kSlotATmp = "temp/savegame_tests/slot_atom.sav.tmp";

} // namespace

TEST(DiaSaveGame_ManagerAtomic, FailedWriteKeepsPreviousSave)
{
    EnsureTestDir();
    remove(kSlotA);

    ValueSaveable s; s.value = 1;
    SaveRegistry reg; reg.Register(StringCRC("v"), &s);
    SaveManager mgr; SaveConfig cfg = MakeConfig(); mgr.Init(cfg, reg);
    ASSERT_TRUE(mgr.Save(StringCRC("atom")).Ok());
    const std::string before = ReadAll(kSlotA);
    ASSERT_FALSE(before.empty());

    // A directory squatting on the tmp path makes the tmp open fail.
    _mkdir(kSlotATmp);
    s.value = 2;
    SaveResult sr = mgr.Save(StringCRC("atom"));
    EXPECT_EQ(SaveResultCode::FileWriteError, sr.code);
    EXPECT_EQ(before, ReadAll(kSlotA));

    s.value = 0;
    ASSERT_TRUE(mgr.Load(StringCRC("atom")).Ok());
    EXPECT_EQ(1, s.value);

    _rmdir(kSlotATmp);
    remove(kSlotA);
}

TEST(DiaSaveGame_ManagerAtomic, SuccessfulSaveOverwritesAndLeavesNoTmp)
{
    EnsureTestDir();
    remove(kSlotA);

    ValueSaveable s; s.value = 1;
    SaveRegistry reg; reg.Register(StringCRC("v"), &s);
    SaveManager mgr; SaveConfig cfg = MakeConfig(); mgr.Init(cfg, reg);
    ASSERT_TRUE(mgr.Save(StringCRC("atom")).Ok());

    // Stale tmp from a "crash" is overwritten.
    FILE* f = fopen(kSlotATmp, "wb"); ASSERT_NE(nullptr, f); fwrite("junk", 1, 4, f); fclose(f);

    s.value = 7;
    ASSERT_TRUE(mgr.Save(StringCRC("atom")).Ok());
    EXPECT_FALSE(Exists(kSlotATmp));

    s.value = 0;
    ASSERT_TRUE(mgr.Load(StringCRC("atom")).Ok());
    EXPECT_EQ(7, s.value);
    remove(kSlotA);
}

TEST(DiaSaveGame_ManagerAtomic, SerializeOverflowReturnsErrorAndKeepsExistingSave)
{
    EnsureTestDir();
    remove(kSlotA);

    ValueSaveable good; good.value = 1;
    SaveRegistry reg; reg.Register(StringCRC("v"), &good);
    SaveManager mgr; SaveConfig cfg = MakeConfig(); mgr.Init(cfg, reg);
    ASSERT_TRUE(mgr.Save(StringCRC("atom")).Ok());
    const std::string before = ReadAll(kSlotA);

    HugeSaveable huge;
    SaveRegistry reg2; reg2.Register(StringCRC("h"), &huge);
    SaveManager mgr2; mgr2.Init(cfg, reg2);
    EXPECT_EQ(SaveResultCode::SerializeError, mgr2.Save(StringCRC("atom")).code);
    EXPECT_EQ(before, ReadAll(kSlotA));
    EXPECT_FALSE(Exists(kSlotATmp));
    remove(kSlotA);
}

TEST(DiaSaveGame_ManagerAtomic, LoadOfOversizedFileFailsCleanly)
{
    EnsureTestDir();
    const char* path = "temp/savegame_tests/slot_huge.sav";
    FILE* f = fopen(path, "wb"); ASSERT_NE(nullptr, f);
    std::string junk(1 << 20, '{');
    for (int i = 0; i < 11; ++i) fwrite(junk.data(), 1, junk.size(), f);
    fclose(f);

    ValueSaveable s; s.value = 5;
    SaveRegistry reg; reg.Register(StringCRC("v"), &s);
    SaveManager mgr; SaveConfig cfg = MakeConfig(); mgr.Init(cfg, reg);
    LoadResult lr = mgr.Load(StringCRC("huge"));
    EXPECT_FALSE(lr.Ok());
    EXPECT_EQ(5, s.value);
    remove(path);
}

TEST(DiaSaveGame_ManagerSlotId, InvalidIdsRejectedWithoutTouchingDisk)
{
    EnsureTestDir();
    remove("temp/escaped.sav");
    remove("temp/slot_escaped.sav");

    ValueSaveable s; s.value = 1;
    SaveRegistry reg; reg.Register(StringCRC("v"), &s);
    SaveManager mgr; SaveConfig cfg = MakeConfig(); mgr.Init(cfg, reg);

    const char* bad[] = { "", "../x", "a/b", "a\\b", "C:x", ".hidden", "a..b", "CON" };
    for (const char* id : bad)
    {
        EXPECT_EQ(SaveResultCode::InvalidSlotId, mgr.Save(StringCRC(id)).code) << id;
        EXPECT_EQ(LoadResultCode::InvalidSlotId, mgr.Load(StringCRC(id)).code) << id;
    }
    EXPECT_FALSE(Exists("temp/escaped.sav"));

    // Delete / Rename with invalid ids must not move or delete anything.
    ASSERT_TRUE(mgr.Save(StringCRC("ok1")).Ok());
    mgr.RenameSlot(StringCRC("ok1"), StringCRC("../escaped"));
    EXPECT_TRUE(Exists("temp/savegame_tests/slot_ok1.sav"));
    EXPECT_FALSE(Exists("temp/slot_escaped.sav"));
    mgr.RenameSlot(StringCRC("a/b"), StringCRC("ok2"));
    EXPECT_FALSE(Exists("temp/savegame_tests/slot_ok2.sav"));
    mgr.DeleteSlot(StringCRC("../savegame_tests/slot_ok1"));
    EXPECT_TRUE(Exists("temp/savegame_tests/slot_ok1.sav"));
    remove("temp/savegame_tests/slot_ok1.sav");
}
