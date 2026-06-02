// Integration tests for DiaBlueprintEditorPlugin.
//
// Uses a real WebUIBridge(nullptr) — null UISystem is safe for all handler
// operations (Register/Invoke/Unregister). NotifyUIDataChanged is a no-op
// with null UISystem. No mocking required.

#include <gtest/gtest.h>
#include <DiaBlueprintEditor/DiaBlueprintEditorPlugin.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <fstream>
#include <cstring>

using namespace Dia::BlueprintEditor;
using namespace Dia::Editor;
using namespace Dia::Core;

// ===========================================================================
// Fixture
// ===========================================================================

class BlueprintEditorPluginTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		mBridge = new WebUIBridge(nullptr);  // null UISystem — safe for handler tests
		mModel  = new EditorModel();

		EditorPluginContext ctx;
		ctx.mBridge       = mBridge;
		ctx.mModel        = mModel;
		ctx.mView         = nullptr;
		ctx.mPluginLoader = nullptr;
		ctx.mServices     = nullptr;

		mPlugin.OnLoad(ctx);
	}

	void TearDown() override
	{
		mPlugin.OnUnload();
		delete mModel;
		delete mBridge;
		CleanTempFiles();
	}

	Json::Value Invoke(const char* command, const Json::Value& data = Json::Value(Json::objectValue))
	{
		return mBridge->InvokeRequestHandler(StringCRC(command), data);
	}

	// Write a temp blueprint file and record it for cleanup.
	void WriteTempFile(const char* path, const char* content)
	{
		std::ofstream f(path, std::ios::out | std::ios::trunc);
		f << content;
		mTempFiles[mTempCount++] = path;
	}

	void CleanTempFiles()
	{
		for (unsigned int i = 0; i < mTempCount; ++i)
			std::remove(mTempFiles[i]);
		mTempCount = 0;
	}

	// Minimal entity blueprint JSON.
	static const char* kEntityJson()
	{
		return R"({"entity_blueprint":{"id":"hero","components":[{"type":"Transform2D","fields":{"x":0,"y":0}}]}})";
	}

	static const char* kEmptyEntityJson()
	{
		return R"({"entity_blueprint":{"id":"empty","components":[]}})";
	}

	DiaBlueprintEditorPlugin  mPlugin;
	WebUIBridge*              mBridge = nullptr;
	EditorModel*              mModel  = nullptr;

	static const unsigned int kMaxTempFiles = 16;
	const char*  mTempFiles[kMaxTempFiles] = {};
	unsigned int mTempCount = 0;
};

// ===========================================================================
// Lifecycle — handler registration
// ===========================================================================

TEST_F(BlueprintEditorPluginTest, OnLoad_HandlerRegistered_GetList)
{
	// Should return success (empty registry is still a valid response)
	Json::Value r = Invoke("blueprint_editor.get_list");
	EXPECT_TRUE(r.isMember("success"));
}

TEST_F(BlueprintEditorPluginTest, OnLoad_HandlerRegistered_Load)
{
	// Missing path → error response, not empty value
	Json::Value r = Invoke("blueprint_editor.load");
	EXPECT_FALSE(r.isNull());
	EXPECT_TRUE(r.isMember("success"));
}

TEST_F(BlueprintEditorPluginTest, OnLoad_HandlerRegistered_Save)
{
	Json::Value r = Invoke("blueprint_editor.save");
	EXPECT_FALSE(r.isNull());
	EXPECT_TRUE(r.isMember("success"));
}

TEST_F(BlueprintEditorPluginTest, OnLoad_HandlerRegistered_UpdateField)
{
	Json::Value r = Invoke("blueprint_editor.update_field");
	EXPECT_FALSE(r.isNull());
	EXPECT_TRUE(r.isMember("success"));
}

TEST_F(BlueprintEditorPluginTest, OnLoad_HandlerRegistered_AddComponent)
{
	Json::Value r = Invoke("blueprint_editor.add_component");
	EXPECT_FALSE(r.isNull());
	EXPECT_TRUE(r.isMember("success"));
}

