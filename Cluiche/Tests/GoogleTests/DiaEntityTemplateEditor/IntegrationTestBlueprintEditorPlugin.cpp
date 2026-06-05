// Integration tests for DiaEntityTemplateEditorPlugin.
//
// Uses a real WebUIBridge(nullptr) — null UISystem is safe for all handler
// operations (Register/Invoke/Unregister). NotifyUIDataChanged is a no-op
// with null UISystem. No mocking required.

#include <gtest/gtest.h>
#include <DiaEntityTemplateEditor/DiaEntityTemplateEditorPlugin.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <fstream>
#include <cstring>

using namespace Dia::EntityTemplateEditor;
using namespace Dia::Editor;
using namespace Dia::Core;

// ===========================================================================
// Fixture
// ===========================================================================

class EntityTemplateEditorPluginTest : public ::testing::Test
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
		return R"({"entity_template":{"id":"hero","components":[{"type":"Transform2D","fields":{"x":0,"y":0}}]}})";
	}

	static const char* kEmptyEntityJson()
	{
		return R"({"entity_template":{"id":"empty","components":[]}})";
	}

	DiaEntityTemplateEditorPlugin  mPlugin;
	WebUIBridge*              mBridge = nullptr;
	EditorModel*              mModel  = nullptr;

	static const unsigned int kMaxTempFiles = 16;
	const char*  mTempFiles[kMaxTempFiles] = {};
	unsigned int mTempCount = 0;
};

// ===========================================================================
// get_project_state — no project
// ===========================================================================

TEST_F(EntityTemplateEditorPluginTest, GetProjectState_NoProjectLoaded_IsValidFalse)
{
	Json::Value r = Invoke("entity_template_editor.get_project_state");
	EXPECT_FALSE(r.isNull());
	EXPECT_FALSE(r["isValid"].asBool());
}

TEST_F(EntityTemplateEditorPluginTest, GetProjectState_NoProjectLoaded_DiagamePathEmpty)
{
	Json::Value r = Invoke("entity_template_editor.get_project_state");
	EXPECT_EQ(r["diagamePath"].asString(), "");
}

// ===========================================================================
// Regression: project loaded before OnLoad — get_project_state valid immediately
//
// Before the fix, RegisterRequestHandlers() was called before mDiagamePath was
// populated from the model, so get_project_state returned isValid=false on
// the first UI poll even when a project was already open.
// ===========================================================================

namespace
{
	const char* WriteDiagame(const char* path)
	{
		std::ofstream f(path);
		f << "{\"name\":\"Test\",\"version\":\"1.0\",\"imports\":[],\"config\":{}}\n";
		return path;
	}
}

TEST(EntityTemplateEditorPluginProjectState, OnLoad_WithPreloadedProject_GetProjectState_IsValidTrue)
{
	const char* path = "test_bpeditor_preloaded.diagame";
	WriteDiagame(path);

	WebUIBridge bridge(nullptr);
	EditorModel model;
	model.LoadDiagameProject(path);
	ASSERT_TRUE(model.GetDiagameProject().IsValid());

	DiaEntityTemplateEditorPlugin plugin;
	EditorPluginContext ctx;
	ctx.mBridge = &bridge;
	ctx.mModel  = &model;
	plugin.OnLoad(ctx);

	Json::Value r = bridge.InvokeRequestHandler(StringCRC("entity_template_editor.get_project_state"), Json::Value(Json::objectValue));
	EXPECT_TRUE(r["isValid"].asBool());
	EXPECT_STREQ(r["diagamePath"].asCString(), path);

	plugin.OnUnload();
	std::remove(path);
}

TEST(EntityTemplateEditorPluginProjectState, OnLoad_ProjectLoadedAfterOnLoad_GetProjectState_UpdatesViaCallback)
{
	const char* path = "test_bpeditor_postload.diagame";
	WriteDiagame(path);

	WebUIBridge bridge(nullptr);
	EditorModel model;

	DiaEntityTemplateEditorPlugin plugin;
	EditorPluginContext ctx;
	ctx.mBridge = &bridge;
	ctx.mModel  = &model;
	plugin.OnLoad(ctx);

	Json::Value before = bridge.InvokeRequestHandler(StringCRC("entity_template_editor.get_project_state"), Json::Value(Json::objectValue));
	EXPECT_FALSE(before["isValid"].asBool());

	model.LoadDiagameProject(path);

	Json::Value after = bridge.InvokeRequestHandler(StringCRC("entity_template_editor.get_project_state"), Json::Value(Json::objectValue));
	EXPECT_TRUE(after["isValid"].asBool());
	EXPECT_STREQ(after["diagamePath"].asCString(), path);

	plugin.OnUnload();
	std::remove(path);
}

