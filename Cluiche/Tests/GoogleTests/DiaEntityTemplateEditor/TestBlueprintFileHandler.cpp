#include <gtest/gtest.h>
#include <DiaEntityTemplateEditor/BlueprintFileHandler.h>
#include <DiaCore/Json/external/json/json.h>
#include <fstream>
#include <cstring>

using namespace Dia::EntityTemplateEditor;

// ===========================================================================
// TopLevelKeyForExtension
// ===========================================================================

TEST(BlueprintFileHandler, TopLevelKey_DiaEntity_ReturnsEntityBlueprint)
{
	EXPECT_STREQ(BlueprintFileHandler::TopLevelKeyForExtension(".diaentitytemplatetemplate"), "entity_blueprint");
}

TEST(BlueprintFileHandler, TopLevelKey_DiaCamera_ReturnsCameraBlueprint)
{
	EXPECT_STREQ(BlueprintFileHandler::TopLevelKeyForExtension(".diacamera"), "camera_blueprint");
}

TEST(BlueprintFileHandler, TopLevelKey_DiaLight_ReturnsLightBlueprint)
{
	EXPECT_STREQ(BlueprintFileHandler::TopLevelKeyForExtension(".dialight"), "light_blueprint");
}

TEST(BlueprintFileHandler, TopLevelKey_Unknown_DefaultsToEntityBlueprint)
{
	EXPECT_STREQ(BlueprintFileHandler::TopLevelKeyForExtension(".unknown"), "entity_blueprint");
}

TEST(BlueprintFileHandler, TopLevelKey_Null_DefaultsToEntityBlueprint)
{
	EXPECT_STREQ(BlueprintFileHandler::TopLevelKeyForExtension(nullptr), "entity_blueprint");
}

// ===========================================================================
// Load — invalid inputs
// ===========================================================================

TEST(BlueprintFileHandler, Load_EmptyPath_ReturnsFalse)
{
	BlueprintFileHandler handler;
	Json::Value root;
	char err[128] = {};
	EXPECT_FALSE(handler.Load("", root, err, sizeof(err)));
	EXPECT_GT(strlen(err), 0u);
}

TEST(BlueprintFileHandler, Load_NullPath_ReturnsFalse)
{
	BlueprintFileHandler handler;
	Json::Value root;
	char err[128] = {};
	EXPECT_FALSE(handler.Load(nullptr, root, err, sizeof(err)));
	EXPECT_GT(strlen(err), 0u);
}

TEST(BlueprintFileHandler, Load_NonexistentFile_ReturnsFalse)
{
	BlueprintFileHandler handler;
	Json::Value root;
	char err[128] = {};
	EXPECT_FALSE(handler.Load("nonexistent_blueprint_xyz.diaentitytemplatetemplate", root, err, sizeof(err)));
	EXPECT_GT(strlen(err), 0u);
}

// ===========================================================================
// Load / Save — round-trip
// ===========================================================================

namespace
{
	// Writes a temp file, loads it, saves it to a second path, reloads and compares.
	void WriteTempFile(const char* path, const char* content)
	{
		std::ofstream f(path, std::ios::out | std::ios::trunc);
		f << content;
	}

	void DeleteTempFile(const char* path)
	{
		std::remove(path);
	}
}

TEST(BlueprintFileHandler, Load_ValidJson_Succeeds)
{
	const char* kPath = "tmp_test_blueprint_load.diaentitytemplatetemplate";
	const char* kJson = R"({"entity_blueprint":{"id":"test_entity","components":[]}})";

	WriteTempFile(kPath, kJson);

	BlueprintFileHandler handler;
	Json::Value root;
	char err[128] = {};
	bool ok = handler.Load(kPath, root, err, sizeof(err));

	DeleteTempFile(kPath);

	EXPECT_TRUE(ok);
	EXPECT_TRUE(root.isMember("entity_blueprint"));
	EXPECT_EQ(root["entity_blueprint"]["id"].asString(), "test_entity");
}

TEST(BlueprintFileHandler, Load_MalformedJson_ReturnsFalse)
{
	const char* kPath = "tmp_test_blueprint_malformed.diaentitytemplatetemplate";
	WriteTempFile(kPath, "{ not valid json {{");

	BlueprintFileHandler handler;
	Json::Value root;
	char err[128] = {};
	bool ok = handler.Load(kPath, root, err, sizeof(err));

	DeleteTempFile(kPath);

	EXPECT_FALSE(ok);
	EXPECT_GT(strlen(err), 0u);
}

