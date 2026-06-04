#include <gtest/gtest.h>
#include <DiaEntityTemplateEditor/BlueprintPropertyController.h>
#include <DiaEntityTemplateEditor/SchemaReader.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <stdio.h>

using namespace Dia::EntityTemplateEditor;
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
	SchemaReader schema;
	Json::Value root;    // empty — no "entity_blueprint" key
	Json::Value result = ctrl.BuildPropertyJson(root, "entity_blueprint", schema);

	EXPECT_TRUE(result.isMember("error"));
}

TEST(BlueprintPropertyController, BuildPropertyJson_EmptyComponents_ReturnsIdAndEmptyArray)
{
	BlueprintPropertyController ctrl;
	SchemaReader schema;
	Json::Value root = MakeEntityRoot("hero");
	Json::Value result = ctrl.BuildPropertyJson(root, "entity_blueprint", schema);

	EXPECT_EQ(result["id"].asString(), "hero");
	EXPECT_EQ(result["components"].size(), 0u);
}

TEST(BlueprintPropertyController, BuildPropertyJson_UnregisteredComponent_ExposesRawFields)
{
	BlueprintPropertyController ctrl;
	SchemaReader schema;
	Json::Value root = MakeEntityRoot();
	root = AddComponent(root, "SomeUnknownComp", "max_hp", 100);

	Json::Value result = ctrl.BuildPropertyJson(root, "entity_blueprint", schema);

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
	SchemaReader schema;
	Json::Value root = MakeEntityRoot();
	root = AddComponent(root, "CompA", "x", 1);
	root = AddComponent(root, "CompB", "y", 2);

	Json::Value result = ctrl.BuildPropertyJson(root, "entity_blueprint", schema);

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
	SchemaReader schema;  // not loaded
	Json::Value result = ctrl.BuildAvailableComponentsJson(root, "entity_blueprint", schema);

	// Should return an array (may or may not be empty depending on registry state,
	// but must be an array and not crash)
	EXPECT_TRUE(result.isArray());
}

TEST(BlueprintPropertyController, BuildAvailableComponents_DoesNotIncludeAlreadyPresentTypes)
{
	BlueprintPropertyController ctrl;
	Json::Value root = MakeEntityRoot();
	root = AddComponent(root, "SomeUnknownComp", "x", 0);

	SchemaReader schema;  // not loaded
	Json::Value available = ctrl.BuildAvailableComponentsJson(root, "entity_blueprint", schema);

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
	SchemaReader schema;
	Json::Value root;
	root["camera_blueprint"]["id"]         = "follow_cam";
	root["camera_blueprint"]["components"] = Json::Value(Json::arrayValue);

	Json::Value result = ctrl.BuildPropertyJson(root, "camera_blueprint", schema);

	EXPECT_EQ(result["id"].asString(), "follow_cam");
	EXPECT_EQ(result["components"].size(), 0u);
}

TEST(BlueprintPropertyController, BuildPropertyJson_LightBlueprint_UsesCorrectTopKey)
{
	BlueprintPropertyController ctrl;
	SchemaReader schema;
	Json::Value root;
	root["light_blueprint"]["id"]         = "warm_light";
	root["light_blueprint"]["components"] = Json::Value(Json::arrayValue);

	Json::Value result = ctrl.BuildPropertyJson(root, "light_blueprint", schema);

	EXPECT_EQ(result["id"].asString(), "warm_light");
}