// ===========================================================================
// get_project_state — after project cleared
// ===========================================================================

TEST_F(EntityTemplateEditorPluginTest, GetProjectState_AfterClear_IsValidFalse)
{
	mModel->ClearDiagameProject();

	Json::Value r = Invoke("entity_template_editor.get_project_state");
	EXPECT_FALSE(r["isValid"].asBool());
	EXPECT_EQ(r["diagamePath"].asString(), "");
}

// ===========================================================================
// project_changed push payload — isValid field present
// ===========================================================================

TEST_F(EntityTemplateEditorPluginTest, ProjectChanged_Push_PayloadHasBothRequiredFields)
{
	// get_project_state always returns both fields regardless of project state.
	// This verifies the UI overlay can always read isValid safely.
	Json::Value r = Invoke("entity_template_editor.get_project_state");
	EXPECT_TRUE(r.isMember("isValid"));
	EXPECT_TRUE(r.isMember("diagamePath"));
}

// ===========================================================================
// Lifecycle — handler registration
// ===========================================================================

TEST_F(EntityTemplateEditorPluginTest, OnLoad_HandlerRegistered_GetList)
{
	// Should return success (empty registry is still a valid response)
	Json::Value r = Invoke("entity_template_editor.get_list");
	EXPECT_TRUE(r.isMember("success"));
}

TEST_F(EntityTemplateEditorPluginTest, OnLoad_HandlerRegistered_Load)
{
	// Missing path → error response, not empty value
	Json::Value r = Invoke("entity_template_editor.load");
	EXPECT_FALSE(r.isNull());
	EXPECT_TRUE(r.isMember("success"));
}

TEST_F(EntityTemplateEditorPluginTest, OnLoad_HandlerRegistered_Save)
{
	Json::Value r = Invoke("entity_template_editor.save");
	EXPECT_FALSE(r.isNull());
	EXPECT_TRUE(r.isMember("success"));
}

TEST_F(EntityTemplateEditorPluginTest, OnLoad_HandlerRegistered_UpdateField)
{
	Json::Value r = Invoke("entity_template_editor.update_field");
	EXPECT_FALSE(r.isNull());
	EXPECT_TRUE(r.isMember("success"));
}

TEST_F(EntityTemplateEditorPluginTest, OnLoad_HandlerRegistered_AddComponent)
{
	Json::Value r = Invoke("entity_template_editor.add_component");
	EXPECT_FALSE(r.isNull());
	EXPECT_TRUE(r.isMember("success"));
}

TEST_F(EntityTemplateEditorPluginTest, OnLoad_HandlerRegistered_RemoveComponent)
{
	Json::Value r = Invoke("entity_template_editor.remove_component");
	EXPECT_FALSE(r.isNull());
	EXPECT_TRUE(r.isMember("success"));
}

TEST_F(EntityTemplateEditorPluginTest, OnLoad_HandlerRegistered_GetAvailableComponents)
{
	Json::Value r = Invoke("entity_template_editor.get_available_components");
	EXPECT_FALSE(r.isNull());
	EXPECT_TRUE(r.isMember("success"));
}

TEST_F(EntityTemplateEditorPluginTest, OnLoad_HandlerRegistered_GetUsage)
{
	Json::Value r = Invoke("entity_template_editor.get_usage");
	EXPECT_FALSE(r.isNull());
	EXPECT_TRUE(r.isMember("success"));
}

// register_catalogue_asset removed — list is populated by querying asset_catalogue.query_by_type directly

// ===========================================================================
// Lifecycle — OnUnload removes all handlers
// ===========================================================================