TEST_F(BlueprintEditorPluginTest, OnLoad_HandlerRegistered_RemoveComponent)
{
	Json::Value r = Invoke("blueprint_editor.remove_component");
	EXPECT_FALSE(r.isNull());
	EXPECT_TRUE(r.isMember("success"));
}

TEST_F(BlueprintEditorPluginTest, OnLoad_HandlerRegistered_GetAvailableComponents)
{
	Json::Value r = Invoke("blueprint_editor.get_available_components");
	EXPECT_FALSE(r.isNull());
	EXPECT_TRUE(r.isMember("success"));
}

TEST_F(BlueprintEditorPluginTest, OnLoad_HandlerRegistered_GetUsage)
{
	Json::Value r = Invoke("blueprint_editor.get_usage");
	EXPECT_FALSE(r.isNull());
	EXPECT_TRUE(r.isMember("success"));
}

TEST_F(BlueprintEditorPluginTest, OnLoad_HandlerRegistered_RegisterCatalogueAsset)
{
	Json::Value r = Invoke("blueprint_editor.register_catalogue_asset");
	EXPECT_FALSE(r.isNull());
	EXPECT_TRUE(r.isMember("success"));
}

// ===========================================================================
// Lifecycle — OnUnload removes all handlers
// ===========================================================================

TEST_F(BlueprintEditorPluginTest, OnUnload_RemovesAllHandlers)
{
	mPlugin.OnUnload();

	// After unload InvokeRequestHandler returns empty Json::Value (no handler found)
	EXPECT_TRUE(Invoke("blueprint_editor.get_list").isNull());
	EXPECT_TRUE(Invoke("blueprint_editor.load").isNull());
	EXPECT_TRUE(Invoke("blueprint_editor.save").isNull());
	EXPECT_TRUE(Invoke("blueprint_editor.update_field").isNull());
	EXPECT_TRUE(Invoke("blueprint_editor.add_component").isNull());
	EXPECT_TRUE(Invoke("blueprint_editor.remove_component").isNull());
	EXPECT_TRUE(Invoke("blueprint_editor.get_available_components").isNull());
	EXPECT_TRUE(Invoke("blueprint_editor.get_usage").isNull());
	EXPECT_TRUE(Invoke("blueprint_editor.register_catalogue_asset").isNull());

	// Re-load so TearDown's OnUnload doesn't double-unregister
	EditorPluginContext ctx;
	ctx.mBridge = mBridge;
	ctx.mModel  = mModel;
	mPlugin.OnLoad(ctx);
}

// ===========================================================================
// Lifecycle — null inputs don't crash
// ===========================================================================

TEST(BlueprintEditorPluginLifecycle, OnLoad_NullBridge_DoesNotCrash)
{
	DiaBlueprintEditorPlugin plugin;
	EditorPluginContext ctx;
	ctx.mBridge = nullptr;
	ctx.mModel  = nullptr;
	EXPECT_NO_FATAL_FAILURE(plugin.OnLoad(ctx));
	EXPECT_NO_FATAL_FAILURE(plugin.OnUnload());
}

TEST(BlueprintEditorPluginLifecycle, OnLoad_NullModel_DoesNotCrash)
{
	WebUIBridge bridge(nullptr);
	DiaBlueprintEditorPlugin plugin;
	EditorPluginContext ctx;
	ctx.mBridge = &bridge;
	ctx.mModel  = nullptr;
	EXPECT_NO_FATAL_FAILURE(plugin.OnLoad(ctx));
	EXPECT_NO_FATAL_FAILURE(plugin.OnUnload());
}

// ===========================================================================
// Handler: load — error paths
// ===========================================================================

TEST_F(BlueprintEditorPluginTest, Handler_Load_MissingPath_ReturnsError)
{
	Json::Value r = Invoke("blueprint_editor.load");
	EXPECT_FALSE(r["success"].asBool());
	EXPECT_FALSE(r["error"].asString().empty());
}

