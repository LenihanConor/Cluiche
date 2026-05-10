#include <gtest/gtest.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/Command/CommandHistory.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>
#include <DiaEditor/EditorManifestLoader.h>
#include <DiaEditor/Plugin/EditorPluginRegistry.h>
#include <DiaCore/CRC/StringCRC.h>
#include <fstream>

using namespace Dia::Editor;
using namespace Dia::Core;

// These tests exercise the library classes working together,
// without any DiaApplicationFlow dependency.

TEST(EditorLibraryLifecycle, ModelAndHistoryTogether)
{
    EditorModel model;
    CommandHistory history;

    EXPECT_FALSE(model.HasOpenProject());
    EXPECT_FALSE(history.CanUndo());

    model.MarkDirty();
    EXPECT_TRUE(model.IsDirty());
}

TEST(EditorLibraryLifecycle, ManifestLoaderAndRegistry)
{
    const char* projPath = "lifecycle_test.cluicheproj";
    const char* manifestPath = "lifecycle_test.diaapp";

    {
        std::ofstream f(manifestPath);
        f << R"({ "editor": { "enabled": true, "plugins": [{ "type": "StubEditorPlugin", "instance_id": "lifecycle_stub" }] } })";
    }
    {
        std::ofstream f(projPath);
        f << R"({ "version": 1, "name": "Lifecycle", "manifests": ["lifecycle_test.diaapp"], "editor_state": {} })";
    }

    EditorModel model;
    model.LoadProject(projPath);
    EXPECT_TRUE(model.HasOpenProject());
    EXPECT_EQ(model.GetManifestCount(), 1u);

    int pluginsLoaded = 0;
    EditorManifestLoader::Load(manifestPath,
        [](const EditorManifestLoader::PluginEntry& entry, void* ud) {
            auto& registry = EditorPluginRegistry::Instance();
            if (registry.IsPluginRegistered(Dia::Core::StringCRC(entry.typeId)))
                ++(*static_cast<int*>(ud));
        }, &pluginsLoaded);

    EXPECT_EQ(pluginsLoaded, 1);

    std::remove(projPath);
    std::remove(manifestPath);
}
