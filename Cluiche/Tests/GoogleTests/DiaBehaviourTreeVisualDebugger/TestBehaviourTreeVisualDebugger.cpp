////////////////////////////////////////////////////////////////////////////////
// TestBehaviourTreeVisualDebugger.cpp
// Mandatory IDebugDomain contract shapes for BehaviourTreeVisualDebugger.
//
//   Suite DiaBehaviourTreeVisualDebugger_Toggle — TreeView drawer default + toggle
//   Suite DiaBehaviourTreeVisualDebugger_JSON   — JSON schema, tick visits, asset
//
// System spec: docs/specs/applications/dia/systems/diabehaviourtreevisualdebugger/
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaBehaviourTreeVisualDebugger/BehaviourTreeVisualDebugger.h>
#include <DiaBehaviourTree/BehaviourTreeComponent.h>
#include <DiaBehaviourTree/BehaviourTreeAsset.h>
#include <DiaBehaviourTree/ActionRegistry.h>
#include <DiaBehaviourTree/NodeResult.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

using namespace Dia::BehaviourTree;
using Dia::Core::StringCRC;
using Dia::Core::Containers::DynamicArrayC;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{

// Single-action tree: root node "a" runs action "Spy" -> kSuccess
constexpr const char* kSimpleTreeJson = R"({
    "root": "a",
    "nodes": {
        "a": { "type": "action", "action_id": "Spy", "params": [] }
    }
})";

BehaviourTreeAsset LoadFromJsonString(const char* json)
{
    Json::Value root;
    Json::Reader().parse(json, root);
    DynamicArrayC<const char*, 32> errors;
    return BehaviourTreeAsset::LoadFromJson(root, errors);
}

static NodeResult SpyAction(void*, const DynamicArrayC<StringCRC, 8>&)
{
    return NodeResult::kSuccess;
}

Json::Value GetState(BehaviourTreeVisualDebugger& domain)
{
    Json::Value out;
    domain.GetJSONState(out);
    return out;
}

} // anonymous namespace

// ===========================================================================
// Toggle suite
// ===========================================================================

TEST(DiaBehaviourTreeVisualDebugger_Toggle, TreeViewEnabled_DefaultTrue)
{
    BehaviourTreeComponent comp;
    BehaviourTreeVisualDebugger domain(comp);

    const Json::Value state = GetState(domain);

    ASSERT_TRUE(state.isMember("drawers"));
    ASSERT_EQ(state["drawers"].size(), 1u);
    EXPECT_STREQ(state["drawers"][0u]["name"].asCString(), "TreeView");
    EXPECT_TRUE(state["drawers"][0u]["enabled"].asBool());
}

TEST(DiaBehaviourTreeVisualDebugger_Toggle, ToggleTreeView_DisablesDrawer)
{
    BehaviourTreeComponent comp;
    BehaviourTreeVisualDebugger domain(comp);

    Json::Value toggleArgs(Json::objectValue);
    toggleArgs["drawer"] = "TreeView";
    domain.OnCommand(StringCRC("toggle"), toggleArgs);

    const Json::Value state = GetState(domain);

    ASSERT_EQ(state["drawers"].size(), 1u);
    EXPECT_FALSE(state["drawers"][0u]["enabled"].asBool());
}

// ===========================================================================
// JSON suite
// ===========================================================================

TEST(DiaBehaviourTreeVisualDebugger_JSON, NoTick_EmitsEmptyLastTickNodes)
{
    BehaviourTreeComponent comp;
    BehaviourTreeVisualDebugger domain(comp);

    const Json::Value state = GetState(domain);

    ASSERT_TRUE(state.isMember("lastTickNodes"));
    EXPECT_TRUE(state["lastTickNodes"].isArray());
    EXPECT_EQ(state["lastTickNodes"].size(), 0u);

    ASSERT_TRUE(state.isMember("stats"));
    EXPECT_STREQ(state["stats"]["lastResult"].asCString(), "None");
}

TEST(DiaBehaviourTreeVisualDebugger_JSON, AfterTick_EmitsVisitedNodes)
{
    BehaviourTreeAsset asset = LoadFromJsonString(kSimpleTreeJson);
    ASSERT_TRUE(asset.IsValid());

    ActionRegistry registry;
    registry.Register(StringCRC{"Spy"}, SpyAction);

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    BehaviourTreeVisualDebugger domain(comp);

    comp.Tick(0.0f);

    const Json::Value state = GetState(domain);

    ASSERT_TRUE(state.isMember("lastTickNodes"));
    EXPECT_GT(state["lastTickNodes"].size(), 0u) << "at least one node should have been visited";

    ASSERT_TRUE(state.isMember("stats"));
    EXPECT_GT(state["stats"]["nodeCount"].asInt(), 0);
    EXPECT_STRNE(state["stats"]["lastResult"].asCString(), "None");
}

TEST(DiaBehaviourTreeVisualDebugger_JSON, OnTreeCompleted_ClearsBuffer)
{
    BehaviourTreeAsset asset = LoadFromJsonString(kSimpleTreeJson);
    ASSERT_TRUE(asset.IsValid());

    ActionRegistry registry;
    registry.Register(StringCRC{"Spy"}, SpyAction);

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    BehaviourTreeVisualDebugger domain(comp);

    // First tick — capture its node count
    comp.Tick(0.0f);
    const Json::Value state1 = GetState(domain);
    const unsigned int firstTickNodeCount = state1["lastTickNodes"].size();
    ASSERT_GT(firstTickNodeCount, 0u);

    // Reset and tick again so the second tick produces a fresh set of visits
    comp.Reset();
    comp.Tick(0.0f);
    const Json::Value state2 = GetState(domain);

    // Second GetJSONState should reflect only the second tick (same single-node tree)
    EXPECT_GT(state2["lastTickNodes"].size(), 0u) << "second tick should produce visits";
    EXPECT_EQ(state2["lastTickNodes"].size(), firstTickNodeCount)
        << "both ticks run the same tree so node count must match";
}

TEST(DiaBehaviourTreeVisualDebugger_JSON, NoAsset_OmitsRootNodeId)
{
    BehaviourTreeComponent comp;
    BehaviourTreeVisualDebugger domain(comp);

    const Json::Value state = GetState(domain);

    EXPECT_FALSE(state.isMember("rootNodeId"))
        << "rootNodeId must be absent when no asset is loaded";
}

#endif // DIA_DEBUG