TEST_F(BlueprintEditorPluginTest, Handler_Load_NonexistentFile_ReturnsError)
{
	Json::Value data;
	data["path"] = "nonexistent_xyz.diaentity";
	Json::Value r = Invoke("blueprint_editor.load", data);
	EXPECT_FALSE(r["success"].asBool());
}

// ===========================================================================
// Handler: load — happy path
// ===========================================================================

TEST_F(BlueprintEditorPluginTest, Handler_Load_ValidFile_ReturnsProperties)
{
	const char* kPath = "itmp_load_valid.diaentity";
	WriteTempFile(kPath, kEntityJson());

	Json::Value data;
	data["path"] = kPath;
	Json::Value r = Invoke("blueprint_editor.load", data);

	EXPECT_TRUE(r["success"].asBool());
	EXPECT_EQ(r["properties"]["id"].asString(), "hero");
	EXPECT_EQ(r["properties"]["components"].size(), 1u);
}

TEST_F(BlueprintEditorPluginTest, Handler_Load_DeterminesTopKeyFromExtension)
{
	const char* kPath = "itmp_camera.diacamera";
	WriteTempFile(kPath,
		R"({"camera_blueprint":{"id":"follow_cam","components":[]}})");

	Json::Value data;
	data["path"] = kPath;
	Json::Value r = Invoke("blueprint_editor.load", data);

	EXPECT_TRUE(r["success"].asBool());
	EXPECT_EQ(r["properties"]["id"].asString(), "follow_cam");
}

// ===========================================================================
// Handler: save — error paths
// ===========================================================================

TEST_F(BlueprintEditorPluginTest, Handler_Save_MissingPath_ReturnsError)
{
	Json::Value data;
	data["blueprint"]["entity_blueprint"]["id"] = "x";
	Json::Value r = Invoke("blueprint_editor.save", data);
	EXPECT_FALSE(r["success"].asBool());
}

TEST_F(BlueprintEditorPluginTest, Handler_Save_MissingBlueprint_ReturnsError)
{
	Json::Value data;
	data["path"] = "itmp_save.diaentity";
	Json::Value r = Invoke("blueprint_editor.save", data);
	EXPECT_FALSE(r["success"].asBool());
}

// ===========================================================================
// Handler: save — happy path + round-trip with load
// ===========================================================================

TEST_F(BlueprintEditorPluginTest, Handler_Save_ValidInput_CreatesFile)
{
	const char* kPath = "itmp_save_valid.diaentity";
	mTempFiles[mTempCount++] = kPath;

	Json::Value blueprint;
	blueprint["entity_blueprint"]["id"] = "saved_entity";
	blueprint["entity_blueprint"]["components"] = Json::Value(Json::arrayValue);

	Json::Value data;
	data["path"]      = kPath;
	data["blueprint"] = blueprint;
	Json::Value r = Invoke("blueprint_editor.save", data);

	EXPECT_TRUE(r["success"].asBool());

	// Reload and verify
	Json::Value loadData;
	loadData["path"] = kPath;
	Json::Value loaded = Invoke("blueprint_editor.load", loadData);
	EXPECT_TRUE(loaded["success"].asBool());
	EXPECT_EQ(loaded["properties"]["id"].asString(), "saved_entity");
}

// ===========================================================================
// Handler: update_field — error paths
// ===========================================================================

TEST_F(BlueprintEditorPluginTest, Handler_UpdateField_MissingParams_ReturnsError)
{
	Json::Value r = Invoke("blueprint_editor.update_field");
	EXPECT_FALSE(r["success"].asBool());
}

TEST_F(BlueprintEditorPluginTest, Handler_UpdateField_ComponentNotFound_ReturnsError)
{
	const char* kPath = "itmp_update_notfound.diaentity";
	WriteTempFile(kPath, kEntityJson());

	Json::Value data;
	data["path"]          = kPath;
	data["componentType"] = "DoesNotExist";
	data["fieldName"]     = "x";
	data["value"]         = 5;
	Json::Value r = Invoke("blueprint_editor.update_field", data);

	EXPECT_FALSE(r["success"].asBool());
	EXPECT_FALSE(r["error"].asString().empty());
}

