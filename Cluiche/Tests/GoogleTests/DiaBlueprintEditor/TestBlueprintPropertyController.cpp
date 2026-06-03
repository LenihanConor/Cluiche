#include <gtest/gtest.h>
#include <DiaBlueprintEditor/BlueprintPropertyController.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::BlueprintEditor;
using namespace Dia::Core;

// Helper: build a minimal entity_blueprint JSON root.
static Json::Value MakeEntityRoot(const char* id = "test_entity")
{
	Json::Value root;
	root["entity_blueprint"]["id"] = id;
	root["entity_blueprint"]["components"] = Json::Value(Json::arrayValue);
	return root;
}

static Json::Value AddComponent(Json::Value root, const char* type,
                                const char* field, int value)
{
	Json::Value comp;
	comp["type"]          = type;
	comp["fields"][field] = value;
	root["entity_blueprint"]["components"].append(comp);
	return root;
}

// ===========================================================================
// BuildPropertyJson — structural tests (no ComponentRegistry dependency)
// ===========================================================================

TEST(BlueprintPropertyController, BuildPropertyJson_MissingTopLevelKey_ReturnsError)
{
	BlueprintPropertyController ctrl;
	Json::Value root;    // empty — no "entity_blueprint" key
	Json::Value result = ctrl.BuildPropertyJson(root, "entity_blueprint");

	EXPECT_TRUE(result.isMember("error"));
}

TEST(BlueprintPropertyController, BuildPropertyJson_EmptyComponents_ReturnsIdAndEmptyArray)
{
	BlueprintPropertyController ctrl;
	Json::Value root = MakeEntityRoot("hero");
	Json::Value result = ctrl.BuildPropertyJson(root, "entity_blueprint");

	EXPECT_EQ(result["id"].asString(), "hero");
	EXPECT_EQ(result["components"].size(), 0u);
}

TEST(BlueprintPropertyController, BuildPropertyJson_UnregisteredComponent_ExposesRawFields)
{
	BlueprintPropertyController ctrl;
	Json::Value root = MakeEntityRoot();
	root = AddComponent(root, "SomeUnknownComp", "max_hp", 100);

	Json::Value result = ctrl.BuildPropertyJson(root, "entity_blueprint");

	ASSERT_EQ(result["components"].size(), 1u);
	EXPECT_EQ(result["components"][0]["type"].asString(), "SomeUnknownComp");
	// Raw fallback: fields still present
	ASSERT_EQ(result["components"][0]["fields"].size(), 1u);
	EXPECT_EQ(result["components"][0]["fields"][0]["name"].asString(), "max_hp");
	EXPECT_EQ(result["components"][0]["fields"][0]["kind"].asString(), "primitive");
	EXPECT_EQ(result["components"][0]["fields"][0]["value"].asInt(), 100);
}

TEST(BlueprintPropertyController, BuildPropertyJson_MultipleComponents_AllPresent)
{
	BlueprintPropertyController ctrl;
	Json::Value root = MakeEntityRoot();
	root = AddComponent(root, "CompA", "x", 1);
	root = AddComponent(root, "CompB", "y", 2);

	Json::Value result = ctrl.BuildPropertyJson(root, "entity_blueprint");

	ASSERT_EQ(result["components"].size(), 2u);
	EXPECT_EQ(result["components"][0]["type"].asString(), "CompA");
	EXPECT_EQ(result["components"][1]["type"].asString(), "CompB");
}

// ===========================================================================
// BuildAvailableComponentsJson — with empty blueprint (no present types)
// ===========================================================================

TEST(BlueprintPropertyController, BuildAvailableComponents_MissingTopLevelKey_ReturnsEmptyArray)
{
	BlueprintPropertyController ctrl;
	Json::Value root;  // no top-level key
	Json::Value result = ctrl.BuildAvailableComponentsJson(root, "entity_blueprint");

	// Should return an array (may or may not be empty depending on registry state,
	// but must be an array and not crash)
	EXPECT_TRUE(result.isArray());
}