TEST_F(EntityTemplateEditorPluginTest, OnUnload_RemovesAllHandlers)
{
	mPlugin.OnUnload();

	// After unload InvokeRequestHandler returns empty Json::Value (no handler found)
	EXPECT_TRUE(Invoke("entity_template_editor.get_list").isNull());
	EXPECT_TRUE(Invoke("entity_template_editor.load").isNull());
	EXPECT_TRUE(Invoke("entity_template_editor.save").isNull());
	EXPECT_TRUE(Invoke("entity_template_editor.update_field").isNull());
	EXPECT_TRUE(Invoke("entity_template_editor.add_component").isNull());
	EXPECT_TRUE(Invoke("entity_template_editor.remove_component").isNull());
	EXPECT_TRUE(Invoke("entity_template_editor.get_available_components").isNull());
	EXPECT_TRUE(Invoke("entity_template_editor.get_usage").isNull());
	EXPECT_TRUE(Invoke("entity_template_editor.create_from_template").isNull());

	// Re-load so TearDown's OnUnload doesn't double-unregister
	EditorPluginContext ctx;
	ctx.mBridge = mBridge;
	ctx.mModel  = mModel;
	mPlugin.OnLoad(ctx);
}

// ===========================================================================
// Lifecycle — null inputs don't crash
// ===========================================================================

TEST(EntityTemplateEditorPluginLifecycle, OnLoad_NullBridge_DoesNotCrash)
{
	DiaEntityTemplateEditorPlugin plugin;
	EditorPluginContext ctx;
	ctx.mBridge = nullptr;
	ctx.mModel  = nullptr;
	EXPECT_NO_FATAL_FAILURE(plugin.OnLoad(ctx));
	EXPECT_NO_FATAL_FAILURE(plugin.OnUnload());
}

TEST(EntityTemplateEditorPluginLifecycle, OnLoad_NullModel_DoesNotCrash)
{
	WebUIBridge bridge(nullptr);
	DiaEntityTemplateEditorPlugin plugin;
	EditorPluginContext ctx;
	ctx.mBridge = &bridge;
	ctx.mModel  = nullptr;
	EXPECT_NO_FATAL_FAILURE(plugin.OnLoad(ctx));
	EXPECT_NO_FATAL_FAILURE(plugin.OnUnload());
}

// ===========================================================================
// Handler: load — error paths
// ===========================================================================

TEST_F(EntityTemplateEditorPluginTest, Handler_Load_MissingPath_ReturnsError)
{
	Json::Value r = Invoke("entity_template_editor.load");
	EXPECT_FALSE(r["success"].asBool());
	EXPECT_FALSE(r["error"].asString().empty());
}

TEST_F(EntityTemplateEditorPluginTest, Handler_Load_NonexistentFile_ReturnsError)
{
	Json::Value data;
	data["path"] = "nonexistent_xyz.diaentitytemplate";
	Json::Value r = Invoke("entity_template_editor.load", data);
	EXPECT_FALSE(r["success"].asBool());
}

// ===========================================================================
// Handler: load — happy path
// ===========================================================================

TEST_F(EntityTemplateEditorPluginTest, Handler_Load_ValidFile_ReturnsProperties)
{
	const char* kPath = "itmp_load_valid.diaentitytemplate";
	WriteTempFile(kPath, kEntityJson());

	Json::Value data;
	data["path"] = kPath;
	Json::Value r = Invoke("entity_template_editor.load", data);

	EXPECT_TRUE(r["success"].asBool());
	EXPECT_EQ(r["properties"]["id"].asString(), "hero");
	EXPECT_EQ(r["properties"]["components"].size(), 1u);
}

TEST_F(EntityTemplateEditorPluginTest, Handler_Load_DeterminesTopKeyFromExtension)
{
	const char* kPath = "itmp_camera.diacamera";
	WriteTempFile(kPath,
		R"({"camera_blueprint":{"id":"follow_cam","components":[]}})");

	Json::Value data;
	data["path"] = kPath;
	Json::Value r = Invoke("entity_template_editor.load", data);

	EXPECT_TRUE(r["success"].asBool());
	EXPECT_EQ(r["properties"]["id"].asString(), "follow_cam");
}

// ===========================================================================
// Handler: save — error paths
// ===========================================================================

TEST_F(EntityTemplateEditorPluginTest, Handler_Save_MissingPath_ReturnsError)
{
	Json::Value data;
	data["blueprint"]["entity_template"]["id"] = "x";
	Json::Value r = Invoke("entity_template_editor.save", data);
	EXPECT_FALSE(r["success"].asBool());
}

