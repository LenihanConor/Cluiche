#include <gtest/gtest.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/Project/ProjectContext.h>
#include <DiaEditor/Plugin/EditorPluginRegistry.h>
#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <fstream>
#include <filesystem>
#include <cstdio>
#include <cstring>

using namespace Dia::Editor;
using Dia::Core::StringCRC;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{
    // Write a minimal valid .diagame JSON file and return its path.
    // Caller is responsible for removing the file after use.
    const char* WriteDiagame(const char* path,
                              const char* appManifest = nullptr,
                              const char* assetCatalogue = nullptr,
                              const char* assetRoot = nullptr)
    {
        std::ofstream f(path);
        f << "{\n";
        f << "  \"name\": \"TestGame\",\n";
        f << "  \"version\": \"1.0\",\n";
        f << "  \"imports\": [";
        if (appManifest)
            f << "{\"path\": \"" << appManifest << "\", \"type\": \"manifest\"}";
        f << "],\n";
        f << "  \"config\": {";
        bool first = true;
        if (assetCatalogue)
        {
            f << "\"asset_catalogue\": \"" << assetCatalogue << "\"";
            first = false;
        }
        if (assetRoot)
        {
            if (!first) f << ", ";
            f << "\"asset_root\": \"" << assetRoot << "\"";
        }
        f << "}\n}\n";
        return path;
    }

    struct CallbackCapture
    {
        int callCount = 0;
        ProjectContext lastCtx{};

        static void Cb(const ProjectContext& ctx, void* ud)
        {
            auto* self = static_cast<CallbackCapture*>(ud);
            self->lastCtx = ctx;
            ++self->callCount;
        }
    };
}

// ---------------------------------------------------------------------------
// Initial state
// ---------------------------------------------------------------------------

TEST(EditorModelDiagame, InitialContextIsInvalid)
{
    EditorModel model;
    EXPECT_FALSE(model.GetDiagameProject().IsValid());
    EXPECT_EQ(model.GetRecentProjectCount(), 0u);
}

TEST(EditorModelDiagame, GetRecentProjectOutOfRangeReturnsNull)
{
    EditorModel model;
    EXPECT_EQ(model.GetRecentProject(0), nullptr);
    EXPECT_EQ(model.GetRecentProject(99), nullptr);
}

// ---------------------------------------------------------------------------
// LoadDiagameProject — success path
// ---------------------------------------------------------------------------

TEST(EditorModelDiagame, LoadDiagameProject_ValidFile_ContextIsValid)
{
    const char* path = "test_editormodel_load.diagame";
    WriteDiagame(path);

    EditorModel model;
    bool ok = model.LoadDiagameProject(path);

    EXPECT_TRUE(ok);
    EXPECT_TRUE(model.GetDiagameProject().IsValid());
    EXPECT_STREQ(model.GetDiagameProject().diagamePath, path);

    std::remove(path);
}

TEST(EditorModelDiagame, LoadDiagameProject_SetsApplicationManifestPath)
{
    const char* path = "test_editormodel_appman.diagame";
    WriteDiagame(path, "app/main.diaapp");

    EditorModel model;
    model.LoadDiagameProject(path);

    // applicationManifestPath should be dir(path) + "/" + "app/main.diaapp"
    const char* appPath = model.GetDiagameProject().applicationManifestPath;
    EXPECT_TRUE(strstr(appPath, "main.diaapp") != nullptr);

    std::remove(path);
}

TEST(EditorModelDiagame, LoadDiagameProject_SetsAssetCataloguePath)
{
    const char* path = "test_editormodel_cat.diagame";
    WriteDiagame(path, nullptr, "data/catalogue.json");

    EditorModel model;
    model.LoadDiagameProject(path);

    const char* catPath = model.GetDiagameProject().assetCataloguePath;
    EXPECT_TRUE(strstr(catPath, "catalogue.json") != nullptr);

    std::remove(path);
}