TEST(BlueprintPropertyController, BuildAvailableComponents_DoesNotIncludeAlreadyPresentTypes)
{
	BlueprintPropertyController ctrl;
	Json::Value root = MakeEntityRoot();
	root = AddComponent(root, "SomeUnknownComp", "x", 0);

	Json::Value available = ctrl.BuildAvailableComponentsJson(root, "entity_blueprint");

	// SomeUnknownComp should not appear in available list (it's already present)
	for (unsigned int i = 0; i < available.size(); ++i)
		EXPECT_NE(available[i]["typeId"].asString(), "SomeUnknownComp");
}

// ===========================================================================
// BuildUsageJson — takes pre-fetched refs array from asset_catalogue.get_reverse_refs
// ===========================================================================

TEST(BlueprintPropertyController, BuildUsageJson_EmptyRefs_ReturnsEmptyUsages)
{
	BlueprintPropertyController ctrl;
	Json::Value emptyRefs(Json::arrayValue);
	Json::Value result = ctrl.BuildUsageJson(emptyRefs);
	EXPECT_TRUE(result.isMember("usages"));
	EXPECT_EQ(result["usages"].size(), 0u);
}

TEST(BlueprintPropertyController, BuildUsageJson_NullRefs_ReturnsEmptyUsages)
{
	BlueprintPropertyController ctrl;
	Json::Value result = ctrl.BuildUsageJson(Json::Value(Json::nullValue));
	EXPECT_EQ(result["usages"].size(), 0u);
}

// ===========================================================================
// BuildUsageJson — with a reverse reference
// ===========================================================================

TEST(BlueprintPropertyController, BuildPropertyJson_CameraBlueprint_UsesCorrectTopKey)
{
	BlueprintPropertyController ctrl;
	Json::Value root;
	root["camera_blueprint"]["id"]         = "follow_cam";
	root["camera_blueprint"]["components"] = Json::Value(Json::arrayValue);

	Json::Value result = ctrl.BuildPropertyJson(root, "camera_blueprint");

	EXPECT_EQ(result["id"].asString(), "follow_cam");
	EXPECT_EQ(result["components"].size(), 0u);
}

TEST(BlueprintPropertyController, BuildPropertyJson_LightBlueprint_UsesCorrectTopKey)
{
	BlueprintPropertyController ctrl;
	Json::Value root;
	root["light_blueprint"]["id"]         = "warm_light";
	root["light_blueprint"]["components"] = Json::Value(Json::arrayValue);

	Json::Value result = ctrl.BuildPropertyJson(root, "light_blueprint");

	EXPECT_EQ(result["id"].asString(), "warm_light");
}

TEST(BlueprintPropertyController, BuildPropertyJson_ComponentWithNoFields_DoesNotCrash)
{
	BlueprintPropertyController ctrl;
	Json::Value root = MakeEntityRoot();
	Json::Value comp;
	comp["type"]   = "EmptyComp";
	comp["fields"] = Json::Value(Json::objectValue);
	root["entity_blueprint"]["components"].append(comp);

	Json::Value result = ctrl.BuildPropertyJson(root, "entity_blueprint");

	ASSERT_EQ(result["components"].size(), 1u);
	EXPECT_EQ(result["components"][0]["fields"].size(), 0u);
}

TEST(BlueprintPropertyController, BuildUsageJson_MultipleRefs_AllReturned)
{
	BlueprintPropertyController ctrl;
	Json::Value refs(Json::arrayValue);
	for (int i = 0; i < 3; ++i)
	{
		char id[64];
		snprintf(id, sizeof(id), "diascene.level%02d", i);
		Json::Value ref;
		ref["source"] = id;
		ref["rel"]    = "uses";
		refs.append(ref);
	}

	Json::Value result = ctrl.BuildUsageJson(refs);
	EXPECT_EQ(result["usages"].size(), 3u);
}

TEST(BlueprintPropertyController, BuildUsageJson_WithRef_ReturnsSceneId)
{
	BlueprintPropertyController ctrl;
	Json::Value refs(Json::arrayValue);
	Json::Value ref;
	ref["source"] = "diascene.level01";
	ref["rel"]    = "uses";
	refs.append(ref);

	Json::Value result = ctrl.BuildUsageJson(refs);
	ASSERT_EQ(result["usages"].size(), 1u);
	EXPECT_EQ(result["usages"][0]["sceneId"].asString(), "diascene.level01");
}
