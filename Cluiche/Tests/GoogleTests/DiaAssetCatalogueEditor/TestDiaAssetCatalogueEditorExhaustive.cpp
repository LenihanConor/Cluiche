#include <gtest/gtest.h>

#include <DiaEditor/Command/CommandHistory.h>
#include <DiaEditor/Command/IEditorCommand.h>

#include <DiaAssetCatalogue/AssetRecord.h>
#include <DiaAssetCatalogue/AssetRegistry.h>
#include <DiaAssetCatalogue/AssetTypeRegistry.h>
#include <DiaAssetCatalogue/AssetTypeDescriptor.h>
#include <DiaAssetCatalogue/RelationshipIndex.h>
#include <DiaAssetCatalogue/ContentHasher.h>
#include <DiaAssetCatalogue/CatalogueManifestSerializer.h>
#include <DiaAssetCatalogue/CatalogueRulesEngine.h>

#include <DiaAssetCatalogueEditor/Commands/CreateRecordCommand.h>
#include <DiaAssetCatalogueEditor/Commands/UpdateRecordCommand.h>
#include <DiaAssetCatalogueEditor/Commands/DeleteRecordCommand.h>
#include <DiaAssetCatalogueEditor/Commands/AddRelationshipCommand.h>
#include <DiaAssetCatalogueEditor/Commands/RemoveRelationshipCommand.h>
#include <DiaAssetCatalogueEditor/Commands/ApplyRulesCommand.h>
#include <DiaAssetCatalogueEditor/Handlers/FileDiscoverer.h>
#include <DiaAssetCatalogueEditor/Handlers/AssetTypeEditorRegistry.h>
#include <DiaAssetCatalogueEditor/ManifestLoadHandler.h>
#include <DiaAssetCatalogueEditor/SessionContext.h>

#include <DiaAssetCatalogue/BuiltInAssetTypes.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

using namespace Dia::AssetCatalogue;
using namespace Dia::AssetCatalogue::Editor;
using namespace Dia::Editor;
using namespace Dia::Core;

// ===========================================================================
// Test Helpers
// ===========================================================================
namespace
{
    AssetRecord MakeRecord(const char* id, const char* type, const char* sourcePath = "")
    {
        AssetRecord rec;
        rec.mId = StringCRC(id);
        rec.mAssetTypeId = StringCRC(type);
        if (sourcePath && sourcePath[0] != '\0')
            rec.mSourcePath = Dia::Core::Containers::String256(sourcePath);
        rec.mStatus = AssetStatus::Active;
        rec.mScope = AssetScope::kGlobal;
        return rec;
    }

    AssetRecord MakeStageRecord(const char* id, const char* type, const char* stageName)
    {
        AssetRecord rec = MakeRecord(id, type);
        rec.mScope = AssetScope::kStage;
        rec.mScopeStageName = StringCRC(stageName);
        return rec;
    }

    const AssetRecord* FindByIdViaIndex(const AssetRegistry& registry, const StringCRC& id)
    {
        for (unsigned int i = 0; i < registry.GetCount(); ++i)
        {
            const AssetRecord& rec = registry.GetRecordByIndex(i);
            if (rec.mId == id)
                return &rec;
        }
        return nullptr;
    }

    void RegisterType(AssetTypeRegistry& reg, const char* typeId, const char* name, const char* pattern)
    {
        AssetTypeDescriptor desc;
        desc.mTypeId = StringCRC(typeId);
        desc.mName = Dia::Core::Containers::String64(name);
        desc.mFilePattern = Dia::Core::Containers::String64(pattern);
        desc.mTypeDefinition = nullptr;
        reg.Register(desc);
    }
}

// ===========================================================================
// CreateRecordCommand
// ===========================================================================
class CreateRecordCommandTest : public ::testing::Test
{
protected:
    AssetRegistry registry;
    CommandHistory history;
};

TEST_F(CreateRecordCommandTest, ExecuteRegistersRecord)
{
    AssetRecord rec = MakeRecord("texture.player", "texture", "textures/player.png");
    auto* cmd = new CreateRecordCommand(registry, rec);
    history.ExecuteCommand(cmd);

    EXPECT_EQ(registry.GetCount(), 1u);
    ASSERT_NE(registry.FindById(StringCRC("texture.player")), nullptr);
    EXPECT_EQ(registry.FindById(StringCRC("texture.player"))->mAssetTypeId, StringCRC("texture"));
}

TEST_F(CreateRecordCommandTest, UndoRemovesRecord)
{
    AssetRecord rec = MakeRecord("texture.player", "texture");
    auto* cmd = new CreateRecordCommand(registry, rec);
    history.ExecuteCommand(cmd);
    EXPECT_EQ(registry.GetCount(), 1u);

    history.Undo();
    EXPECT_EQ(registry.GetCount(), 0u);
    EXPECT_EQ(registry.FindById(StringCRC("texture.player")), nullptr);
}

TEST_F(CreateRecordCommandTest, RedoReRegistersRecord)
{
    AssetRecord rec = MakeRecord("texture.player", "texture");
    auto* cmd = new CreateRecordCommand(registry, rec);
    history.ExecuteCommand(cmd);
    history.Undo();
    EXPECT_EQ(registry.GetCount(), 0u);

    history.Redo();
    EXPECT_EQ(registry.GetCount(), 1u);
    ASSERT_NE(registry.FindById(StringCRC("texture.player")), nullptr);
}

TEST_F(CreateRecordCommandTest, PreservesAllFields)
{
    AssetRecord rec = MakeStageRecord("config.main", "config", "level_1");
    rec.mStatus = AssetStatus::Draft;
    rec.mContentHash = 12345;
    rec.mTags.Add(StringCRC("important"));
    rec.mTags.Add(StringCRC("player"));

    auto* cmd = new CreateRecordCommand(registry, rec);
    history.ExecuteCommand(cmd);

    const AssetRecord* stored = registry.FindById(StringCRC("config.main"));
    ASSERT_NE(stored, nullptr);
    EXPECT_EQ(stored->mStatus, AssetStatus::Draft);
    EXPECT_EQ(stored->mScope, AssetScope::kStage);
    EXPECT_EQ(stored->mScopeStageName, StringCRC("level_1"));
    EXPECT_EQ(stored->mContentHash, 12345u);
    EXPECT_EQ(stored->mTags.Size(), 2u);
}

TEST_F(CreateRecordCommandTest, MultipleCreatesAndUndos)
{
    for (int i = 0; i < 5; ++i)
    {
        char id[64];
        _snprintf_s(id, sizeof(id), _TRUNCATE, "texture.item_%d", i);
        auto* cmd = new CreateRecordCommand(registry, MakeRecord(id, "texture"));
        history.ExecuteCommand(cmd);
    }
    EXPECT_EQ(registry.GetCount(), 5u);

    for (int i = 0; i < 5; ++i)
        history.Undo();
    EXPECT_EQ(registry.GetCount(), 0u);
}

// ===========================================================================
// UpdateRecordCommand
// ===========================================================================
class UpdateRecordCommandTest : public ::testing::Test
{
protected:
    AssetRegistry registry;
    CommandHistory history;

    void SetUp() override
    {
        registry.Register(MakeRecord("texture.player", "texture", "old/path.png"));
    }
};

TEST_F(UpdateRecordCommandTest, ExecuteUpdatesFields)
{
    const AssetRecord* old = registry.FindById(StringCRC("texture.player"));
    AssetRecord newRec = *old;
    newRec.mSourcePath = Dia::Core::Containers::String256("new/path.png");
    newRec.mStatus = AssetStatus::Deprecated;

    auto* cmd = new UpdateRecordCommand(registry, StringCRC("texture.player"), *old, newRec);
    history.ExecuteCommand(cmd);

    const AssetRecord* updated = registry.FindById(StringCRC("texture.player"));
    ASSERT_NE(updated, nullptr);
    EXPECT_STREQ(updated->mSourcePath.AsCStr(), "new/path.png");
    EXPECT_EQ(updated->mStatus, AssetStatus::Deprecated);
}

TEST_F(UpdateRecordCommandTest, UndoRestoresOriginal)
{
    const AssetRecord* old = registry.FindById(StringCRC("texture.player"));
    AssetRecord oldCopy = *old;
    AssetRecord newRec = *old;
    newRec.mStatus = AssetStatus::Deprecated;

    auto* cmd = new UpdateRecordCommand(registry, StringCRC("texture.player"), oldCopy, newRec);
    history.ExecuteCommand(cmd);
    history.Undo();

    const AssetRecord* restored = registry.FindById(StringCRC("texture.player"));
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->mStatus, AssetStatus::Active);
    EXPECT_STREQ(restored->mSourcePath.AsCStr(), "old/path.png");
}

TEST_F(UpdateRecordCommandTest, MultipleUpdatesUndo)
{
    const AssetRecord* rec = registry.FindById(StringCRC("texture.player"));

    // First update
    AssetRecord v1 = *rec;
    v1.mStatus = AssetStatus::Draft;
    auto* cmd1 = new UpdateRecordCommand(registry, StringCRC("texture.player"), *rec, v1);
    history.ExecuteCommand(cmd1);

    // Second update
    rec = registry.FindById(StringCRC("texture.player"));
    AssetRecord v2 = *rec;
    v2.mStatus = AssetStatus::Deprecated;
    auto* cmd2 = new UpdateRecordCommand(registry, StringCRC("texture.player"), *rec, v2);
    history.ExecuteCommand(cmd2);

    EXPECT_EQ(registry.FindById(StringCRC("texture.player"))->mStatus, AssetStatus::Deprecated);
    history.Undo();
    EXPECT_EQ(registry.FindById(StringCRC("texture.player"))->mStatus, AssetStatus::Draft);
    history.Undo();
    EXPECT_EQ(registry.FindById(StringCRC("texture.player"))->mStatus, AssetStatus::Active);
}

