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
