#include <gtest/gtest.h>
#include <DiaBlueprintEditor/BlueprintMutator.h>
#include <DiaCore/Json/external/json/json.h>
#include <cstring>

using namespace Dia::BlueprintEditor;

// ===========================================================================
// Helpers
// ===========================================================================

static Json::Value MakeRoot(const char* topKey = "entity_blueprint",
                             const char* id     = "test")
{
	Json::Value root;
	root[topKey]["id"]         = id;
	root[topKey]["components"] = Json::Value(Json::arrayValue);
	return root;
}

static Json::Value WithComponent(Json::Value root, const char* topKey,
                                  const char* type)
{
	Json::Value comp;
	comp["type"]   = type;
	comp["fields"] = Json::Value(Json::objectValue);
	root[topKey]["components"].append(comp);
	return root;
}

// ===========================================================================
// UpdateField — happy path
// ===========================================================================

TEST(BlueprintMutator, UpdateField_ExistingComponent_ReturnsTrue)
{
	Json::Value root = WithComponent(MakeRoot(), "entity_blueprint", "Health");
	EXPECT_TRUE(BlueprintMutator::UpdateField(root, "entity_blueprint",
	    "Health", "max_hp", Json::Value(100)));
}

TEST(BlueprintMutator, UpdateField_SetsFieldValue)
{
	Json::Value root = WithComponent(MakeRoot(), "entity_blueprint", "Health");
	BlueprintMutator::UpdateField(root, "entity_blueprint", "Health", "max_hp", Json::Value(42));
	EXPECT_EQ(root["entity_blueprint"]["components"][0]["fields"]["max_hp"].asInt(), 42);
}

TEST(BlueprintMutator, UpdateField_OverwritesExistingValue)
{
	Json::Value root = WithComponent(MakeRoot(), "entity_blueprint", "Health");
	root["entity_blueprint"]["components"][0]["fields"]["max_hp"] = 10;
	BlueprintMutator::UpdateField(root, "entity_blueprint", "Health", "max_hp", Json::Value(99));
	EXPECT_EQ(root["entity_blueprint"]["components"][0]["fields"]["max_hp"].asInt(), 99);
}

TEST(BlueprintMutator, UpdateField_StringValue_RoundTrips)
{
	Json::Value root = WithComponent(MakeRoot(), "entity_blueprint", "Tag");
	BlueprintMutator::UpdateField(root, "entity_blueprint", "Tag", "name", Json::Value("player"));
	EXPECT_EQ(root["entity_blueprint"]["components"][0]["fields"]["name"].asString(), "player");
}

TEST(BlueprintMutator, UpdateField_BoolValue_RoundTrips)
{
	Json::Value root = WithComponent(MakeRoot(), "entity_blueprint", "Physics");
	BlueprintMutator::UpdateField(root, "entity_blueprint", "Physics", "is_static", Json::Value(true));
	EXPECT_TRUE(root["entity_blueprint"]["components"][0]["fields"]["is_static"].asBool());
}

TEST(BlueprintMutator, UpdateField_ArrayValue_RoundTrips)
{
	Json::Value root = WithComponent(MakeRoot(), "entity_blueprint", "Transform2D");
	Json::Value pos(Json::arrayValue);
	pos.append(1.0); pos.append(2.0);
	BlueprintMutator::UpdateField(root, "entity_blueprint", "Transform2D", "position", pos);
	EXPECT_EQ(root["entity_blueprint"]["components"][0]["fields"]["position"][0].asDouble(), 1.0);
	EXPECT_EQ(root["entity_blueprint"]["components"][0]["fields"]["position"][1].asDouble(), 2.0);
}

TEST(BlueprintMutator, UpdateField_OnlyUpdatesTargetComponent)
{
	Json::Value root = WithComponent(MakeRoot(), "entity_blueprint", "Health");
	root = WithComponent(root, "entity_blueprint", "Speed");
	BlueprintMutator::UpdateField(root, "entity_blueprint", "Health", "max_hp", Json::Value(50));
	EXPECT_FALSE(root["entity_blueprint"]["components"][1]["fields"].isMember("max_hp"));
}

// ===========================================================================
// UpdateField — error cases
// ===========================================================================

TEST(BlueprintMutator, UpdateField_MissingTopKey_ReturnsFalse)
{
	Json::Value root;  // empty
	char err[128] = {};
	EXPECT_FALSE(BlueprintMutator::UpdateField(root, "entity_blueprint",
	    "Health", "max_hp", Json::Value(1), err, sizeof(err)));
	EXPECT_GT(strlen(err), 0u);
}

