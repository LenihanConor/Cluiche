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

TEST(EditorMemory, VersionMismatch_ReturnsFalse)
{
    const char* tmpPath = "test_editor_memory_version.json";
    WriteTempFile(tmpPath, "{\"version\":2,\"plugins\":[],\"last_project\":\"\"}");

    EditorMemory mem;
    EXPECT_FALSE(mem.Load(tmpPath));

    std::remove(tmpPath);
}

TEST(EditorMemory, SaveToInvalidPath_ReturnsFalse)
{
    EditorMemory mem;
    mem.AddPlugin("SomePlugin", "some_inst");
    EXPECT_FALSE(mem.Save("Z:/nonexistent_dir_xyz/.memory.json"));
}

TEST(EditorMemory, ClearPlugins_RemovesAll)
{
    EditorMemory mem;
    mem.AddPlugin("PluginA", "inst_a");
    mem.AddPlugin("PluginB", "inst_b");
    mem.AddPlugin("PluginC", "inst_c");
    EXPECT_EQ(mem.GetPluginCount(), 3u);

    mem.ClearPlugins();
    EXPECT_EQ(mem.GetPluginCount(), 0u);
}

TEST(EditorMemory, AddPlugin_AtCapacity_NoOverflow)
{
    EditorMemory mem;
    // Fill to capacity (16)
    for (unsigned int i = 0; i < 16; ++i)
    {
        char typeId[32];
        snprintf(typeId, sizeof(typeId), "Plugin%02u", i);
        mem.AddPlugin(typeId, typeId);
    }
    EXPECT_EQ(mem.GetPluginCount(), 16u);

    // Adding beyond capacity should be silently ignored
    mem.AddPlugin("OverflowPlugin", "overflow_inst");
    EXPECT_EQ(mem.GetPluginCount(), 16u);
}

TEST(EditorMemory, NullLayoutRoundTrip)
{
    const char* tmpPath = "test_editor_memory_null_layout.json";

    EditorMemory save;
    save.AddPlugin("SomePlugin", "inst");
    // Do NOT set a layout tree
    EXPECT_TRUE(save.Save(tmpPath));

    EditorMemory load;
    EXPECT_TRUE(load.Load(tmpPath));
    EXPECT_EQ(load.GetPluginCount(), 1u);
    EXPECT_TRUE(load.GetLayoutTree().isNull());

    std::remove(tmpPath);
}
