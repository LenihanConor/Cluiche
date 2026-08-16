#include <gtest/gtest.h>
#include <DiaBehaviourTree/BehaviourTreeComponent.h>
#include <DiaBehaviourTree/BehaviourTreeAsset.h>
#include <DiaBehaviourTree/ActionRegistry.h>
#include <DiaBehaviourTree/IBehaviourTreeEventListener.h>
#include <DiaBehaviourTree/NodeResult.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>

#include <vector>
#include <utility>

using namespace Dia::BehaviourTree;
using Dia::Core::StringCRC;
using Dia::Core::Containers::DynamicArrayC;

// ============================================================================
// Static action stubs
// ============================================================================

static NodeResult EvtSuccessAction(void*, const DynamicArrayC<StringCRC, 8>&)
{
    return NodeResult::kSuccess;
}

static NodeResult EvtFailAction(void*, const DynamicArrayC<StringCRC, 8>&)
{
    return NodeResult::kFailure;
}

// ============================================================================
// SpyEventListener
// ============================================================================

namespace
{

struct SpyEventListener : public IBehaviourTreeEventListener
{
    std::vector<StringCRC>                           enteredNodes;
    std::vector<std::pair<StringCRC, NodeResult>>    completedNodes;
    NodeResult                                       treeResult    = NodeResult::kRunning;
    bool                                             treeCompleted = false;

    void OnNodeEntered(StringCRC nodeId) override
    {
        enteredNodes.push_back(nodeId);
    }
    void OnNodeCompleted(StringCRC nodeId, NodeResult result) override
    {
        completedNodes.push_back({nodeId, result});
    }
    void OnTreeCompleted(NodeResult result) override
    {
        treeResult    = result;
        treeCompleted = true;
    }
    void Reset()
    {
        enteredNodes.clear();
        completedNodes.clear();
        treeResult    = NodeResult::kRunning;
        treeCompleted = false;
    }
};

BehaviourTreeAsset LoadFromJsonString(const char* json)
{
    Json::Value root;
    Json::Reader().parse(json, root);
    DynamicArrayC<const char*, 32> errors;
    return BehaviourTreeAsset::LoadFromJson(root, errors);
}

} // namespace

// ============================================================================
// DiaBehaviourTree_EventListener
// ============================================================================

TEST(DiaBehaviourTree_EventListener, OnNodeEntered_FiresPerVisitedNodeInOrder)
{
    BehaviourTreeAsset asset = LoadFromJsonString(R"({
        "root": "seq",
        "nodes": {
            "seq": { "type": "sequence", "children": ["a", "b"] },
            "a":   { "type": "action", "action_id": "SuccessAction", "params": [] },
            "b":   { "type": "action", "action_id": "SuccessAction", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    ActionRegistry registry;
    registry.Register(StringCRC{"SuccessAction"}, EvtSuccessAction);

    SpyEventListener spy;
    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);
    comp.AddEventListener(&spy);

    comp.Tick(0.0f);

    ASSERT_EQ(spy.enteredNodes.size(), 3u);
    EXPECT_EQ(spy.enteredNodes[0], StringCRC{"seq"});
    EXPECT_EQ(spy.enteredNodes[1], StringCRC{"a"});
    EXPECT_EQ(spy.enteredNodes[2], StringCRC{"b"});
}

TEST(DiaBehaviourTree_EventListener, OnNodeCompleted_FiresWithCorrectResult)
{
    BehaviourTreeAsset asset = LoadFromJsonString(R"({
        "root": "a",
        "nodes": {
            "a": { "type": "action", "action_id": "SuccessAction", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    ActionRegistry registry;
    registry.Register(StringCRC{"SuccessAction"}, EvtSuccessAction);

    SpyEventListener spy;
    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);
    comp.AddEventListener(&spy);

    comp.Tick(0.0f);

    ASSERT_EQ(spy.completedNodes.size(), 1u);
    EXPECT_EQ(spy.completedNodes[0].first, StringCRC{"a"});
    EXPECT_EQ(spy.completedNodes[0].second, NodeResult::kSuccess);
}

TEST(DiaBehaviourTree_EventListener, OnTreeCompleted_FiresOnSuccessAndFailure)
{
    ActionRegistry registry;
    registry.Register(StringCRC{"SuccessAction"}, EvtSuccessAction);
    registry.Register(StringCRC{"FailAction"}, EvtFailAction);

    // Success case
    {
        BehaviourTreeAsset asset = LoadFromJsonString(R"({
            "root": "a",
            "nodes": {
                "a": { "type": "action", "action_id": "SuccessAction", "params": [] }
            }
        })");
        ASSERT_TRUE(asset.IsValid());

        SpyEventListener spy;
        BehaviourTreeComponent comp;
        comp.SetAsset(&asset);
        comp.SetActionRegistry(&registry);
        comp.AddEventListener(&spy);

        comp.Tick(0.0f);

        EXPECT_TRUE(spy.treeCompleted);
        EXPECT_EQ(spy.treeResult, NodeResult::kSuccess);
    }

    // Failure case
    {
        BehaviourTreeAsset asset = LoadFromJsonString(R"({
            "root": "a",
            "nodes": {
                "a": { "type": "action", "action_id": "FailAction", "params": [] }
            }
        })");
        ASSERT_TRUE(asset.IsValid());

        SpyEventListener spy;
        BehaviourTreeComponent comp;
        comp.SetAsset(&asset);
        comp.SetActionRegistry(&registry);
        comp.AddEventListener(&spy);

        comp.Tick(0.0f);

        EXPECT_TRUE(spy.treeCompleted);
        EXPECT_EQ(spy.treeResult, NodeResult::kFailure);
    }
}

TEST(DiaBehaviourTree_EventListener, MultipleListeners_BothReceiveSameEvents)
{
    BehaviourTreeAsset asset = LoadFromJsonString(R"({
        "root": "a",
        "nodes": {
            "a": { "type": "action", "action_id": "SuccessAction", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    ActionRegistry registry;
    registry.Register(StringCRC{"SuccessAction"}, EvtSuccessAction);

    SpyEventListener spy1, spy2;
    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);
    comp.AddEventListener(&spy1);
    comp.AddEventListener(&spy2);

    comp.Tick(0.0f);

    EXPECT_EQ(spy1.enteredNodes.size(), spy2.enteredNodes.size());
    EXPECT_EQ(spy1.completedNodes.size(), spy2.completedNodes.size());
    EXPECT_EQ(spy1.treeResult, spy2.treeResult);
    EXPECT_EQ(spy1.treeCompleted, spy2.treeCompleted);
}

TEST(DiaBehaviourTree_EventListener, RemovedListener_DoesNotReceiveEvents)
{
    BehaviourTreeAsset asset = LoadFromJsonString(R"({
        "root": "a",
        "nodes": {
            "a": { "type": "action", "action_id": "SuccessAction", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    ActionRegistry registry;
    registry.Register(StringCRC{"SuccessAction"}, EvtSuccessAction);

    SpyEventListener spy;
    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);
    comp.AddEventListener(&spy);
    comp.RemoveEventListener(&spy);

    comp.Tick(0.0f);

    EXPECT_TRUE(spy.enteredNodes.empty());
    EXPECT_TRUE(spy.completedNodes.empty());
    EXPECT_FALSE(spy.treeCompleted);
}
