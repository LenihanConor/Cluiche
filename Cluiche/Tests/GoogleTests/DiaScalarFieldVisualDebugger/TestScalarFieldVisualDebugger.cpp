////////////////////////////////////////////////////////////////////////////////
// TestScalarFieldVisualDebugger.cpp
// IDebugDomain for ScalarField — identity, JSON state, OnCommand.
// System spec: docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaScalarFieldVisualDebugger/ScalarFieldVisualDebugger.h>
#include <DiaDebugDraw/Domain/DebugGroupAccents.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstring>

using namespace Dia::ScalarField;

namespace
{

Json::Value ToggleArgs(const char* drawerName)
{
    Json::Value args;
    args["drawer"] = drawerName;
    return args;
}

Json::Value ScaleArgs(const char* key, float value)
{
    Json::Value args;
    args["key"]   = key;
    args["value"] = value;
    return args;
}

} // namespace

// ===========================================================================
// Identity
// ===========================================================================

TEST(ScalarFieldVisualDebugger_Identity, DomainIdIsScalarField)
{
    ScalarFieldVisualDebugger domain;

    EXPECT_EQ(domain.GetDomainId(), Dia::Core::StringCRC("scalarfield"));
    EXPECT_STREQ(domain.GetDisplayName(), "Scalar Field");
}

TEST(ScalarFieldVisualDebugger_Identity, GroupIsSpatial)
{
    ScalarFieldVisualDebugger domain;

    EXPECT_EQ(domain.GetGroup(), Dia::Core::StringCRC("Spatial"));
    EXPECT_EQ(domain.GetAccentColour(), Dia::VisualDebugger::DebugGroupAccents::kSpatial);
}

TEST(ScalarFieldVisualDebugger_Identity, HasWorldDrawersFalse)
{
    ScalarFieldVisualDebugger domain;

    EXPECT_FALSE(domain.HasWorldDrawers());
    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_EQ(domain.GetDrawer(0), nullptr);
}

TEST(ScalarFieldVisualDebugger_Identity, DescriptionWithin80Chars)
{
    ScalarFieldVisualDebugger domain;

    ASSERT_NE(domain.GetDescription(), nullptr);
    EXPECT_LE(strlen(domain.GetDescription()), 80u);
}

// ===========================================================================
// JSON state — AC10
// ===========================================================================

TEST(ScalarFieldVisualDebugger_JSONState, ReportsDrawersAndStats)
{
    ScalarFieldVisualDebugger domain;

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_TRUE(state.isMember("drawers"));
    ASSERT_TRUE(state["drawers"].isArray());
    EXPECT_EQ(state["drawers"].size(), 2u);
    ASSERT_TRUE(state.isMember("stats"));
    EXPECT_TRUE(state["stats"].isObject());
}

TEST(ScalarFieldVisualDebugger_JSONState, DrawerNames_AreHeatmap_Gradient)
{
    ScalarFieldVisualDebugger domain;

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_EQ(state["drawers"].size(), 2u);
    EXPECT_STREQ(state["drawers"][0u]["name"].asCString(), "Heatmap");
    EXPECT_STREQ(state["drawers"][1u]["name"].asCString(), "Gradient");
}

TEST(ScalarFieldVisualDebugger_JSONState, AllDrawers_EnabledByDefault)
{
    ScalarFieldVisualDebugger domain;

    Json::Value state;
    domain.GetJSONState(state);

    EXPECT_TRUE(state["drawers"][0u]["enabled"].asBool());
    EXPECT_TRUE(state["drawers"][1u]["enabled"].asBool());
}

TEST(ScalarFieldVisualDebugger_JSONState, Stats_HasArrowScale)
{
    ScalarFieldVisualDebugger domain;

    Json::Value state;
    domain.GetJSONState(state);

    EXPECT_TRUE(state["stats"].isMember("arrowScale"));
    EXPECT_FLOAT_EQ(state["stats"]["arrowScale"].asFloat(), 0.35f);
}

// ===========================================================================
// OnCommand toggle — AC11
// ===========================================================================

TEST(ScalarFieldVisualDebugger_OnCommand, Toggle_Heatmap_DisablesIt)
{
    ScalarFieldVisualDebugger domain;

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Heatmap"));

    Json::Value state;
    domain.GetJSONState(state);

    EXPECT_FALSE(state["drawers"][0u]["enabled"].asBool());
    EXPECT_TRUE (state["drawers"][1u]["enabled"].asBool());
}

TEST(ScalarFieldVisualDebugger_OnCommand, Toggle_Gradient_DisablesIt)
{
    ScalarFieldVisualDebugger domain;

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Gradient"));

    Json::Value state;
    domain.GetJSONState(state);

    EXPECT_TRUE (state["drawers"][0u]["enabled"].asBool());
    EXPECT_FALSE(state["drawers"][1u]["enabled"].asBool());
}

TEST(ScalarFieldVisualDebugger_OnCommand, Toggle_Heatmap_Twice_Restores)
{
    ScalarFieldVisualDebugger domain;

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Heatmap"));
    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Heatmap"));

    Json::Value state;
    domain.GetJSONState(state);

    EXPECT_TRUE(state["drawers"][0u]["enabled"].asBool());
}

// ===========================================================================
// OnCommand setScale — AC12
// ===========================================================================

TEST(ScalarFieldVisualDebugger_OnCommand, SetScale_ArrowScale_UpdatesStats)
{
    ScalarFieldVisualDebugger domain;

    domain.OnCommand(Dia::Core::StringCRC("setScale"), ScaleArgs("arrowScale", 0.75f));

    Json::Value state;
    domain.GetJSONState(state);

    EXPECT_FLOAT_EQ(state["stats"]["arrowScale"].asFloat(), 0.75f);
}

TEST(ScalarFieldVisualDebugger_OnCommand, GetArrowScale_ReflectsSetScale)
{
    ScalarFieldVisualDebugger domain;

    domain.OnCommand(Dia::Core::StringCRC("setScale"), ScaleArgs("arrowScale", 1.5f));

    EXPECT_FLOAT_EQ(domain.GetArrowScale(), 1.5f);
}

TEST(ScalarFieldVisualDebugger_OnCommand, SetScale_UnknownKey_NoOp)
{
    ScalarFieldVisualDebugger domain;

    Json::Value args;
    args["key"]   = "notAKey";
    args["value"] = 5.0;
    EXPECT_NO_FATAL_FAILURE(
        domain.OnCommand(Dia::Core::StringCRC("setScale"), args));
}

TEST(ScalarFieldVisualDebugger_OnCommand, UnknownCommand_NoOp)
{
    ScalarFieldVisualDebugger domain;

    Json::Value empty(Json::objectValue);
    EXPECT_NO_FATAL_FAILURE(
        domain.OnCommand(Dia::Core::StringCRC("notACommand"), empty));
}

TEST(ScalarFieldVisualDebugger_OnCommand, MalformedToggle_NoOp)
{
    ScalarFieldVisualDebugger domain;

    Json::Value empty(Json::objectValue);
    domain.OnCommand(Dia::Core::StringCRC("toggle"), empty);

    Json::Value wrongType(Json::objectValue);
    wrongType["drawer"] = 42;
    domain.OnCommand(Dia::Core::StringCRC("toggle"), wrongType);

    Json::Value state;
    domain.GetJSONState(state);
    EXPECT_TRUE(state["drawers"][0u]["enabled"].asBool());
}

#endif // DIA_DEBUG