// ===========================================================================
// Handler: update_field — round-trip
// ===========================================================================

TEST_F(BlueprintEditorPluginTest, Handler_UpdateField_RoundTrip_ValuePersisted)
{
	const char* kPath = "itmp_update_roundtrip.diaentity";
	WriteTempFile(kPath, kEntityJson());

	Json::Value data;
	data["path"]          = kPath;
	data["componentType"] = "Transform2D";
	data["fieldName"]     = "x";
	data["value"]         = 42;
	Json::Value r = Invoke("blueprint_editor.update_field", data);
	ASSERT_TRUE(r["success"].asBool());

	// Reload and check
	Json::Value loadData;
	loadData["path"] = kPath;
	Json::Value loaded = Invoke("blueprint_editor.load", loadData);
	ASSERT_TRUE(loaded["success"].asBool());

	const Json::Value& comps = loaded["properties"]["components"];
	ASSERT_EQ(comps.size(), 1u);
	// Field value should appear (raw fallback since Transform2D not in test registry)
	bool found = false;
	for (unsigned int i = 0; i < comps[0]["fields"].size(); ++i)
	{
		if (comps[0]["fields"][i]["name"].asString() == "x")
		{
			EXPECT_EQ(comps[0]["fields"][i]["value"].asInt(), 42);
			found = true;
		}
	}
	EXPECT_TRUE(found);
}

TEST_F(BlueprintEditorPluginTest, Handler_UpdateField_StringValue_Persisted)
{
	const char* kPath = "itmp_update_string.diaentity";
	WriteTempFile(kPath,
		R"({"entity_blueprint":{"id":"e","components":[{"type":"Tag","fields":{"name":"old"}}]}})");

	Json::Value data;
	data["path"]          = kPath;
	data["componentType"] = "Tag";
	data["fieldName"]     = "name";
	data["value"]         = "new_name";
	Json::Value r = Invoke("blueprint_editor.update_field", data);
	ASSERT_TRUE(r["success"].asBool());

	Json::Value loadData;
	loadData["path"] = kPath;
	Json::Value loaded = Invoke("blueprint_editor.load", loadData);
	ASSERT_TRUE(loaded["success"].asBool());

	const Json::Value& fields = loaded["properties"]["components"][0]["fields"];
	bool found = false;
	for (unsigned int i = 0; i < fields.size(); ++i)
	{
		if (fields[i]["name"].asString() == "name")
		{
			EXPECT_EQ(fields[i]["value"].asString(), "new_name");
			found = true;
		}
	}
	EXPECT_TRUE(found);
}

// ===========================================================================
// Handler: add_component — error paths
// ===========================================================================

TEST_F(BlueprintEditorPluginTest, Handler_AddComponent_MissingParams_ReturnsError)
{
	Json::Value r = Invoke("blueprint_editor.add_component");
	EXPECT_FALSE(r["success"].asBool());
}

TEST_F(BlueprintEditorPluginTest, Handler_AddComponent_DuplicateType_ReturnsError)
{
	const char* kPath = "itmp_add_dup.diaentity";
	WriteTempFile(kPath, kEntityJson());  // already has Transform2D

	Json::Value data;
	data["path"]          = kPath;
	data["componentType"] = "Transform2D";
	Json::Value r = Invoke("blueprint_editor.add_component", data);

	EXPECT_FALSE(r["success"].asBool());
}

// ===========================================================================
// Handler: add_component — round-trip
// ===========================================================================

