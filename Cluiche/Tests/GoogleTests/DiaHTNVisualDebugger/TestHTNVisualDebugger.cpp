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
#include <DiaDebugDraw/Domain/DebugGroupAccents.h>
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

// ===========================================================================
// Domain-specific shapes
// ===========================================================================

TEST(HTNVisualDebugger_JSONState, NoPlan_EmptyArray_NoAssert)
{
    // Component with no plan — should not crash and plan should be empty array
    Dia::HTN::HTNPlannerComponent comp;
    Dia::HTN::HTNVisualDebugger domain(comp);

    ASSERT_NO_FATAL_FAILURE({
        const Json::Value state = GetState(domain);
        EXPECT_TRUE(state.isMember("plan"));
        EXPECT_TRUE(state["plan"].isArray());
        EXPECT_EQ(state["plan"].size(), 0u);
    });
}

TEST(HTNVisualDebugger_JSONState, Stats_Current_And_Total_Match_NoPlan)
{
    Dia::HTN::HTNPlannerComponent comp;
    Dia::HTN::HTNVisualDebugger domain(comp);

    const Json::Value state = GetState(domain);

    EXPECT_EQ(state["stats"]["current"].asInt(), 0);
    EXPECT_EQ(state["stats"]["total"].asInt(), 0);
}

TEST(HTNVisualDebugger_JSONState, Stats_Current_And_Total_Match_WithPlan)
{
    auto domainObj = LoadDomain(kLinearDomainJson);
    Dia::HTN::OperatorRegistry reg;
    Dia::HTN::Testing::MockHTNContext ctx;

    Dia::HTN::HTNPlannerComponent comp;
    comp.SetDomain(&domainObj);
    comp.SetRegistry(&reg);
    comp.SetRootTask(Dia::Core::StringCRC("Root"));
    comp.Replan(ctx);

    Dia::HTN::HTNVisualDebugger domain(comp);
    const Json::Value state = GetState(domain);

    EXPECT_EQ(state["stats"]["total"].asInt(), 3) << "linear domain has 3 tasks";
    EXPECT_EQ(state["stats"]["current"].asInt(), 0) << "cursor starts at 0";
}

TEST(HTNVisualDebugger_JSONState, PlanArray_StatusCorrect_InitialCursor)
{
    auto domainObj = LoadDomain(kLinearDomainJson);
    Dia::HTN::OperatorRegistry reg;
    Dia::HTN::Testing::MockHTNContext ctx;

    Dia::HTN::HTNPlannerComponent comp;
    comp.SetDomain(&domainObj);
    comp.SetRegistry(&reg);
    comp.SetRootTask(Dia::Core::StringCRC("Root"));
    comp.Replan(ctx);

    Dia::HTN::HTNVisualDebugger domain(comp);
    const Json::Value state = GetState(domain);

    ASSERT_TRUE(state.isMember("plan"));
    ASSERT_EQ(state["plan"].size(), 3u);

    // cursor at 0: first is "current", rest are "pending"
    EXPECT_STREQ(state["plan"][0u]["status"].asCString(), "current");
    EXPECT_STREQ(state["plan"][1u]["status"].asCString(), "pending");
    EXPECT_STREQ(state["plan"][2u]["status"].asCString(), "pending");

    // 1-based index
    EXPECT_EQ(state["plan"][0u]["index"].asInt(), 1);
    EXPECT_EQ(state["plan"][1u]["index"].asInt(), 2);
    EXPECT_EQ(state["plan"][2u]["index"].asInt(), 3);
}

TEST(HTNVisualDebugger_JSONState, Diverged_Flag_AlwaysFalse_NoContext)
{
    // HasDiverged() requires IConditionContext which the debugger doesn't hold.
    // GetJSONState() always emits diverged:false as a safe default.
    Dia::HTN::HTNPlannerComponent comp;
    Dia::HTN::HTNVisualDebugger domain(comp);

    const Json::Value state = GetState(domain);
    EXPECT_FALSE(state["diverged"].asBool());
}

TEST(HTNVisualDebugger_JSONState, PlanArray_NamesMatchOperatorIds)
{
    auto domainObj = LoadDomain(kLinearDomainJson);
    Dia::HTN::OperatorRegistry reg;
    Dia::HTN::Testing::MockHTNContext ctx;

    Dia::HTN::HTNPlannerComponent comp;
    comp.SetDomain(&domainObj);
    comp.SetRegistry(&reg);
    comp.SetRootTask(Dia::Core::StringCRC("Root"));
    comp.Replan(ctx);

    Dia::HTN::HTNVisualDebugger domain(comp);
    const Json::Value state = GetState(domain);

    ASSERT_EQ(state["plan"].size(), 3u);
    EXPECT_STREQ(state["plan"][0u]["name"].asCString(), "OpA");
    EXPECT_STREQ(state["plan"][1u]["name"].asCString(), "OpB");
    EXPECT_STREQ(state["plan"][2u]["name"].asCString(), "OpC");
}

#endif // DIA_DEBUG
