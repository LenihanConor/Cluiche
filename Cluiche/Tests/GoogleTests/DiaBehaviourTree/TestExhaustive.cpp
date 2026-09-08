#include <gtest/gtest.h>
#include <DiaBehaviourTree/BehaviourTreeComponent.h>
#include <DiaBehaviourTree/BehaviourTreeAsset.h>
#include <DiaBehaviourTree/ActionRegistry.h>
#include <DiaBehaviourTree/DecoratorRegistry.h>
#include <DiaBehaviourTree/IBehaviourTreeEventListener.h>
#include <DiaBehaviourTree/BehaviourTreeSystem.h>
#include <DiaBehaviourTree/NodeResult.h>
#include <DiaBlackboard/Blackboard.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>

#include <vector>
#include <utility>

using namespace Dia::BehaviourTree;
using Dia::Core::StringCRC;
using Dia::Core::Containers::DynamicArrayC;

// ============================================================================
// Static action stubs & spy globals
// ============================================================================

static int        gExSpyCallCount = 0;
static NodeResult gExSpyResult    = NodeResult::kSuccess;

static NodeResult ExSuccessAction(void*, const DynamicArrayC<StringCRC, 8>&)
{
    return NodeResult::kSuccess;
}

static NodeResult ExFailAction(void*, const DynamicArrayC<StringCRC, 8>&)
{
    return NodeResult::kFailure;
}

static NodeResult ExSpyAction(void*, const DynamicArrayC<StringCRC, 8>&)
{
    ++gExSpyCallCount;
    return gExSpyResult;
}

// For ResetMidExecution test
static int gRunCount = 0;
static NodeResult RunThenSuccessAction(void*, const DynamicArrayC<StringCRC, 8>&)
{
    ++gRunCount;
    return (gRunCount == 1) ? NodeResult::kRunning : NodeResult::kSuccess;
}

// For IsComplete_FalseWhenRunning test
static NodeResult RunningFn(void*, const DynamicArrayC<StringCRC, 8>&)
{
    return NodeResult::kRunning;
}

// For Selector_RunningChild_ResumedNextTick test
static int gStage = 0;
static NodeResult StagedAction(void*, const DynamicArrayC<StringCRC, 8>&)
{
    ++gStage;
    return (gStage == 1) ? NodeResult::kRunning : NodeResult::kSuccess;
}

// ============================================================================
// Helpers
// ============================================================================

namespace
{

void ResetExSpy()
{
    gExSpyCallCount = 0;
    gExSpyResult    = NodeResult::kSuccess;
}

BehaviourTreeAsset LoadJson(const char* json)
{
    Json::Value root;
    Json::Reader().parse(json, root);
    DynamicArrayC<const char*, 32> errors;
    return BehaviourTreeAsset::LoadFromJson(root, errors);
}

struct SpyListener : public IBehaviourTreeEventListener
{
    std::vector<StringCRC>                        enteredNodes;
    std::vector<std::pair<StringCRC, NodeResult>> completedNodes;
    NodeResult treeResult    = NodeResult::kRunning;
    bool       treeCompleted = false;

    void OnNodeEntered(StringCRC id) override { enteredNodes.push_back(id); }
    void OnNodeCompleted(StringCRC id, NodeResult r) override { completedNodes.push_back({id, r}); }
    void OnTreeCompleted(NodeResult r) override { treeResult = r; treeCompleted = true; }
};

struct TickSpy : public IBehaviourTreeEventListener
{
    int tickCount = 0;
    void OnNodeEntered(StringCRC) override {}
    void OnNodeCompleted(StringCRC, NodeResult) override {}
    void OnTreeCompleted(NodeResult) override { ++tickCount; }
};

} // namespace

// ============================================================================
// DiaBehaviourTree_Exhaustive
// ============================================================================

TEST(DiaBehaviourTree_Exhaustive, EmptyNodes_IsInvalid)
{
    BehaviourTreeAsset asset = LoadJson(R"({"root":"a","nodes":{}})");
    EXPECT_FALSE(asset.IsValid());
}

