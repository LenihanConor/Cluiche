#include <gtest/gtest.h>
#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaEditor/Plugin/OutputConsoleEditorPlugin.h>
#include <DiaEditor/Plugin/PluginBrowserEditorPlugin.h>
#include <DiaEditor/Plugin/HelloEditorPlugin.h>

using namespace Dia::Editor;

TEST(EditorToolbarItem, DefaultUsesFirstCharOfName)
{
    HelloEditorPlugin plugin;
    EditorToolbarItem item = plugin.GetToolbarItem();

    EXPECT_STREQ(item.label, "HelloEditorPlugin");
    EXPECT_EQ(item.iconChar[0], 'H');
    EXPECT_EQ(item.iconChar[1], '\0');
    EXPECT_FALSE(item.pinned);
}

TEST(EditorToolbarItem, OutputConsole_Pinned)
{
    OutputConsoleEditorPlugin plugin;
    EditorToolbarItem item = plugin.GetToolbarItem();

    EXPECT_STREQ(item.label, "Output Console");
    EXPECT_EQ(item.iconChar[0], 'C');
    EXPECT_TRUE(item.pinned);
}

TEST(EditorToolbarItem, PluginBrowser_Pinned)
{
    PluginBrowserEditorPlugin plugin;
    EditorToolbarItem item = plugin.GetToolbarItem();

    EXPECT_STREQ(item.label, "Plugin Browser");
    EXPECT_EQ(item.iconChar[0], 'P');
    EXPECT_TRUE(item.pinned);
}

TEST(EditorToolbarItem, DefaultConstructor)
{
    EditorToolbarItem item;

    EXPECT_EQ(item.label[0], '\0');
    EXPECT_EQ(item.iconChar[0], '\0');
    EXPECT_FALSE(item.pinned);
}