TEST(BlueprintPropertyController, BuildPropertyJson_ComponentWithNoFields_DoesNotCrash)
{
	BlueprintPropertyController ctrl;
	SchemaReader schema;
	Json::Value root = MakeEntityRoot();
	Json::Value comp;
	comp["type"]   = "EmptyComp";
	comp["fields"] = Json::Value(Json::objectValue);
	root["entity_blueprint"]["components"].append(comp);

	Json::Value result = ctrl.BuildPropertyJson(root, "entity_blueprint", schema);

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

// ===========================================================================
// BuildAvailableComponentsJson — schema-aware tests
// ===========================================================================

static void WriteSchemaForBPC(const char* path, const char* content)
{
	FILE* f = nullptr;
	fopen_s(&f, path, "w");
	if (f) { fputs(content, f); fclose(f); }
}

static const char* kBPCTestSchemaPath = "TestBPCSchema_temp.diaschema";

static const char* kBPCSingleComponentSchema =
	"{"
	"  \"version\": { \"major\": 1, \"minor\": 0 },"
	"  \"game\": \"cluichetest\","
	"  \"components\": ["
	"    { \"type_id\": \"cluichetest.transform\", \"debug_name\": \"TransformComponent\","
	"      \"fields\": [ {\"name\": \"x\", \"kind\": \"primitive\"} ] }"
	"  ]"
	"}";

TEST(BlueprintPropertyController, BuildAvailableComponents_NoSchema_ReturnsStatusMessage)
{
	BlueprintPropertyController ctrl;
	Json::Value root = MakeEntityRoot();
	SchemaReader schema;  // not loaded

	Json::Value result = ctrl.BuildAvailableComponentsJson(root, "entity_blueprint", schema);

	ASSERT_TRUE(result.isArray());
	bool foundStatus = false;
	for (unsigned int i = 0; i < result.size(); ++i)
	{
		if (result[i].isMember("statusMessage"))
		{
			std::string msg = result[i]["statusMessage"].asString();
			EXPECT_NE(msg.find("No schema found"), std::string::npos);
			foundStatus = true;
			break;
		}
	}
	EXPECT_TRUE(foundStatus) << "Expected a statusMessage entry when schema is not loaded";
}

TEST(BlueprintPropertyController, BuildAvailableComponents_WithSchema_ReturnsComponents)
{
	WriteSchemaForBPC(kBPCTestSchemaPath, kBPCSingleComponentSchema);

	SchemaReader schema;
	schema.LoadFromFile(kBPCTestSchemaPath);
	ASSERT_TRUE(schema.IsLoaded());

	BlueprintPropertyController ctrl;
	Json::Value root = MakeEntityRoot();  // empty blueprint — no components yet

	Json::Value result = ctrl.BuildAvailableComponentsJson(root, "entity_blueprint", schema);

	ASSERT_TRUE(result.isArray());
	bool found = false;
	for (unsigned int i = 0; i < result.size(); ++i)
	{
		if (result[i]["typeId"].asString() == "cluichetest.transform")
		{
			found = true;
			break;
		}
	}
	EXPECT_TRUE(found) << "Expected cluichetest.transform in available components";

	remove(kBPCTestSchemaPath);
}

TEST(BlueprintPropertyController, BuildAvailableComponents_SchemaComponent_AlreadyPresent_Filtered)
{
	WriteSchemaForBPC(kBPCTestSchemaPath, kBPCSingleComponentSchema);

	SchemaReader schema;
	schema.LoadFromFile(kBPCTestSchemaPath);
	ASSERT_TRUE(schema.IsLoaded());

	BlueprintPropertyController ctrl;
	Json::Value root = MakeEntityRoot();
	// Add the component that is in the schema
	root = AddComponent(root, "cluichetest.transform", "x", 0);

	Json::Value result = ctrl.BuildAvailableComponentsJson(root, "entity_blueprint", schema);

	ASSERT_TRUE(result.isArray());
	for (unsigned int i = 0; i < result.size(); ++i)
		EXPECT_NE(result[i]["typeId"].asString(), "cluichetest.transform")
		    << "cluichetest.transform is already in the blueprint and should be filtered out";

	remove(kBPCTestSchemaPath);
}

// ===========================================================================
// BuildPropertyJson — schema-aware tests (codeDefault injection)
// ===========================================================================

static const char* kBPCSchemaWithDefaultsPath = "TestBPCSchemaDefaults_temp.diaschema";

static const char* kBPCSchemaWithDefaults =
	"{"
	"  \"version\": { \"major\": 1, \"minor\": 0 },"
	"  \"game\": \"cluichetest\","
	"  \"components\": ["
	"    { \"type_id\": \"cluichetest.transform\", \"debug_name\": \"TransformComponent\","
	"      \"fields\": [ {\"name\": \"x\", \"kind\": \"primitive\"} ],"
	"      \"default_values\": { \"x\": 42 } }"
	"  ]"
	"}";

TEST(BlueprintPropertyController, BuildPropertyJson_WithSchema_FieldHasCodeDefault)
{
	WriteSchemaForBPC(kBPCSchemaWithDefaultsPath, kBPCSchemaWithDefaults);

	SchemaReader schema;
	schema.LoadFromFile(kBPCSchemaWithDefaultsPath);
	ASSERT_TRUE(schema.IsLoaded());

	BlueprintPropertyController ctrl;

	// Build a blueprint root with a cluichetest.transform component that has field "x"
	// but no explicit value — uses the raw fallback path since ComponentRegistry is empty
	Json::Value root = MakeEntityRoot();
	root = AddComponent(root, "cluichetest.transform", "x", 0);

	Json::Value result = ctrl.BuildPropertyJson(root, "entity_blueprint", schema);

	ASSERT_EQ(result["components"].size(), 1u);
	const Json::Value& fields = result["components"][0]["fields"];
	ASSERT_GE(fields.size(), 1u);

	// Find the "x" field and check codeDefault is injected
	bool foundX = false;
	for (unsigned int i = 0; i < fields.size(); ++i)
	{
		if (fields[i]["name"].asString() == "x")
		{
			EXPECT_TRUE(fields[i].isMember("codeDefault"))
			    << "Field 'x' should have codeDefault injected from schema";
			EXPECT_EQ(fields[i]["codeDefault"].asInt(), 42)
			    << "codeDefault for 'x' should be 42";
			foundX = true;
			break;
		}
	}
	EXPECT_TRUE(foundX) << "Expected a field named 'x' in result";

	remove(kBPCSchemaWithDefaultsPath);
}