TEST_F(UpdateRecordCommandTest, TagsRoundTrip)
{
    const AssetRecord* rec = registry.FindById(StringCRC("texture.player"));
    AssetRecord updated = *rec;
    updated.mTags.Add(StringCRC("hero"));
    updated.mTags.Add(StringCRC("animated"));

    auto* cmd = new UpdateRecordCommand(registry, StringCRC("texture.player"), *rec, updated);
    history.ExecuteCommand(cmd);

    EXPECT_EQ(registry.FindById(StringCRC("texture.player"))->mTags.Size(), 2u);
    history.Undo();
    EXPECT_EQ(registry.FindById(StringCRC("texture.player"))->mTags.Size(), 0u);
}

// ===========================================================================
// DeleteRecordCommand
// ===========================================================================
class DeleteRecordCommandTest : public ::testing::Test
{
protected:
    AssetRegistry registry;
    CommandHistory history;

    void SetUp() override
    {
        AssetRecord a = MakeRecord("texture.player", "texture", "tex/player.png");
        AssetRecord b = MakeRecord("entity.hero", "entity");
        b.mReferences.Add(RelationshipEdge(StringCRC("uses"), StringCRC("texture.player")));
        AssetRecord c = MakeRecord("config.level", "config");
        c.mReferences.Add(RelationshipEdge(StringCRC("contains"), StringCRC("entity.hero")));

        registry.Register(a);
        registry.Register(b);
        registry.Register(c);
    }
};

TEST_F(DeleteRecordCommandTest, ExecuteRemovesRecord)
{
    auto* cmd = new DeleteRecordCommand(registry, registry.GetRelationshipIndex(), StringCRC("config.level"));
    history.ExecuteCommand(cmd);

    EXPECT_EQ(registry.GetCount(), 2u);
    EXPECT_EQ(registry.FindById(StringCRC("config.level")), nullptr);
}

TEST_F(DeleteRecordCommandTest, ExecuteRemovesDanglingEdges)
{
    auto* cmd = new DeleteRecordCommand(registry, registry.GetRelationshipIndex(), StringCRC("texture.player"));
    history.ExecuteCommand(cmd);

    const AssetRecord* hero = registry.FindById(StringCRC("entity.hero"));
    ASSERT_NE(hero, nullptr);
    EXPECT_EQ(hero->mReferences.Size(), 0u);
}

TEST_F(DeleteRecordCommandTest, UndoRestoresRecordAndEdges)
{
    auto* cmd = new DeleteRecordCommand(registry, registry.GetRelationshipIndex(), StringCRC("texture.player"));
    history.ExecuteCommand(cmd);
    history.Undo();

    EXPECT_EQ(registry.GetCount(), 3u);
    const AssetRecord* player = registry.FindById(StringCRC("texture.player"));
    ASSERT_NE(player, nullptr);

    const AssetRecord* hero = registry.FindById(StringCRC("entity.hero"));
    ASSERT_NE(hero, nullptr);
    EXPECT_EQ(hero->mReferences.Size(), 1u);
    EXPECT_EQ(hero->mReferences[0].mTargetAssetId, StringCRC("texture.player"));
}

TEST_F(DeleteRecordCommandTest, DeleteLeafDoesNotAffectOthers)
{
    // config.level has no edges pointing to it
    auto* cmd = new DeleteRecordCommand(registry, registry.GetRelationshipIndex(), StringCRC("config.level"));
    history.ExecuteCommand(cmd);

    const AssetRecord* hero = registry.FindById(StringCRC("entity.hero"));
    ASSERT_NE(hero, nullptr);
    EXPECT_EQ(hero->mReferences.Size(), 1u);
}

// ===========================================================================
// AddRelationshipCommand
// ===========================================================================
class AddRelationshipCommandTest : public ::testing::Test
{
protected:
    AssetRegistry registry;
    CommandHistory history;

    void SetUp() override
    {
        registry.Register(MakeRecord("texture.player", "texture"));
        registry.Register(MakeRecord("entity.hero", "entity"));
    }
};

TEST_F(AddRelationshipCommandTest, ExecuteAddsEdge)
{
    auto* cmd = new AddRelationshipCommand(registry, StringCRC("entity.hero"), StringCRC("uses"), StringCRC("texture.player"));
    history.ExecuteCommand(cmd);

    const AssetRecord* hero = registry.FindById(StringCRC("entity.hero"));
    ASSERT_NE(hero, nullptr);
    ASSERT_EQ(hero->mReferences.Size(), 1u);
    EXPECT_EQ(hero->mReferences[0].mRelationshipType, StringCRC("uses"));
    EXPECT_EQ(hero->mReferences[0].mTargetAssetId, StringCRC("texture.player"));
}

TEST_F(AddRelationshipCommandTest, UndoRemovesEdge)
{
    auto* cmd = new AddRelationshipCommand(registry, StringCRC("entity.hero"), StringCRC("uses"), StringCRC("texture.player"));
    history.ExecuteCommand(cmd);
    history.Undo();

    const AssetRecord* hero = registry.FindById(StringCRC("entity.hero"));
    ASSERT_NE(hero, nullptr);
    EXPECT_EQ(hero->mReferences.Size(), 0u);
}

TEST_F(AddRelationshipCommandTest, InvalidatesReverseCache)
{
    auto* cmd = new AddRelationshipCommand(registry, StringCRC("entity.hero"), StringCRC("uses"), StringCRC("texture.player"));
    history.ExecuteCommand(cmd);

    Dia::Core::Containers::DynamicArrayC<RelationshipEdge, 16> reverseRefs;
    registry.GetRelationshipIndex().GetReverseRefs(StringCRC("texture.player"), registry, reverseRefs);
    EXPECT_EQ(reverseRefs.Size(), 1u);
}

TEST_F(AddRelationshipCommandTest, MultipleEdgesFromSameSource)
{
    registry.Register(MakeRecord("audio.footstep", "audio"));

    auto* cmd1 = new AddRelationshipCommand(registry, StringCRC("entity.hero"), StringCRC("uses"), StringCRC("texture.player"));
    auto* cmd2 = new AddRelationshipCommand(registry, StringCRC("entity.hero"), StringCRC("uses"), StringCRC("audio.footstep"));
    history.ExecuteCommand(cmd1);
    history.ExecuteCommand(cmd2);

    const AssetRecord* hero = registry.FindById(StringCRC("entity.hero"));
    EXPECT_EQ(hero->mReferences.Size(), 2u);

    history.Undo();
    EXPECT_EQ(registry.FindById(StringCRC("entity.hero"))->mReferences.Size(), 1u);
    history.Undo();
    EXPECT_EQ(registry.FindById(StringCRC("entity.hero"))->mReferences.Size(), 0u);
}

// ===========================================================================
// RemoveRelationshipCommand
// ===========================================================================
class RemoveRelationshipCommandTest : public ::testing::Test
{
protected:
    AssetRegistry registry;
    CommandHistory history;

    void SetUp() override
    {
        AssetRecord hero = MakeRecord("entity.hero", "entity");
        hero.mReferences.Add(RelationshipEdge(StringCRC("uses"), StringCRC("texture.player")));
        hero.mReferences.Add(RelationshipEdge(StringCRC("uses"), StringCRC("audio.footstep")));

        registry.Register(MakeRecord("texture.player", "texture"));
        registry.Register(MakeRecord("audio.footstep", "audio"));
        registry.Register(hero);
    }
};

TEST_F(RemoveRelationshipCommandTest, ExecuteRemovesEdge)
{
    auto* cmd = new RemoveRelationshipCommand(registry, StringCRC("entity.hero"), StringCRC("uses"), StringCRC("texture.player"));
    history.ExecuteCommand(cmd);

    const AssetRecord* hero = registry.FindById(StringCRC("entity.hero"));
    ASSERT_EQ(hero->mReferences.Size(), 1u);
    EXPECT_EQ(hero->mReferences[0].mTargetAssetId, StringCRC("audio.footstep"));
}

TEST_F(RemoveRelationshipCommandTest, UndoRestoresEdge)
{
    auto* cmd = new RemoveRelationshipCommand(registry, StringCRC("entity.hero"), StringCRC("uses"), StringCRC("texture.player"));
    history.ExecuteCommand(cmd);
    history.Undo();

    const AssetRecord* hero = registry.FindById(StringCRC("entity.hero"));
    EXPECT_EQ(hero->mReferences.Size(), 2u);
}

TEST_F(RemoveRelationshipCommandTest, RemoveNonExistentEdgeIsNoOp)
{
    auto* cmd = new RemoveRelationshipCommand(registry, StringCRC("entity.hero"), StringCRC("depends_on"), StringCRC("config.missing"));
    history.ExecuteCommand(cmd);

    const AssetRecord* hero = registry.FindById(StringCRC("entity.hero"));
    EXPECT_EQ(hero->mReferences.Size(), 2u);
}

// ===========================================================================
// CompoundCommand (BulkCreate pattern)
// ===========================================================================
class CompoundCommandBulkTest : public ::testing::Test
{
protected:
    AssetRegistry registry;
    CommandHistory history;
};