TEST(EditorModelDiagame, LoadDiagameProject_SetsAssetRoot)
{
    const char* path = "test_editormodel_root.diagame";
    WriteDiagame(path, nullptr, nullptr, "assets/");

    EditorModel model;
    model.LoadDiagameProject(path);

    const char* root = model.GetDiagameProject().assetRoot;
    EXPECT_TRUE(root[0] != '\0');

    std::remove(path);
}

// ---------------------------------------------------------------------------
// LoadDiagameProject — failure paths
// ---------------------------------------------------------------------------

TEST(EditorModelDiagame, LoadDiagameProject_NullPath_ReturnsFalse)
{
    EditorModel model;
    EXPECT_FALSE(model.LoadDiagameProject(nullptr));
    EXPECT_FALSE(model.GetDiagameProject().IsValid());
}

TEST(EditorModelDiagame, LoadDiagameProject_EmptyPath_ReturnsFalse)
{
    EditorModel model;
    EXPECT_FALSE(model.LoadDiagameProject(""));
    EXPECT_FALSE(model.GetDiagameProject().IsValid());
}

TEST(EditorModelDiagame, LoadDiagameProject_MissingFile_ReturnsFalse)
{
    EditorModel model;
    EXPECT_FALSE(model.LoadDiagameProject("nonexistent_file_xyz.diagame"));
    EXPECT_FALSE(model.GetDiagameProject().IsValid());
}

TEST(EditorModelDiagame, LoadDiagameProject_BadJson_ReturnsFalse)
{
    const char* path = "test_editormodel_badjson.diagame";
    {
        std::ofstream f(path);
        f << "{ not valid json {{{";
    }

    EditorModel model;
    EXPECT_FALSE(model.LoadDiagameProject(path));
    EXPECT_FALSE(model.GetDiagameProject().IsValid());

    std::remove(path);
}

// ---------------------------------------------------------------------------
// ClearDiagameProject
// ---------------------------------------------------------------------------

TEST(EditorModelDiagame, ClearDiagameProject_AfterLoad_ContextBecomesInvalid)
{
    const char* path = "test_editormodel_clear.diagame";
    WriteDiagame(path);

    EditorModel model;
    model.LoadDiagameProject(path);
    ASSERT_TRUE(model.GetDiagameProject().IsValid());

    model.ClearDiagameProject();
    EXPECT_FALSE(model.GetDiagameProject().IsValid());

    std::remove(path);
}

TEST(EditorModelDiagame, ClearDiagameProject_WhenAlreadyClear_IsNoop)
{
    EditorModel model;
    model.ClearDiagameProject();
    EXPECT_FALSE(model.GetDiagameProject().IsValid());
}

// ---------------------------------------------------------------------------
// Callbacks — registration and invocation
// ---------------------------------------------------------------------------

TEST(EditorModelDiagame, OnDiagameProjectChanged_CallbackFiredOnLoad)
{
    const char* path = "test_editormodel_cb_load.diagame";
    WriteDiagame(path);

    CallbackCapture cap;
    EditorModel model;
    model.OnDiagameProjectChanged(&CallbackCapture::Cb, &cap);

    model.LoadDiagameProject(path);

    EXPECT_EQ(cap.callCount, 1);
    EXPECT_TRUE(cap.lastCtx.IsValid());

    std::remove(path);
}

TEST(EditorModelDiagame, OnDiagameProjectChanged_CallbackFiredOnClear)
{
    const char* path = "test_editormodel_cb_clear.diagame";
    WriteDiagame(path);

    CallbackCapture cap;
    EditorModel model;
    model.OnDiagameProjectChanged(&CallbackCapture::Cb, &cap);

    model.LoadDiagameProject(path);
    model.ClearDiagameProject();

    EXPECT_EQ(cap.callCount, 2);
    EXPECT_FALSE(cap.lastCtx.IsValid());

    std::remove(path);
}

TEST(EditorModelDiagame, OnDiagameProjectChanged_NullCallbackIgnored)
{
    EditorModel model;
    model.OnDiagameProjectChanged(nullptr, nullptr);
    // Should not crash when project is loaded/cleared.
    model.ClearDiagameProject();
    EXPECT_FALSE(model.GetDiagameProject().IsValid());
}

