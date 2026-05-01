#include <gtest/gtest.h>
#include <DiaEditor/Plugin/EditorPluginRegistry.h>
#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <cstring>

using namespace Dia::Editor;
using namespace Dia::Core;

namespace
{
    class TestPlugin : public IEditorPlugin
    {
    public:
        const char* GetName() const override { return "TestPlugin"; }
        const char* GetVersion() const override { return "1.0"; }
        const char* GetDescription() const override { return "Test"; }
        const char* GetUIPath() const override { return "dia://test"; }
        LayoutMode GetLayoutMode() const override { return LayoutMode::kDockable; }
        void OnLoad(const EditorPluginContext&) override {}
        void OnUnload() override {}
        void OnUpdate(float) override {}
    };

    class TestPluginFactory : public IEditorPluginFactory
    {
    public:
        IEditorPlugin* Create() override { return new TestPlugin(); }
        EditorPluginInfo GetPluginInfo() override
        {
            EditorPluginInfo info = {};
            TestPlugin tmp;
            strncpy_s(info.name, sizeof(info.name), tmp.GetName(), _TRUNCATE);
            strncpy_s(info.version, sizeof(info.version), tmp.GetVersion(), _TRUNCATE);
            strncpy_s(info.description, sizeof(info.description), tmp.GetDescription(), _TRUNCATE);
            info.layoutMode = tmp.GetLayoutMode();
            return info;
        }
    };
}

TEST(EditorPluginRegistry, RegisterAndQuery)
{
    EditorPluginRegistry& registry = EditorPluginRegistry::Instance();
    TestPluginFactory factory;
    const StringCRC typeId("TestPluginRegistryUniqueA");

    if (!registry.IsPluginRegistered(typeId))
        registry.RegisterPlugin(typeId, &factory);

    EXPECT_TRUE(registry.IsPluginRegistered(typeId));
}

TEST(EditorPluginRegistry, CreatePlugin)
{
    EditorPluginRegistry& registry = EditorPluginRegistry::Instance();
    TestPluginFactory factory;
    const StringCRC typeId("TestPluginRegistryUniqueB");

    if (!registry.IsPluginRegistered(typeId))
        registry.RegisterPlugin(typeId, &factory);

    IEditorPlugin* plugin = registry.CreatePlugin(typeId);
    ASSERT_NE(plugin, nullptr);
    EXPECT_STREQ(plugin->GetName(), "TestPlugin");
    delete plugin;
}

TEST(EditorPluginRegistry, UnknownTypeReturnsNull)
{
    EditorPluginRegistry& registry = EditorPluginRegistry::Instance();
    IEditorPlugin* plugin = registry.CreatePlugin(StringCRC("NonExistentPlugin_XYZ"));
    EXPECT_EQ(plugin, nullptr);
}

TEST(EditorPluginRegistry, IsRegisteredFalseForUnknown)
{
    EditorPluginRegistry& registry = EditorPluginRegistry::Instance();
    EXPECT_FALSE(registry.IsPluginRegistered(StringCRC("NotRegistered_XYZ")));
}

TEST(EditorPluginRegistry, GetFactory_Valid)
{
    EditorPluginRegistry& registry = EditorPluginRegistry::Instance();
    TestPluginFactory factory;
    const StringCRC typeId("TestPluginRegistryUniqueC");

    if (!registry.IsPluginRegistered(typeId))
        registry.RegisterPlugin(typeId, &factory);

    bool found = false;
    for (unsigned int i = 0; i < registry.GetRegisteredCount(); ++i)
    {
        if (registry.GetRegisteredTypeId(i) == typeId)
        {
            IEditorPluginFactory* f = registry.GetFactory(i);
            ASSERT_NE(f, nullptr);
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(EditorPluginRegistry, GetPluginInfo_FromFactory)
{
    EditorPluginRegistry& registry = EditorPluginRegistry::Instance();
    TestPluginFactory factory;
    const StringCRC typeId("TestPluginRegistryUniqueD");

    if (!registry.IsPluginRegistered(typeId))
        registry.RegisterPlugin(typeId, &factory);

    for (unsigned int i = 0; i < registry.GetRegisteredCount(); ++i)
    {
        if (registry.GetRegisteredTypeId(i) == typeId)
        {
            EditorPluginInfo info = registry.GetFactory(i)->GetPluginInfo();
            EXPECT_STREQ(info.name, "TestPlugin");
            EXPECT_STREQ(info.version, "1.0");
            break;
        }
    }
}
