#include <gtest/gtest.h>
#include <DiaBehaviourTree/BehaviourTreeComponent.h>
#include <DiaBehaviourTree/BehaviourTreeAsset.h>
#include <DiaBehaviourTree/ActionRegistry.h>
#include <DiaBehaviourTree/NodeResult.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::BehaviourTree;
using Dia::Core::StringCRC;
using Dia::Core::Containers::DynamicArrayC;

// ============================================================================
// Static spy globals — raw function pointers, no captures
// ============================================================================

static NodeResult gSpyAResult    = NodeResult::kSuccess;
static int        gSpyACallCount = 0;

static NodeResult gSpyBResult    = NodeResult::kSuccess;
static int        gSpyBCallCount = 0;

static NodeResult SpyActionA(void*, const DynamicArrayC<StringCRC, 8>&)
{
    ++gSpyACallCount;
    return gSpyAResult;
}

static NodeResult SpyActionB(void*, const DynamicArrayC<StringCRC, 8>&)
{
    ++gSpyBCallCount;
    return gSpyBResult;
}

// Variants that return kRunning on first call, kSuccess on subsequent calls
static NodeResult SpyActionA_RunningOnce(void*, const DynamicArrayC<StringCRC, 8>&)
{
    ++gSpyACallCount;
    return (gSpyACallCount == 1) ? NodeResult::kRunning : NodeResult::kSuccess;
}

static NodeResult SpyActionB_RunningOnce(void*, const DynamicArrayC<StringCRC, 8>&)
{
    ++gSpyBCallCount;
    return (gSpyBCallCount == 1) ? NodeResult::kRunning : NodeResult::kSuccess;
}

// ============================================================================
// Helpers
// ============================================================================

namespace
{
    void ResetSpies()
    {
        gSpyAResult    = NodeResult::kSuccess;
        gSpyACallCount = 0;
        gSpyBResult    = NodeResult::kSuccess;
        gSpyBCallCount = 0;
    }

    BehaviourTreeAsset LoadFromJsonString(const char* json)
    {
        Json::Value root;
        Json::Reader().parse(json, root);
        DynamicArrayC<const char*, 32> errors;
        return BehaviourTreeAsset::LoadFromJson(root, errors);
    }

    BehaviourTreeAsset MakeSequenceTree()
    {
        return LoadFromJsonString(R"({
            "root": "seq",
            "nodes": {
                "seq": { "type": "sequence", "children": ["a", "b"] },
                "a":   { "type": "action", "action_id": "ActionA", "params": [] },
                "b":   { "type": "action", "action_id": "ActionB", "params": [] }
            }
        })");
    }

    BehaviourTreeAsset MakeSelectorTree()
    {
        return LoadFromJsonString(R"({
            "root": "sel",
            "nodes": {
                "sel": { "type": "selector", "children": ["a", "b"] },
                "a":   { "type": "action", "action_id": "ActionA", "params": [] },
                "b":   { "type": "action", "action_id": "ActionB", "params": [] }
            }
        })");
    }

    BehaviourTreeAsset MakeParallelTree(const char* policy)
    {
        char buf[512];
        snprintf(buf, sizeof(buf), R"({
            "root": "par",
            "nodes": {
                "par": { "type": "parallel", "policy": "%s", "children": ["a", "b"] },
                "a":   { "type": "action", "action_id": "ActionA", "params": [] },
                "b":   { "type": "action", "action_id": "ActionB", "params": [] }
            }
        })", policy);
        return LoadFromJsonString(buf);
    }

    ActionRegistry MakeRegistry()
    {
        ActionRegistry registry;
        registry.Register(StringCRC{"ActionA"}, SpyActionA);
        registry.Register(StringCRC{"ActionB"}, SpyActionB);
        return registry;
    }
} // namespace

// ============================================================================
// DiaBehaviourTree_ControlFlow — Sequence
// ============================================================================

TEST(DiaBehaviourTree_ControlFlow, Sequence_AllChildrenSucceed_ReturnsSuccess)
{
    ResetSpies();
    BehaviourTreeAsset asset = MakeSequenceTree();
    ASSERT_TRUE(asset.IsValid());

    gSpyAResult = NodeResult::kSuccess;
    gSpyBResult = NodeResult::kSuccess;

    ActionRegistry registry = MakeRegistry();
    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kSuccess);
    EXPECT_EQ(gSpyACallCount, 1);
    EXPECT_EQ(gSpyBCallCount, 1);
}