TEST(EditorModelDiagame, OnDiagameProjectChanged_MultipleCallbacksAllFired)
{
    const char* path = "test_editormodel_multicb.diagame";
    WriteDiagame(path);

    CallbackCapture cap1, cap2, cap3;
    EditorModel model;
    model.OnDiagameProjectChanged(&CallbackCapture::Cb, &cap1);
    model.OnDiagameProjectChanged(&CallbackCapture::Cb, &cap2);
    model.OnDiagameProjectChanged(&CallbackCapture::Cb, &cap3);

    model.LoadDiagameProject(path);

    EXPECT_EQ(cap1.callCount, 1);
    EXPECT_EQ(cap2.callCount, 1);
    EXPECT_EQ(cap3.callCount, 1);

    std::remove(path);
}

TEST(EditorModelDiagame, OnDiagameProjectChanged_CallbackReceivesCorrectPath)
{
    const char* path = "test_editormodel_cbpath.diagame";
    WriteDiagame(path);

    CallbackCapture cap;
    EditorModel model;
    model.OnDiagameProjectChanged(&CallbackCapture::Cb, &cap);

    model.LoadDiagameProject(path);

    EXPECT_STREQ(cap.lastCtx.diagamePath, path);

    std::remove(path);
}

// ---------------------------------------------------------------------------
// SetRecentProjects / GetRecentProject
// ---------------------------------------------------------------------------

TEST(EditorModelDiagame, SetRecentProjects_StoresAndRetrieves)
{
    EditorModel model;
    const char* paths[] = {"proj/a.diagame", "proj/b.diagame", "proj/c.diagame"};
    model.SetRecentProjects(paths, 3);

    EXPECT_EQ(model.GetRecentProjectCount(), 3u);
    EXPECT_STREQ(model.GetRecentProject(0), "proj/a.diagame");
    EXPECT_STREQ(model.GetRecentProject(1), "proj/b.diagame");
    EXPECT_STREQ(model.GetRecentProject(2), "proj/c.diagame");
}

TEST(EditorModelDiagame, SetRecentProjects_EmptyList_ClearsRecent)
{
    EditorModel model;
    const char* initial[] = {"old.diagame"};
    model.SetRecentProjects(initial, 1);
    ASSERT_EQ(model.GetRecentProjectCount(), 1u);

    model.SetRecentProjects(nullptr, 0);
    EXPECT_EQ(model.GetRecentProjectCount(), 0u);
}

TEST(EditorModelDiagame, SetRecentProjects_ClampsToMaxFive)
{
    EditorModel model;
    const char* paths[] = {"a.diagame", "b.diagame", "c.diagame", "d.diagame", "e.diagame", "f.diagame"};
    model.SetRecentProjects(paths, 6);
    EXPECT_EQ(model.GetRecentProjectCount(), 5u);
}

TEST(EditorModelDiagame, GetRecentProject_OutOfRangeAfterSet_ReturnsNull)
{
    EditorModel model;
    const char* paths[] = {"a.diagame"};
    model.SetRecentProjects(paths, 1);
    EXPECT_EQ(model.GetRecentProject(1), nullptr);
}

// ---------------------------------------------------------------------------
// Game-scoped editor plugins (editor.manifests)
// ---------------------------------------------------------------------------

namespace
{
    class ModelScopeStubFactory : public IEditorPluginFactory
    {
    public:
        IEditorPlugin* Create() override { return nullptr; }
        EditorPluginInfo GetPluginInfo() override { return EditorPluginInfo(); }
    };

    ModelScopeStubFactory sModelStubFactory;

    const StringCRC kModelGameA("ModelScopeTest_GameA");
    const StringCRC kModelGameB("ModelScopeTest_GameB");

