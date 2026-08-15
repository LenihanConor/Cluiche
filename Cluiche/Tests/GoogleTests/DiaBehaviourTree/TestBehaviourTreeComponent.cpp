#include <gtest/gtest.h>
#include <DiaBehaviourTree/BehaviourTreeComponent.h>
#include <DiaBehaviourTree/BehaviourTreeAsset.h>
#include <DiaBehaviourTree/NodeResult.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

using namespace Dia::BehaviourTree;

// ============================================================================
// Helpers
// ============================================================================

namespace
{
    constexpr const char* kPatrolTreeJson = R"({
      "root": "Patrol",
      "nodes": {
        "Patrol": { "type": "selector", "children": ["AttackIfEnemy", "Idle"] },
        "AttackIfEnemy": { "type": "sequence", "children": ["EnemyVisible", "AttackTarget"] },
        "EnemyVisible": { "type": "condition", "blackboard_key": "enemy_visible" },
        "AttackTarget": { "type": "action", "action_id": "Attack", "params": [] },
        "Idle": { "type": "action", "action_id": "Idle", "params": [] }
      }
    })";

    BehaviourTreeAsset LoadTestAsset()
    {
        Json::Value root;
        Json::Reader().parse(kPatrolTreeJson, root);
        Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
        return BehaviourTreeAsset::LoadFromJson(root, errors);
    }
} // namespace

// ============================================================================
// DiaBehaviourTree_Component
// ============================================================================

TEST(DiaBehaviourTree_Component, DefaultConstruct_HasAsset_False)
{
    BehaviourTreeComponent comp;
    EXPECT_FALSE(comp.HasAsset());
}

TEST(DiaBehaviourTree_Component, DefaultConstruct_IsComplete_False)
{
    BehaviourTreeComponent comp;
    EXPECT_FALSE(comp.IsComplete());
}

TEST(DiaBehaviourTree_Component, DefaultConstruct_LastResult_IsFailure)
{
    BehaviourTreeComponent comp;
    EXPECT_EQ(comp.LastResult(), NodeResult::kFailure);
}

TEST(DiaBehaviourTree_Component, SetAsset_NonNull_HasAsset_True)
{
    BehaviourTreeAsset asset = LoadTestAsset();
    ASSERT_TRUE(asset.IsValid());

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);

    EXPECT_TRUE(comp.HasAsset());
}

TEST(DiaBehaviourTree_Component, SetAsset_Null_HasAsset_False)
{
    BehaviourTreeAsset asset = LoadTestAsset();
    ASSERT_TRUE(asset.IsValid());

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.SetAsset(nullptr);

    EXPECT_FALSE(comp.HasAsset());
}

TEST(DiaBehaviourTree_Component, Tick_NoAsset_ReturnsFailure)
{
    BehaviourTreeComponent comp;
    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kFailure);
}

TEST(DiaBehaviourTree_Component, Tick_WithAssetNoRegistries_ReturnsFailure)
{
    // With no action registry or blackboard, all leaves fail and the composite
    // tree evaluates to kFailure (not kRunning — composites are fully implemented).
    BehaviourTreeAsset asset = LoadTestAsset();
    ASSERT_TRUE(asset.IsValid());

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);

    EXPECT_EQ(comp.Tick(0.0f), NodeResult::kFailure);
}

TEST(DiaBehaviourTree_Component, Reset_ClearsCursor)
{
    BehaviourTreeAsset asset = LoadTestAsset();
    ASSERT_TRUE(asset.IsValid());

    BehaviourTreeComponent comp;
    comp.SetAsset(&asset);
    comp.Tick(0.0f);
    comp.Reset();

    EXPECT_FALSE(comp.IsComplete());
    EXPECT_EQ(comp.LastResult(), NodeResult::kFailure);
}
