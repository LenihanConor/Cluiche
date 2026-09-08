#include <gtest/gtest.h>
#include <DiaBehaviourTree/Testing/BTTestHelpers.h>
#include <DiaBehaviourTree/BehaviourTreeComponent.h>
#include <DiaBehaviourTree/BehaviourTreeAsset.h>
#include <DiaBehaviourTree/ActionRegistry.h>
#include <DiaBehaviourTree/NodeResult.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::BehaviourTree;
using namespace Dia::BehaviourTree::Testing;
using Dia::Core::StringCRC;
using Dia::Core::Containers::DynamicArrayC;

// ============================================================================
// Helpers
// ============================================================================

namespace
{

BehaviourTreeAsset LoadJson(const char* json)
{
    Json::Value root;
    Json::Reader().parse(json, root);
    DynamicArrayC<const char*, 32> errors;
    return BehaviourTreeAsset::LoadFromJson(root, errors);
}

} // namespace

// ============================================================================
// DiaBehaviourTree_TestHelpers — SpyAction
// ============================================================================

TEST(DiaBehaviourTree_TestHelpers, SpyAction_RecordsCallCount)
{
    BehaviourTreeAsset asset = LoadJson(R"({
        "root": "a",
        "nodes": {
            "a": { "type": "action", "action_id": "SpyFn", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    SpyAction spy;
    ActionRegistry registry;
    spy.RegisterIn(registry, StringCRC{"SpyFn"});

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);
    comp.SetActionContext(spy.AsContext());

    comp.Tick(0.0f);
    EXPECT_EQ(spy.GetCallCount(), 1);

    comp.Tick(0.0f);
    EXPECT_EQ(spy.GetCallCount(), 2);
}

TEST(DiaBehaviourTree_TestHelpers, SpyAction_ForwardsParams)
{
    BehaviourTreeAsset asset = LoadJson(R"({
        "root": "a",
        "nodes": {
            "a": { "type": "action", "action_id": "SpyFn", "params": ["p1", "p2"] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    SpyAction spy;
    ActionRegistry registry;
    spy.RegisterIn(registry, StringCRC{"SpyFn"});

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);
    comp.SetActionContext(spy.AsContext());

    comp.Tick(0.0f);

    ASSERT_EQ(spy.LastParams().Size(), 2);
    EXPECT_EQ(spy.LastParams()[0], StringCRC{"p1"});
    EXPECT_EQ(spy.LastParams()[1], StringCRC{"p2"});
}

TEST(DiaBehaviourTree_TestHelpers, SpyAction_SetResultHonoured)
{
    BehaviourTreeAsset asset = LoadJson(R"({
        "root": "a",
        "nodes": {
            "a": { "type": "action", "action_id": "SpyFn", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    SpyAction spy;
    spy.SetResult(NodeResult::kFailure);

    ActionRegistry registry;
    spy.RegisterIn(registry, StringCRC{"SpyFn"});

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);
    comp.SetActionContext(spy.AsContext());

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kFailure);
}

TEST(DiaBehaviourTree_TestHelpers, AssertNodeVisited_PassesWhenNodeEntered)
{
    BehaviourTreeAsset asset = LoadJson(R"({
        "root": "a",
        "nodes": {
            "a": { "type": "action", "action_id": "SpyFn", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    SpyAction spy;
    ActionRegistry registry;
    spy.RegisterIn(registry, StringCRC{"SpyFn"});

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);
    comp.SetActionContext(spy.AsContext());

    // Should not fail — node "a" is the root and will be entered
    AssertNodeVisited(comp, StringCRC{"a"});
}

TEST(DiaBehaviourTree_TestHelpers, AssertLastResult_MatchesAfterTick)
{
    BehaviourTreeAsset asset = LoadJson(R"({
        "root": "a",
        "nodes": {
            "a": { "type": "action", "action_id": "SpyFn", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    SpyAction spy;
    spy.SetResult(NodeResult::kSuccess);

    ActionRegistry registry;
    spy.RegisterIn(registry, StringCRC{"SpyFn"});

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);
    comp.SetActionContext(spy.AsContext());

    comp.Tick(0.0f);
    AssertLastResult(comp, NodeResult::kSuccess);
}
