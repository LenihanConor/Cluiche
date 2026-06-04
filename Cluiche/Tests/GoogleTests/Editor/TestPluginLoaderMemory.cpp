#include <gtest/gtest.h>
#include <DiaEditor/Memory/EditorMemory.h>
#include <DiaEditor/Plugin/EditorPluginRegistry.h>
#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaCore/CRC/StringCRC.h>
#include <cstring>
#include <cstdio>

using namespace Dia::Editor;
using namespace Dia::Core;

namespace
{
    // Minimal IEditorPlugin stub for registry tests
    class StubPlugin : public IEditorPlugin
    {
    public:
        explicit StubPlugin(const char* name) { strncpy_s(mName, sizeof(mName), name, _TRUNCATE); }
        const char* GetName() const override { return mName; }
        const char* GetVersion() const override { return "1.0"; }
        const char* GetDescription() const override { return "stub"; }
        const char* GetUIPath() const override { return "dia://stub"; }
        LayoutMode GetLayoutMode() const override { return LayoutMode::kDockable; }
        void OnLoad(const EditorPluginContext&) override {}
        void OnUnload() override {}
        void OnUpdate(float) override {}
    private:
        char mName[64];
    };

    class StubFactory : public IEditorPluginFactory
    {
    public:
        explicit StubFactory(const char* name) : mName(name) {}
        IEditorPlugin* Create() override { return new StubPlugin(mName); }
        EditorPluginInfo GetPluginInfo() override
        {
            EditorPluginInfo info = {};
            strncpy_s(info.name, sizeof(info.name), mName, _TRUNCATE);
            strncpy_s(info.version, sizeof(info.version), "1.0", _TRUNCATE);
            strncpy_s(info.description, sizeof(info.description), "stub", _TRUNCATE);
            info.layoutMode = LayoutMode::kDockable;
            return info;
        }
    private:
        const char* mName;
    };

    // Simulates the DoStop built-in filter logic extracted for unit testing
    static void CollectNonBuiltinPlugins(
        EditorMemory& memory,
        const StringCRC* loadedTypeIds,
        unsigned int count)
    {
        static const StringCRC kBuiltinHome("HomeEditorPlugin");
        static const StringCRC kBuiltinOutput("OutputConsoleEditorPlugin");
        static const StringCRC kBuiltinGameConn("GameConnectionEditorPlugin");
        static const StringCRC kBuiltinBrowser("PluginBrowserEditorPlugin");

        for (unsigned int i = 0; i < count; ++i)
        {
            const StringCRC& typeId = loadedTypeIds[i];
            if (typeId == kBuiltinHome || typeId == kBuiltinOutput ||
                typeId == kBuiltinGameConn || typeId == kBuiltinBrowser)
                continue;
            memory.AddPlugin(typeId.AsChar(), typeId.AsChar());
        }
    }

    // Simulates the DoStart restore logic extracted for unit testing
    static unsigned int RestoreFromMemory(
        const EditorMemory& memory,
        EditorPluginRegistry& registry,
        StringCRC* outLoaded,
        unsigned int maxLoaded)
    {
        unsigned int loadedCount = 0;
        for (unsigned int i = 0; i < memory.GetPluginCount(); ++i)
        {
            const MemoryPluginEntry& entry = memory.GetPlugin(i);
            StringCRC typeId(entry.typeId);
            if (!registry.IsPluginRegistered(typeId))
                continue;  // graceful skip (AC3)
            if (loadedCount < maxLoaded)
                outLoaded[loadedCount++] = typeId;
        }
        return loadedCount;
    }
}

// DoStop logic: built-ins are excluded from the saved plugin list
TEST(PluginLoaderMemory, DoStop_BuiltinsExcludedFromSave)
{
    const StringCRC loaded[] = {
        StringCRC("HomeEditorPlugin"),
        StringCRC("OutputConsoleEditorPlugin"),
        StringCRC("GameConnectionEditorPlugin"),
        StringCRC("PluginBrowserEditorPlugin"),
        StringCRC("DiaSceneEditor"),
        StringCRC("DiaEntityTemplateEditor"),
    };

    EditorMemory memory;
    CollectNonBuiltinPlugins(memory, loaded, 6);

    // Only the 2 non-built-ins should be saved
    EXPECT_EQ(memory.GetPluginCount(), 2u);
    EXPECT_STREQ(memory.GetPlugin(0).typeId, "DiaSceneEditor");
    EXPECT_STREQ(memory.GetPlugin(1).typeId, "DiaEntityTemplateEditor");
}