    // Write a .diaapp with an editor.plugins section listing the given type ids.
    void WriteEditorManifest(const char* path, const char* const* types, unsigned int count)
    {
        std::ofstream f(path);
        f << "{ \"editor\": { \"enabled\": true, \"plugins\": [";
        for (unsigned int i = 0; i < count; ++i)
        {
            if (i > 0) f << ", ";
            f << "{\"type\": \"" << types[i] << "\"}";
        }
        f << "] } }\n";
    }

    // Write a .diagame that lists the given editor manifests (relative paths).
    void WriteDiagameWithEditor(const char* path, const char* const* manifests, unsigned int count)
    {
        std::ofstream f(path);
        f << "{ \"name\": \"TestGame\", \"version\": \"1.0\", \"imports\": [], \"config\": {}";
        f << ", \"editor\": { \"manifests\": [";
        for (unsigned int i = 0; i < count; ++i)
        {
            if (i > 0) f << ", ";
            f << "\"" << manifests[i] << "\"";
        }
        f << "] } }\n";
    }
}

class EditorModelDiagameScope : public ::testing::Test
{
protected:
    void SetUp() override
    {
        EditorPluginRegistry& r = EditorPluginRegistry::Instance();
        const StringCRC ids[] = { kModelGameA, kModelGameB };
        for (const StringCRC& id : ids)
        {
            if (!r.IsPluginRegistered(id))
                r.RegisterPlugin(id, &sModelStubFactory);
        }
        Reset();
    }

    void TearDown() override { Reset(); }

    void Reset()
    {
        EditorPluginRegistry& r = EditorPluginRegistry::Instance();
        r.ClearGameScope();
        r.TagGameScoped(kModelGameA, StringCRC());
        r.TagGameScoped(kModelGameB, StringCRC());
    }
};

TEST_F(EditorModelDiagameScope, Load_SetsGameScope)
{
    const char* manifestPath = "test_editormodel_scope_a.diaapp";
    const char* gamePath = "test_editormodel_scope_a.diagame";
    const char* types[] = { "ModelScopeTest_GameA" };
    WriteEditorManifest(manifestPath, types, 1);
    const char* manifests[] = { manifestPath };
    WriteDiagameWithEditor(gamePath, manifests, 1);

    EditorPluginRegistry& r = EditorPluginRegistry::Instance();
    EXPECT_FALSE(r.IsGameScoped(kModelGameA));

    EditorModel model;
    ASSERT_TRUE(model.LoadDiagameProject(gamePath));
    EXPECT_TRUE(r.IsGameScoped(kModelGameA));
    EXPECT_TRUE(r.IsAvailable(kModelGameA));
    EXPECT_FALSE(r.IsGameScoped(kModelGameB));

    std::remove(gamePath);
    std::remove(manifestPath);
}

TEST_F(EditorModelDiagameScope, SecondLoad_ReplacesScope)
{
    const char* manA = "test_editormodel_scope_ra.diaapp";
    const char* manB = "test_editormodel_scope_rb.diaapp";
    const char* gameA = "test_editormodel_scope_ra.diagame";
    const char* gameB = "test_editormodel_scope_rb.diagame";
    const char* typesA[] = { "ModelScopeTest_GameA" };
    const char* typesB[] = { "ModelScopeTest_GameB" };
    WriteEditorManifest(manA, typesA, 1);
    WriteEditorManifest(manB, typesB, 1);
    const char* mA[] = { manA };
    const char* mB[] = { manB };
    WriteDiagameWithEditor(gameA, mA, 1);
    WriteDiagameWithEditor(gameB, mB, 1);

    EditorPluginRegistry& r = EditorPluginRegistry::Instance();
    EditorModel model;
    ASSERT_TRUE(model.LoadDiagameProject(gameA));
    ASSERT_TRUE(r.IsGameScoped(kModelGameA));
    ASSERT_TRUE(r.IsAvailable(kModelGameA));
    ASSERT_TRUE(model.LoadDiagameProject(gameB));

    EXPECT_FALSE(r.IsAvailable(kModelGameA));
    EXPECT_TRUE(r.IsAvailable(kModelGameB));

    std::remove(gameA); std::remove(gameB);
    std::remove(manA); std::remove(manB);
}

