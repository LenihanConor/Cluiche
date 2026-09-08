#include <gtest/gtest.h>
#include <DiaEditor/Plugin/GameConnectionEditorPlugin.h>
#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaEditor/Plugin/EditorPluginRegistry.h>
#include <DiaEditor/Layout/DockingLayout.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::Editor;
using namespace Dia::Core;

TEST(PluginLoaderHeadless, GameConnectionPlugin_IsHeadless)
{
    IEditorPlugin* plugin = EditorPluginRegistry::Instance().CreatePlugin(StringCRC("GameConnectionEditorPlugin"));
    ASSERT_NE(plugin, nullptr);
    EXPECT_EQ(plugin->GetLayoutMode(), LayoutMode::kHeadless);
    EXPECT_STREQ(plugin->GetUIPath(), "");
    delete plugin;
}

TEST(PluginLoaderHeadless, DockingLayout_RemovePanel_HeadlessEntry)
{
    DockingLayout layout;
    layout.RegisterPanel("Home",            "dia://home");
    layout.RegisterPanel("Output Console",  "dia://console");
    layout.RegisterPanel("Game Connection", "dia://gameconnection");
    ASSERT_EQ(layout.GetPanelCount(), 3u);

    IEditorPlugin* plugin = EditorPluginRegistry::Instance().CreatePlugin(StringCRC("GameConnectionEditorPlugin"));
    ASSERT_NE(plugin, nullptr);
    if (plugin->GetLayoutMode() == LayoutMode::kHeadless)
        layout.RemovePanel(plugin->GetName());
    delete plugin;

    EXPECT_EQ(layout.GetPanelCount(), 2u);
    EXPECT_FALSE(layout.IsPanelRegistered("Game Connection"));
    EXPECT_TRUE(layout.IsPanelRegistered("Home"));
    EXPECT_TRUE(layout.IsPanelRegistered("Output Console"));
}

TEST(PluginLoaderHeadless, NonHeadlessPlugins_NotHeadless)
{
    IEditorPlugin* home = EditorPluginRegistry::Instance().CreatePlugin(StringCRC("HomeEditorPlugin"));
    ASSERT_NE(home, nullptr);
    EXPECT_NE(home->GetLayoutMode(), LayoutMode::kHeadless);
    delete home;

    IEditorPlugin* output = EditorPluginRegistry::Instance().CreatePlugin(StringCRC("OutputConsoleEditorPlugin"));
    ASSERT_NE(output, nullptr);
    EXPECT_NE(output->GetLayoutMode(), LayoutMode::kHeadless);
    delete output;
}