TEST_F(BlueprintEditorPluginTest, Handler_AddComponent_RoundTrip_ComponentPresent)
{
	const char* kPath = "itmp_add_roundtrip.diaentity";
	WriteTempFile(kPath, kEmptyEntityJson());

	Json::Value data;
	data["path"]          = kPath;
	data["componentType"] = "Health";
	Json::Value r = Invoke("blueprint_editor.add_component", data);
	ASSERT_TRUE(r["success"].asBool());

	Json::Value loadData;
	loadData["path"] = kPath;
	Json::Value loaded = Invoke("blueprint_editor.load", loadData);
	ASSERT_TRUE(loaded["success"].asBool());

	const Json::Value& comps = loaded["properties"]["components"];
	ASSERT_EQ(comps.size(), 1u);
	EXPECT_EQ(comps[0]["type"].asString(), "Health");
}

TEST_F(BlueprintEditorPluginTest, Handler_AddComponent_PreservesExistingComponents)
{
	const char* kPath = "itmp_add_preserve.diaentity";
	WriteTempFile(kPath, kEntityJson());  // has Transform2D

	Json::Value data;
	data["path"]          = kPath;
	data["componentType"] = "Health";
	ASSERT_TRUE(Invoke("blueprint_editor.add_component", data)["success"].asBool());

	Json::Value loadData;
	loadData["path"] = kPath;
	Json::Value loaded = Invoke("blueprint_editor.load", loadData);
	ASSERT_TRUE(loaded["success"].asBool());

	EXPECT_EQ(loaded["properties"]["components"].size(), 2u);
	EXPECT_EQ(loaded["properties"]["components"][0]["type"].asString(), "Transform2D");
	EXPECT_EQ(loaded["properties"]["components"][1]["type"].asString(), "Health");
}

TEST_F(BlueprintEditorPluginTest, Handler_AddComponent_NewComponent_HasEmptyFields)
{
	const char* kPath = "itmp_add_emptyfields.diaentity";
	WriteTempFile(kPath, kEmptyEntityJson());

	Json::Value data;
	data["path"]          = kPath;
	data["componentType"] = "Speed";
	ASSERT_TRUE(Invoke("blueprint_editor.add_component", data)["success"].asBool());

	Json::Value loadData;
	loadData["path"] = kPath;
	Json::Value loaded = Invoke("blueprint_editor.load", loadData);
	ASSERT_TRUE(loaded["success"].asBool());

	EXPECT_EQ(loaded["properties"]["components"][0]["fields"].size(), 0u);
}

// ===========================================================================
// Handler: remove_component — error paths
// ===========================================================================

TEST_F(BlueprintEditorPluginTest, Handler_RemoveComponent_MissingParams_ReturnsError)
{
	Json::Value r = Invoke("blueprint_editor.remove_component");
	EXPECT_FALSE(r["success"].asBool());
}

TEST_F(BlueprintEditorPluginTest, Handler_RemoveComponent_NotFound_ReturnsError)
{
	const char* kPath = "itmp_remove_notfound.diaentity";
	WriteTempFile(kPath, kEntityJson());

	Json::Value data;
	data["path"]          = kPath;
	data["componentType"] = "DoesNotExist";
	Json::Value r = Invoke("blueprint_editor.remove_component", data);

	EXPECT_FALSE(r["success"].asBool());
}

// ===========================================================================
// Handler: remove_component — round-trip
// ===========================================================================

TEST_F(BlueprintEditorPluginTest, Handler_RemoveComponent_RoundTrip_ComponentGone)
{
	const char* kPath = "itmp_remove_roundtrip.diaentity";
	WriteTempFile(kPath, kEntityJson());  // has Transform2D

	Json::Value data;
	data["path"]          = kPath;
	data["componentType"] = "Transform2D";
	ASSERT_TRUE(Invoke("blueprint_editor.remove_component", data)["success"].asBool());

	Json::Value loadData;
	loadData["path"] = kPath;
	Json::Value loaded = Invoke("blueprint_editor.load", loadData);
	ASSERT_TRUE(loaded["success"].asBool());

	EXPECT_EQ(loaded["properties"]["components"].size(), 0u);
}

