#include <gtest/gtest.h>
#include <DiaEditor/EditorManifestLoader.h>
#include <DiaEditor/Plugin/EditorPluginRegistry.h>
#include <DiaEditor/Plugin/StubEditorPlugin.h>
#include <DiaCore/CRC/StringCRC.h>
#include <fstream>

using namespace Dia::Editor;
using namespace Dia::Core;

TEST(EditorManifestLoader, LoadsPluginsFromManifest)
{
    const char* manifestPath = "test_manifest_loader.diaapp";
    {
        std::ofstream f(manifestPath);
        f << R"({ "editor": { "enabled": true, "plugins": [{ "type": "StubEditorPlugin", "instance_id": "stub_1" }] } })";
    }

    struct Ctx { int count; char lastName[128]; };
    Ctx ctx{ 0, {} };

    bool ok = EditorManifestLoader::Load(manifestPath,
        [](const EditorManifestLoader::PluginEntry& entry, void* ud) {
            auto* c = static_cast<Ctx*>(ud);
            ++c->count;
            strncpy_s(c->lastName, sizeof(c->lastName), entry.typeId, _TRUNCATE);
        }, &ctx);

    EXPECT_TRUE(ok);
    EXPECT_EQ(ctx.count, 1);
    EXPECT_STREQ(ctx.lastName, "StubEditorPlugin");

    std::remove(manifestPath);
}

TEST(EditorManifestLoader, DisabledManifestLoadsNothing)
{
    const char* manifestPath = "test_manifest_disabled.diaapp";
    {
        std::ofstream f(manifestPath);
        f << R"({ "editor": { "enabled": false, "plugins": [{ "type": "StubEditorPlugin" }] } })";
    }

    int count = 0;
    EditorManifestLoader::Load(manifestPath,
        [](const EditorManifestLoader::PluginEntry&, void* ud) { ++(*static_cast<int*>(ud)); }, &count);

    EXPECT_EQ(count, 0);
    std::remove(manifestPath);
}

TEST(EditorPluginRegistry, StubPluginRegistered)
{
    EXPECT_TRUE(EditorPluginRegistry::Instance().IsPluginRegistered(StringCRC("StubEditorPlugin")));
}

TEST(EditorPluginRegistry, CreateStubPlugin)
{
    IEditorPlugin* plugin = EditorPluginRegistry::Instance().CreatePlugin(StringCRC("StubEditorPlugin"));
    ASSERT_NE(plugin, nullptr);
    EXPECT_STREQ(plugin->GetName(), "StubEditorPlugin");
    delete plugin;
}