TEST_F(EditorModelDiagameScope, ClearDiagameProject_ClearsScope)
{
    const char* manifestPath = "test_editormodel_scope_c.diaapp";
    const char* gamePath = "test_editormodel_scope_c.diagame";
    const char* types[] = { "ModelScopeTest_GameA" };
    WriteEditorManifest(manifestPath, types, 1);
    const char* manifests[] = { manifestPath };
    WriteDiagameWithEditor(gamePath, manifests, 1);

    EditorPluginRegistry& r = EditorPluginRegistry::Instance();
    EditorModel model;
    ASSERT_TRUE(model.LoadDiagameProject(gamePath));
    ASSERT_TRUE(r.IsGameScoped(kModelGameA));
    ASSERT_TRUE(r.IsAvailable(kModelGameA));

    model.ClearDiagameProject();
    EXPECT_FALSE(r.IsAvailable(kModelGameA));
    EXPECT_TRUE(r.GetActiveGameId() == StringCRC());

    std::remove(gamePath);
    std::remove(manifestPath);
}

TEST_F(EditorModelDiagameScope, DiagameWithoutEditor_ClearsPreviousGameScope)
{
    const char* manifestPath = "test_editormodel_scope_n.diaapp";
    const char* gameA = "test_editormodel_scope_n_a.diagame";
    const char* plain = "test_editormodel_scope_n_plain.diagame";
    const char* types[] = { "ModelScopeTest_GameA" };
    WriteEditorManifest(manifestPath, types, 1);
    const char* manifests[] = { manifestPath };
    WriteDiagameWithEditor(gameA, manifests, 1);
    WriteDiagame(plain);

    EditorPluginRegistry& r = EditorPluginRegistry::Instance();
    EditorModel model;
    ASSERT_TRUE(model.LoadDiagameProject(gameA));
    ASSERT_TRUE(r.IsGameScoped(kModelGameA));
    ASSERT_TRUE(r.IsAvailable(kModelGameA));

    ASSERT_TRUE(model.LoadDiagameProject(plain));
    EXPECT_FALSE(r.IsAvailable(kModelGameA));  // tag stays, active game moved on
    EXPECT_FALSE(r.IsGameScoped(kModelGameB));
    EXPECT_FALSE(r.GetActiveGameId() == StringCRC());

    std::remove(gameA); std::remove(plain);
    std::remove(manifestPath);
}

TEST_F(EditorModelDiagameScope, FailedLoad_LeavesScopeUntouched)
{
    const char* manifestPath = "test_editormodel_scope_f.diaapp";
    const char* gamePath = "test_editormodel_scope_f.diagame";
    const char* types[] = { "ModelScopeTest_GameA" };
    WriteEditorManifest(manifestPath, types, 1);
    const char* manifests[] = { manifestPath };
    WriteDiagameWithEditor(gamePath, manifests, 1);

    EditorPluginRegistry& r = EditorPluginRegistry::Instance();
    EditorModel model;
    ASSERT_TRUE(model.LoadDiagameProject(gamePath));
    ASSERT_TRUE(r.IsGameScoped(kModelGameA));
    ASSERT_TRUE(r.IsAvailable(kModelGameA));

    EXPECT_FALSE(model.LoadDiagameProject("nonexistent_scope_game.diagame"));
    EXPECT_TRUE(r.IsAvailable(kModelGameA));

    std::remove(gamePath);
    std::remove(manifestPath);
}

TEST_F(EditorModelDiagameScope, UnregisteredTypeInManifest_IsSkipped)
{
    const char* manifestPath = "test_editormodel_scope_u.diaapp";
    const char* gamePath = "test_editormodel_scope_u.diagame";
    const char* types[] = { "ModelScopeTest_NotCompiledIn", "ModelScopeTest_GameA" };
    WriteEditorManifest(manifestPath, types, 2);
    const char* manifests[] = { manifestPath };
    WriteDiagameWithEditor(gamePath, manifests, 1);

    EditorPluginRegistry& r = EditorPluginRegistry::Instance();
    EditorModel model;
    ASSERT_TRUE(model.LoadDiagameProject(gamePath));
    EXPECT_TRUE(r.IsGameScoped(kModelGameA));
    EXPECT_TRUE(r.IsAvailable(kModelGameA));
    EXPECT_FALSE(r.IsGameScoped(StringCRC("ModelScopeTest_NotCompiledIn")));

    std::remove(gamePath);
    std::remove(manifestPath);
}

