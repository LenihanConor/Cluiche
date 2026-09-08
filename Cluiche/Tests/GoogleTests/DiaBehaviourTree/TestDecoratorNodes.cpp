#include <gtest/gtest.h>
#include <DiaBehaviourTree/BehaviourTreeComponent.h>
#include <DiaBehaviourTree/BehaviourTreeAsset.h>
#include <DiaBehaviourTree/ActionRegistry.h>
#include <DiaBehaviourTree/DecoratorRegistry.h>
#include <DiaBehaviourTree/IDecoratorNode.h>
#include <DiaBehaviourTree/NodeResult.h>
#include <DiaBlackboard/Blackboard.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::BehaviourTree;
using Dia::Core::StringCRC;
using Dia::Core::Containers::DynamicArrayC;

// ============================================================================
// Static spy globals — raw function pointers, no captures
// ============================================================================

static NodeResult gDecSpyResult     = NodeResult::kSuccess;
static int        gDecSpyCallCount  = 0;
static int        gDecSpySuccessMax = 999;  // succeed first N calls, then fail

static NodeResult DecoratorSpyAction(void*, const DynamicArrayC<StringCRC, 8>&)
{
    ++gDecSpyCallCount;
    if (gDecSpyCallCount > gDecSpySuccessMax)
        return NodeResult::kFailure;
    return gDecSpyResult;
}

// ============================================================================
// Helpers
// ============================================================================

namespace
{
    void ResetDecSpies()
    {
        gDecSpyResult     = NodeResult::kSuccess;
        gDecSpyCallCount  = 0;
        gDecSpySuccessMax = 999;
    }

    BehaviourTreeAsset LoadFromJsonString(const char* json)
    {
        Json::Value root;
        Json::Reader().parse(json, root);
        DynamicArrayC<const char*, 32> errors;
        return BehaviourTreeAsset::LoadFromJson(root, errors);
    }

    ActionRegistry MakeDecRegistry()
    {
        ActionRegistry registry;
        registry.Register(StringCRC{"DecSpyAction"}, DecoratorSpyAction);
        return registry;
    }
} // namespace

// ============================================================================
// Custom IDecoratorNode for registry dispatch test — static spy globals
// ============================================================================

static int        gMockShouldTickCallCount = 0;
static int        gMockEvaluateCallCount   = 0;
static bool       gMockShouldTickReturn    = true;
static NodeResult gMockEvaluateReturn      = NodeResult::kSuccess;

static void ResetMockSpies()
{
    gMockShouldTickCallCount = 0;
    gMockEvaluateCallCount   = 0;
    gMockShouldTickReturn    = true;
    gMockEvaluateReturn      = NodeResult::kSuccess;
}

class SpyDecoratorNode : public IDecoratorNode
{
public:
    bool ShouldTickChild(const DecoratorContext&) const override
    {
        printf("[SpyDecoratorNode] ShouldTickChild called! gMockShouldTickReturn=%d\n",
               gMockShouldTickReturn ? 1 : 0);
        fflush(stdout);
        ++gMockShouldTickCallCount;
        return gMockShouldTickReturn;
    }

    NodeResult Evaluate(NodeResult /*childResult*/, DecoratorContext&) const override
    {
        ++gMockEvaluateCallCount;
        return gMockEvaluateReturn;
    }

    void OnReset(DecoratorContext&) const override {}

    Dia::Core::StringCRC GetTypeId() const override
    {
        return Dia::Core::StringCRC{"mock_decorator"};
    }
};

static SpyDecoratorNode gSpyDecoratorNodeInstance;

// ============================================================================
// DiaBehaviourTree_Decorator — Inverter
// ============================================================================

