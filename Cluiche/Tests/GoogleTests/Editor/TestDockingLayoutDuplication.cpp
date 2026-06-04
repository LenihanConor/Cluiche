#include <gtest/gtest.h>
#include <DiaEditor/Layout/DockingLayout.h>

using namespace Dia::Editor;

TEST(DockingLayout, RegisterPanel_DuplicateIgnored)
{
    DockingLayout layout;
    layout.RegisterPanel("Home", "dia://home");
    layout.RegisterPanel("Home", "dia://home");

    EXPECT_EQ(layout.GetPanelCount(), 1u);
    EXPECT_TRUE(layout.IsPanelRegistered("Home"));
}

TEST(DockingLayout, RegisterPanel_DuplicateAfterOthers)
{
    DockingLayout layout;
    layout.RegisterPanel("Home",    "dia://home");
    layout.RegisterPanel("Console", "dia://console");
    layout.RegisterPanel("Home",    "dia://home");

    EXPECT_EQ(layout.GetPanelCount(), 2u);
}

TEST(DockingLayout, RegisterPanel_AllThreeBuiltins_NoDuplication)
{
    DockingLayout layout;
    const char* names[] = { "Home", "Output Console", "Plugin Browser" };
    const char* paths[] = {
        "dia://home", "dia://console", "dia://pluginbrowser"
    };

    for (int i = 0; i < 3; ++i) layout.RegisterPanel(names[i], paths[i]);
    for (int i = 0; i < 3; ++i) layout.RegisterPanel(names[i], paths[i]);

    EXPECT_EQ(layout.GetPanelCount(), 3u);
}

TEST(DockingLayout, RegisterPanel_DuplicateDifferentPath_FirstWins)
{
    DockingLayout layout;
    layout.RegisterPanel("Panel", "dia://original");
    layout.RegisterPanel("Panel", "dia://different");

    EXPECT_EQ(layout.GetPanelCount(), 1u);
    EXPECT_STREQ(layout.GetPanel(0).uiPath, "dia://original");
}
