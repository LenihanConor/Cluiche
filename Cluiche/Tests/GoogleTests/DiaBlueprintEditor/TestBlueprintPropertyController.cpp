#include <gtest/gtest.h>
#include <DiaBlueprintEditor/BlueprintPropertyController.h>
#include <DiaAssetCatalogue/AssetRegistry.h>
#include <DiaAssetCatalogue/AssetRecord.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::BlueprintEditor;
using namespace Dia::AssetCatalogue;
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
// BuildUsageJson — empty registry (no reverse refs)
// ===========================================================================

TEST(BlueprintPropertyController, BuildUsageJson_NoReverseRefs_ReturnsEmptyUsages)
{
	BlueprintPropertyController ctrl;
	AssetRegistry registry;

	AssetRecord entity;
	entity.mId          = StringCRC("diaentity.player");
	entity.mAssetTypeId = StringCRC("diaentity");
	entity.mSourcePath  = "Assets/player.diaentity";
	registry.Register(entity);

	Json::Value result = ctrl.BuildUsageJson(StringCRC("diaentity.player"), registry);

	EXPECT_TRUE(result.isMember("usages"));
	EXPECT_EQ(result["usages"].size(), 0u);
}

// ===========================================================================
// BuildUsageJson — with a reverse reference
// ===========================================================================

TEST(BlueprintPropertyController, BuildUsageJson_WithReverseRef_ReturnsScene)
{
	BlueprintPropertyController ctrl;
	AssetRegistry registry;

	AssetRecord entity;
	entity.mId          = StringCRC("diaentity.enemy");
	entity.mAssetTypeId = StringCRC("diaentity");
	entity.mSourcePath  = "Assets/enemy.diaentity";
	registry.Register(entity);

	AssetRecord scene;
	scene.mId          = StringCRC("diascene.level01");
	scene.mAssetTypeId = StringCRC("diascene");
	scene.mSourcePath  = "Assets/level01.diascene";
	// Scene references the entity blueprint
	RelationshipEdge edge(StringCRC("uses"), StringCRC("diaentity.enemy"));
	scene.mReferences.Add(edge);
	registry.Register(scene);

	Json::Value result = ctrl.BuildUsageJson(StringCRC("diaentity.enemy"), registry);

	EXPECT_TRUE(result.isMember("usages"));
	ASSERT_EQ(result["usages"].size(), 1u);
	EXPECT_EQ(result["usages"][0]["sceneId"].asString(), "diascene.level01");
}
