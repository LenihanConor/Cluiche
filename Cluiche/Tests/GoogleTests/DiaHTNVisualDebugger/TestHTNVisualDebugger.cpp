////////////////////////////////////////////////////////////////////////////////
// TestHTNVisualDebugger.cpp
// Mandatory IDebugDomain contract shapes for HTNVisualDebugger.
//
//   Suite HTNVisualDebugger_Identity    — identity methods, no-drawers
//   Suite HTNVisualDebugger_JSONState   — JSON schema round-trip, plan gate
//   Suite HTNVisualDebugger_OnCommand   — toggle, setScale no-op, malformed
//
// Feature spec: docs/specs/applications/dia/systems/diahtnvisualdebugger/diahtnvisualdebugger.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaHTNVisualDebugger/HTNVisualDebugger.h>
#include <DiaHTN/HTNPlannerComponent.h>
#include <DiaHTN/HTNPlanner.h>
#include <DiaHTN/HTNDomain.h>
#include <DiaHTN/OperatorRegistry.h>
#include <DiaHTN/Testing/HTNTestHelpers.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{

constexpr const char* kLinearDomainJson = R"({
    "tasks": {
        "Root": {
            "type": "compound",
            "methods": [{ "id": "m", "subtasks": ["A", "B", "C"] }]
        },
        "A": { "type": "primitive", "operator": "OpA", "params": [] },
        "B": { "type": "primitive", "operator": "OpB", "params": [] },
        "C": { "type": "primitive", "operator": "OpC", "params": [] }
    }
})";

Dia::HTN::HTNDomain LoadDomain(const char* json)
{
    Json::Value root;
    Json::Reader().parse(json, root);
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    return Dia::HTN::HTNDomain::LoadFromJson(root, errors);
}

Json::Value GetState(Dia::HTN::HTNVisualDebugger& domain)
{
    Json::Value out;
    domain.GetJSONState(out);
    return out;
}

Json::Value ToggleArgs(const char* drawerName)
{
    Json::Value args(Json::objectValue);
    args["drawer"] = drawerName;
    return args;
}

} // anonymous namespace

// ===========================================================================
// Identity
// ===========================================================================

TEST(HTNVisualDebugger_Identity, PrimitiveType_NoWorldDrawers)
{
    Dia::HTN::HTNPlannerComponent comp;
    Dia::HTN::HTNVisualDebugger domain(comp);

    EXPECT_FALSE(domain.HasWorldDrawers());
    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_EQ(domain.GetDrawer(0), nullptr);
}

TEST(HTNVisualDebugger_Identity, DomainId_And_Group_Canonical)
{
    Dia::HTN::HTNPlannerComponent comp;
    Dia::HTN::HTNVisualDebugger domain(comp);

    EXPECT_EQ(domain.GetDomainId(), Dia::Core::StringCRC("htn"));
    EXPECT_STREQ(domain.GetDisplayName(), "HTN");
    EXPECT_EQ(domain.GetGroup(), Dia::Core::StringCRC("AIBehavior"));
}

TEST(HTNVisualDebugger_Identity, AccentIsAIBehaviorConstant)
{
    Dia::HTN::HTNPlannerComponent comp;
    Dia::HTN::HTNVisualDebugger domain(comp);

    EXPECT_EQ(domain.GetAccentColour(), Dia::VisualDebugger::DebugGroupAccents::kAIBehavior);
}

TEST(HTNVisualDebugger_Identity, DescriptionWithin80Chars)
{
    Dia::HTN::HTNPlannerComponent comp;
    Dia::HTN::HTNVisualDebugger domain(comp);

    ASSERT_NE(domain.GetDescription(), nullptr);
    EXPECT_LE(strlen(domain.GetDescription()), 80u);
}

// ===========================================================================
// JSON state — contract shapes
// ===========================================================================

TEST(HTNVisualDebugger_JSONState, JSONRoundTrip_HasRequiredKeys)
{
    Dia::HTN::HTNPlannerComponent comp;
    Dia::HTN::HTNVisualDebugger domain(comp);

    const Json::Value state = GetState(domain);

    ASSERT_TRUE(state.isMember("drawers"))  << "drawers key required";
    ASSERT_TRUE(state.isMember("stats"))    << "stats key required";
    ASSERT_TRUE(state.isMember("diverged")) << "diverged key required";

    EXPECT_TRUE(state["drawers"].isArray());
    EXPECT_TRUE(state["stats"].isObject());
    EXPECT_TRUE(state["diverged"].isBool());
}

TEST(HTNVisualDebugger_JSONState, DrawersArray_HasPlanViewEntry)
{
    Dia::HTN::HTNPlannerComponent comp;
    Dia::HTN::HTNVisualDebugger domain(comp);

    const Json::Value state = GetState(domain);

    ASSERT_EQ(state["drawers"].size(), 1u);
    EXPECT_STREQ(state["drawers"][0u]["name"].asCString(), "PlanView");
    EXPECT_TRUE(state["drawers"][0u]["enabled"].asBool());
}

TEST(HTNVisualDebugger_JSONState, DrawerGate_DisablePlanView_RemovesPlanKey)
{
    Dia::HTN::HTNPlannerComponent comp;
    Dia::HTN::HTNVisualDebugger domain(comp);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("PlanView"));

    const Json::Value state = GetState(domain);
    EXPECT_FALSE(state.isMember("plan")) << "plan key must be absent when PlanView disabled";
}

// ===========================================================================
// OnCommand — contract shapes
// ===========================================================================

TEST(HTNVisualDebugger_OnCommand, OnCommandRoundTrip_ToggleTwiceRestores)
{
    Dia::HTN::HTNPlannerComponent comp;
    Dia::HTN::HTNVisualDebugger domain(comp);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("PlanView"));
    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("PlanView"));

    const Json::Value state = GetState(domain);
    EXPECT_TRUE(state.isMember("plan")) << "plan key must be present after toggling twice";
    EXPECT_TRUE(state["drawers"][0u]["enabled"].asBool());
}

TEST(HTNVisualDebugger_OnCommand, ScaleSensitivity_SetScale_NoOp)
{
    Dia::HTN::HTNPlannerComponent comp;
    Dia::HTN::HTNVisualDebugger domain(comp);

    Json::Value args(Json::objectValue);
    args["key"]   = "debugScale";
    args["value"] = 2.5;

    EXPECT_NO_FATAL_FAILURE(
        domain.OnCommand(Dia::Core::StringCRC("setScale"), args));
}

TEST(HTNVisualDebugger_OnCommand, UnknownCommand_NoOp)
{
    Dia::HTN::HTNPlannerComponent comp;
    Dia::HTN::HTNVisualDebugger domain(comp);

    Json::Value empty(Json::objectValue);
    EXPECT_NO_FATAL_FAILURE(
        domain.OnCommand(Dia::Core::StringCRC("notACommand"), empty));
}

TEST(HTNVisualDebugger_OnCommand, MalformedToggle_NoOp_NoCrash)
{
    Dia::HTN::HTNPlannerComponent comp;
    Dia::HTN::HTNVisualDebugger domain(comp);

    Json::Value empty(Json::objectValue);
    EXPECT_NO_FATAL_FAILURE(
        domain.OnCommand(Dia::Core::StringCRC("toggle"), empty));

    // State must be unchanged — plan key still present
    const Json::Value state = GetState(domain);
    EXPECT_TRUE(state.isMember("plan"));
}

#endif // DIA_DEBUG