TEST(BlueprintFileHandler, RoundTrip_LoadSaveLoad_ProducesIdenticalStructure)
{
	const char* kPathA = "tmp_roundtrip_a.diaentitytemplatetemplate";
	const char* kPathB = "tmp_roundtrip_b.diaentitytemplatetemplate";
	const char* kJson = R"({
	"entity_blueprint": {
		"id": "player",
		"components": [
			{ "type": "Transform2D", "fields": { "position": [0, 0], "rotation": 0 } }
		]
	}
})";

	WriteTempFile(kPathA, kJson);

	BlueprintFileHandler handler;
	Json::Value rootA;
	char err[128] = {};
	ASSERT_TRUE(handler.Load(kPathA, rootA, err, sizeof(err)));
	ASSERT_TRUE(handler.Save(kPathB, rootA, err, sizeof(err)));

	Json::Value rootB;
	ASSERT_TRUE(handler.Load(kPathB, rootB, err, sizeof(err)));

	DeleteTempFile(kPathA);
	DeleteTempFile(kPathB);

	EXPECT_EQ(rootA["entity_blueprint"]["id"].asString(),
	          rootB["entity_blueprint"]["id"].asString());
	EXPECT_EQ(rootA["entity_blueprint"]["components"].size(),
	          rootB["entity_blueprint"]["components"].size());
	EXPECT_EQ(
		rootA["entity_blueprint"]["components"][0]["type"].asString(),
		rootB["entity_blueprint"]["components"][0]["type"].asString());
}

// ===========================================================================
// Save — invalid inputs
// ===========================================================================

TEST(BlueprintFileHandler, Save_EmptyPath_ReturnsFalse)
{
	BlueprintFileHandler handler;
	Json::Value root;
	root["entity_blueprint"]["id"] = "x";
	char err[128] = {};
	EXPECT_FALSE(handler.Save("", root, err, sizeof(err)));
	EXPECT_GT(strlen(err), 0u);
}

TEST(BlueprintFileHandler, Save_NullPath_ReturnsFalse)
{
	BlueprintFileHandler handler;
	Json::Value root;
	root["entity_blueprint"]["id"] = "x";
	char err[128] = {};
	EXPECT_FALSE(handler.Save(nullptr, root, err, sizeof(err)));
	EXPECT_GT(strlen(err), 0u);
}

TEST(BlueprintFileHandler, Load_NullErrorOut_DoesNotCrash)
{
	BlueprintFileHandler handler;
	Json::Value root;
	EXPECT_FALSE(handler.Load("", root, nullptr, 0));
}

TEST(BlueprintFileHandler, Load_EmptyFile_ReturnsFalse)
{
	const char* kPath = "tmp_empty_blueprint.diaentitytemplatetemplate";
	WriteTempFile(kPath, "");
	BlueprintFileHandler handler;
	Json::Value root;
	char err[128] = {};
	bool ok = handler.Load(kPath, root, err, sizeof(err));
	DeleteTempFile(kPath);
	EXPECT_FALSE(ok);
}

TEST(BlueprintFileHandler, TopLevelKey_EmptyString_DefaultsToEntityBlueprint)
{
	EXPECT_STREQ(BlueprintFileHandler::TopLevelKeyForExtension(""), "entity_blueprint");
}

TEST(BlueprintFileHandler, RoundTrip_FieldValuesPreserved)
{
	const char* kPath = "tmp_fieldval_roundtrip.diaentitytemplatetemplate";
	const char* kJson = R"({"entity_blueprint":{"id":"hero","components":[{"type":"Transform2D","fields":{"rotation":1.5,"position":[3,4]}}]}})";
	WriteTempFile(kPath, kJson);

	BlueprintFileHandler handler;
	Json::Value root;
	char err[128] = {};
	ASSERT_TRUE(handler.Load(kPath, root, err, sizeof(err)));

	const Json::Value& fields = root["entity_blueprint"]["components"][0]["fields"];
	EXPECT_DOUBLE_EQ(fields["rotation"].asDouble(), 1.5);
	EXPECT_EQ(fields["position"][0].asInt(), 3);
	EXPECT_EQ(fields["position"][1].asInt(), 4);

	DeleteTempFile(kPath);
}