TEST(BlueprintMutator, UpdateField_ComponentNotFound_ReturnsFalse)
{
	Json::Value root = MakeRoot();
	char err[128] = {};
	EXPECT_FALSE(BlueprintMutator::UpdateField(root, "entity_blueprint",
	    "Nonexistent", "x", Json::Value(0), err, sizeof(err)));
	EXPECT_GT(strlen(err), 0u);
}

TEST(BlueprintMutator, UpdateField_NullErrorOut_DoesNotCrash)
{
	Json::Value root = MakeRoot();
	EXPECT_FALSE(BlueprintMutator::UpdateField(root, "entity_blueprint",
	    "Nonexistent", "x", Json::Value(0), nullptr, 0));
}

// ===========================================================================
// AddComponent — happy path
// ===========================================================================

TEST(BlueprintMutator, AddComponent_ToEmptyBlueprint_ReturnsTrue)
{
	Json::Value root = MakeRoot();
	EXPECT_TRUE(BlueprintMutator::AddComponent(root, "entity_blueprint", "Health"));
}

TEST(BlueprintMutator, AddComponent_AppendsToComponentArray)
{
	Json::Value root = MakeRoot();
	BlueprintMutator::AddComponent(root, "entity_blueprint", "Health");
	ASSERT_EQ(root["entity_blueprint"]["components"].size(), 1u);
	EXPECT_EQ(root["entity_blueprint"]["components"][0]["type"].asString(), "Health");
}

TEST(BlueprintMutator, AddComponent_NewComponentHasEmptyFields)
{
	Json::Value root = MakeRoot();
	BlueprintMutator::AddComponent(root, "entity_blueprint", "Health");
	EXPECT_TRUE(root["entity_blueprint"]["components"][0]["fields"].isObject());
	EXPECT_EQ(root["entity_blueprint"]["components"][0]["fields"].size(), 0u);
}

TEST(BlueprintMutator, AddComponent_PreservesExistingComponents)
{
	Json::Value root = WithComponent(MakeRoot(), "entity_blueprint", "Transform2D");
	BlueprintMutator::AddComponent(root, "entity_blueprint", "Health");
	ASSERT_EQ(root["entity_blueprint"]["components"].size(), 2u);
	EXPECT_EQ(root["entity_blueprint"]["components"][0]["type"].asString(), "Transform2D");
	EXPECT_EQ(root["entity_blueprint"]["components"][1]["type"].asString(), "Health");
}

TEST(BlueprintMutator, AddComponent_MultipleAdds_AllPresent)
{
	Json::Value root = MakeRoot();
	BlueprintMutator::AddComponent(root, "entity_blueprint", "A");
	BlueprintMutator::AddComponent(root, "entity_blueprint", "B");
	BlueprintMutator::AddComponent(root, "entity_blueprint", "C");
	EXPECT_EQ(root["entity_blueprint"]["components"].size(), 3u);
}

// ===========================================================================
// AddComponent — error cases
// ===========================================================================

TEST(BlueprintMutator, AddComponent_MissingTopKey_ReturnsFalse)
{
	Json::Value root;
	char err[128] = {};
	EXPECT_FALSE(BlueprintMutator::AddComponent(root, "entity_blueprint", "Health", err, sizeof(err)));
	EXPECT_GT(strlen(err), 0u);
}

TEST(BlueprintMutator, AddComponent_DuplicateType_ReturnsFalse)
{
	Json::Value root = WithComponent(MakeRoot(), "entity_blueprint", "Health");
	char err[128] = {};
	EXPECT_FALSE(BlueprintMutator::AddComponent(root, "entity_blueprint", "Health", err, sizeof(err)));
	EXPECT_GT(strlen(err), 0u);
}

TEST(BlueprintMutator, AddComponent_DuplicateType_DoesNotAddSecondEntry)
{
	Json::Value root = WithComponent(MakeRoot(), "entity_blueprint", "Health");
	BlueprintMutator::AddComponent(root, "entity_blueprint", "Health");
	EXPECT_EQ(root["entity_blueprint"]["components"].size(), 1u);
}

// ===========================================================================
// RemoveComponent — happy path
// ===========================================================================

TEST(BlueprintMutator, RemoveComponent_ExistingType_ReturnsTrue)
{
	Json::Value root = WithComponent(MakeRoot(), "entity_blueprint", "Health");
	EXPECT_TRUE(BlueprintMutator::RemoveComponent(root, "entity_blueprint", "Health"));
}

TEST(BlueprintMutator, RemoveComponent_RemovesTargetComponent)
{
	Json::Value root = WithComponent(MakeRoot(), "entity_blueprint", "Health");
	BlueprintMutator::RemoveComponent(root, "entity_blueprint", "Health");
	EXPECT_EQ(root["entity_blueprint"]["components"].size(), 0u);
}