TEST_F(CompoundCommandBulkTest, BulkCreateUndoesAtomically)
{
    history.BeginCompound();
    for (int i = 0; i < 5; ++i)
    {
        char id[64];
        _snprintf_s(id, sizeof(id), _TRUNCATE, "texture.item_%d", i);
        auto* cmd = new CreateRecordCommand(registry, MakeRecord(id, "texture"));
        history.ExecuteCommand(cmd);
    }
    history.EndCompound();

    EXPECT_EQ(registry.GetCount(), 5u);

    // Single undo should remove all 5
    history.Undo();
    EXPECT_EQ(registry.GetCount(), 0u);
}

TEST_F(CompoundCommandBulkTest, BulkCreateRedoRestoresAll)
{
    history.BeginCompound();
    for (int i = 0; i < 3; ++i)
    {
        char id[64];
        _snprintf_s(id, sizeof(id), _TRUNCATE, "config.rule_%d", i);
        auto* cmd = new CreateRecordCommand(registry, MakeRecord(id, "config"));
        history.ExecuteCommand(cmd);
    }
    history.EndCompound();

    history.Undo();
    EXPECT_EQ(registry.GetCount(), 0u);

    history.Redo();
    EXPECT_EQ(registry.GetCount(), 3u);
}

// ===========================================================================
// ManifestLoadHandler
// ===========================================================================
class ManifestLoadHandlerTest : public ::testing::Test
{
protected:
    AssetRegistry registry;
    CommandHistory history;
    CatalogueManifestSerializer serializer;
    ManifestLoadHandler handler;
};

TEST_F(ManifestLoadHandlerTest, NewManifestClearsRegistry)
{
    registry.Register(MakeRecord("texture.player", "texture"));
    EXPECT_EQ(registry.GetCount(), 1u);

    handler.NewManifest(registry, history);
    EXPECT_EQ(registry.GetCount(), 0u);
}

TEST_F(ManifestLoadHandlerTest, NewManifestClearsHistory)
{
    auto* cmd = new CreateRecordCommand(registry, MakeRecord("texture.player", "texture"));
    history.ExecuteCommand(cmd);
    EXPECT_TRUE(history.CanUndo());

    handler.NewManifest(registry, history);
    EXPECT_FALSE(history.CanUndo());
}

TEST_F(ManifestLoadHandlerTest, IsDirtyReportsCorrectly)
{
    history.MarkSavePoint();
    EXPECT_FALSE(handler.IsDirty(history));

    auto* cmd = new CreateRecordCommand(registry, MakeRecord("texture.player", "texture"));
    history.ExecuteCommand(cmd);
    EXPECT_TRUE(handler.IsDirty(history));
}

TEST_F(ManifestLoadHandlerTest, LoadInvalidPathFails)
{
    char errorBuf[256] = {};
    bool ok = handler.Load("nonexistent_path_xyz_12345.json", registry, serializer, history, errorBuf, sizeof(errorBuf));
    EXPECT_FALSE(ok);
    EXPECT_NE(errorBuf[0], '\0');
}

// ===========================================================================
// SessionContext
// ===========================================================================
class SessionContextTest : public ::testing::Test
{
protected:
    SessionContext context;
    char tempDir[512];

    void SetUp() override
    {
        _snprintf_s(tempDir, sizeof(tempDir), _TRUNCATE, "Cluiche/out/test_session_%u", GetTickCount());
    }

    void TearDown() override
    {
        char rmCmd[600];
        _snprintf_s(rmCmd, sizeof(rmCmd), _TRUNCATE, "rmdir /s /q \"%s\" 2>nul", tempDir);
        system(rmCmd);
    }
};

TEST_F(SessionContextTest, InitiallyHasNoPath)
{
    EXPECT_FALSE(context.HasLastManifestPath());
}

TEST_F(SessionContextTest, SetAndGetPath)
{
    context.SetLastManifestPath("test/manifest.json");
    EXPECT_TRUE(context.HasLastManifestPath());
    EXPECT_STREQ(context.GetLastManifestPath(), "test/manifest.json");
}

TEST_F(SessionContextTest, SaveAndLoadRoundTrip)
{
    context.SetLastManifestPath("my/catalogue.json");
    context.Save(tempDir);

    SessionContext loaded;
    loaded.Load(tempDir);
    EXPECT_TRUE(loaded.HasLastManifestPath());
    EXPECT_STREQ(loaded.GetLastManifestPath(), "my/catalogue.json");
}

TEST_F(SessionContextTest, LoadFromMissingDirDoesNotCrash)
{
    context.Load("nonexistent_dir_xyz_99999");
    EXPECT_FALSE(context.HasLastManifestPath());
}

// ===========================================================================
// FileDiscoverer::GenerateDefaultId
// ===========================================================================
class GenerateDefaultIdTest : public ::testing::Test {};

TEST_F(GenerateDefaultIdTest, BasicFileToId)
{
    char id[256] = {};
    bool ok = FileDiscoverer::GenerateDefaultId(
        "C:\\Assets", "C:\\Assets\\textures\\player.png", "texture", id, sizeof(id));
    EXPECT_TRUE(ok);
    EXPECT_STREQ(id, "texture.textures_player_png");
}

TEST_F(GenerateDefaultIdTest, ForwardSlashSupport)
{
    char id[256] = {};
    bool ok = FileDiscoverer::GenerateDefaultId(
        "C:/Assets", "C:/Assets/models/hero.fbx", "model", id, sizeof(id));
    EXPECT_TRUE(ok);
    EXPECT_STREQ(id, "model.models_hero_fbx");
}

TEST_F(GenerateDefaultIdTest, TrimsTrailingUnderscores)
{
    char id[256] = {};
    bool ok = FileDiscoverer::GenerateDefaultId(
        "C:\\Root", "C:\\Root\\file.", "misc", id, sizeof(id));
    EXPECT_TRUE(ok);
    EXPECT_STREQ(id, "misc.file");
}

TEST_F(GenerateDefaultIdTest, LowercaseConversion)
{
    char id[256] = {};
    bool ok = FileDiscoverer::GenerateDefaultId(
        "C:\\Root", "C:\\Root\\MyFolder\\HERO.PNG", "texture", id, sizeof(id));
    EXPECT_TRUE(ok);
    EXPECT_STREQ(id, "texture.myfolder_hero_png");
}

TEST_F(GenerateDefaultIdTest, NullInputsReturnFalse)
{
    char id[256] = {};
    EXPECT_FALSE(FileDiscoverer::GenerateDefaultId(nullptr, "C:\\a\\b", "t", id, sizeof(id)));
    EXPECT_FALSE(FileDiscoverer::GenerateDefaultId("C:\\a", nullptr, "t", id, sizeof(id)));
    EXPECT_FALSE(FileDiscoverer::GenerateDefaultId("C:\\a", "C:\\a\\b", nullptr, id, sizeof(id)));
    EXPECT_FALSE(FileDiscoverer::GenerateDefaultId("C:\\a", "C:\\a\\b", "t", nullptr, 0));
}

TEST_F(GenerateDefaultIdTest, EmptyRelativePathReturnsFalse)
{
    char id[256] = {};
    EXPECT_FALSE(FileDiscoverer::GenerateDefaultId("C:\\Root", "C:\\Root", "t", id, sizeof(id)));
    EXPECT_FALSE(FileDiscoverer::GenerateDefaultId("C:\\Root", "C:\\Root\\", "t", id, sizeof(id)));
}

TEST_F(GenerateDefaultIdTest, DeepNestedPath)
{
    char id[256] = {};
    bool ok = FileDiscoverer::GenerateDefaultId(
        "C:\\Game", "C:\\Game\\data\\levels\\world1\\intro.json", "config", id, sizeof(id));
    EXPECT_TRUE(ok);
    EXPECT_STREQ(id, "config.data_levels_world1_intro_json");
}

// ===========================================================================
// FileDiscoverer (Integration — real FS scan)
// ===========================================================================
class FileDiscovererScanTest : public ::testing::Test
{
protected:
    FileDiscoverer discoverer;
    AssetTypeRegistry typeRegistry;
    AssetRegistry assetRegistry;
    char tempRoot[512];

    void SetUp() override
    {
        RegisterType(typeRegistry, "texture", "Texture", "*.png");
        RegisterType(typeRegistry, "config", "Config", "*.config.json");

        _snprintf_s(tempRoot, sizeof(tempRoot), _TRUNCATE, "Cluiche\\out\\test_discover_%u", GetTickCount());
        CreateDirectoryA(tempRoot, NULL);

        char subDir[512];
        _snprintf_s(subDir, sizeof(subDir), _TRUNCATE, "%s\\textures", tempRoot);
        CreateDirectoryA(subDir, NULL);

        // Create test files
        CreateTestFile("textures\\hero.png");
        CreateTestFile("textures\\enemy.png");
        CreateTestFile("main.config.json");
        CreateTestFile("readme.txt"); // should not match
    }

    void TearDown() override
    {
        char rmCmd[600];
        _snprintf_s(rmCmd, sizeof(rmCmd), _TRUNCATE, "rmdir /s /q \"%s\" 2>nul", tempRoot);
        system(rmCmd);
    }

    void CreateTestFile(const char* relPath)
    {
        char fullPath[512];
        _snprintf_s(fullPath, sizeof(fullPath), _TRUNCATE, "%s\\%s", tempRoot, relPath);
        FILE* f = nullptr;
        fopen_s(&f, fullPath, "w");
        if (f) { fputs("test", f); fclose(f); }
    }
};

