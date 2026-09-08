// Unit tests for DiaSceneEditorPlugin extracted handler methods.
// Tests the Handle* methods directly — no bridge, no file I/O, pure logic only.

#include <gtest/gtest.h>
#include <DiaSceneEditor/DiaSceneEditorPlugin.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::SceneEditor;

namespace
{
	Json::Value MakeMinimalScene()
	{
		Json::Value root;
		root["scene2d"]["layers"]   = Json::Value(Json::arrayValue);
		root["scene2d"]["cameras"]  = Json::Value(Json::arrayValue);
		root["scene2d"]["lights"]   = Json::Value(Json::arrayValue);
		root["scene2d"]["entities"] = Json::Value(Json::arrayValue);

		// Add default layer
		Json::Value layer(Json::objectValue);
		Json::Value layerId(Json::objectValue);
		layerId["value"] = "default";
		layer["id"]      = layerId;
		layer["visible"] = true;
		layer["locked"]  = false;
		layer["enabled"] = true;
		root["scene2d"]["layers"].append(layer);

		return root;
	}

	Json::Value MakeWrappedId(const char* id)
	{
		Json::Value v(Json::objectValue);
		v["value"] = id;
		return v;
	}

	void AddEntity(Json::Value& root, const char* id, const char* blueprint = "bp_default")
	{
		Json::Value entity(Json::objectValue);
		entity["id"]            = MakeWrappedId(id);
		entity["blueprint"]     = MakeWrappedId(blueprint);
		entity["enabled"]       = true;
		entity["instance_data"] = Json::Value(Json::objectValue);
		root["scene2d"]["entities"].append(entity);
	}

	void AddCamera(Json::Value& root, const char* id, bool active, const char* blueprint = "cam_bp")
	{
		Json::Value cam(Json::objectValue);
		cam["id"]            = MakeWrappedId(id);
		cam["blueprint"]     = MakeWrappedId(blueprint);
		cam["active"]        = active;
		cam["enabled"]       = true;
		cam["instance_data"] = Json::Value(Json::objectValue);
		root["scene2d"]["cameras"].append(cam);
	}
}

// ===========================================================================
// Fixture — constructs plugin directly, uses SetTestScene
// ===========================================================================

class SceneEditorHandlerTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		// Plugin is default-constructed — no OnLoad called
	}

	void LoadMinimalScene()
	{
		mPlugin.SetTestScene(MakeMinimalScene());
	}

	void LoadSceneWithEntity(const char* entityId, const char* blueprint = "bp_hero")
	{
		Json::Value scene = MakeMinimalScene();
		AddEntity(scene, entityId, blueprint);
		mPlugin.SetTestScene(scene);
	}

	DiaSceneEditorPlugin mPlugin;
};

// ═══════════════════════════════════════════════════════════════════════════
// HandleAddItem
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(SceneEditorHandlerTest, AddItem_ValidData_ReturnsSuccessAndHierarchy)
{
	LoadMinimalScene();

	Json::Value data;
	data["itemType"]    = "entity";
	data["entityTemplateId"] = "bp_hero";

	Json::Value result = mPlugin.HandleAddItem(data);
	EXPECT_TRUE(result["success"].asBool());
	EXPECT_TRUE(result.isMember("hierarchy"));
}

TEST_F(SceneEditorHandlerTest, AddItem_NoSceneLoaded_ReturnsError)
{
	// No SetTestScene called — no scene loaded
	Json::Value data;
	data["itemType"]    = "entity";
	data["entityTemplateId"] = "bp_hero";

	Json::Value result = mPlugin.HandleAddItem(data);
	EXPECT_TRUE(result.isMember("error"));
	EXPECT_FALSE(result.get("success", false).asBool());
}

TEST_F(SceneEditorHandlerTest, AddItem_MissingItemType_ReturnsError)
{
	LoadMinimalScene();

	Json::Value data;
	data["entityTemplateId"] = "bp_hero";
	// missing "itemType"

	Json::Value result = mPlugin.HandleAddItem(data);
	EXPECT_TRUE(result.isMember("error"));
	EXPECT_FALSE(result.get("success", false).asBool());
}

TEST_F(SceneEditorHandlerTest, AddItem_MissingEntityTemplateId_ReturnsError)
{
	LoadMinimalScene();

	Json::Value data;
	data["itemType"] = "entity";
	// missing "entityTemplateId"

	Json::Value result = mPlugin.HandleAddItem(data);
	EXPECT_TRUE(result.isMember("error"));
	EXPECT_FALSE(result.get("success", false).asBool());
}

// ═══════════════════════════════════════════════════════════════════════════
// HandleDeleteItem
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(SceneEditorHandlerTest, DeleteItem_ValidItem_ReturnsSuccess)
{
	LoadSceneWithEntity("player");

	Json::Value data;
	data["itemType"] = "entity";
	data["itemId"]   = "player";

	Json::Value result = mPlugin.HandleDeleteItem(data);
	EXPECT_TRUE(result["success"].asBool());
	EXPECT_TRUE(result.isMember("hierarchy"));
}

TEST_F(SceneEditorHandlerTest, DeleteItem_NonexistentItem_ReturnsError)
{
	LoadMinimalScene();

	Json::Value data;
	data["itemType"] = "entity";
	data["itemId"]   = "does_not_exist";

	Json::Value result = mPlugin.HandleDeleteItem(data);
	EXPECT_TRUE(result.isMember("error"));
	EXPECT_FALSE(result.get("success", false).asBool());
}