TEST(DiaBehaviourTree_Decorator, Inverter_FlipsSuccess)
{
    ResetDecSpies();
    gDecSpyResult = NodeResult::kSuccess;

    BehaviourTreeAsset asset = LoadFromJsonString(R"({
        "root": "deco",
        "nodes": {
            "deco": { "type": "decorator", "decorator": "inverter", "child": "a" },
            "a":    { "type": "action", "action_id": "DecSpyAction", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    ActionRegistry registry = MakeDecRegistry();
    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kFailure);  // kSuccess → kFailure
    EXPECT_EQ(gDecSpyCallCount, 1);
}

TEST(DiaBehaviourTree_Decorator, Inverter_FlipsFailure)
{
    ResetDecSpies();
    gDecSpyResult = NodeResult::kFailure;

    BehaviourTreeAsset asset = LoadFromJsonString(R"({
        "root": "deco",
        "nodes": {
            "deco": { "type": "decorator", "decorator": "inverter", "child": "a" },
            "a":    { "type": "action", "action_id": "DecSpyAction", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    ActionRegistry registry = MakeDecRegistry();
    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kSuccess);  // kFailure → kSuccess
    EXPECT_EQ(gDecSpyCallCount, 1);
}

// ============================================================================
// DiaBehaviourTree_Decorator — Repeater
// ============================================================================

TEST(DiaBehaviourTree_Decorator, Repeater_N3_StopsAfter3)
{
    ResetDecSpies();
    gDecSpyResult = NodeResult::kSuccess;

    BehaviourTreeAsset asset = LoadFromJsonString(R"({
        "root": "rep",
        "nodes": {
            "rep": { "type": "decorator", "decorator": "repeater", "repeat_count": 3, "child": "a" },
            "a":   { "type": "action", "action_id": "DecSpyAction", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    ActionRegistry registry = MakeDecRegistry();
    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kRunning);  // count=1, need 2 more
    EXPECT_EQ(gDecSpyCallCount, 1);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kRunning);  // count=2, need 1 more
    EXPECT_EQ(gDecSpyCallCount, 2);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kSuccess);  // count=3 >= 3, done
    EXPECT_EQ(gDecSpyCallCount, 3);
}

TEST(DiaBehaviourTree_Decorator, Repeater_InfiniteUntilFailure)
{
    // repeat_count=0 (infinite), break_on_failure=true
    // child succeeds first 2 calls, fails on 3rd
    ResetDecSpies();
    gDecSpySuccessMax = 2;

    BehaviourTreeAsset asset = LoadFromJsonString(R"({
        "root": "rep",
        "nodes": {
            "rep": { "type": "decorator", "decorator": "repeater", "repeat_count": 0, "break_on_failure": true, "child": "a" },
            "a":   { "type": "action", "action_id": "DecSpyAction", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    ActionRegistry registry = MakeDecRegistry();
    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kRunning);  // child succeeds (call 1), loop
    EXPECT_EQ(gDecSpyCallCount, 1);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kRunning);  // child succeeds (call 2), loop
    EXPECT_EQ(gDecSpyCallCount, 2);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kFailure);  // child fails (call 3), break
    EXPECT_EQ(gDecSpyCallCount, 3);
}

// ============================================================================
// DiaBehaviourTree_Decorator — Cooldown
// ============================================================================

TEST(DiaBehaviourTree_Decorator, Cooldown_BlocksWithinWindow)
{
    ResetDecSpies();
    gDecSpyResult = NodeResult::kSuccess;

    BehaviourTreeAsset asset = LoadFromJsonString(R"({
        "root": "cd",
        "nodes": {
            "cd": { "type": "decorator", "decorator": "cooldown", "cooldown_seconds": 1.0, "child": "a" },
            "a":  { "type": "action", "action_id": "DecSpyAction", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    ActionRegistry registry = MakeDecRegistry();
    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    // accumulator=0.0 < 1.0 → child blocked, accumulator advances to 0.1
    EXPECT_EQ(comp.Tick(0.1f), NodeResult::kFailure);
    EXPECT_EQ(gDecSpyCallCount, 0);  // child must NOT have been called
}

TEST(DiaBehaviourTree_Decorator, Cooldown_PassesAfterElapsed)
{
    ResetDecSpies();
    gDecSpyResult = NodeResult::kSuccess;

    BehaviourTreeAsset asset = LoadFromJsonString(R"({
        "root": "cd",
        "nodes": {
            "cd": { "type": "decorator", "decorator": "cooldown", "cooldown_seconds": 0.5, "child": "a" },
            "a":  { "type": "action", "action_id": "DecSpyAction", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    ActionRegistry registry = MakeDecRegistry();
    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    // Tick 1: accumulator=0.0 < 0.5 → blocked, accumulator advances to 0.6
    EXPECT_EQ(comp.Tick(0.6f), NodeResult::kFailure);
    EXPECT_EQ(gDecSpyCallCount, 0);

    // Tick 2: accumulator=0.6 >= 0.5 → child is ticked, returns kSuccess
    EXPECT_EQ(comp.Tick(0.1f), NodeResult::kSuccess);
    EXPECT_EQ(gDecSpyCallCount, 1);
}

// ============================================================================
// DiaBehaviourTree_Decorator — Guard
// ============================================================================

TEST(DiaBehaviourTree_Decorator, Guard_FalseKey_ReturnsFailure)
{
    ResetDecSpies();

    BehaviourTreeAsset asset = LoadFromJsonString(R"({
        "root": "guard",
        "nodes": {
            "guard": { "type": "decorator", "decorator": "guard", "blackboard_key": "my_flag", "child": "a" },
            "a":     { "type": "action", "action_id": "DecSpyAction", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    Dia::Blackboard::Blackboard bb;
    bool& flag = bb.Register<bool>(StringCRC{"my_flag"});
    flag = false;

    ActionRegistry registry = MakeDecRegistry();
    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);
    comp.SetBlackboard(&bb);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kFailure);
    EXPECT_EQ(gDecSpyCallCount, 0);  // child must NOT have been called
}

TEST(DiaBehaviourTree_Decorator, Guard_TrueKey_TicksChild)
{
    ResetDecSpies();
    gDecSpyResult = NodeResult::kSuccess;

    BehaviourTreeAsset asset = LoadFromJsonString(R"({
        "root": "guard",
        "nodes": {
            "guard": { "type": "decorator", "decorator": "guard", "blackboard_key": "my_flag", "child": "a" },
            "a":     { "type": "action", "action_id": "DecSpyAction", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    Dia::Blackboard::Blackboard bb;
    bool& flag = bb.Register<bool>(StringCRC{"my_flag"});
    flag = true;

    ActionRegistry registry = MakeDecRegistry();
    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);
    comp.SetBlackboard(&bb);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kSuccess);
    EXPECT_EQ(gDecSpyCallCount, 1);  // child was called
}

// ============================================================================
// DiaBehaviourTree_Decorator — Custom IDecoratorNode via DecoratorRegistry
// ============================================================================

TEST(DiaBehaviourTree_Decorator, CustomDecorator_DispatchedViaRegistry)
{
    ResetDecSpies();
    ResetMockSpies();
    gDecSpyResult        = NodeResult::kSuccess;
    gMockShouldTickReturn = true;
    gMockEvaluateReturn   = NodeResult::kSuccess;

    BehaviourTreeAsset asset = LoadFromJsonString(R"({
        "root": "deco",
        "nodes": {
            "deco": { "type": "decorator", "decorator": "mock_decorator", "child": "a" },
            "a":    { "type": "action", "action_id": "DecSpyAction", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    // Verify the JSON parsed the decoratorType correctly
    const BehaviourTreeAsset::NodeDescriptor* decoNode = asset.GetNode(StringCRC{"deco"});
    ASSERT_NE(decoNode, nullptr);
    ASSERT_EQ(decoNode->decoratorType, StringCRC{"mock_decorator"});
    // Verify no CRC collision with built-in types
    ASSERT_NE(decoNode->decoratorType, StringCRC{"inverter"});
    ASSERT_NE(decoNode->decoratorType, StringCRC{"repeater"});
    ASSERT_NE(decoNode->decoratorType, StringCRC{"cooldown"});
    ASSERT_NE(decoNode->decoratorType, StringCRC{"guard"});

    DecoratorRegistry decRegistry;
    decRegistry.Register(StringCRC{"mock_decorator"}, &gSpyDecoratorNodeInstance);

    // Verify the registry find works independently
    ASSERT_EQ(decRegistry.Find(StringCRC{"mock_decorator"}), &gSpyDecoratorNodeInstance);

    ActionRegistry registry = MakeDecRegistry();
    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);
    comp.SetDecoratorRegistry(&decRegistry);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kSuccess);
    EXPECT_EQ(gMockShouldTickCallCount, 1);
    EXPECT_EQ(gMockEvaluateCallCount, 1);
    EXPECT_EQ(gDecSpyCallCount, 1);  // child was ticked (ShouldTickChild returned true)
}


TEST(DiaBehaviourTree_Decorator, CustomDecorator_ShouldNotTickChild_ReturnsFailure)
{
    // When ShouldTickChild returns false the child is never ticked
    ResetDecSpies();
    ResetMockSpies();
    gMockShouldTickReturn = false;

    BehaviourTreeAsset asset = LoadFromJsonString(R"({
        "root": "deco",
        "nodes": {
            "deco": { "type": "decorator", "decorator": "mock_decorator", "child": "a" },
            "a":    { "type": "action", "action_id": "DecSpyAction", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    DecoratorRegistry decRegistry;
    decRegistry.Register(StringCRC{"mock_decorator"}, &gSpyDecoratorNodeInstance);

    ActionRegistry registry = MakeDecRegistry();
    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);
    comp.SetDecoratorRegistry(&decRegistry);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kFailure);
    EXPECT_EQ(gMockShouldTickCallCount, 1);
    EXPECT_EQ(gMockEvaluateCallCount, 0);  // Evaluate must NOT be called
    EXPECT_EQ(gDecSpyCallCount, 0);        // child must NOT be called
}

// ============================================================================
// DiaBehaviourTree_Decorator — Reset() zeroes per-node decorator state
// ============================================================================

TEST(DiaBehaviourTree_Decorator, Reset_ZeroesCounterAndAccumulator)
{
    ResetDecSpies();
    gDecSpyResult = NodeResult::kSuccess;

    // Use a repeater (repeat_count=5) to exercise the counter path
    BehaviourTreeAsset asset = LoadFromJsonString(R"({
        "root": "rep",
        "nodes": {
            "rep": { "type": "decorator", "decorator": "repeater", "repeat_count": 5, "child": "a" },
            "a":   { "type": "action", "action_id": "DecSpyAction", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    ActionRegistry registry = MakeDecRegistry();
    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    // Advance counter to 2
    comp.Tick(0.0f);  // counter=1
    comp.Tick(0.0f);  // counter=2
    EXPECT_EQ(gDecSpyCallCount, 2);

    // Reset must clear all NodeState (counter→0, accumulator→0)
    comp.Reset();
    ResetDecSpies();

    // After reset, counter starts at 0 again → first tick is kRunning (not kSuccess)
    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kRunning);
    EXPECT_EQ(gDecSpyCallCount, 1);
}