TEST_F(EntityTemplateEditorPluginTest, Handler_Save_MissingBlueprint_ReturnsError)
{
	Json::Value data;
	data["path"] = "itmp_save.diaentitytemplate";
	Json::Value r = Invoke("entity_template_editor.save", data);
	EXPECT_FALSE(r["success"].asBool());
}

// ===========================================================================
// Handler: save — happy path + round-trip with load
// ===========================================================================

TEST_F(EntityTemplateEditorPluginTest, Handler_Save_ValidInput_CreatesFile)
{
	const char* kPath = "itmp_save_valid.diaentitytemplate";
	mTempFiles[mTempCount++] = kPath;

	Json::Value blueprint;
	blueprint["entity_template"]["id"] = "saved_entity";
	blueprint["entity_template"]["components"] = Json::Value(Json::arrayValue);

	Json::Value data;
	data["path"]      = kPath;
	data["blueprint"] = blueprint;
	Json::Value r = Invoke("entity_template_editor.save", data);

	EXPECT_TRUE(r["success"].asBool());

	// Reload and verify
	Json::Value loadData;
	loadData["path"] = kPath;
	Json::Value loaded = Invoke("entity_template_editor.load", loadData);
	EXPECT_TRUE(loaded["success"].asBool());
	EXPECT_EQ(loaded["properties"]["id"].asString(), "saved_entity");
}

// ===========================================================================
// Handler: update_field — error paths
// ===========================================================================

TEST_F(EntityTemplateEditorPluginTest, Handler_UpdateField_MissingParams_ReturnsError)
{
	Json::Value r = Invoke("entity_template_editor.update_field");
	EXPECT_FALSE(r["success"].asBool());
}

TEST_F(EntityTemplateEditorPluginTest, Handler_UpdateField_ComponentNotFound_ReturnsError)
{
	const char* kPath = "itmp_update_notfound.diaentitytemplate";
	WriteTempFile(kPath, kEntityJson());

	Json::Value data;
	data["path"]          = kPath;
	data["componentType"] = "DoesNotExist";
	data["fieldName"]     = "x";
	data["value"]         = 5;
	Json::Value r = Invoke("entity_template_editor.update_field", data);

	EXPECT_FALSE(r["success"].asBool());
	EXPECT_FALSE(r["error"].asString().empty());
}

// ===========================================================================
// Handler: update_field — round-trip
// ===========================================================================