TEST_F(SceneEditorHandlerTest, DeleteItem_NoSceneLoaded_ReturnsError)
{
	Json::Value data;
	data["itemType"] = "entity";
	data["itemId"]   = "player";

	Json::Value result = mPlugin.HandleDeleteItem(data);
	EXPECT_TRUE(result.isMember("error"));
}

TEST_F(SceneEditorHandlerTest, DeleteItem_MissingFields_ReturnsError)
{
	LoadMinimalScene();

	Json::Value data;
	data["itemType"] = "entity";
	// missing "itemId"

	Json::Value result = mPlugin.HandleDeleteItem(data);
	EXPECT_TRUE(result.isMember("error"));
}

// ═══════════════════════════════════════════════════════════════════════════
// HandleDuplicateItem
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(SceneEditorHandlerTest, DuplicateItem_ValidItem_ReturnsSuccess)
{
	LoadSceneWithEntity("enemy");

	Json::Value data;
	data["itemType"] = "entity";
	data["itemId"]   = "enemy";

	Json::Value result = mPlugin.HandleDuplicateItem(data);
	EXPECT_TRUE(result["success"].asBool());
	EXPECT_TRUE(result.isMember("hierarchy"));
}

TEST_F(SceneEditorHandlerTest, DuplicateItem_NonexistentItem_ReturnsError)
{
	LoadMinimalScene();

	Json::Value data;
	data["itemType"] = "entity";
	data["itemId"]   = "ghost";

	Json::Value result = mPlugin.HandleDuplicateItem(data);
	EXPECT_TRUE(result.isMember("error"));
	EXPECT_FALSE(result.get("success", false).asBool());
}

TEST_F(SceneEditorHandlerTest, DuplicateItem_NoSceneLoaded_ReturnsError)
{
	Json::Value data;
	data["itemType"] = "entity";
	data["itemId"]   = "enemy";

	Json::Value result = mPlugin.HandleDuplicateItem(data);
	EXPECT_TRUE(result.isMember("error"));
}

// ═══════════════════════════════════════════════════════════════════════════
// HandleRenameItem
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(SceneEditorHandlerTest, RenameItem_ValidRename_ReturnsSuccessWithNewId)
{
	LoadSceneWithEntity("old_name");

	Json::Value data;
	data["itemType"] = "entity";
	data["oldId"]    = "old_name";
	data["newId"]    = "new_name";

	Json::Value result = mPlugin.HandleRenameItem(data);
	EXPECT_TRUE(result["success"].asBool());
	EXPECT_TRUE(result.isMember("hierarchy"));

	// Verify the hierarchy contains the new id
	Json::StreamWriterBuilder builder;
	std::string hierarchyStr = Json::writeString(builder, result["hierarchy"]);
	EXPECT_NE(hierarchyStr.find("new_name"), std::string::npos);
}

TEST_F(SceneEditorHandlerTest, RenameItem_MissingFields_ReturnsError)
{
	LoadSceneWithEntity("player");

	Json::Value data;
	data["itemType"] = "entity";
	data["oldId"]    = "player";
	// missing "newId"

	Json::Value result = mPlugin.HandleRenameItem(data);
	EXPECT_TRUE(result.isMember("error"));
}

TEST_F(SceneEditorHandlerTest, RenameItem_NoSceneLoaded_ReturnsError)
{
	Json::Value data;
	data["itemType"] = "entity";
	data["oldId"]    = "player";
	data["newId"]    = "hero";

	Json::Value result = mPlugin.HandleRenameItem(data);
	EXPECT_TRUE(result.isMember("error"));
}

// ═══════════════════════════════════════════════════════════════════════════
// HandleValidate
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(SceneEditorHandlerTest, Validate_ValidScene_ReturnsValidationReport)
{
	Json::Value scene = MakeMinimalScene();
	AddCamera(scene, "main_cam", true);
	mPlugin.SetTestScene(scene);

	Json::Value data(Json::objectValue);
	Json::Value result = mPlugin.HandleValidate(data);
	EXPECT_TRUE(result["success"].asBool());
	EXPECT_TRUE(result.isMember("validation"));
	EXPECT_TRUE(result["validation"].isMember("valid"));
}

TEST_F(SceneEditorHandlerTest, Validate_NoSceneLoaded_ReturnsError)
{
	Json::Value data(Json::objectValue);
	Json::Value result = mPlugin.HandleValidate(data);
	EXPECT_TRUE(result.isMember("error"));
	EXPECT_FALSE(result.get("success", false).asBool());
}

// ═══════════════════════════════════════════════════════════════════════════
// HandleLoadScene — skipped (requires file I/O via SceneFileHandler)
// HandleSaveScene — skipped (requires file I/O via SceneFileHandler)
// These are tested via integration tests in IntegrationTestSceneEditorPlugin.
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(SceneEditorHandlerTest, LoadScene_MissingPath_ReturnsError)
{
	Json::Value data(Json::objectValue);
	// no "path" field
	Json::Value result = mPlugin.HandleLoadScene(data);
	EXPECT_TRUE(result.isMember("error"));
	EXPECT_FALSE(result.get("success", false).asBool());
}

TEST_F(SceneEditorHandlerTest, SaveScene_NoPathAndNoScene_ReturnsError)
{
	// No scene loaded, no path in data — should return "no path"
	Json::Value data(Json::objectValue);
	Json::Value result = mPlugin.HandleSaveScene(data);
	EXPECT_TRUE(result.isMember("error"));
	EXPECT_FALSE(result.get("success", false).asBool());
}
