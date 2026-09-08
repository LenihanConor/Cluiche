#include <gtest/gtest.h>
#include <DiaBehaviourTree/BehaviourTreeComponent.h>
#include <DiaBehaviourTree/BehaviourTreeAsset.h>
#include <DiaBehaviourTree/ActionRegistry.h>
#include <DiaBehaviourTree/NodeResult.h>
#include <DiaBlackboard/Blackboard.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::BehaviourTree;
using Dia::Core::StringCRC;
using Dia::Core::Containers::DynamicArrayC;

// ============================================================================
// Helpers
// ============================================================================

namespace
{
    BehaviourTreeAsset LoadAsset(const char* json)
    {
        Json::Value root;
        Json::Reader().parse(json, root);
        DynamicArrayC<const char*, 32> errors;
        return BehaviourTreeAsset::LoadFromJson(root, errors);
    }

    struct SpyContext
    {
        int        callCount  = 0;
        NodeResult nextResult = NodeResult::kSuccess;
        DynamicArrayC<StringCRC, 8> lastParams;
    };

    // ActionFn is a raw function pointer — no lambdas with capture.
    static NodeResult SpyActionFn(void* ctx,
                                   const DynamicArrayC<StringCRC, 8>& params)
    {
        auto* spy = static_cast<SpyContext*>(ctx);
        spy->callCount++;
        spy->lastParams = params;
        return spy->nextResult;
    }

    // Returns kRunning on first call, kSuccess on second+.
    static NodeResult RunningThenSuccessActionFn(void* ctx,
                                                  const DynamicArrayC<StringCRC, 8>& /*params*/)
    {
        auto* spy = static_cast<SpyContext*>(ctx);
        spy->callCount++;
        return (spy->callCount == 1) ? NodeResult::kRunning : NodeResult::kSuccess;
    }

    constexpr const char* kConditionTreeJson =
        R"({"root":"C","nodes":{"C":{"type":"condition","blackboard_key":"flag"}}})";

    constexpr const char* kActionTreeJson =
        R"({"root":"A","nodes":{"A":{"type":"action","action_id":"DoThing","params":[]}}})";

    constexpr const char* kActionWithParamsJson =
        R"({"root":"A","nodes":{"A":{"type":"action","action_id":"Move","params":["wp"]}}})";

} // namespace

// ============================================================================
// DiaBehaviourTree_LeafNodes
// ============================================================================

TEST(DiaBehaviourTree_LeafNodes, ConditionLeaf_SlotTrue_ReturnsSuccess)
{
    BehaviourTreeAsset asset = LoadAsset(kConditionTreeJson);
    ASSERT_TRUE(asset.IsValid());

    Dia::Blackboard::Blackboard bb;
    bool& flag = bb.Register<bool>(StringCRC{"flag"});
    flag = true;

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetBlackboard(&bb);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kSuccess);
}

TEST(DiaBehaviourTree_LeafNodes, ConditionLeaf_SlotFalse_ReturnsFailure)
{
    BehaviourTreeAsset asset = LoadAsset(kConditionTreeJson);
    ASSERT_TRUE(asset.IsValid());

    Dia::Blackboard::Blackboard bb;
    bool& flag = bb.Register<bool>(StringCRC{"flag"});
    flag = false;

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetBlackboard(&bb);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kFailure);
}

TEST(DiaBehaviourTree_LeafNodes, ConditionLeaf_AbsentKey_ReturnsFailure)
{
    BehaviourTreeAsset asset = LoadAsset(kConditionTreeJson);
    ASSERT_TRUE(asset.IsValid());

    Dia::Blackboard::Blackboard bb;
    // "flag" slot is not registered

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetBlackboard(&bb);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kFailure);
}

TEST(DiaBehaviourTree_LeafNodes, ConditionLeaf_NullBlackboard_ReturnsFailure)
{
    BehaviourTreeAsset asset = LoadAsset(kConditionTreeJson);
    ASSERT_TRUE(asset.IsValid());

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetBlackboard(nullptr);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kFailure);
}

TEST(DiaBehaviourTree_LeafNodes, ActionLeaf_RegisteredAction_Dispatches)
{
    BehaviourTreeAsset asset = LoadAsset(kActionTreeJson);
    ASSERT_TRUE(asset.IsValid());

    SpyContext spy;
    ActionRegistry registry;
    registry.Register(StringCRC{"DoThing"}, SpyActionFn);

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);
    comp.SetActionContext(&spy);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kSuccess);
    EXPECT_EQ(spy.callCount, 1);
}

TEST(DiaBehaviourTree_LeafNodes, ActionLeaf_UnregisteredAction_ReturnsFailure)
{
    BehaviourTreeAsset asset = LoadAsset(kActionTreeJson);
    ASSERT_TRUE(asset.IsValid());

    ActionRegistry registry;
    // "DoThing" not registered

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kFailure);
}

TEST(DiaBehaviourTree_LeafNodes, ActionLeaf_ReturnsRunning_ComponentNotComplete)
{
    BehaviourTreeAsset asset = LoadAsset(kActionTreeJson);
    ASSERT_TRUE(asset.IsValid());

    SpyContext spy;
    spy.nextResult = NodeResult::kRunning;

    ActionRegistry registry;
    registry.Register(StringCRC{"DoThing"}, SpyActionFn);

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);
    comp.SetActionContext(&spy);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kRunning);
    EXPECT_FALSE(comp.IsComplete());
}

TEST(DiaBehaviourTree_LeafNodes, ActionLeaf_RunningThenSuccess_CompletesOnSecondTick)
{
    BehaviourTreeAsset asset = LoadAsset(kActionTreeJson);
    ASSERT_TRUE(asset.IsValid());

    SpyContext spy;
    ActionRegistry registry;
    registry.Register(StringCRC{"DoThing"}, RunningThenSuccessActionFn);

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);
    comp.SetActionContext(&spy);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kRunning);
    EXPECT_FALSE(comp.IsComplete());

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kSuccess);
    EXPECT_TRUE(comp.IsComplete());
}

TEST(DiaBehaviourTree_LeafNodes, ActionLeaf_WithParams_ParamsForwarded)
{
    BehaviourTreeAsset asset = LoadAsset(kActionWithParamsJson);
    ASSERT_TRUE(asset.IsValid());

    SpyContext spy;
    ActionRegistry registry;
    registry.Register(StringCRC{"Move"}, SpyActionFn);

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);
    comp.SetActionContext(&spy);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kSuccess);
    ASSERT_EQ(spy.lastParams.Size(), 1u);
    EXPECT_EQ(spy.lastParams[0], StringCRC{"wp"});
}

TEST(DiaBehaviourTree_LeafNodes, ActionLeaf_NullRegistry_ReturnsFailure)
{
    BehaviourTreeAsset asset = LoadAsset(kActionTreeJson);
    ASSERT_TRUE(asset.IsValid());

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(nullptr);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kFailure);
}
