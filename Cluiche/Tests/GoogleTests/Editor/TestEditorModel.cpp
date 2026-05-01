#include <gtest/gtest.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaCore/Architecture/Observer.h>
#include <fstream>

using namespace Dia::Editor;
using namespace Dia::Core;

namespace
{
    class TestObserver : public Dia::Core::Observer
    {
    public:
        int lastMessage = -1;
        int callCount = 0;
        void ObserverNotification(const Dia::Core::ObserverSubject*, int message) override { lastMessage = message; ++callCount; }
    };
}

TEST(EditorModel, InitialState)
{
    EditorModel model;
    EXPECT_FALSE(model.HasOpenProject());
    EXPECT_FALSE(model.IsDirty());
    EXPECT_FALSE(model.IsCloseRequested());
    EXPECT_EQ(model.GetManifestCount(), 0u);
}

TEST(EditorModel, DirtyFlag)
{
    EditorModel model;
    model.MarkDirty();
    EXPECT_TRUE(model.IsDirty());
    model.ClearDirty();
    EXPECT_FALSE(model.IsDirty());
}

TEST(EditorModel, RequestClose)
{
    EditorModel model;
    EXPECT_FALSE(model.IsCloseRequested());
    model.RequestClose();
    EXPECT_TRUE(model.IsCloseRequested());
}

TEST(EditorModel, LoadProjectFromDisk)
{
    const char* tmpPath = "test_editor_model_proj.cluicheproj";
    {
        std::ofstream f(tmpPath);
        f << R"({ "version": 1, "name": "UnitTest", "manifests": ["Data/a.diaapp", "Data/b.diaapp"], "editor_state": {} })";
    }

    EditorModel model;
    model.LoadProject(tmpPath);

    EXPECT_TRUE(model.HasOpenProject());
    EXPECT_STREQ(model.GetProjectPath(), tmpPath);
    EXPECT_EQ(model.GetManifestCount(), 2u);
    EXPECT_STREQ(model.GetManifestPath(0), "Data/a.diaapp");
    EXPECT_STREQ(model.GetManifestPath(1), "Data/b.diaapp");

    std::remove(tmpPath);
}