// DoStop logic: if only built-ins are loaded, nothing is saved
TEST(PluginLoaderMemory, DoStop_OnlyBuiltins_SavesEmptyList)
{
    const StringCRC loaded[] = {
        StringCRC("HomeEditorPlugin"),
        StringCRC("OutputConsoleEditorPlugin"),
        StringCRC("GameConnectionEditorPlugin"),
        StringCRC("PluginBrowserEditorPlugin"),
    };

    EditorMemory memory;
    CollectNonBuiltinPlugins(memory, loaded, 4);

    EXPECT_EQ(memory.GetPluginCount(), 0u);
}

// DoStart logic: unregistered plugin in memory is skipped (AC3)
TEST(PluginLoaderMemory, DoStart_UnregisteredPluginSkipped)
{
    EditorPluginRegistry& registry = EditorPluginRegistry::Instance();

    // Register one real plugin
    static StubFactory realFactory("RealPlugin_MemTest");
    const StringCRC realId("RealPlugin_MemTest_Unique");
    if (!registry.IsPluginRegistered(realId))
        registry.RegisterPlugin(realId, &realFactory);

    // Memory has real + fake
    EditorMemory memory;
    memory.AddPlugin("RealPlugin_MemTest_Unique", "real_inst");
    memory.AddPlugin("FakeGhostPlugin_XYZ", "ghost_inst");

    StringCRC restored[8];
    unsigned int count = RestoreFromMemory(memory, registry, restored, 8);

    // Only the registered plugin is restored
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(restored[0], realId);
}

// DoStart logic: registered plugin is loaded, unregistered is not
TEST(PluginLoaderMemory, DoStart_RegisteredPluginLoaded)
{
    EditorPluginRegistry& registry = EditorPluginRegistry::Instance();

    static StubFactory factoryA("PluginA_MemTest");
    static StubFactory factoryB("PluginB_MemTest");
    const StringCRC idA("PluginA_MemTest_Unique");
    const StringCRC idB("PluginB_MemTest_Unique");

    if (!registry.IsPluginRegistered(idA)) registry.RegisterPlugin(idA, &factoryA);
    if (!registry.IsPluginRegistered(idB)) registry.RegisterPlugin(idB, &factoryB);

    EditorMemory memory;
    memory.AddPlugin("PluginA_MemTest_Unique", "inst_a");
    memory.AddPlugin("PluginB_MemTest_Unique", "inst_b");
    memory.AddPlugin("NotRegistered_XYZ", "ghost");

    StringCRC restored[8];
    unsigned int count = RestoreFromMemory(memory, registry, restored, 8);

    EXPECT_EQ(count, 2u);
    EXPECT_EQ(restored[0], idA);
    EXPECT_EQ(restored[1], idB);
}

// Round-trip: DoStop saves to file, DoStart reloads from file
TEST(PluginLoaderMemory, SaveRestoreRoundTrip)
{
    const char* tmpPath = "test_plugin_loader_memory_roundtrip.json";

    // Simulate DoStop — save non-builtin plugins
    {
        const StringCRC loaded[] = {
            StringCRC("HomeEditorPlugin"),
            StringCRC("DiaSceneEditor"),
            StringCRC("DiaEntityTemplateEditor"),
        };

        EditorMemory memory;
        CollectNonBuiltinPlugins(memory, loaded, 3);
        memory.SetLastProject("C:/Games/Test/test.diagame");
        EXPECT_TRUE(memory.Save(tmpPath));
    }

    // Simulate DoStart — load and verify
    {
        EditorMemory memory;
        EXPECT_TRUE(memory.Load(tmpPath));
        EXPECT_EQ(memory.GetPluginCount(), 2u);
        EXPECT_STREQ(memory.GetPlugin(0).typeId, "DiaSceneEditor");
        EXPECT_STREQ(memory.GetPlugin(1).typeId, "DiaEntityTemplateEditor");
        EXPECT_STREQ(memory.GetLastProject(), "C:/Games/Test/test.diagame");
    }

    std::remove(tmpPath);
}