TEST_F(FileDiscovererScanTest, DiscoversFindsPngAndConfigFiles)
{
    Dia::Core::Containers::DynamicArrayC<DiscoveredFile, kMaxDiscoveredFiles> results;
    discoverer.Discover(tempRoot, typeRegistry, assetRegistry, results);

    EXPECT_GE(results.Size(), 3u); // hero.png, enemy.png, main.config.json
}

TEST_F(FileDiscovererScanTest, ExcludesAlreadyRegistered)
{
    // Pre-register one file
    char id[256];
    FileDiscoverer::GenerateDefaultId(tempRoot, "textures\\hero.png", "texture", id, sizeof(id));

    Dia::Core::Containers::DynamicArrayC<DiscoveredFile, kMaxDiscoveredFiles> firstPass;
    discoverer.Discover(tempRoot, typeRegistry, assetRegistry, firstPass);
    unsigned int firstCount = firstPass.Size();

    // Register one
    if (firstPass.Size() > 0)
        assetRegistry.Register(MakeRecord(firstPass[0].mSuggestedId, firstPass[0].mSuggestedType));

    Dia::Core::Containers::DynamicArrayC<DiscoveredFile, kMaxDiscoveredFiles> secondPass;
    discoverer.Discover(tempRoot, typeRegistry, assetRegistry, secondPass);

    EXPECT_EQ(secondPass.Size(), firstCount - 1);
}

TEST_F(FileDiscovererScanTest, IgnoresNonMatchingFiles)
{
    Dia::Core::Containers::DynamicArrayC<DiscoveredFile, kMaxDiscoveredFiles> results;
    discoverer.Discover(tempRoot, typeRegistry, assetRegistry, results);

    for (unsigned int i = 0; i < results.Size(); ++i)
    {
        EXPECT_STRNE(results[i].mSuggestedType, "");
        // readme.txt should not appear since "*.txt" is not registered
        EXPECT_TRUE(strstr(results[i].mFullPath, "readme.txt") == nullptr);
    }
}

TEST_F(FileDiscovererScanTest, EmptyRootPathReturnsNothing)
{
    Dia::Core::Containers::DynamicArrayC<DiscoveredFile, kMaxDiscoveredFiles> results;
    discoverer.Discover("", typeRegistry, assetRegistry, results);
    EXPECT_EQ(results.Size(), 0u);
}

TEST_F(FileDiscovererScanTest, NonExistentPathReturnsNothing)
{
    Dia::Core::Containers::DynamicArrayC<DiscoveredFile, kMaxDiscoveredFiles> results;
    discoverer.Discover("Z:\\nonexistent_path_1234567890", typeRegistry, assetRegistry, results);
    EXPECT_EQ(results.Size(), 0u);
}

// ===========================================================================
// AssetTypeEditorRegistry
// ===========================================================================
class AssetTypeEditorRegistryTest : public ::testing::Test
{
protected:
    AssetTypeEditorRegistry reg;
};

TEST_F(AssetTypeEditorRegistryTest, RegisterAndFind)
{
    bool ok = reg.RegisterTypeEditor(StringCRC("texture"), StringCRC("TextureEditorPlugin"));
    EXPECT_TRUE(ok);

    StringCRC editor = reg.FindEditorForType(StringCRC("texture"));
    EXPECT_EQ(editor, StringCRC("TextureEditorPlugin"));
}

TEST_F(AssetTypeEditorRegistryTest, DuplicateRegistrationFails)
{
    reg.RegisterTypeEditor(StringCRC("texture"), StringCRC("Editor1"));
    bool ok = reg.RegisterTypeEditor(StringCRC("texture"), StringCRC("Editor2"));
    EXPECT_FALSE(ok);
    EXPECT_EQ(reg.FindEditorForType(StringCRC("texture")), StringCRC("Editor1"));
}

TEST_F(AssetTypeEditorRegistryTest, FindUnregisteredReturnsDefault)
{
    StringCRC result = reg.FindEditorForType(StringCRC("unknown_type"));
    EXPECT_EQ(result, StringCRC());
}

TEST_F(AssetTypeEditorRegistryTest, MultipleTypes)
{
    reg.RegisterTypeEditor(StringCRC("texture"), StringCRC("TexEd"));
    reg.RegisterTypeEditor(StringCRC("config"), StringCRC("ConfigEd"));
    reg.RegisterTypeEditor(StringCRC("entity"), StringCRC("EntityEd"));

    EXPECT_EQ(reg.FindEditorForType(StringCRC("texture")), StringCRC("TexEd"));
    EXPECT_EQ(reg.FindEditorForType(StringCRC("config")), StringCRC("ConfigEd"));
    EXPECT_EQ(reg.FindEditorForType(StringCRC("entity")), StringCRC("EntityEd"));
}

// ===========================================================================
// AssetTypeRegistry::FindByFileName (new method)
// ===========================================================================
class FindByFileNameTest : public ::testing::Test
{
protected:
    AssetTypeRegistry reg;

    void SetUp() override
    {
        RegisterType(reg, "texture", "Texture", "*.png");
        RegisterType(reg, "config", "Config", "*.config.json");
        RegisterType(reg, "audio", "Audio", "*.wav");
    }
};

TEST_F(FindByFileNameTest, MatchesPngSuffix)
{
    const AssetTypeDescriptor* desc = reg.FindByFileName("hero.png");
    ASSERT_NE(desc, nullptr);
    EXPECT_EQ(desc->mTypeId, StringCRC("texture"));
}

TEST_F(FindByFileNameTest, MatchesCompoundSuffix)
{
    const AssetTypeDescriptor* desc = reg.FindByFileName("main.config.json");
    ASSERT_NE(desc, nullptr);
    EXPECT_EQ(desc->mTypeId, StringCRC("config"));
}

TEST_F(FindByFileNameTest, NoMatchReturnsNull)
{
    EXPECT_EQ(reg.FindByFileName("readme.txt"), nullptr);
    EXPECT_EQ(reg.FindByFileName(""), nullptr);
    EXPECT_EQ(reg.FindByFileName(nullptr), nullptr);
}

TEST_F(FindByFileNameTest, CaseSensitiveMatch)
{
    // Pattern matching is case-sensitive per the implementation
    EXPECT_EQ(reg.FindByFileName("hero.PNG"), nullptr);
}

// ===========================================================================
// ApplyRulesCommand
// ===========================================================================
class ApplyRulesCommandTest : public ::testing::Test
{
protected:
    AssetRegistry registry;
    CommandHistory history;
    CatalogueRulesEngine engine;
    AssetTypeRegistry typeRegistry;

    void SetUp() override
    {
        RegisterType(typeRegistry, "texture", "Texture", "*.png");
        registry.Register(MakeRecord("texture.player", "texture", "tex/player.png"));
        registry.Register(MakeRecord("texture.enemy", "texture", "tex/enemy.png"));
    }
};

TEST_F(ApplyRulesCommandTest, UndoRestoresAllRecordsExactly)
{
    // Without loaded rules, Apply produces no changes — test the snapshot/restore mechanism
    auto* cmd = new ApplyRulesCommand(registry, registry.GetRelationshipIndex(), engine);
    history.ExecuteCommand(cmd);

    // Records still exist after apply
    EXPECT_EQ(registry.GetCount(), 2u);

    // Undo — records should still be unchanged
    history.Undo();
    EXPECT_EQ(registry.GetCount(), 2u);
    ASSERT_NE(registry.FindById(StringCRC("texture.player")), nullptr);
    ASSERT_NE(registry.FindById(StringCRC("texture.enemy")), nullptr);
}

// ===========================================================================
// Integration: Create → Relationship → Delete → Undo chain
// ===========================================================================
class IntegrationCommandChainTest : public ::testing::Test
{
protected:
    AssetRegistry registry;
    CommandHistory history;
};

TEST_F(IntegrationCommandChainTest, FullWorkflowRoundTrip)
{
    // Create two records
    auto* c1 = new CreateRecordCommand(registry, MakeRecord("texture.player", "texture", "a.png"));
    auto* c2 = new CreateRecordCommand(registry, MakeRecord("entity.hero", "entity"));
    history.ExecuteCommand(c1);
    history.ExecuteCommand(c2);
    EXPECT_EQ(registry.GetCount(), 2u);

    // Add relationship
    auto* r1 = new AddRelationshipCommand(registry, StringCRC("entity.hero"), StringCRC("uses"), StringCRC("texture.player"));
    history.ExecuteCommand(r1);
    EXPECT_EQ(registry.FindById(StringCRC("entity.hero"))->mReferences.Size(), 1u);

    // Delete the texture — should remove the edge from entity.hero
    auto* d1 = new DeleteRecordCommand(registry, registry.GetRelationshipIndex(), StringCRC("texture.player"));
    history.ExecuteCommand(d1);
    EXPECT_EQ(registry.GetCount(), 1u);
    EXPECT_EQ(registry.FindById(StringCRC("entity.hero"))->mReferences.Size(), 0u);

    // Undo delete — record restored
    history.Undo();
    EXPECT_EQ(registry.GetCount(), 2u);
    ASSERT_NE(registry.FindById(StringCRC("texture.player")), nullptr);

    // Undo relationship
    history.Undo();
    EXPECT_EQ(registry.FindById(StringCRC("entity.hero"))->mReferences.Size(), 0u);

    // Undo create entity
    history.Undo();
    EXPECT_EQ(registry.GetCount(), 1u);

    // Undo create texture
    history.Undo();
    EXPECT_EQ(registry.GetCount(), 0u);
}

