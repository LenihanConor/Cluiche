#include <gtest/gtest.h>
#include <DiaEditor/Memory/EditorMemory.h>
#include <DiaCore/Json/external/json/json.h>
#include <fstream>
#include <cstring>

using namespace Dia::Editor;

// Helper: write a string to a temp file
static void WriteTempFile(const char* path, const char* content)
{
    std::ofstream f(path);
    f << content;
}

TEST(EditorMemory, EmptyOnConstruction)
{
    EditorMemory mem;
    EXPECT_EQ(mem.GetPluginCount(), 0u);
    EXPECT_TRUE(mem.GetLayoutTree().isNull());
    // GetLastProject returns "" (empty char buffer)
    EXPECT_EQ(std::strlen(mem.GetLastProject()), 0u);
}

TEST(EditorMemory, RoundTripSerializeDeserialize)
{
    const char* tmpPath = "test_editor_memory_roundtrip.json";

    EditorMemory save;
    save.AddPlugin("SceneEditorPlugin", "scene_main");
    save.AddPlugin("BlueprintEditorPlugin", "bp_main");

    Json::Value layout;
    layout["type"] = "msaa";
    layout["first"] = "SceneEditorPlugin";
    save.SetLayoutTree(layout);
    save.SetLastProject("C:/Games/CoW/cow.diagame");

    EXPECT_TRUE(save.Save(tmpPath));

    EditorMemory load;
    EXPECT_TRUE(load.Load(tmpPath));

    EXPECT_EQ(load.GetPluginCount(), 2u);
    EXPECT_STREQ(load.GetPlugin(0).typeId, "SceneEditorPlugin");
    EXPECT_STREQ(load.GetPlugin(0).instanceId, "scene_main");
    EXPECT_STREQ(load.GetPlugin(1).typeId, "BlueprintEditorPlugin");
    EXPECT_STREQ(load.GetPlugin(1).instanceId, "bp_main");
    EXPECT_STREQ(load.GetLastProject(), "C:/Games/CoW/cow.diagame");
    EXPECT_FALSE(load.GetLayoutTree().isNull());
    EXPECT_EQ(load.GetLayoutTree()["type"].asString(), "msaa");

    std::remove(tmpPath);
}

TEST(EditorMemory, MissingFileReturnsFalse)
{
    EditorMemory mem;
    EXPECT_FALSE(mem.Load("nonexistent_memory_file_12345.json"));
}

TEST(EditorMemory, CorruptFileReturnsFalse)
{
    const char* tmpPath = "test_editor_memory_corrupt.json";
    WriteTempFile(tmpPath, "this is not valid json {{{{");

    EditorMemory mem;
    EXPECT_FALSE(mem.Load(tmpPath));

    std::remove(tmpPath);
}

TEST(EditorMemory, LoadPreservesUnknownPlugin)
{
    // Confirm EditorMemory faithfully loads a plugin typeId it doesn't know about.
    // The graceful-skip logic lives in PluginLoaderModule, not EditorMemory.
    const char* tmpPath = "test_editor_memory_unknown_plugin.json";

    EditorMemory save;
    save.AddPlugin("FakeUnregisteredPlugin", "fake_instance");
    EXPECT_TRUE(save.Save(tmpPath));

    EditorMemory load;
    EXPECT_TRUE(load.Load(tmpPath));
    EXPECT_EQ(load.GetPluginCount(), 1u);
    EXPECT_STREQ(load.GetPlugin(0).typeId, "FakeUnregisteredPlugin");

    std::remove(tmpPath);
}