TEST_F(EntityTemplateEditorPluginTest, Handler_UpdateField_RoundTrip_ValuePersisted)
{
	const char* kPath = "itmp_update_roundtrip.diaentitytemplate";
	WriteTempFile(kPath, kEntityJson());

	Json::Value data;
	data["path"]          = kPath;
	data["componentType"] = "Transform2D";
	data["fieldName"]     = "x";
	data["value"]         = 42;
	Json::Value r = Invoke("entity_template_editor.update_field", data);
	ASSERT_TRUE(r["success"].asBool());

	// Reload and check
	Json::Value loadData;
	loadData["path"] = kPath;
	Json::Value loaded = Invoke("entity_template_editor.load", loadData);
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

TEST_F(EntityTemplateEditorPluginTest, Handler_UpdateField_StringValue_Persisted)
{
	const char* kPath = "itmp_update_string.diaentitytemplate";
	WriteTempFile(kPath,
		R"({"entity_template":{"id":"e","components":[{"type":"Tag","fields":{"name":"old"}}]}})");

	Json::Value data;
	data["path"]          = kPath;
	data["componentType"] = "Tag";
	data["fieldName"]     = "name";
	data["value"]         = "new_name";
	Json::Value r = Invoke("entity_template_editor.update_field", data);
	ASSERT_TRUE(r["success"].asBool());

	Json::Value loadData;
	loadData["path"] = kPath;
	Json::Value loaded = Invoke("entity_template_editor.load", loadData);
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

TEST_F(EntityTemplateEditorPluginTest, Handler_AddComponent_MissingParams_ReturnsError)
{
	Json::Value r = Invoke("entity_template_editor.add_component");
	EXPECT_FALSE(r["success"].asBool());
}

TEST_F(EntityTemplateEditorPluginTest, Handler_AddComponent_DuplicateType_ReturnsError)
{
	const char* kPath = "itmp_add_dup.diaentitytemplate";
	WriteTempFile(kPath, kEntityJson());  // already has Transform2D

	Json::Value data;
	data["path"]          = kPath;
	data["componentType"] = "Transform2D";
	Json::Value r = Invoke("entity_template_editor.add_component", data);

	EXPECT_FALSE(r["success"].asBool());
}

// ===========================================================================
// Handler: add_component — round-trip
// ===========================================================================

TEST_F(EntityTemplateEditorPluginTest, Handler_AddComponent_RoundTrip_ComponentPresent)
{
	const char* kPath = "itmp_add_roundtrip.diaentitytemplate";
	WriteTempFile(kPath, kEmptyEntityJson());

	Json::Value data;
	data["path"]          = kPath;
	data["componentType"] = "Health";
	Json::Value r = Invoke("entity_template_editor.add_component", data);
	ASSERT_TRUE(r["success"].asBool());

	Json::Value loadData;
	loadData["path"] = kPath;
	Json::Value loaded = Invoke("entity_template_editor.load", loadData);
	ASSERT_TRUE(loaded["success"].asBool());

	const Json::Value& comps = loaded["properties"]["components"];
	ASSERT_EQ(comps.size(), 1u);
	EXPECT_EQ(comps[0]["type"].asString(), "Health");
}

TEST_F(EntityTemplateEditorPluginTest, Handler_AddComponent_PreservesExistingComponents)
{
	const char* kPath = "itmp_add_preserve.diaentitytemplate";
	WriteTempFile(kPath, kEntityJson());  // has Transform2D

	Json::Value data;
	data["path"]          = kPath;
	data["componentType"] = "Health";
	ASSERT_TRUE(Invoke("entity_template_editor.add_component", data)["success"].asBool());

	Json::Value loadData;
	loadData["path"] = kPath;
	Json::Value loaded = Invoke("entity_template_editor.load", loadData);
	ASSERT_TRUE(loaded["success"].asBool());

	EXPECT_EQ(loaded["properties"]["components"].size(), 2u);
	EXPECT_EQ(loaded["properties"]["components"][0]["type"].asString(), "Transform2D");
	EXPECT_EQ(loaded["properties"]["components"][1]["type"].asString(), "Health");
}

TEST_F(EntityTemplateEditorPluginTest, Handler_AddComponent_NewComponent_HasEmptyFields)
{
	const char* kPath = "itmp_add_emptyfields.diaentitytemplate";
	WriteTempFile(kPath, kEmptyEntityJson());

	Json::Value data;
	data["path"]          = kPath;
	data["componentType"] = "Speed";
	ASSERT_TRUE(Invoke("entity_template_editor.add_component", data)["success"].asBool());

	Json::Value loadData;
	loadData["path"] = kPath;
	Json::Value loaded = Invoke("entity_template_editor.load", loadData);
	ASSERT_TRUE(loaded["success"].asBool());

	EXPECT_EQ(loaded["properties"]["components"][0]["fields"].size(), 0u);
}

// ===========================================================================
// Handler: remove_component — error paths
// ===========================================================================

TEST_F(EntityTemplateEditorPluginTest, Handler_RemoveComponent_MissingParams_ReturnsError)
{
	Json::Value r = Invoke("entity_template_editor.remove_component");
	EXPECT_FALSE(r["success"].asBool());
}

TEST_F(EntityTemplateEditorPluginTest, Handler_RemoveComponent_NotFound_ReturnsError)
{
	const char* kPath = "itmp_remove_notfound.diaentitytemplate";
	WriteTempFile(kPath, kEntityJson());

	Json::Value data;
	data["path"]          = kPath;
	data["componentType"] = "DoesNotExist";
	Json::Value r = Invoke("entity_template_editor.remove_component", data);

	EXPECT_FALSE(r["success"].asBool());
}

// ===========================================================================
// Handler: remove_component — round-trip
// ===========================================================================

TEST_F(EntityTemplateEditorPluginTest, Handler_RemoveComponent_RoundTrip_ComponentGone)
{
	const char* kPath = "itmp_remove_roundtrip.diaentitytemplate";
	WriteTempFile(kPath, kEntityJson());  // has Transform2D

	Json::Value data;
	data["path"]          = kPath;
	data["componentType"] = "Transform2D";
	ASSERT_TRUE(Invoke("entity_template_editor.remove_component", data)["success"].asBool());

	Json::Value loadData;
	loadData["path"] = kPath;
	Json::Value loaded = Invoke("entity_template_editor.load", loadData);
	ASSERT_TRUE(loaded["success"].asBool());

	EXPECT_EQ(loaded["properties"]["components"].size(), 0u);
}

TEST_F(EntityTemplateEditorPluginTest, Handler_RemoveComponent_PreservesOtherComponents)
{
	const char* kPath = "itmp_remove_preserve.diaentitytemplate";
	WriteTempFile(kPath,
		R"({"entity_template":{"id":"e","components":[{"type":"A","fields":{}},{"type":"B","fields":{}},{"type":"C","fields":{}}]}})");

	Json::Value data;
	data["path"]          = kPath;
	data["componentType"] = "B";
	ASSERT_TRUE(Invoke("entity_template_editor.remove_component", data)["success"].asBool());

	Json::Value loadData;
	loadData["path"] = kPath;
	Json::Value loaded = Invoke("entity_template_editor.load", loadData);
	ASSERT_TRUE(loaded["success"].asBool());

	ASSERT_EQ(loaded["properties"]["components"].size(), 2u);
	EXPECT_EQ(loaded["properties"]["components"][0]["type"].asString(), "A");
	EXPECT_EQ(loaded["properties"]["components"][1]["type"].asString(), "C");
}

// ===========================================================================
// Handler: get_list — queries asset_catalogue.query_by_type
// (In unit tests there is no catalogue plugin, so catalogue calls return null.
//  get_list must still return a valid success response with empty groups.)
// ===========================================================================

TEST_F(EntityTemplateEditorPluginTest, Handler_GetList_NoCataloguePlugin_ReturnsSuccessAndNoGroups)
{
	// asset_catalogue.query_by_type not registered → returns null → treated as empty
	Json::Value r = Invoke("entity_template_editor.get_list");
	EXPECT_TRUE(r["success"].asBool());
	EXPECT_EQ(r["groups"].size(), 0u);
}

// Simulate catalogue returning records by registering a stub query_by_type handler
TEST_F(EntityTemplateEditorPluginTest, Handler_GetList_WithCatalogueStub_ShowsAssets)
{
	// Register a stub catalogue handler that returns one diaentitytemplate record
	mBridge->RegisterRequestHandler(
		Dia::Core::StringCRC("asset_catalogue.query_by_type"),
		[](const Json::Value& data) -> Json::Value
		{
			Json::Value result;
			result["success"] = true;
			Json::Value records(Json::arrayValue);
			const std::string typeId = data.get("typeId", "").asString();
			if (typeId == "diaentitytemplate")
			{
				Json::Value rec;
				rec["id"]          = "diaentitytemplate.player";
				rec["source_path"] = "Assets/player.diaentitytemplate";
				records.append(rec);
			}
			result["records"] = records;
			return result;
		});

	Json::Value r = Invoke("entity_template_editor.get_list");
	EXPECT_TRUE(r["success"].asBool());
	ASSERT_EQ(r["groups"].size(), 1u);
	EXPECT_EQ(r["groups"][0]["label"].asString(), "Entity");
	EXPECT_EQ(r["groups"][0]["items"][0]["id"].asString(), "diaentitytemplate.player");

	mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_catalogue.query_by_type"));
}

// ===========================================================================
// Handler: get_usage — error path
// ===========================================================================

TEST_F(EntityTemplateEditorPluginTest, Handler_GetUsage_MissingAssetId_ReturnsError)
{
	Json::Value r = Invoke("entity_template_editor.get_usage");
	EXPECT_FALSE(r["success"].asBool());
}

TEST_F(EntityTemplateEditorPluginTest, Handler_GetUsage_NoCataloguePlugin_ReturnsEmptyUsages)
{
	// asset_catalogue.get_reverse_refs not registered → returns null → empty usages
	Json::Value data;
	data["assetId"] = "diaentitytemplate.nonexistent";
	Json::Value r = Invoke("entity_template_editor.get_usage", data);
	EXPECT_TRUE(r["success"].asBool());
	EXPECT_EQ(r["usage"]["usages"].size(), 0u);
}

// ===========================================================================
// Handler: get_available_components — error path
// ===========================================================================

TEST_F(EntityTemplateEditorPluginTest, Handler_GetAvailableComponents_MissingPath_ReturnsError)
{
	Json::Value r = Invoke("entity_template_editor.get_available_components");
	EXPECT_FALSE(r["success"].asBool());
}

TEST_F(EntityTemplateEditorPluginTest, Handler_GetAvailableComponents_ValidFile_ReturnsArray)
{
	const char* kPath = "itmp_avail_comps.diaentitytemplate";
	WriteTempFile(kPath, kEntityJson());

	Json::Value data;
	data["path"] = kPath;
	Json::Value r = Invoke("entity_template_editor.get_available_components", data);

	EXPECT_TRUE(r["success"].asBool());
	EXPECT_TRUE(r["components"].isArray());
	// Transform2D is already present so should not appear in available list
	for (unsigned int i = 0; i < r["components"].size(); ++i)
		EXPECT_NE(r["components"][i]["typeId"].asString(), "Transform2D");
}

// ===========================================================================
// Handler: create_from_template — error paths
// ===========================================================================

TEST_F(EntityTemplateEditorPluginTest, OnLoad_HandlerRegistered_CreateFromTemplate)
{
	Json::Value r = Invoke("entity_template_editor.create_from_template");
	EXPECT_FALSE(r.isNull());
	EXPECT_TRUE(r.isMember("success"));
}

TEST_F(EntityTemplateEditorPluginTest, Handler_CreateFromTemplate_MissingParams_ReturnsError)
{
	Json::Value r = Invoke("entity_template_editor.create_from_template");
	EXPECT_FALSE(r["success"].asBool());
	EXPECT_FALSE(r["error"].asString().empty());
}

TEST_F(EntityTemplateEditorPluginTest, Handler_CreateFromTemplate_MissingPath_ReturnsError)
{
	Json::Value data;
	data["instanceId"] = "diaentitytemplate.test";
	Json::Value r = Invoke("entity_template_editor.create_from_template", data);
	EXPECT_FALSE(r["success"].asBool());
}

TEST_F(EntityTemplateEditorPluginTest, Handler_CreateFromTemplate_MissingInstanceId_ReturnsError)
{
	Json::Value data;
	data["path"] = "itmp_create.diaentitytemplate";
	Json::Value r = Invoke("entity_template_editor.create_from_template", data);
	EXPECT_FALSE(r["success"].asBool());
}

// ===========================================================================
// Handler: create_from_template — happy path
// ===========================================================================

TEST_F(EntityTemplateEditorPluginTest, Handler_CreateFromTemplate_Entity_CreatesValidFile)
{
	const char* kPath = "itmp_create_entity.diaentitytemplate";
	mTempFiles[mTempCount++] = kPath;

	Json::Value data;
	data["instanceId"] = "diaentitytemplate.hero";
	data["path"]       = kPath;
	Json::Value r = Invoke("entity_template_editor.create_from_template", data);

	ASSERT_TRUE(r["success"].asBool());

	// Load the created file and verify structure
	Json::Value loadData;
	loadData["path"] = kPath;
	Json::Value loaded = Invoke("entity_template_editor.load", loadData);

	ASSERT_TRUE(loaded["success"].asBool());
	EXPECT_EQ(loaded["properties"]["id"].asString(), "diaentitytemplate.hero");
	EXPECT_EQ(loaded["properties"]["components"].size(), 0u);
}

TEST_F(EntityTemplateEditorPluginTest, Handler_CreateFromTemplate_Camera_UsesCorrectTopKey)
{
	const char* kPath = "itmp_create_camera.diacamera";
	mTempFiles[mTempCount++] = kPath;

	Json::Value data;
	data["instanceId"] = "diacamera.main";
	data["path"]       = kPath;
	Json::Value r = Invoke("entity_template_editor.create_from_template", data);

	ASSERT_TRUE(r["success"].asBool());

	// Verify it loads with the camera top key
	Json::Value loadData;
	loadData["path"] = kPath;
	Json::Value loaded = Invoke("entity_template_editor.load", loadData);

	ASSERT_TRUE(loaded["success"].asBool());
	EXPECT_EQ(loaded["properties"]["id"].asString(), "diacamera.main");
}

TEST_F(EntityTemplateEditorPluginTest, Handler_CreateFromTemplate_Light_UsesCorrectTopKey)
{
	const char* kPath = "itmp_create_light.dialight";
	mTempFiles[mTempCount++] = kPath;

	Json::Value data;
	data["instanceId"] = "dialight.sun";
	data["path"]       = kPath;
	Json::Value r = Invoke("entity_template_editor.create_from_template", data);

	ASSERT_TRUE(r["success"].asBool());

	Json::Value loadData;
	loadData["path"] = kPath;
	Json::Value loaded = Invoke("entity_template_editor.load", loadData);

	ASSERT_TRUE(loaded["success"].asBool());
	EXPECT_EQ(loaded["properties"]["id"].asString(), "dialight.sun");
}

TEST_F(EntityTemplateEditorPluginTest, Handler_CreateFromTemplate_CreatedFileIsLoadableAndEditable)
{
	const char* kPath = "itmp_create_editable.diaentitytemplate";
	mTempFiles[mTempCount++] = kPath;

	// Create
	Json::Value createData;
	createData["instanceId"] = "diaentitytemplate.npc";
	createData["path"]       = kPath;
	ASSERT_TRUE(Invoke("entity_template_editor.create_from_template", createData)["success"].asBool());

	// Add a component to the created file
	Json::Value addData;
	addData["path"]          = kPath;
	addData["componentType"] = "Transform2D";
	ASSERT_TRUE(Invoke("entity_template_editor.add_component", addData)["success"].asBool());

	// Verify
	Json::Value loadData;
	loadData["path"] = kPath;
	Json::Value loaded = Invoke("entity_template_editor.load", loadData);
	ASSERT_TRUE(loaded["success"].asBool());
	EXPECT_EQ(loaded["properties"]["components"].size(), 1u);
	EXPECT_EQ(loaded["properties"]["components"][0]["type"].asString(), "Transform2D");
}

TEST_F(EntityTemplateEditorPluginTest, Handler_CreateFromTemplate_InvalidDirectory_ReturnsError)
{
	Json::Value data;
	data["instanceId"] = "diaentitytemplate.test";
	data["path"]       = "nonexistent_dir_xyz/sub/file.diaentitytemplate";
	Json::Value r = Invoke("entity_template_editor.create_from_template", data);

	EXPECT_FALSE(r["success"].asBool());
}

// ===========================================================================
// Compound integration: full add → update → remove → verify pipeline
// ===========================================================================

TEST_F(EntityTemplateEditorPluginTest, Compound_AddUpdateRemove_FileStateCorrect)
{
	const char* kPath = "itmp_compound.diaentitytemplate";
	WriteTempFile(kPath, kEmptyEntityJson());

	// Add
	{ Json::Value d; d["path"] = kPath; d["componentType"] = "Health";
	  ASSERT_TRUE(Invoke("entity_template_editor.add_component", d)["success"].asBool()); }

	// Update
	{ Json::Value d; d["path"] = kPath; d["componentType"] = "Health";
	  d["fieldName"] = "max_hp"; d["value"] = 100;
	  ASSERT_TRUE(Invoke("entity_template_editor.update_field", d)["success"].asBool()); }

	// Verify value
	{ Json::Value d; d["path"] = kPath;
	  Json::Value loaded = Invoke("entity_template_editor.load", d);
	  ASSERT_TRUE(loaded["success"].asBool());
	  const Json::Value& fields = loaded["properties"]["components"][0]["fields"];
	  bool found = false;
	  for (unsigned int i = 0; i < fields.size(); ++i)
	      if (fields[i]["name"].asString() == "max_hp") { found = true; EXPECT_EQ(fields[i]["value"].asInt(), 100); }
	  EXPECT_TRUE(found); }

	// Remove
	{ Json::Value d; d["path"] = kPath; d["componentType"] = "Health";
	  ASSERT_TRUE(Invoke("entity_template_editor.remove_component", d)["success"].asBool()); }

	// Verify empty
	{ Json::Value d; d["path"] = kPath;
	  Json::Value loaded = Invoke("entity_template_editor.load", d);
	  ASSERT_TRUE(loaded["success"].asBool());
	  EXPECT_EQ(loaded["properties"]["components"].size(), 0u); }
}