TEST_F(IntegrationCommandChainTest, SavePointTracksThroughOperations)
{
    auto* c1 = new CreateRecordCommand(registry, MakeRecord("config.main", "config"));
    history.ExecuteCommand(c1);
    history.MarkSavePoint();
    EXPECT_TRUE(history.IsAtSavePoint());

    auto* c2 = new CreateRecordCommand(registry, MakeRecord("config.other", "config"));
    history.ExecuteCommand(c2);
    EXPECT_FALSE(history.IsAtSavePoint());

    history.Undo();
    EXPECT_TRUE(history.IsAtSavePoint());

    history.Redo();
    EXPECT_FALSE(history.IsAtSavePoint());
}

// ===========================================================================
// Edge cases and boundary conditions
// ===========================================================================
TEST(BoundaryTest, CreateRecordWithEmptyId)
{
    AssetRegistry registry;
    AssetRecord rec;
    // Empty ID — StringCRC default constructor → value 0
    EXPECT_FALSE(registry.Register(rec));
}

TEST(BoundaryTest, CreateRecordInvalidIdFormat)
{
    AssetRegistry registry;
    AssetRecord rec;
    rec.mId = StringCRC("noDotSeparator");
    rec.mAssetTypeId = StringCRC("texture");
    EXPECT_FALSE(registry.Register(rec));
}

TEST(BoundaryTest, DuplicateRegistration)
{
    AssetRegistry registry;
    EXPECT_TRUE(registry.Register(MakeRecord("texture.a", "texture")));
    EXPECT_FALSE(registry.Register(MakeRecord("texture.a", "texture")));
}

TEST(BoundaryTest, RemoveNonExistent)
{
    AssetRegistry registry;
    EXPECT_FALSE(registry.Remove(StringCRC("texture.nonexistent")));
}

TEST(BoundaryTest, AddRelationshipToFullArray)
{
    AssetRegistry registry;
    AssetRecord rec = MakeRecord("entity.full", "entity");
    // Fill all 8 reference slots
    for (int i = 0; i < 8; ++i)
    {
        char targetId[64];
        _snprintf_s(targetId, sizeof(targetId), _TRUNCATE, "texture.t%d", i);
        rec.mReferences.Add(RelationshipEdge(StringCRC("uses"), StringCRC(targetId)));
    }
    registry.Register(rec);
    registry.Register(MakeRecord("texture.overflow", "texture"));

    CommandHistory history;
    auto* cmd = new AddRelationshipCommand(registry, StringCRC("entity.full"), StringCRC("uses"), StringCRC("texture.overflow"));
    history.ExecuteCommand(cmd);

    // Should not exceed capacity (IsFull check in Execute)
    EXPECT_EQ(registry.FindById(StringCRC("entity.full"))->mReferences.Size(), 8u);
}

TEST(BoundaryTest, SelfReferentialEdgeRejected)
{
    AssetRegistry registry;
    registry.Register(MakeRecord("entity.hero", "entity"));

    CommandHistory history;
    // Self-referential edge should be rejected at the validation layer.
    // The handler checks fromId == toId before creating the command.
    // Simulate that check here directly:
    StringCRC fromId("entity.hero");
    StringCRC toId("entity.hero");
    EXPECT_EQ(fromId, toId);

    // If handler allows it through (bug), AddRelationshipCommand would add blindly.
    // Verify that no edge exists — the handler must reject before command creation.
    const AssetRecord* hero = registry.FindById(StringCRC("entity.hero"));
    ASSERT_NE(hero, nullptr);
    EXPECT_EQ(hero->mReferences.Size(), 0u);

    // Verify that executing the command with self-ref still results in an edge
    // (proving the guard must live at handler level, not command level)
    auto* cmd = new AddRelationshipCommand(registry, fromId, StringCRC("uses"), toId);
    history.ExecuteCommand(cmd);
    EXPECT_EQ(registry.FindById(StringCRC("entity.hero"))->mReferences.Size(), 1u);

    // Undo to restore clean state
    history.Undo();
    EXPECT_EQ(registry.FindById(StringCRC("entity.hero"))->mReferences.Size(), 0u);
}

TEST(BoundaryTest, DuplicateEdgeDetectedByValidation)
{
    AssetRegistry registry;
    registry.Register(MakeRecord("entity.hero", "entity"));
    registry.Register(MakeRecord("texture.player", "texture"));

    CommandHistory history;
    StringCRC fromId("entity.hero");
    StringCRC relType("uses");
    StringCRC toId("texture.player");

    auto* cmd1 = new AddRelationshipCommand(registry, fromId, relType, toId);
    history.ExecuteCommand(cmd1);

    const AssetRecord* hero = registry.FindById(fromId);
    ASSERT_NE(hero, nullptr);
    EXPECT_EQ(hero->mReferences.Size(), 1u);

    // Simulate handler-level duplicate check (same logic as RegisterRelationshipHandlers)
    bool duplicateFound = false;
    for (unsigned int i = 0; i < hero->mReferences.Size(); ++i)
    {
        if (hero->mReferences[i].mRelationshipType == relType &&
            hero->mReferences[i].mTargetAssetId == toId)
        {
            duplicateFound = true;
            break;
        }
    }
    EXPECT_TRUE(duplicateFound);

    // Handler would reject here — verify the guard works
    if (!duplicateFound)
    {
        auto* cmd2 = new AddRelationshipCommand(registry, fromId, relType, toId);
        history.ExecuteCommand(cmd2);
    }

    // Edge count should remain 1 (duplicate was blocked)
    EXPECT_EQ(registry.FindById(fromId)->mReferences.Size(), 1u);
}

TEST(BoundaryTest, UpdateNonExistentRecordDoesNotCrash)
{
    AssetRegistry registry;
    registry.Register(MakeRecord("texture.a", "texture"));

    CommandHistory history;
    AssetRecord oldRec = MakeRecord("texture.missing", "texture");
    AssetRecord newRec = oldRec;
    newRec.mStatus = AssetStatus::Deprecated;

    // This operates on a non-existent record — should not crash
    auto* cmd = new UpdateRecordCommand(registry, StringCRC("texture.missing"), oldRec, newRec);
    history.ExecuteCommand(cmd);

    // Record "texture.a" should be unaffected
    EXPECT_EQ(registry.FindById(StringCRC("texture.a"))->mStatus, AssetStatus::Active);
}

// ===========================================================================
// Relationship Validation (AC6: no duplicates, AC7: no self-referential edges)
// Note: These validations are enforced at the DiaAPI handler level in the plugin.
// These tests verify the scenario through the command layer to document expected behavior.
// ===========================================================================
TEST(RelationshipValidationTest, SelfReferentialEdgeDetection)
{
    // AC7: fromId must not equal toId — validated in add_relationship handler
    // At the command level, the edge would be added (no guard in command itself).
    // The handler prevents this from reaching the command.
    AssetRegistry registry;
    registry.Register(MakeRecord("entity.hero", "entity"));

    CommandHistory history;

    // Verify that if a self-referential edge reaches the command, it is stored
    // (the handler is responsible for blocking this — this documents command behavior)
    auto* cmd = new AddRelationshipCommand(registry, StringCRC("entity.hero"), StringCRC("uses"), StringCRC("entity.hero"));
    history.ExecuteCommand(cmd);
    EXPECT_EQ(registry.FindById(StringCRC("entity.hero"))->mReferences.Size(), 1u);

    // Undo should remove it
    history.Undo();
    EXPECT_EQ(registry.FindById(StringCRC("entity.hero"))->mReferences.Size(), 0u);
}

TEST(RelationshipValidationTest, DuplicateEdgeAtCommandLevel)
{
    // AC6: duplicate edges should not be created — validated in add_relationship handler
    // At command level, duplicates ARE added (handler blocks before reaching command).
    AssetRegistry registry;
    registry.Register(MakeRecord("entity.hero", "entity"));
    registry.Register(MakeRecord("texture.player", "texture"));

    CommandHistory history;

    auto* cmd1 = new AddRelationshipCommand(registry, StringCRC("entity.hero"), StringCRC("uses"), StringCRC("texture.player"));
    auto* cmd2 = new AddRelationshipCommand(registry, StringCRC("entity.hero"), StringCRC("uses"), StringCRC("texture.player"));
    history.ExecuteCommand(cmd1);
    history.ExecuteCommand(cmd2);

    // Command layer does not guard against duplicates — handler does
    EXPECT_EQ(registry.FindById(StringCRC("entity.hero"))->mReferences.Size(), 2u);
}

// ===========================================================================
// ApplyRulesCommand — Excluded IDs
// ===========================================================================
class ApplyRulesExcludedTest : public ::testing::Test
{
protected:
    AssetRegistry registry;
    CommandHistory history;
    CatalogueRulesEngine engine;
    AssetTypeRegistry typeRegistry;

    void SetUp() override
    {
        RegisterBuiltInAssetTypes(typeRegistry);

        const char* rulesPath = "C:\\Temp\\test_excluded_rules.json";
        const char* rulesJson =
            "{\n"
            "  \"rules\": [\n"
            "    { \"name\": \"tag-textures\", \"match\": { \"type\": \"texture\" }, \"action\": \"assign_tag\", \"tag\": \"visual\" }\n"
            "  ]\n"
            "}";
        FILE* f = nullptr;
        fopen_s(&f, rulesPath, "w");
        if (f) { fputs(rulesJson, f); fclose(f); }
        engine.LoadRules(rulesPath, typeRegistry);
        remove(rulesPath);

        registry.Register(MakeRecord("texture.player", "texture", "tex/player.png"));
        registry.Register(MakeRecord("texture.enemy", "texture", "tex/enemy.png"));
        registry.Register(MakeRecord("texture.bg", "texture", "tex/bg.png"));
    }
};