TEST_F(BlueprintEditorPluginTest, Handler_RemoveComponent_PreservesOtherComponents)
{
	const char* kPath = "itmp_remove_preserve.diaentity";
	WriteTempFile(kPath,
		R"({"entity_blueprint":{"id":"e","components":[{"type":"A","fields":{}},{"type":"B","fields":{}},{"type":"C","fields":{}}]}})");

	Json::Value data;
	data["path"]          = kPath;
	data["componentType"] = "B";
	ASSERT_TRUE(Invoke("blueprint_editor.remove_component", data)["success"].asBool());

	Json::Value loadData;
	loadData["path"] = kPath;
	Json::Value loaded = Invoke("blueprint_editor.load", loadData);
	ASSERT_TRUE(loaded["success"].asBool());

	ASSERT_EQ(loaded["properties"]["components"].size(), 2u);
	EXPECT_EQ(loaded["properties"]["components"][0]["type"].asString(), "A");
	EXPECT_EQ(loaded["properties"]["components"][1]["type"].asString(), "C");
}

// ===========================================================================
// Handler: register_catalogue_asset → get_list pipeline
// ===========================================================================

TEST_F(BlueprintEditorPluginTest, Handler_RegisterCatalogueAsset_ThenGetList_ShowsAsset)
{
	Json::Value regData;
	regData["id"]         = "diaentity.player";
	regData["typeId"]     = "diaentity";
	regData["sourcePath"] = "Assets/player.diaentity";
	ASSERT_TRUE(Invoke("blueprint_editor.register_catalogue_asset", regData)["success"].asBool());

	Json::Value listResult = Invoke("blueprint_editor.get_list");
	EXPECT_TRUE(listResult["success"].asBool());
	ASSERT_GE(listResult["groups"].size(), 1u);
	EXPECT_EQ(listResult["groups"][0]["items"][0]["id"].asString(), "diaentity.player");
}

TEST_F(BlueprintEditorPluginTest, Handler_RegisterCatalogueAsset_NonBlueprintType_ReturnsError)
{
	Json::Value data;
	data["id"]         = "texture.player";
	data["typeId"]     = "texture";
	data["sourcePath"] = "Assets/player.png";
	Json::Value r = Invoke("blueprint_editor.register_catalogue_asset", data);
	EXPECT_FALSE(r["success"].asBool());
}

TEST_F(BlueprintEditorPluginTest, Handler_RegisterCatalogueAsset_MissingFields_ReturnsError)
{
	Json::Value data;
	data["id"] = "diaentity.player";
	// missing typeId and sourcePath
	Json::Value r = Invoke("blueprint_editor.register_catalogue_asset", data);
	EXPECT_FALSE(r["success"].asBool());
}

TEST_F(BlueprintEditorPluginTest, Handler_RegisterCatalogueAsset_Idempotent)
{
	Json::Value data;
	data["id"]         = "diaentity.player";
	data["typeId"]     = "diaentity";
	data["sourcePath"] = "Assets/player.diaentity";

	ASSERT_TRUE(Invoke("blueprint_editor.register_catalogue_asset", data)["success"].asBool());
	// Second registration should not fail
	EXPECT_TRUE(Invoke("blueprint_editor.register_catalogue_asset", data)["success"].asBool());

	// Still shows exactly one item
	Json::Value listResult = Invoke("blueprint_editor.get_list");
	EXPECT_EQ(listResult["groups"][0]["items"].size(), 1u);
}

TEST_F(BlueprintEditorPluginTest, Handler_GetList_EmptyRegistry_ReturnsSuccessAndNoGroups)
{
	Json::Value r = Invoke("blueprint_editor.get_list");
	EXPECT_TRUE(r["success"].asBool());
	EXPECT_EQ(r["groups"].size(), 0u);
}

TEST_F(BlueprintEditorPluginTest, Handler_GetList_AllThreeTypes_ThreeGroups)
{
	auto registerAsset = [&](const char* id, const char* typeId, const char* path)
	{
		Json::Value d;
		d["id"] = id; d["typeId"] = typeId; d["sourcePath"] = path;
		Invoke("blueprint_editor.register_catalogue_asset", d);
	};
	registerAsset("diaentity.hero",   "diaentity", "Assets/hero.diaentity");
	registerAsset("diacamera.follow", "diacamera", "Assets/follow.diacamera");
	registerAsset("dialight.warm",    "dialight",  "Assets/warm.dialight");

	Json::Value r = Invoke("blueprint_editor.get_list");
	EXPECT_TRUE(r["success"].asBool());
	EXPECT_EQ(r["groups"].size(), 3u);
}