TEST(DiaBehaviourTree_ControlFlow, Sequence_FirstChildFails_ReturnsFailureImmediately)
{
    ResetSpies();
    BehaviourTreeAsset asset = MakeSequenceTree();
    ASSERT_TRUE(asset.IsValid());

    gSpyAResult = NodeResult::kFailure;
    gSpyBResult = NodeResult::kSuccess;

    ActionRegistry registry = MakeRegistry();
    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kFailure);
    EXPECT_EQ(gSpyACallCount, 1);
    EXPECT_EQ(gSpyBCallCount, 0);  // B must NOT be called
}

TEST(DiaBehaviourTree_ControlFlow, Sequence_SecondChildFails_ReturnsFailure)
{
    ResetSpies();
    BehaviourTreeAsset asset = MakeSequenceTree();
    ASSERT_TRUE(asset.IsValid());

    gSpyAResult = NodeResult::kSuccess;
    gSpyBResult = NodeResult::kFailure;

    ActionRegistry registry = MakeRegistry();
    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kFailure);
    EXPECT_EQ(gSpyACallCount, 1);
    EXPECT_EQ(gSpyBCallCount, 1);
}

TEST(DiaBehaviourTree_ControlFlow, Sequence_ChildRunning_ReturnsRunning)
{
    ResetSpies();
    BehaviourTreeAsset asset = MakeSequenceTree();
    ASSERT_TRUE(asset.IsValid());

    gSpyAResult = NodeResult::kRunning;

    ActionRegistry registry = MakeRegistry();
    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kRunning);
    EXPECT_EQ(gSpyACallCount, 1);
}

TEST(DiaBehaviourTree_ControlFlow, Sequence_ResumesFromRunningChild)
{
    // A returns kSuccess, B returns kRunning on tick 1.
    // On tick 2: A must NOT be called again, B is called and returns kSuccess.
    ResetSpies();
    BehaviourTreeAsset asset = MakeSequenceTree();
    ASSERT_TRUE(asset.IsValid());

    gSpyAResult = NodeResult::kSuccess;  // A always succeeds

    ActionRegistry registry;
    registry.Register(StringCRC{"ActionA"}, SpyActionA);
    registry.Register(StringCRC{"ActionB"}, SpyActionB_RunningOnce);  // kRunning first, kSuccess second

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kRunning);  // tick 1: A->kSuccess, B->kRunning
    EXPECT_EQ(gSpyACallCount, 1);
    EXPECT_EQ(gSpyBCallCount, 1);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kSuccess);  // tick 2: B->kSuccess (A not called)
    EXPECT_EQ(gSpyACallCount, 1);   // A called only once total
    EXPECT_EQ(gSpyBCallCount, 2);   // B called twice total
}

// ============================================================================
// DiaBehaviourTree_ControlFlow — Selector
// ============================================================================

TEST(DiaBehaviourTree_ControlFlow, Selector_FirstChildSucceeds_ReturnsSuccess)
{
    ResetSpies();
    BehaviourTreeAsset asset = MakeSelectorTree();
    ASSERT_TRUE(asset.IsValid());

    gSpyAResult = NodeResult::kSuccess;

    ActionRegistry registry = MakeRegistry();
    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kSuccess);
    EXPECT_EQ(gSpyACallCount, 1);
    EXPECT_EQ(gSpyBCallCount, 0);  // B must NOT be called
}

TEST(DiaBehaviourTree_ControlFlow, Selector_AllChildrenFail_ReturnsFailure)
{
    ResetSpies();
    BehaviourTreeAsset asset = MakeSelectorTree();
    ASSERT_TRUE(asset.IsValid());

    gSpyAResult = NodeResult::kFailure;
    gSpyBResult = NodeResult::kFailure;

    ActionRegistry registry = MakeRegistry();
    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kFailure);
    EXPECT_EQ(gSpyACallCount, 1);
    EXPECT_EQ(gSpyBCallCount, 1);
}

TEST(DiaBehaviourTree_ControlFlow, Selector_FirstFailsThenSecondSucceeds)
{
    ResetSpies();
    BehaviourTreeAsset asset = MakeSelectorTree();
    ASSERT_TRUE(asset.IsValid());

    gSpyAResult = NodeResult::kFailure;
    gSpyBResult = NodeResult::kSuccess;

    ActionRegistry registry = MakeRegistry();
    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kSuccess);
    EXPECT_EQ(gSpyACallCount, 1);
    EXPECT_EQ(gSpyBCallCount, 1);
}