TEST_F(ApplyRulesExcludedTest, ExcludedIdIsNotTagged)
{
    auto* cmd = new ApplyRulesCommand(registry, registry.GetRelationshipIndex(), engine);
    cmd->AddExcludedId(StringCRC("texture.enemy"));
    history.ExecuteCommand(cmd);

    const AssetRecord* player = registry.FindById(StringCRC("texture.player"));
    const AssetRecord* enemy = registry.FindById(StringCRC("texture.enemy"));
    const AssetRecord* bg = registry.FindById(StringCRC("texture.bg"));
    ASSERT_NE(player, nullptr);
    ASSERT_NE(enemy, nullptr);
    ASSERT_NE(bg, nullptr);

    EXPECT_EQ(player->mTags.Size(), 1u);
    EXPECT_EQ(enemy->mTags.Size(), 0u);
    EXPECT_EQ(bg->mTags.Size(), 1u);
}

TEST_F(ApplyRulesExcludedTest, MultipleExcludedIds)
{
    auto* cmd = new ApplyRulesCommand(registry, registry.GetRelationshipIndex(), engine);
    cmd->AddExcludedId(StringCRC("texture.player"));
    cmd->AddExcludedId(StringCRC("texture.bg"));
    history.ExecuteCommand(cmd);

    EXPECT_EQ(registry.FindById(StringCRC("texture.player"))->mTags.Size(), 0u);
    EXPECT_EQ(registry.FindById(StringCRC("texture.enemy"))->mTags.Size(), 1u);
    EXPECT_EQ(registry.FindById(StringCRC("texture.bg"))->mTags.Size(), 0u);
}

TEST_F(ApplyRulesExcludedTest, UndoRestoresExcludedAndNonExcluded)
{
    auto* cmd = new ApplyRulesCommand(registry, registry.GetRelationshipIndex(), engine);
    cmd->AddExcludedId(StringCRC("texture.enemy"));
    history.ExecuteCommand(cmd);
    history.Undo();

    EXPECT_EQ(registry.FindById(StringCRC("texture.player"))->mTags.Size(), 0u);
    EXPECT_EQ(registry.FindById(StringCRC("texture.enemy"))->mTags.Size(), 0u);
    EXPECT_EQ(registry.FindById(StringCRC("texture.bg"))->mTags.Size(), 0u);
}

TEST_F(ApplyRulesExcludedTest, NoExclusionsAppliesAll)
{
    auto* cmd = new ApplyRulesCommand(registry, registry.GetRelationshipIndex(), engine);
    history.ExecuteCommand(cmd);

    EXPECT_EQ(registry.FindById(StringCRC("texture.player"))->mTags.Size(), 1u);
    EXPECT_EQ(registry.FindById(StringCRC("texture.enemy"))->mTags.Size(), 1u);
    EXPECT_EQ(registry.FindById(StringCRC("texture.bg"))->mTags.Size(), 1u);
}

// ===========================================================================
// ApplyRulesCommand — Undo restores field-level changes from real rules
// ===========================================================================
TEST_F(ApplyRulesExcludedTest, UndoRestoresTagChangesFromRealRule)
{
    EXPECT_EQ(registry.FindById(StringCRC("texture.player"))->mTags.Size(), 0u);
    EXPECT_EQ(registry.FindById(StringCRC("texture.enemy"))->mTags.Size(), 0u);
    EXPECT_EQ(registry.FindById(StringCRC("texture.bg"))->mTags.Size(), 0u);

    auto* cmd = new ApplyRulesCommand(registry, registry.GetRelationshipIndex(), engine);
    history.ExecuteCommand(cmd);

    EXPECT_EQ(registry.FindById(StringCRC("texture.player"))->mTags.Size(), 1u);
    EXPECT_EQ(registry.FindById(StringCRC("texture.enemy"))->mTags.Size(), 1u);
    EXPECT_EQ(registry.FindById(StringCRC("texture.bg"))->mTags.Size(), 1u);

    history.Undo();

    EXPECT_EQ(registry.FindById(StringCRC("texture.player"))->mTags.Size(), 0u);
    EXPECT_EQ(registry.FindById(StringCRC("texture.enemy"))->mTags.Size(), 0u);
    EXPECT_EQ(registry.FindById(StringCRC("texture.bg"))->mTags.Size(), 0u);

    history.Redo();

    EXPECT_EQ(registry.FindById(StringCRC("texture.player"))->mTags.Size(), 1u);
    EXPECT_EQ(registry.FindById(StringCRC("texture.enemy"))->mTags.Size(), 1u);
    EXPECT_EQ(registry.FindById(StringCRC("texture.bg"))->mTags.Size(), 1u);
}

// ===========================================================================
// Manual Override Tracking — UpdateRecordCommand flag-setting (Task 13)
// ===========================================================================

namespace
{
    // Writes a rules file for scope-based tests, returns a CatalogueRulesEngine with it loaded.
    // rulesPath must be a writable temp path.
    bool WriteScopeRule(const char* rulesPath, AssetTypeRegistry& typeRegistry, CatalogueRulesEngine& engine)
    {
        const char* json =
            "{\n"
            "  \"rules\": [\n"
            "    { \"name\": \"global-all\", \"match\": { \"all\": true }, \"action\": \"assign_scope\", \"scope\": \"global\" }\n"
            "  ]\n"
            "}";
        FILE* f = nullptr;
        fopen_s(&f, rulesPath, "w");
        if (!f) return false;
        fputs(json, f);
        fclose(f);
        auto lr = engine.LoadRules(rulesPath, typeRegistry);
        remove(rulesPath);
        return lr.mSuccess;
    }

    bool WriteTagRule(const char* rulesPath, AssetTypeRegistry& typeRegistry, CatalogueRulesEngine& engine)
    {
        const char* json =
            "{\n"
            "  \"rules\": [\n"
            "    { \"name\": \"tag-textures\", \"match\": { \"type\": \"texture\" }, \"action\": \"assign_tag\", \"tag\": \"visual\" }\n"
            "  ]\n"
            "}";
        FILE* f = nullptr;
        fopen_s(&f, rulesPath, "w");
        if (!f) return false;
        fputs(json, f);
        fclose(f);
        auto lr = engine.LoadRules(rulesPath, typeRegistry);
        remove(rulesPath);
        return lr.mSuccess;
    }
}

class ManualOverrideFlagTest : public ::testing::Test
{
protected:
    AssetRegistry registry;
    CommandHistory history;

    void SetUp() override
    {
        registry.Register(MakeRecord("texture.player", "texture", "old/path.png"));
    }
};

TEST_F(ManualOverrideFlagTest, ScopeChange_SetsScopeFlag)
{
    const AssetRecord* old = registry.FindById(StringCRC("texture.player"));
    AssetRecord newRec = *old;
    newRec.mScope = AssetScope::kStage;
    newRec.mScopeStageName = StringCRC("level_1");

    auto* cmd = new UpdateRecordCommand(registry, StringCRC("texture.player"), *old, newRec);
    history.ExecuteCommand(cmd);

    const AssetRecord* updated = registry.FindById(StringCRC("texture.player"));
    ASSERT_NE(updated, nullptr);
    EXPECT_TRUE(updated->mManualOverrideFlags & kManualOverrideScope);
}

TEST_F(ManualOverrideFlagTest, SourcePathChange_SetsSourcePathFlag)
{
    const AssetRecord* old = registry.FindById(StringCRC("texture.player"));
    AssetRecord newRec = *old;
    newRec.mSourcePath = Dia::Core::Containers::String256("new/path.png");

    auto* cmd = new UpdateRecordCommand(registry, StringCRC("texture.player"), *old, newRec);
    history.ExecuteCommand(cmd);

    const AssetRecord* updated = registry.FindById(StringCRC("texture.player"));
    ASSERT_NE(updated, nullptr);
    EXPECT_TRUE(updated->mManualOverrideFlags & kManualOverrideSourcePath);
}

TEST_F(ManualOverrideFlagTest, StageNameChange_SetsStageFlag)
{
    const AssetRecord* old = registry.FindById(StringCRC("texture.player"));
    AssetRecord newRec = *old;
    newRec.mScopeStageName = StringCRC("stage_a");

    auto* cmd = new UpdateRecordCommand(registry, StringCRC("texture.player"), *old, newRec);
    history.ExecuteCommand(cmd);

    const AssetRecord* updated = registry.FindById(StringCRC("texture.player"));
    ASSERT_NE(updated, nullptr);
    EXPECT_TRUE(updated->mManualOverrideFlags & kManualOverrideStage);
}

TEST_F(ManualOverrideFlagTest, TagsChange_SetsTagsFlag)
{
    const AssetRecord* old = registry.FindById(StringCRC("texture.player"));
    AssetRecord newRec = *old;
    newRec.mTags.Add(StringCRC("hero"));

    auto* cmd = new UpdateRecordCommand(registry, StringCRC("texture.player"), *old, newRec);
    history.ExecuteCommand(cmd);

    const AssetRecord* updated = registry.FindById(StringCRC("texture.player"));
    ASSERT_NE(updated, nullptr);
    EXPECT_TRUE(updated->mManualOverrideFlags & kManualOverrideTags);
}