TEST(BlueprintMutator, RemoveComponent_PreservesOtherComponents)
{
	Json::Value root = WithComponent(MakeRoot(), "entity_blueprint", "Transform2D");
	root = WithComponent(root, "entity_blueprint", "Health");
	root = WithComponent(root, "entity_blueprint", "Speed");
	BlueprintMutator::RemoveComponent(root, "entity_blueprint", "Health");
	ASSERT_EQ(root["entity_blueprint"]["components"].size(), 2u);
	EXPECT_EQ(root["entity_blueprint"]["components"][0]["type"].asString(), "Transform2D");
	EXPECT_EQ(root["entity_blueprint"]["components"][1]["type"].asString(), "Speed");
}

TEST(BlueprintMutator, RemoveComponent_OrderPreserved_AfterRemoveFromMiddle)
{
	Json::Value root = WithComponent(MakeRoot(), "entity_blueprint", "A");
	root = WithComponent(root, "entity_blueprint", "B");
	root = WithComponent(root, "entity_blueprint", "C");
	BlueprintMutator::RemoveComponent(root, "entity_blueprint", "B");
	ASSERT_EQ(root["entity_blueprint"]["components"].size(), 2u);
	EXPECT_EQ(root["entity_blueprint"]["components"][0]["type"].asString(), "A");
	EXPECT_EQ(root["entity_blueprint"]["components"][1]["type"].asString(), "C");
}

// ===========================================================================
// RemoveComponent — error cases
// ===========================================================================

TEST(BlueprintMutator, RemoveComponent_MissingTopKey_ReturnsFalse)
{
	Json::Value root;
	char err[128] = {};
	EXPECT_FALSE(BlueprintMutator::RemoveComponent(root, "entity_blueprint", "Health", err, sizeof(err)));
	EXPECT_GT(strlen(err), 0u);
}

TEST(BlueprintMutator, RemoveComponent_ComponentNotFound_ReturnsFalse)
{
	Json::Value root = MakeRoot();
	char err[128] = {};
	EXPECT_FALSE(BlueprintMutator::RemoveComponent(root, "entity_blueprint", "Nonexistent", err, sizeof(err)));
	EXPECT_GT(strlen(err), 0u);
}

TEST(BlueprintMutator, RemoveComponent_ComponentNotFound_DoesNotMutateArray)
{
	Json::Value root = WithComponent(MakeRoot(), "entity_blueprint", "Health");
	BlueprintMutator::RemoveComponent(root, "entity_blueprint", "Nonexistent");
	EXPECT_EQ(root["entity_blueprint"]["components"].size(), 1u);
}

// ===========================================================================
// All three keys (.diaentity / .diacamera / .dialight)
// ===========================================================================

TEST(BlueprintMutator, AddComponent_CameraBlueprint_Works)
{
	Json::Value root;
	root["camera_blueprint"]["id"]         = "cam";
	root["camera_blueprint"]["components"] = Json::Value(Json::arrayValue);
	EXPECT_TRUE(BlueprintMutator::AddComponent(root, "camera_blueprint", "Camera2D"));
	EXPECT_EQ(root["camera_blueprint"]["components"][0]["type"].asString(), "Camera2D");
}

TEST(BlueprintMutator, RemoveComponent_LightBlueprint_Works)
{
	Json::Value root;
	root["light_blueprint"]["id"]         = "light";
	root["light_blueprint"]["components"] = Json::Value(Json::arrayValue);
	BlueprintMutator::AddComponent(root, "light_blueprint", "PointLight2D");
	EXPECT_TRUE(BlueprintMutator::RemoveComponent(root, "light_blueprint", "PointLight2D"));
	EXPECT_EQ(root["light_blueprint"]["components"].size(), 0u);
}

// ===========================================================================
// Compound: add then update then remove
// ===========================================================================

TEST(BlueprintMutator, Compound_AddUpdateRemove_StateCorrect)
{
	Json::Value root = MakeRoot();

	ASSERT_TRUE(BlueprintMutator::AddComponent(root, "entity_blueprint", "Health"));
	ASSERT_TRUE(BlueprintMutator::UpdateField(root, "entity_blueprint", "Health", "max_hp", Json::Value(50)));
	EXPECT_EQ(root["entity_blueprint"]["components"][0]["fields"]["max_hp"].asInt(), 50);

	ASSERT_TRUE(BlueprintMutator::RemoveComponent(root, "entity_blueprint", "Health"));
	EXPECT_EQ(root["entity_blueprint"]["components"].size(), 0u);
}
