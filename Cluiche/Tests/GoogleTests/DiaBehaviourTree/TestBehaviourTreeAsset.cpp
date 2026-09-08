#include <gtest/gtest.h>
#include <DiaBehaviourTree/BehaviourTreeAsset.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::BehaviourTree;
using namespace Dia::Core;

// ============================================================================
// Helpers
// ============================================================================

static Json::Value ParseJson(const char* jsonStr)
{
    Json::Value root;
    Json::Reader reader;
    reader.parse(jsonStr, root);
    return root;
}

static const char* kPatrolTreeJson = R"({
  "root": "Patrol",
  "nodes": {
    "Patrol": {
      "type": "selector",
      "children": ["AttackIfEnemy", "PatrolRoute"]
    },
    "AttackIfEnemy": {
      "type": "sequence",
      "children": ["EnemyVisible", "AttackTarget"]
    },
    "EnemyVisible": {
      "type": "condition",
      "blackboard_key": "enemy_visible"
    },
    "AttackTarget": {
      "type": "action",
      "action_id": "AttackOrder",
      "params": []
    },
    "PatrolRoute": {
      "type": "decorator",
      "decorator": "repeater",
      "repeat_count": 0,
      "break_on_failure": true,
      "child": "MoveToWaypoint"
    },
    "CooldownAbility": {
      "type": "decorator",
      "decorator": "cooldown",
      "cooldown_seconds": 2.0,
      "child": "FireAbility"
    },
    "InvertCheck": {
      "type": "decorator",
      "decorator": "inverter",
      "child": "IsIdle"
    },
    "FlankGuard": {
      "type": "decorator",
      "decorator": "guard",
      "blackboard_key": "flank_available",
      "child": "FlankManoeuvre"
    },
    "MultiTask": {
      "type": "parallel",
      "policy": "require_all",
      "children": ["MoveToTarget", "FaceTarget"]
    },
    "MoveToWaypoint": {
      "type": "action",
      "action_id": "MoveToOrder",
      "params": ["next_waypoint"]
    }
  }
})";

// ============================================================================
// DiaBehaviourTree_Asset
// ============================================================================

TEST(DiaBehaviourTree_Asset, LoadFromJson_ValidPatrolTree_IsValid)
{
    Containers::DynamicArrayC<const char*, 32> errors;
    auto asset = BehaviourTreeAsset::LoadFromJson(ParseJson(kPatrolTreeJson), errors);

    EXPECT_TRUE(asset.IsValid());
    EXPECT_EQ(asset.GetNodeCount(), 10);
    EXPECT_EQ(asset.GetRootNodeId(), StringCRC("Patrol"));
}

TEST(DiaBehaviourTree_Asset, LoadFromJson_MissingRootField_IsInvalid)
{
    const char* json = R"({"nodes":{"A":{"type":"sequence","children":[]}}})";
    Containers::DynamicArrayC<const char*, 32> errors;
    auto asset = BehaviourTreeAsset::LoadFromJson(ParseJson(json), errors);

    EXPECT_FALSE(asset.IsValid());
    EXPECT_GT(errors.Size(), 0);
}

TEST(DiaBehaviourTree_Asset, LoadFromJson_RootNotInNodes_IsInvalid)
{
    const char* json = R"({"root":"Missing","nodes":{"A":{"type":"sequence","children":[]}}})";
    Containers::DynamicArrayC<const char*, 32> errors;
    auto asset = BehaviourTreeAsset::LoadFromJson(ParseJson(json), errors);

    EXPECT_FALSE(asset.IsValid());
    EXPECT_GT(errors.Size(), 0);
}

TEST(DiaBehaviourTree_Asset, LoadFromJson_ChildReferencesUnknownNode_IsInvalid)
{
    const char* json = R"({"root":"A","nodes":{"A":{"type":"sequence","children":["B","C"]},"B":{"type":"action","action_id":"Foo","params":[]}}})";
    Containers::DynamicArrayC<const char*, 32> errors;
    auto asset = BehaviourTreeAsset::LoadFromJson(ParseJson(json), errors);

    EXPECT_FALSE(asset.IsValid());
    EXPECT_GT(errors.Size(), 0);
}

TEST(DiaBehaviourTree_Asset, LoadFromJson_CycleDetected_IsInvalid)
{
    const char* json = R"({"root":"A","nodes":{"A":{"type":"sequence","children":["B"]},"B":{"type":"sequence","children":["A"]}}})";
    Containers::DynamicArrayC<const char*, 32> errors;
    auto asset = BehaviourTreeAsset::LoadFromJson(ParseJson(json), errors);

    EXPECT_FALSE(asset.IsValid());
    EXPECT_GT(errors.Size(), 0);
}

TEST(DiaBehaviourTree_Asset, LoadFromJson_EmptyNodes_IsInvalid)
{
    const char* json = R"({"root":"X","nodes":{}})";
    Containers::DynamicArrayC<const char*, 32> errors;
    auto asset = BehaviourTreeAsset::LoadFromJson(ParseJson(json), errors);

    EXPECT_FALSE(asset.IsValid());
    EXPECT_GT(errors.Size(), 0);
}

TEST(DiaBehaviourTree_Asset, GetNode_KnownId_ReturnsDescriptor)
{
    Containers::DynamicArrayC<const char*, 32> errors;
    auto asset = BehaviourTreeAsset::LoadFromJson(ParseJson(kPatrolTreeJson), errors);

    ASSERT_TRUE(asset.IsValid());
    const BehaviourTreeAsset::NodeDescriptor* node = asset.GetNode(StringCRC("Patrol"));
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->type, StringCRC("selector"));
}

TEST(DiaBehaviourTree_Asset, HasNode_KnownId_ReturnsTrue)
{
    Containers::DynamicArrayC<const char*, 32> errors;
    auto asset = BehaviourTreeAsset::LoadFromJson(ParseJson(kPatrolTreeJson), errors);

    ASSERT_TRUE(asset.IsValid());
    EXPECT_TRUE(asset.HasNode(StringCRC("EnemyVisible")));
}

TEST(DiaBehaviourTree_Asset, HasNode_UnknownId_ReturnsFalse)
{
    Containers::DynamicArrayC<const char*, 32> errors;
    auto asset = BehaviourTreeAsset::LoadFromJson(ParseJson(kPatrolTreeJson), errors);

    ASSERT_TRUE(asset.IsValid());
    EXPECT_FALSE(asset.HasNode(StringCRC("DoesNotExist")));
}