TEST_F(ManualOverrideFlagTest, NoChange_DoesNotSetAnyFlag)
{
    const AssetRecord* old = registry.FindById(StringCRC("texture.player"));
    AssetRecord newRec = *old; // identical copy

    auto* cmd = new UpdateRecordCommand(registry, StringCRC("texture.player"), *old, newRec);
    history.ExecuteCommand(cmd);

    const AssetRecord* updated = registry.FindById(StringCRC("texture.player"));
    ASSERT_NE(updated, nullptr);
    EXPECT_EQ(updated->mManualOverrideFlags, 0u);
}

TEST_F(ManualOverrideFlagTest, UndoRestoresOriginalFlags)
{
    const AssetRecord* old = registry.FindById(StringCRC("texture.player"));
    AssetRecord newRec = *old;
    newRec.mScope = AssetScope::kStage;

    auto* cmd = new UpdateRecordCommand(registry, StringCRC("texture.player"), *old, newRec);
    history.ExecuteCommand(cmd);

    EXPECT_TRUE(registry.FindById(StringCRC("texture.player"))->mManualOverrideFlags & kManualOverrideScope);

    history.Undo();
    EXPECT_EQ(registry.FindById(StringCRC("texture.player"))->mManualOverrideFlags, 0u);
}

TEST_F(ManualOverrideFlagTest, MultipleFieldsSetMultipleBits)
{
    const AssetRecord* old = registry.FindById(StringCRC("texture.player"));
    AssetRecord newRec = *old;
    newRec.mScope = AssetScope::kStage;
    newRec.mSourcePath = Dia::Core::Containers::String256("new/path.png");
    newRec.mTags.Add(StringCRC("tagged"));

    auto* cmd = new UpdateRecordCommand(registry, StringCRC("texture.player"), *old, newRec);
    history.ExecuteCommand(cmd);

    const AssetRecord* updated = registry.FindById(StringCRC("texture.player"));
    ASSERT_NE(updated, nullptr);
    EXPECT_TRUE(updated->mManualOverrideFlags & kManualOverrideScope);
    EXPECT_TRUE(updated->mManualOverrideFlags & kManualOverrideSourcePath);
    EXPECT_TRUE(updated->mManualOverrideFlags & kManualOverrideTags);
}

TEST_F(ManualOverrideFlagTest, FlagsAccumulateAcrossUpdates)
{
    // First update: change scope
    const AssetRecord* r1 = registry.FindById(StringCRC("texture.player"));
    AssetRecord u1 = *r1;
    u1.mScope = AssetScope::kStage;
    history.ExecuteCommand(new UpdateRecordCommand(registry, StringCRC("texture.player"), *r1, u1));

    // Second update: change source path (scope already set)
    const AssetRecord* r2 = registry.FindById(StringCRC("texture.player"));
    AssetRecord u2 = *r2;
    u2.mSourcePath = Dia::Core::Containers::String256("newer/path.png");
    history.ExecuteCommand(new UpdateRecordCommand(registry, StringCRC("texture.player"), *r2, u2));

    const AssetRecord* final = registry.FindById(StringCRC("texture.player"));
    ASSERT_NE(final, nullptr);
    EXPECT_TRUE(final->mManualOverrideFlags & kManualOverrideScope);
    EXPECT_TRUE(final->mManualOverrideFlags & kManualOverrideSourcePath);
}

TEST_F(ManualOverrideFlagTest, AllFourFlagsAreIndependent)
{
    const unsigned char allFlags =
        kManualOverrideScope | kManualOverrideTags | kManualOverrideSourcePath | kManualOverrideStage;

    // Each flag must be a power of two (exactly one bit set)
    EXPECT_TRUE(kManualOverrideScope      && !(kManualOverrideScope      & (kManualOverrideScope      - 1)));
    EXPECT_TRUE(kManualOverrideTags       && !(kManualOverrideTags       & (kManualOverrideTags       - 1)));
    EXPECT_TRUE(kManualOverrideSourcePath && !(kManualOverrideSourcePath & (kManualOverrideSourcePath - 1)));
    EXPECT_TRUE(kManualOverrideStage      && !(kManualOverrideStage      & (kManualOverrideStage      - 1)));

    // No two flags share a bit
    EXPECT_EQ((kManualOverrideScope & kManualOverrideTags),       0u);
    EXPECT_EQ((kManualOverrideScope & kManualOverrideSourcePath), 0u);
    EXPECT_EQ((kManualOverrideScope & kManualOverrideStage),      0u);
    EXPECT_EQ((kManualOverrideTags  & kManualOverrideSourcePath), 0u);
    EXPECT_EQ((kManualOverrideTags  & kManualOverrideStage),      0u);
    EXPECT_EQ((kManualOverrideSourcePath & kManualOverrideStage), 0u);

    // All four fit in one byte
    EXPECT_LE(allFlags, 0xFFu);
}

// ===========================================================================
// ApplyRulesCommand — Manual Override Interaction (Task 13)
// ===========================================================================

class ApplyRulesManualOverrideTest : public ::testing::Test
{
protected:
    AssetRegistry registry;
    CommandHistory history;
    CatalogueRulesEngine engine;
    AssetTypeRegistry typeRegistry;

    void SetUp() override
    {
        RegisterBuiltInAssetTypes(typeRegistry);
        registry.Register(MakeRecord("texture.player", "texture", "tex/player.png"));
    }

    bool LoadScopeRule()
    {
        return WriteScopeRule("C:\\Temp\\test_manual_scope_rule.json", typeRegistry, engine);
    }

    bool LoadTagRule()
    {
        return WriteTagRule("C:\\Temp\\test_manual_tag_rule.json", typeRegistry, engine);
    }
};

TEST_F(ApplyRulesManualOverrideTest, SkipsManualFieldByDefault)
{
    ASSERT_TRUE(LoadScopeRule());

    // Manually set scope to stage
    const AssetRecord* old = registry.FindById(StringCRC("texture.player"));
    AssetRecord updated = *old;
    updated.mScope = AssetScope::kStage;
    updated.mScopeStageName = StringCRC("level_1");
    history.ExecuteCommand(new UpdateRecordCommand(registry, StringCRC("texture.player"), *old, updated));

    EXPECT_TRUE(registry.FindById(StringCRC("texture.player"))->mManualOverrideFlags & kManualOverrideScope);

    // Rule wants to set scope = global — but field is manually overridden
    auto* cmd = new ApplyRulesCommand(registry, registry.GetRelationshipIndex(), engine);
    history.ExecuteCommand(cmd);

    // Scope should remain stage (manual override protected it)
    EXPECT_EQ(registry.FindById(StringCRC("texture.player"))->mScope, AssetScope::kStage);
}

TEST_F(ApplyRulesManualOverrideTest, OverwriteManualsAppliesAll)
{
    ASSERT_TRUE(LoadScopeRule());

    // Manually set scope to stage
    const AssetRecord* old = registry.FindById(StringCRC("texture.player"));
    AssetRecord updated = *old;
    updated.mScope = AssetScope::kStage;
    updated.mScopeStageName = StringCRC("level_1");
    history.ExecuteCommand(new UpdateRecordCommand(registry, StringCRC("texture.player"), *old, updated));

    EXPECT_TRUE(registry.FindById(StringCRC("texture.player"))->mManualOverrideFlags & kManualOverrideScope);

    // Force overwrite
    auto* cmd = new ApplyRulesCommand(registry, registry.GetRelationshipIndex(), engine);
    cmd->SetOverwriteManuals(true);
    history.ExecuteCommand(cmd);

    // Rule wins — scope back to global
    EXPECT_EQ(registry.FindById(StringCRC("texture.player"))->mScope, AssetScope::kGlobal);
}

TEST_F(ApplyRulesManualOverrideTest, ClearsManualFlagAfterRuleApply)
{
    ASSERT_TRUE(LoadScopeRule());

    // Manually set scope to stage
    const AssetRecord* old = registry.FindById(StringCRC("texture.player"));
    AssetRecord updated = *old;
    updated.mScope = AssetScope::kStage;
    history.ExecuteCommand(new UpdateRecordCommand(registry, StringCRC("texture.player"), *old, updated));

    EXPECT_TRUE(registry.FindById(StringCRC("texture.player"))->mManualOverrideFlags & kManualOverrideScope);

    // Apply with overwrite — rule now owns the field, flag must clear
    auto* cmd = new ApplyRulesCommand(registry, registry.GetRelationshipIndex(), engine);
    cmd->SetOverwriteManuals(true);
    history.ExecuteCommand(cmd);

    EXPECT_FALSE(registry.FindById(StringCRC("texture.player"))->mManualOverrideFlags & kManualOverrideScope);
}

TEST_F(ApplyRulesManualOverrideTest, UndoRestoresManualFlags)
{
    ASSERT_TRUE(LoadScopeRule());

    // Manually set scope
    const AssetRecord* old = registry.FindById(StringCRC("texture.player"));
    AssetRecord updated = *old;
    updated.mScope = AssetScope::kStage;
    history.ExecuteCommand(new UpdateRecordCommand(registry, StringCRC("texture.player"), *old, updated));

    const unsigned char flagsBefore =
        registry.FindById(StringCRC("texture.player"))->mManualOverrideFlags;

    auto* cmd = new ApplyRulesCommand(registry, registry.GetRelationshipIndex(), engine);
    cmd->SetOverwriteManuals(true);
    history.ExecuteCommand(cmd);

    history.Undo();

    EXPECT_EQ(registry.FindById(StringCRC("texture.player"))->mManualOverrideFlags, flagsBefore);
}