// ===========================================================================
// Handler: get_usage — error path
// ===========================================================================

TEST_F(BlueprintEditorPluginTest, Handler_GetUsage_MissingAssetId_ReturnsError)
{
	Json::Value r = Invoke("blueprint_editor.get_usage");
	EXPECT_FALSE(r["success"].asBool());
}

TEST_F(BlueprintEditorPluginTest, Handler_GetUsage_UnknownAsset_ReturnsEmptyUsages)
{
	Json::Value data;
	data["assetId"] = "diaentity.nonexistent";
	Json::Value r = Invoke("blueprint_editor.get_usage", data);
	EXPECT_TRUE(r["success"].asBool());
	EXPECT_EQ(r["usage"]["usages"].size(), 0u);
}

// ===========================================================================
// Handler: get_available_components — error path
// ===========================================================================

TEST_F(BlueprintEditorPluginTest, Handler_GetAvailableComponents_MissingPath_ReturnsError)
{
	Json::Value r = Invoke("blueprint_editor.get_available_components");
	EXPECT_FALSE(r["success"].asBool());
}

TEST_F(BlueprintEditorPluginTest, Handler_GetAvailableComponents_ValidFile_ReturnsArray)
{
	const char* kPath = "itmp_avail_comps.diaentity";
	WriteTempFile(kPath, kEntityJson());

	Json::Value data;
	data["path"] = kPath;
	Json::Value r = Invoke("blueprint_editor.get_available_components", data);

	EXPECT_TRUE(r["success"].asBool());
	EXPECT_TRUE(r["components"].isArray());
	// Transform2D is already present so should not appear in available list
	for (unsigned int i = 0; i < r["components"].size(); ++i)
		EXPECT_NE(r["components"][i]["typeId"].asString(), "Transform2D");
}

// ===========================================================================
// Compound integration: full add → update → remove → verify pipeline
// ===========================================================================

TEST_F(BlueprintEditorPluginTest, Compound_AddUpdateRemove_FileStateCorrect)
{
	const char* kPath = "itmp_compound.diaentity";
	WriteTempFile(kPath, kEmptyEntityJson());

	// Add
	{ Json::Value d; d["path"] = kPath; d["componentType"] = "Health";
	  ASSERT_TRUE(Invoke("blueprint_editor.add_component", d)["success"].asBool()); }

	// Update
	{ Json::Value d; d["path"] = kPath; d["componentType"] = "Health";
	  d["fieldName"] = "max_hp"; d["value"] = 100;
	  ASSERT_TRUE(Invoke("blueprint_editor.update_field", d)["success"].asBool()); }

	// Verify value
	{ Json::Value d; d["path"] = kPath;
	  Json::Value loaded = Invoke("blueprint_editor.load", d);
	  ASSERT_TRUE(loaded["success"].asBool());
	  const Json::Value& fields = loaded["properties"]["components"][0]["fields"];
	  bool found = false;
	  for (unsigned int i = 0; i < fields.size(); ++i)
	      if (fields[i]["name"].asString() == "max_hp") { found = true; EXPECT_EQ(fields[i]["value"].asInt(), 100); }
	  EXPECT_TRUE(found); }

	// Remove
	{ Json::Value d; d["path"] = kPath; d["componentType"] = "Health";
	  ASSERT_TRUE(Invoke("blueprint_editor.remove_component", d)["success"].asBool()); }

	// Verify empty
	{ Json::Value d; d["path"] = kPath;
	  Json::Value loaded = Invoke("blueprint_editor.load", d);
	  ASSERT_TRUE(loaded["success"].asBool());
	  EXPECT_EQ(loaded["properties"]["components"].size(), 0u); }
}
