#include <gtest/gtest.h>

#include <DiaSaveGame/SaveManifest.h>
#include <DiaSaveGame/SaveContext.h>
#include <DiaSaveGame/LoadContext.h>
#include <DiaSaveGame/SaveRegistry.h>
#include <DiaSaveGame/ISaveable.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::SaveGame;
using Dia::Core::StringCRC;

// ---------------------------------------------------------------------------
// Minimal ISaveable for wiring up a SaveRegistry
// ---------------------------------------------------------------------------

struct MockSaveableV1 : ISaveable {
    void     Serialize  (SaveContext&) const override {}
    void     Deserialize(LoadContext&)       override {}
    uint32_t GetVersion () const             override { return 1; }
};

struct MockSaveableV2 : ISaveable {
    void     Serialize  (SaveContext&) const override {}
    void     Deserialize(LoadContext&)       override {}
    uint32_t GetVersion () const             override { return 2; }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static SaveRegistry MakeRegistry(const char* idA, uint32_t verA,
                                  const char* idB, uint32_t verB,
                                  ISaveable* a, ISaveable* b)
{
    SaveRegistry reg;
    reg.Register(StringCRC(idA), a);
    reg.Register(StringCRC(idB), b);
    (void)verA; (void)verB;
    return reg;
}

// ---------------------------------------------------------------------------
// Manifest round-trip
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_Manifest, RoundTrip)
{
    MockSaveableV1 playerSave;
    MockSaveableV2 worldSave;

    SaveRegistry reg;
    reg.Register(StringCRC("player"), &playerSave);
    reg.Register(StringCRC("world"),  &worldSave);

    SaveManifest manifest;
    manifest.Build(reg, SaveFormat::Json);
    manifest.SetTimestamp(12345u);

    // Serialise
    SaveContext saveCtx;
    manifest.Write(saveCtx);

    char buf[4096];
    ASSERT_TRUE(saveCtx.Flush(buf, sizeof(buf)));

    // Deserialise
    Json::Value root;
    Json::Reader reader;
    ASSERT_TRUE(reader.parse(buf, root));

    LoadContext loadCtx(root);
    SaveManifest loaded;
    ASSERT_TRUE(loaded.Read(loadCtx));

    EXPECT_EQ(kEngineVersion, loaded.GetEngineVersion());
    EXPECT_EQ(SaveFormat::Json, loaded.GetFormat());
    EXPECT_EQ(12345u, loaded.GetTimestamp());
    EXPECT_EQ(2u, loaded.GetParticipantCount());

    EXPECT_EQ(StringCRC("player"), loaded.GetParticipantAt(0).id);
    EXPECT_EQ(1u,                  loaded.GetParticipantAt(0).version);
    EXPECT_EQ(StringCRC("world"),  loaded.GetParticipantAt(1).id);
    EXPECT_EQ(2u,                  loaded.GetParticipantAt(1).version);
}

// ---------------------------------------------------------------------------
// CheckCompatibility — engine version mismatch
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_Manifest, CompatibilityRejectsEngineMismatch)
{
    MockSaveableV1 playerSave;
    SaveRegistry reg;
    reg.Register(StringCRC("player"), &playerSave);

    SaveManifest manifest;
    manifest.Build(reg, SaveFormat::Json);

    // Manually corrupt the engine version to simulate an old save file
    SaveContext saveCtx;
    manifest.Write(saveCtx);

    // Patch the JSON before loading
    char buf[4096];
    ASSERT_TRUE(saveCtx.Flush(buf, sizeof(buf)));

    Json::Value root;
    Json::Reader reader;
    ASSERT_TRUE(reader.parse(buf, root));
    root["manifest"]["engine_version"] = Json::Int(999);

    LoadContext loadCtx(root);
    SaveManifest old;
    ASSERT_TRUE(old.Read(loadCtx));

    EXPECT_EQ(CompatResult::EngineMismatch, old.CheckCompatibility(reg));
}

// ---------------------------------------------------------------------------
// CheckCompatibility — participant version delta flags migration
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_Manifest, CompatibilityFlagsParticipantDelta)
{
    // Manifest was written when player was v1
    MockSaveableV1 oldPlayer;
    SaveRegistry oldReg;
    oldReg.Register(StringCRC("player"), &oldPlayer);

    SaveManifest manifest;
    manifest.Build(oldReg, SaveFormat::Json);

    SaveContext saveCtx;
    manifest.Write(saveCtx);

    char buf[4096];
    ASSERT_TRUE(saveCtx.Flush(buf, sizeof(buf)));

    Json::Value root;
    Json::Reader reader;
    ASSERT_TRUE(reader.parse(buf, root));

    LoadContext loadCtx(root);
    SaveManifest loaded;
    ASSERT_TRUE(loaded.Read(loadCtx));

    // Now the live registry has player at v2
    MockSaveableV2 newPlayer;
    SaveRegistry newReg;
    newReg.Register(StringCRC("player"), &newPlayer);

    EXPECT_EQ(CompatResult::ParticipantMigration, loaded.CheckCompatibility(newReg));
}