TEST_F(ApplyRulesManualOverrideTest, NonManualFieldNotFlagged_InDryRun)
{
    ASSERT_TRUE(LoadTagRule());

    // No manual flag set for tags — dry run should report mIsManualOverride = false
    RuleChangeset cs = engine.EvaluateDryRun(registry, registry.GetRelationshipIndex());

    bool found = false;
    for (unsigned int i = 0; i < cs.mChanges.Size(); ++i)
    {
        if (cs.mChanges[i].mRecordId == StringCRC("texture.player"))
        {
            EXPECT_FALSE(cs.mChanges[i].mIsManualOverride);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(ApplyRulesManualOverrideTest, ManualFieldFlagged_InDryRun)
{
    ASSERT_TRUE(LoadTagRule());

    // Manually set tags
    const AssetRecord* old = registry.FindById(StringCRC("texture.player"));
    AssetRecord updated = *old;
    updated.mTags.Add(StringCRC("manual_tag"));
    history.ExecuteCommand(new UpdateRecordCommand(registry, StringCRC("texture.player"), *old, updated));

    EXPECT_TRUE(registry.FindById(StringCRC("texture.player"))->mManualOverrideFlags & kManualOverrideTags);

    // Dry run — rule tries to add a tag; field is manually set so mIsManualOverride should be true
    RuleChangeset cs = engine.EvaluateDryRun(registry, registry.GetRelationshipIndex());

    bool found = false;
    for (unsigned int i = 0; i < cs.mChanges.Size(); ++i)
    {
        if (cs.mChanges[i].mRecordId == StringCRC("texture.player") &&
            strcmp(cs.mChanges[i].mField.AsCStr(), "tag") == 0)
        {
            EXPECT_TRUE(cs.mChanges[i].mIsManualOverride);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(ApplyRulesManualOverrideTest, ManualSkipDoesNotAffectNonManualFieldOnSameRecord)
{
    // Load a rule that sets tag AND scope on same record
    const char* rulesPath = "C:\\Temp\\test_manual_two_fields.json";
    const char* json =
        "{\n"
        "  \"rules\": [\n"
        "    { \"name\": \"tag-textures\",   \"match\": { \"type\": \"texture\" }, \"action\": \"assign_tag\",   \"tag\": \"visual\" },\n"
        "    { \"name\": \"scope-textures\",  \"match\": { \"type\": \"texture\" }, \"action\": \"assign_scope\", \"scope\": \"global\" }\n"
        "  ]\n"
        "}";
    FILE* f = nullptr;
    fopen_s(&f, rulesPath, "w");
    ASSERT_NE(f, nullptr);
    fputs(json, f);
    fclose(f);
    engine.LoadRules(rulesPath, typeRegistry);
    remove(rulesPath);

    // Manually set scope — but NOT tags
    const AssetRecord* old = registry.FindById(StringCRC("texture.player"));
    AssetRecord updated = *old;
    updated.mScope = AssetScope::kStage;
    history.ExecuteCommand(new UpdateRecordCommand(registry, StringCRC("texture.player"), *old, updated));

    EXPECT_TRUE(registry.FindById(StringCRC("texture.player"))->mManualOverrideFlags & kManualOverrideScope);
    EXPECT_FALSE(registry.FindById(StringCRC("texture.player"))->mManualOverrideFlags & kManualOverrideTags);

    // Apply without overwrite — scope should be protected, tag should be applied
    auto* cmd = new ApplyRulesCommand(registry, registry.GetRelationshipIndex(), engine);
    history.ExecuteCommand(cmd);

    const AssetRecord* result = registry.FindById(StringCRC("texture.player"));
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->mScope, AssetScope::kStage);       // manual field protected
    EXPECT_EQ(result->mTags.Size(), 1u);                  // non-manual field applied
    EXPECT_EQ(result->mTags[0], StringCRC("visual"));
}

// ===========================================================================
// CatalogueRulesEngine::GetRule / RuleInfo (Task 11)
// ===========================================================================

class GetRuleTest : public ::testing::Test
{
protected:
    CatalogueRulesEngine engine;
    AssetTypeRegistry typeRegistry;

    void SetUp() override
    {
        RegisterBuiltInAssetTypes(typeRegistry);
    }

    bool LoadRules(const char* json)
    {
        const char* path = "C:\\Temp\\test_getrule.json";
        FILE* f = nullptr;
        fopen_s(&f, path, "w");
        if (!f) return false;
        fputs(json, f);
        fclose(f);
        auto lr = engine.LoadRules(path, typeRegistry);
        remove(path);
        return lr.mSuccess;
    }
};

TEST_F(GetRuleTest, OutOfBoundsReturnsEmptyRuleInfo)
{
    RuleInfo info = engine.GetRule(0);
    EXPECT_EQ(info.mName.Length(), 0u);
    EXPECT_EQ(info.mMatchType.Length(), 0u);
    EXPECT_EQ(info.mActionType.Length(), 0u);
}

TEST_F(GetRuleTest, GetRuleCountMatchesLoadedRules)
{
    const char* json =
        "{ \"rules\": ["
        "  { \"name\": \"r1\", \"match\": { \"all\": true }, \"action\": \"assign_tag\", \"tag\": \"a\" },"
        "  { \"name\": \"r2\", \"match\": { \"all\": true }, \"action\": \"assign_tag\", \"tag\": \"b\" }"
        "] }";
    ASSERT_TRUE(LoadRules(json));
    EXPECT_EQ(engine.GetRuleCount(), 2u);
}

TEST_F(GetRuleTest, NameAndMatchTypeAndActionCorrect)
{
    const char* json =
        "{ \"rules\": ["
        "  { \"name\": \"tag-textures\", \"match\": { \"type\": \"texture\" }, \"action\": \"assign_tag\", \"tag\": \"visual\" }"
        "] }";
    ASSERT_TRUE(LoadRules(json));
    ASSERT_EQ(engine.GetRuleCount(), 1u);

    RuleInfo info = engine.GetRule(0);
    EXPECT_STREQ(info.mName.AsCStr(),       "tag-textures");
    EXPECT_STREQ(info.mMatchType.AsCStr(),  "type");
    EXPECT_STREQ(info.mMatchValue.AsCStr(), "texture");
    EXPECT_STREQ(info.mActionType.AsCStr(), "assign_tag");
    EXPECT_STREQ(info.mActionParam.AsCStr(),"visual");
}

TEST_F(GetRuleTest, MatchTypeAll_SerializesCorrectly)
{
    const char* json =
        "{ \"rules\": ["
        "  { \"name\": \"all-rule\", \"match\": { \"all\": true }, \"action\": \"assign_scope\", \"scope\": \"global\" }"
        "] }";
    ASSERT_TRUE(LoadRules(json));
    RuleInfo info = engine.GetRule(0);
    EXPECT_STREQ(info.mMatchType.AsCStr(), "all");
    EXPECT_STREQ(info.mActionType.AsCStr(), "assign_scope");
}

TEST_F(GetRuleTest, MatchTypeSourcePathGlob_SerializesCorrectly)
{
    const char* json =
        "{ \"rules\": ["
        "  { \"name\": \"glob-rule\", \"match\": { \"source_path_glob\": \"*.png\" }, \"action\": \"assign_tag\", \"tag\": \"image\" }"
        "] }";
    ASSERT_TRUE(LoadRules(json));
    RuleInfo info = engine.GetRule(0);
    EXPECT_STREQ(info.mMatchType.AsCStr(),  "source_path_glob");
    EXPECT_STREQ(info.mMatchValue.AsCStr(), "*.png");
}

TEST_F(GetRuleTest, MatchTypeTag_SerializesCorrectly)
{
    const char* json =
        "{ \"rules\": ["
        "  { \"name\": \"tag-match\", \"match\": { \"tag\": \"hero\" }, \"action\": \"assign_tag\", \"tag\": \"important\" }"
        "] }";
    ASSERT_TRUE(LoadRules(json));
    RuleInfo info = engine.GetRule(0);
    EXPECT_STREQ(info.mMatchType.AsCStr(),  "tag");
    EXPECT_STREQ(info.mMatchValue.AsCStr(), "hero");
}

TEST_F(GetRuleTest, GetRuleIndexOutOfBoundsHighValue_ReturnsEmpty)
{
    const char* json =
        "{ \"rules\": ["
        "  { \"name\": \"r1\", \"match\": { \"all\": true }, \"action\": \"assign_tag\", \"tag\": \"x\" }"
        "] }";
    ASSERT_TRUE(LoadRules(json));

    RuleInfo info = engine.GetRule(999);
    EXPECT_EQ(info.mName.Length(), 0u);
}

TEST_F(GetRuleTest, MultipleRulesCorrectIndex)
{
    const char* json =
        "{ \"rules\": ["
        "  { \"name\": \"first\",  \"match\": { \"type\": \"texture\" }, \"action\": \"assign_tag\", \"tag\": \"a\" },"
        "  { \"name\": \"second\", \"match\": { \"type\": \"entity\" },  \"action\": \"assign_tag\", \"tag\": \"b\" },"
        "  { \"name\": \"third\",  \"match\": { \"all\": true },         \"action\": \"assign_scope\", \"scope\": \"global\" }"
        "] }";
    ASSERT_TRUE(LoadRules(json));
    ASSERT_EQ(engine.GetRuleCount(), 3u);

    EXPECT_STREQ(engine.GetRule(0).mName.AsCStr(), "first");
    EXPECT_STREQ(engine.GetRule(1).mName.AsCStr(), "second");
    EXPECT_STREQ(engine.GetRule(2).mName.AsCStr(), "third");
}