TEST_F(EditorModelDiagameScope, MissingManifestFile_IsSkippedLoadStillSucceeds)
{
    const char* gamePath = "test_editormodel_scope_m.diagame";
    const char* manifests[] = { "test_editormodel_scope_missing.diaapp" };
    WriteDiagameWithEditor(gamePath, manifests, 1);

    EditorModel model;
    EXPECT_TRUE(model.LoadDiagameProject(gamePath));
    EXPECT_FALSE(EditorPluginRegistry::Instance().IsGameScoped(kModelGameA));

    std::remove(gamePath);
}

TEST_F(EditorModelDiagameScope, ManifestPathResolvedRelativeToDiagameDirectory)
{
    const char* dirName = "test_editormodel_scope_sub";
    std::filesystem::create_directories(dirName);
    const char* manifestPath = "test_editormodel_scope_sub/m.diaapp";
    const char* gamePath = "test_editormodel_scope_sub/g.diagame";
    const char* types[] = { "ModelScopeTest_GameB" };
    WriteEditorManifest(manifestPath, types, 1);
    const char* manifests[] = { "m.diaapp" };
    WriteDiagameWithEditor(gamePath, manifests, 1);

    EditorPluginRegistry& r = EditorPluginRegistry::Instance();
    EditorModel model;
    ASSERT_TRUE(model.LoadDiagameProject(gamePath));
    EXPECT_TRUE(r.IsGameScoped(kModelGameB));
    EXPECT_TRUE(r.IsAvailable(kModelGameB));

    std::remove(gamePath);
    std::remove(manifestPath);
    std::filesystem::remove(dirName);
}

// ---------------------------------------------------------------------------
// Reset clears game-project state
// ---------------------------------------------------------------------------

TEST(EditorModelDiagame, Reset_ClearsContext)
{
    const char* path = "test_editormodel_reset.diagame";
    WriteDiagame(path);

    EditorModel model;
    model.LoadDiagameProject(path);
    ASSERT_TRUE(model.GetDiagameProject().IsValid());

    model.Reset();
    EXPECT_FALSE(model.GetDiagameProject().IsValid());
    EXPECT_EQ(model.GetRecentProjectCount(), 0u);

    std::remove(path);
}

namespace
{
    struct ScopeCount
    {
        int n = 0;
        static void Cb(void* ud, const StringCRC&, const StringCRC&) { ++static_cast<ScopeCount*>(ud)->n; }
    };
}

TEST_F(EditorModelDiagameScope, SameFileDifferentSpelling_KeepsGameId)
{
    const char* manifestPath = "test_editormodel_scope_sp.diaapp";
    const char* gamePath = "test_editormodel_scope_sp.diagame";
    const char* types[] = { "ModelScopeTest_GameA" };
    WriteEditorManifest(manifestPath, types, 1);
    const char* manifests[] = { manifestPath };
    WriteDiagameWithEditor(gamePath, manifests, 1);

    ScopeCount count;
    EditorPluginRegistry& r = EditorPluginRegistry::Instance();
    EditorModel model;
    ASSERT_TRUE(model.LoadDiagameProject(gamePath));
    const StringCRC id = r.GetActiveGameId();
    r.AddGameScopeListener(&ScopeCount::Cb, &count);
    ASSERT_TRUE(model.LoadDiagameProject("./test_editormodel_scope_sp.diagame"));
    r.RemoveGameScopeListener(&ScopeCount::Cb, &count);

    EXPECT_TRUE(r.GetActiveGameId() == id);
    EXPECT_EQ(count.n, 0);

    std::remove(gamePath);
    std::remove(manifestPath);
}