TEST(DiaBehaviourTree_ControlFlow, Selector_ChildRunning_ResumesFromSameChild)
{
    // A returns kRunning on tick 1, then kSuccess on tick 2.
    // B must never be called.
    ResetSpies();
    BehaviourTreeAsset asset = MakeSelectorTree();
    ASSERT_TRUE(asset.IsValid());

    ActionRegistry registry;
    registry.Register(StringCRC{"ActionA"}, SpyActionA_RunningOnce);
    registry.Register(StringCRC{"ActionB"}, SpyActionB);

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kRunning);  // tick 1: A->kRunning
    EXPECT_EQ(gSpyACallCount, 1);
    EXPECT_EQ(gSpyBCallCount, 0);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kSuccess);  // tick 2: A->kSuccess (resume from A, not B)
    EXPECT_EQ(gSpyACallCount, 2);   // A called twice total
    EXPECT_EQ(gSpyBCallCount, 0);   // B never called
}

// ============================================================================
// DiaBehaviourTree_ControlFlow — Parallel
// ============================================================================

TEST(DiaBehaviourTree_ControlFlow, Parallel_RequireAll_AllSucceed)
{
    ResetSpies();
    BehaviourTreeAsset asset = MakeParallelTree("require_all");
    ASSERT_TRUE(asset.IsValid());

    gSpyAResult = NodeResult::kSuccess;
    gSpyBResult = NodeResult::kSuccess;

    ActionRegistry registry = MakeRegistry();
    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kSuccess);
}

TEST(DiaBehaviourTree_ControlFlow, Parallel_RequireAll_OneFails)
{
    ResetSpies();
    BehaviourTreeAsset asset = MakeParallelTree("require_all");
    ASSERT_TRUE(asset.IsValid());

    gSpyAResult = NodeResult::kFailure;
    gSpyBResult = NodeResult::kSuccess;

    ActionRegistry registry = MakeRegistry();
    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kFailure);
}

TEST(DiaBehaviourTree_ControlFlow, Parallel_RequireOne_OneSucceeds)
{
    ResetSpies();
    BehaviourTreeAsset asset = MakeParallelTree("require_one");
    ASSERT_TRUE(asset.IsValid());

    gSpyAResult = NodeResult::kSuccess;
    gSpyBResult = NodeResult::kFailure;

    ActionRegistry registry = MakeRegistry();
    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kSuccess);
}

TEST(DiaBehaviourTree_ControlFlow, Parallel_RequireOne_AllFail)
{
    ResetSpies();
    BehaviourTreeAsset asset = MakeParallelTree("require_one");
    ASSERT_TRUE(asset.IsValid());

    gSpyAResult = NodeResult::kFailure;
    gSpyBResult = NodeResult::kFailure;

    ActionRegistry registry = MakeRegistry();
    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kFailure);
}

TEST(DiaBehaviourTree_ControlFlow, Parallel_RequireNone)
{
    // tick 1: A->kRunning (still running), B->kSuccess (done)
    //   doneCount=1, totalChildren=2, require_none -> kRunning
    // tick 2: A->kSuccess (done)
    //   doneCount=2, totalChildren=2, require_none -> kSuccess
    ResetSpies();
    BehaviourTreeAsset asset = MakeParallelTree("require_none");
    ASSERT_TRUE(asset.IsValid());

    gSpyBResult = NodeResult::kSuccess;  // B always succeeds

    ActionRegistry registry;
    registry.Register(StringCRC{"ActionA"}, SpyActionA_RunningOnce);
    registry.Register(StringCRC{"ActionB"}, SpyActionB);

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kRunning);  // tick 1: A running, B done (1 of 2 done)
    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kSuccess);  // tick 2: A done too (2 of 2 done)
}

TEST(DiaBehaviourTree_ControlFlow, Parallel_RequireAll_MidExecution)
{
    // tick 1: A->kSuccess (done, bit 0), B->kRunning (not done)
    //   require_all: failCount=0 ok, doneCount=1 != 2 -> kRunning
    // tick 2: A is already done (skip), B->kSuccess (done)
    //   require_all: failCount=0, doneCount=2 -> kSuccess
    //   A must NOT be called on tick 2.
    ResetSpies();
    BehaviourTreeAsset asset = MakeParallelTree("require_all");
    ASSERT_TRUE(asset.IsValid());

    gSpyAResult = NodeResult::kSuccess;  // A always succeeds

    ActionRegistry registry;
    registry.Register(StringCRC{"ActionA"}, SpyActionA);
    registry.Register(StringCRC{"ActionB"}, SpyActionB_RunningOnce);

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetActionRegistry(&registry);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kRunning);  // tick 1
    EXPECT_EQ(gSpyACallCount, 1);
    EXPECT_EQ(gSpyBCallCount, 1);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kSuccess);  // tick 2: only B called
    EXPECT_EQ(gSpyACallCount, 1);  // A NOT called again
    EXPECT_EQ(gSpyBCallCount, 2);  // B called again
}