TEST(DiaBehaviourTree_Exhaustive, SingleConditionNode_TrueBlackboard_ReturnsSuccess)
{
    BehaviourTreeAsset asset = LoadJson(R"({
        "root": "c",
        "nodes": {
            "c": { "type": "condition", "blackboard_key": "flag" }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    Dia::Blackboard::Blackboard bb;
    bool& flag = bb.Register<bool>(StringCRC{"flag"});
    flag = true;

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetBlackboard(&bb);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kSuccess);
}

TEST(DiaBehaviourTree_Exhaustive, SingleActionNode_ReturnsSuccess)
{
    BehaviourTreeAsset asset = LoadJson(R"({
        "root": "a",
        "nodes": {
            "a": { "type": "action", "action_id": "ExSuccess", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    ActionRegistry registry;
    registry.Register(StringCRC{"ExSuccess"}, ExSuccessAction);

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kSuccess);
}

TEST(DiaBehaviourTree_Exhaustive, DeepNesting_SequenceDecoratorSelectorCondition)
{
    // Sequence → Inverter → Selector → condition(true)
    // condition(true) → kSuccess; Selector succeeds; Inverter flips to kFailure; Sequence fails
    BehaviourTreeAsset asset = LoadJson(R"({
        "root": "seq",
        "nodes": {
            "seq":  { "type": "sequence", "children": ["inv"] },
            "inv":  { "type": "decorator", "decorator": "inverter", "child": "sel" },
            "sel":  { "type": "selector", "children": ["cond"] },
            "cond": { "type": "condition", "blackboard_key": "flag" }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    Dia::Blackboard::Blackboard bb;
    bool& flag = bb.Register<bool>(StringCRC{"flag"});
    flag = true;

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetBlackboard(&bb);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kFailure);
}

TEST(DiaBehaviourTree_Exhaustive, DecoratorChaining_InverterWrappingCooldown_BlockedBecomesSuccess)
{
    // Cooldown blocks child (returns kFailure); Inverter flips to kSuccess.
    ResetExSpy();

    BehaviourTreeAsset asset = LoadJson(R"({
        "root": "inv",
        "nodes": {
            "inv": { "type": "decorator", "decorator": "inverter", "child": "cd" },
            "cd":  { "type": "decorator", "decorator": "cooldown", "cooldown_seconds": 1.0, "child": "spy" },
            "spy": { "type": "action", "action_id": "ExSpy", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    ActionRegistry registry;
    registry.Register(StringCRC{"ExSpy"}, ExSpyAction);

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    // Cooldown accumulator=0 < 1.0 → kFailure → Inverter → kSuccess
    EXPECT_EQ(comp.Tick(0.1f), NodeResult::kSuccess);
    EXPECT_EQ(gExSpyCallCount, 0);  // child never reached
}

TEST(DiaBehaviourTree_Exhaustive, RepeaterBreakOnFailure_InsideSequence)
{
    // Sequence: [always_success, repeater(break_on_failure, child=fail)]
    // First tick: ok succeeds; repeater ticks fail → breaks → kFailure; Sequence fails.
    ResetExSpy();
    gExSpyResult = NodeResult::kFailure;

    BehaviourTreeAsset asset = LoadJson(R"({
        "root": "seq",
        "nodes": {
            "seq": { "type": "sequence", "children": ["ok", "rep"] },
            "ok":  { "type": "action", "action_id": "ExSuccess", "params": [] },
            "rep": { "type": "decorator", "decorator": "repeater", "repeat_count": 0, "break_on_failure": true, "child": "spy" },
            "spy": { "type": "action", "action_id": "ExSpy", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    ActionRegistry registry;
    registry.Register(StringCRC{"ExSuccess"}, ExSuccessAction);
    registry.Register(StringCRC{"ExSpy"}, ExSpyAction);

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kFailure);
    EXPECT_EQ(gExSpyCallCount, 1);
}

TEST(DiaBehaviourTree_Exhaustive, GuardAndParallel_GuardFalse_RequireAllFails)
{
    // Parallel(require_all): [success_action, guard(flag=false)→fails]
    // require_all: one failure → overall kFailure
    BehaviourTreeAsset asset = LoadJson(R"({
        "root": "par",
        "nodes": {
            "par":  { "type": "parallel", "policy": "require_all", "children": ["ok", "gd"] },
            "ok":   { "type": "action", "action_id": "ExSuccess", "params": [] },
            "gd":   { "type": "decorator", "decorator": "guard", "blackboard_key": "flag", "child": "ok2" },
            "ok2":  { "type": "action", "action_id": "ExSuccess", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    Dia::Blackboard::Blackboard bb;
    bool& flag = bb.Register<bool>(StringCRC{"flag"});
    flag = false;

    ActionRegistry registry;
    registry.Register(StringCRC{"ExSuccess"}, ExSuccessAction);

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);
    comp.SetBlackboard(&bb);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kFailure);
}

TEST(DiaBehaviourTree_Exhaustive, ResetMidExecution_RestartsFromRoot)
{
    // Tick 1: run→kRunning; Reset; Tick 2: run ticked again from root
    gRunCount = 0;

    BehaviourTreeAsset asset = LoadJson(R"({
        "root": "seq",
        "nodes": {
            "seq": { "type": "sequence", "children": ["run", "ok"] },
            "run": { "type": "action", "action_id": "RunThenSuccess", "params": [] },
            "ok":  { "type": "action", "action_id": "ExSuccess", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    ActionRegistry registry;
    registry.Register(StringCRC{"RunThenSuccess"}, RunThenSuccessAction);
    registry.Register(StringCRC{"ExSuccess"}, ExSuccessAction);

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kRunning);  // gRunCount=1, returns Running

    comp.Reset();
    gRunCount = 0;  // reset so next call returns kRunning again

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kRunning);  // restarted from root
    EXPECT_EQ(gRunCount, 1);  // "run" was ticked from the start
}

TEST(DiaBehaviourTree_Exhaustive, MultipleListeners_IdenticalEventSequences)
{
    BehaviourTreeAsset asset = LoadJson(R"({
        "root": "seq",
        "nodes": {
            "seq": { "type": "sequence", "children": ["a", "b"] },
            "a":   { "type": "action", "action_id": "ExSuccess", "params": [] },
            "b":   { "type": "action", "action_id": "ExSuccess", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    ActionRegistry registry;
    registry.Register(StringCRC{"ExSuccess"}, ExSuccessAction);

    SpyListener spy1, spy2, spy3;
    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);
    comp.AddEventListener(&spy1);
    comp.AddEventListener(&spy2);
    comp.AddEventListener(&spy3);

    comp.Tick(0.0f);

    ASSERT_EQ(spy1.enteredNodes.size(), spy2.enteredNodes.size());
    ASSERT_EQ(spy2.enteredNodes.size(), spy3.enteredNodes.size());
    for (size_t i = 0; i < spy1.enteredNodes.size(); ++i)
    {
        EXPECT_EQ(spy1.enteredNodes[i], spy2.enteredNodes[i]);
        EXPECT_EQ(spy2.enteredNodes[i], spy3.enteredNodes[i]);
    }
    EXPECT_EQ(spy1.treeResult, NodeResult::kSuccess);
    EXPECT_EQ(spy2.treeResult, NodeResult::kSuccess);
    EXPECT_EQ(spy3.treeResult, NodeResult::kSuccess);
}

TEST(DiaBehaviourTree_Exhaustive, NullBlackboard_GuardNode_ReturnsFailure)
{
    BehaviourTreeAsset asset = LoadJson(R"({
        "root": "gd",
        "nodes": {
            "gd": { "type": "decorator", "decorator": "guard", "blackboard_key": "flag", "child": "a" },
            "a":  { "type": "action", "action_id": "ExSuccess", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    ActionRegistry registry;
    registry.Register(StringCRC{"ExSuccess"}, ExSuccessAction);

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);
    // No blackboard set — Guard should return kFailure immediately

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kFailure);
}

TEST(DiaBehaviourTree_Exhaustive, System_RoundRobin_AllComponentsTickedOverTwoFrames)
{
    struct Fixture
    {
        BehaviourTreeAsset asset;
        ActionRegistry     registry;
        BehaviourTreeComponent comp;
        TickSpy            spy;

        Fixture()
            : asset(LoadJson(R"({"root":"a","nodes":{"a":{"type":"action","action_id":"ExSuc","params":[]}}})"))
        {
            registry.Register(StringCRC{"ExSuc"}, ExSuccessAction);
            comp.SetAsset(&asset);
            comp.SetActionRegistry(&registry);
            comp.AddEventListener(&spy);
        }
    };

    Fixture g1, g2, g3;
    BehaviourTreeSystem sys;
    sys.Register(&g1.comp);
    sys.Register(&g2.comp);
    sys.Register(&g3.comp);

    sys.Update(9999.0f, 0.016f);
    sys.Update(9999.0f, 0.016f);

    EXPECT_EQ(g1.spy.tickCount, 2);
    EXPECT_EQ(g2.spy.tickCount, 2);
    EXPECT_EQ(g3.spy.tickCount, 2);
}

TEST(DiaBehaviourTree_Exhaustive, IsComplete_TrueAfterSuccess_FalseAfterRunning)
{
    ActionRegistry registry;
    registry.Register(StringCRC{"ExSuccess"}, ExSuccessAction);
    registry.Register(StringCRC{"RunAct"}, RunningFn);

    // Success → IsComplete true
    {
        BehaviourTreeAsset asset = LoadJson(R"({
            "root": "a",
            "nodes": { "a": { "type": "action", "action_id": "ExSuccess", "params": [] } }
        })");
        ASSERT_TRUE(asset.IsValid());

        BehaviourTreeComponent comp;
        comp.SetAsset(&asset);
        comp.SetActionRegistry(&registry);

        EXPECT_FALSE(comp.IsComplete());
        comp.Tick(0.0f);
        EXPECT_TRUE(comp.IsComplete());
        EXPECT_EQ(comp.LastResult(), NodeResult::kSuccess);
    }

    // Running → IsComplete false
    {
        BehaviourTreeAsset asset = LoadJson(R"({
            "root": "a",
            "nodes": { "a": { "type": "action", "action_id": "RunAct", "params": [] } }
        })");
        ASSERT_TRUE(asset.IsValid());

        BehaviourTreeComponent comp;
        comp.SetAsset(&asset);
        comp.SetActionRegistry(&registry);

        comp.Tick(0.0f);
        EXPECT_FALSE(comp.IsComplete());
        EXPECT_EQ(comp.LastResult(), NodeResult::kRunning);
    }
}

TEST(DiaBehaviourTree_Exhaustive, Selector_RunningChild_ResumedNextTick)
{
    // Selector: [staged_action, fail_action]
    // Tick 1: staged returns kRunning → selector kRunning (cursor=0)
    // Tick 2: staged returns kSuccess → selector kSuccess (never visits fail)
    gStage = 0;

    BehaviourTreeAsset asset = LoadJson(R"({
        "root": "sel",
        "nodes": {
            "sel":  { "type": "selector", "children": ["stg", "fail"] },
            "stg":  { "type": "action", "action_id": "StagedAct", "params": [] },
            "fail": { "type": "action", "action_id": "ExFail", "params": [] }
        }
    })");
    ASSERT_TRUE(asset.IsValid());

    ActionRegistry registry;
    registry.Register(StringCRC{"StagedAct"}, StagedAction);
    registry.Register(StringCRC{"ExFail"}, ExFailAction);

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kRunning);
    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kSuccess);
}
