#include <gtest/gtest.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/Project/ProjectContext.h>
#include <fstream>
#include <cstdio>
#include <cstring>

using namespace Dia::Editor;

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
