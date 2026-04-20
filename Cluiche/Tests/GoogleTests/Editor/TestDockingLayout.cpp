#include <gtest/gtest.h>
#include <DiaEditor/Layout/DockingLayout.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::Editor;

TEST(DockingLayout, RegisterAndQuery)
{
    DockingLayout layout;
    layout.RegisterPanel("Properties", "dia://editor/properties/index.html");
    layout.RegisterPanel("Console", "dia://editor/console/index.html");

    EXPECT_TRUE(layout.IsPanelRegistered("Properties"));
    EXPECT_TRUE(layout.IsPanelRegistered("Console"));
    EXPECT_FALSE(layout.IsPanelRegistered("NonExistent"));
    EXPECT_EQ(layout.GetPanelCount(), 2u);
}

TEST(DockingLayout, SerializeRoundTrip)
{
    DockingLayout layout;
    layout.RegisterPanel("Props", "dia://props");
    layout.RegisterPanel("Output", "dia://output");

    Json::Value serialized;
    layout.Serialize(serialized);

    DockingLayout layout2;
    layout2.RegisterPanel("Props", "dia://props");
    layout2.RegisterPanel("Output", "dia://output");
    layout2.Deserialize(serialized);

    EXPECT_EQ(layout2.GetPanelCount(), 2u);
    EXPECT_STREQ(layout2.GetPanel(0).name, "Props");
    EXPECT_STREQ(layout2.GetPanel(1).name, "Output");
}

TEST(DockingLayout, ValidateLayoutStripsUnknown)
{
    DockingLayout layout;
    layout.RegisterPanel("KnownPanel", "dia://known");

    Json::Value layoutData;
    Json::Value known, unknown;
    known["name"] = "KnownPanel";  known["uiPath"] = "dia://known";  known["visible"] = true;
    unknown["name"] = "UnknownPanel"; unknown["uiPath"] = "dia://unknown"; unknown["visible"] = true;
    layoutData["panels"].append(known);
    layoutData["panels"].append(unknown);

    layout.ValidateLayout(layoutData);

    bool hasUnknown = false;
    for (const auto& panel : layoutData["panels"])
        if (panel["name"].asString() == "UnknownPanel")
            hasUnknown = true;

    EXPECT_FALSE(hasUnknown);
    EXPECT_EQ(layoutData["panels"].size(), 1u);
}

TEST(DockingLayout, SaveAndLoadDisk)
{
    const char* path = "test_docking_layout.json";

    DockingLayout layout;
    layout.RegisterPanel("A", "dia://a");
    layout.RegisterPanel("B", "dia://b");
    EXPECT_TRUE(layout.SaveToDisk(path));

    DockingLayout layout2;
    layout2.RegisterPanel("A", "dia://a");
    layout2.RegisterPanel("B", "dia://b");
    EXPECT_TRUE(layout2.LoadFromDisk(path));
    EXPECT_EQ(layout2.GetPanelCount(), 2u);

    std::remove(path);
}
