// Tests for the 5 DiaEditorAPI actions added to DiaSceneEditorPlugin:
//   get_entities, place_entity, remove_entity, create_scene, create_asset
//
// These actions are registered via DualRegisterActions() into EditorActionRegistry —
// they are NOT WebUIBridge handlers. Tests call ExecuteAction() on the registry.

#include <gtest/gtest.h>
#include <DiaSceneEditor/DiaSceneEditorPlugin.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/EditorAPI/EditorActionRegistry.h>
#include <DiaEditor/EditorAPI/EditorActionRegistryService.h>
#include <DiaEditor/Plugin/PluginServiceLocator.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::SceneEditor;
using namespace Dia::Editor;
using namespace Dia::Core;

// ===========================================================================
// Scene builder helpers
// ===========================================================================

namespace
{
	Json::Value MakeMinimalScene()
	{
		Json::Value root;
		root["scene2d"]["layers"]   = Json::Value(Json::arrayValue);
		root["scene2d"]["cameras"]  = Json::Value(Json::arrayValue);
		root["scene2d"]["lights"]   = Json::Value(Json::arrayValue);
		root["scene2d"]["entities"] = Json::Value(Json::arrayValue);

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

	void AddCamera(Json::Value& root, const char* id, const char* blueprint = "cam_bp")
	{
		Json::Value cam(Json::objectValue);
		cam["id"]            = MakeWrappedId(id);
		cam["blueprint"]     = MakeWrappedId(blueprint);
		cam["active"]        = false;
		cam["enabled"]       = true;
		cam["instance_data"] = Json::Value(Json::objectValue);
		root["scene2d"]["cameras"].append(cam);
	}
} // anonymous namespace

// ===========================================================================
// Fixture
//
// Wires a real EditorActionRegistry so DualRegisterActions() can register
// the 5 new actions. Tests call ExecuteAction() instead of InvokeRequestHandler().
// ===========================================================================

class SceneEditorNewActionsTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		mBridge   = new WebUIBridge(nullptr);
		mModel    = new EditorModel();
		mRegistry = new EditorActionRegistry();
		mRegistry->Initialize();
		mRegSvc   = new EditorActionRegistryService(mRegistry);
		mServices = new PluginServiceLocator();
		mServices->RegisterService<EditorActionRegistryService>(mRegSvc);

		EditorPluginContext ctx;
		ctx.mBridge       = mBridge;
		ctx.mModel        = mModel;
		ctx.mView         = nullptr;
		ctx.mPluginLoader = nullptr;
		ctx.mServices     = mServices;

		mPlugin.OnLoad(ctx);
	}

	void TearDown() override
	{
		mPlugin.OnUnload();
		mRegistry->Shutdown();
		delete mServices;
		delete mRegSvc;
		delete mRegistry;
		delete mModel;
		delete mBridge;
	}

	Json::Value Execute(const char* action,
	                    const Json::Value& data = Json::Value(Json::objectValue))
	{
		return mRegistry->ExecuteAction(StringCRC(action), data);
	}

	void LoadScene(const Json::Value& scene)
	{
		mPlugin.SetTestScene(scene);
	}

	void LoadSceneWithEntity(const char* entityId, const char* blueprint = "bp_hero")
	{
		Json::Value scene = MakeMinimalScene();
		AddEntity(scene, entityId, blueprint);
		mPlugin.SetTestScene(scene);
	}

	DiaSceneEditorPlugin       mPlugin;
	WebUIBridge*               mBridge   = nullptr;
	EditorModel*               mModel    = nullptr;
	EditorActionRegistry*      mRegistry = nullptr;
	EditorActionRegistryService* mRegSvc = nullptr;
	PluginServiceLocator*      mServices = nullptr;
};

// ═══════════════════════════════════════════════════════════════════════════
// get_entities
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(SceneEditorNewActionsTest, GetEntities_NoSceneLoaded_ReturnsSuccessTrueWithEmptyList)
{
	// No SetTestScene called — plugin has no scene.
	Json::Value r = Execute("scene_editor.get_entities");

	EXPECT_TRUE(r["success"].asBool());
	ASSERT_TRUE(r["entities"].isArray());
	EXPECT_EQ(r["entities"].size(), 0u);
}

TEST_F(SceneEditorNewActionsTest, GetEntities_SceneWithOneEntity_ReturnsOneEntry)
{
	LoadSceneWithEntity("player", "bp_hero");

	Json::Value r = Execute("scene_editor.get_entities");

	EXPECT_TRUE(r["success"].asBool());
	ASSERT_TRUE(r["entities"].isArray());
	ASSERT_EQ(r["entities"].size(), 1u);

	const Json::Value& entry = r["entities"][0];
	EXPECT_STREQ(entry["id"].asCString(), "player");
	EXPECT_STREQ(entry["templateId"].asCString(), "bp_hero");
	EXPECT_TRUE(entry["enabled"].asBool());
}

TEST_F(SceneEditorNewActionsTest, GetEntities_RequiredFields_AllPresent)
{
	LoadSceneWithEntity("npc");

	Json::Value r = Execute("scene_editor.get_entities");

	ASSERT_TRUE(r["entities"].isArray());
	ASSERT_GE(r["entities"].size(), 1u);
	const Json::Value& entry = r["entities"][0];

	EXPECT_TRUE(entry.isMember("id"));
	EXPECT_TRUE(entry.isMember("templateId"));
	EXPECT_TRUE(entry.isMember("enabled"));
	EXPECT_TRUE(entry.isMember("layerId"));
	EXPECT_TRUE(entry.isMember("overrides"));
}

TEST_F(SceneEditorNewActionsTest, GetEntities_MultipleEntities_ReturnsAll)
{
	Json::Value scene = MakeMinimalScene();
	AddEntity(scene, "a");
	AddEntity(scene, "b");
	AddEntity(scene, "c");
	LoadScene(scene);

	Json::Value r = Execute("scene_editor.get_entities");

	EXPECT_TRUE(r["success"].asBool());
	ASSERT_TRUE(r["entities"].isArray());
	EXPECT_EQ(r["entities"].size(), 3u);
}

TEST_F(SceneEditorNewActionsTest, GetEntities_TypeFilterEntity_ReturnsOnlyEntities)
{
	Json::Value scene = MakeMinimalScene();
	AddEntity(scene, "hero");
	AddCamera(scene, "main_cam");
	LoadScene(scene);

	Json::Value data;
	data["type"] = "entity";
	Json::Value r = Execute("scene_editor.get_entities", data);

	EXPECT_TRUE(r["success"].asBool());
	ASSERT_TRUE(r["entities"].isArray());
	EXPECT_EQ(r["entities"].size(), 1u);
	EXPECT_STREQ(r["entities"][0]["id"].asCString(), "hero");
}

TEST_F(SceneEditorNewActionsTest, GetEntities_TypeFilterCamera_ReturnsOnlyCamera)
{
	Json::Value scene = MakeMinimalScene();
	AddEntity(scene, "hero");
	AddCamera(scene, "main_cam");
	LoadScene(scene);

	Json::Value data;
	data["type"] = "camera";
	Json::Value r = Execute("scene_editor.get_entities", data);

	EXPECT_TRUE(r["success"].asBool());
	ASSERT_TRUE(r["entities"].isArray());
	EXPECT_EQ(r["entities"].size(), 1u);
	EXPECT_STREQ(r["entities"][0]["id"].asCString(), "main_cam");
}

TEST_F(SceneEditorNewActionsTest, GetEntities_EntityEnabled_False_ReflectedInResult)
{
	Json::Value scene = MakeMinimalScene();
	Json::Value entity(Json::objectValue);
	entity["id"]            = MakeWrappedId("disabled_npc");
	entity["blueprint"]     = MakeWrappedId("bp_npc");
	entity["enabled"]       = false;
	entity["instance_data"] = Json::Value(Json::objectValue);
	scene["scene2d"]["entities"].append(entity);
	LoadScene(scene);

	Json::Value r = Execute("scene_editor.get_entities");

	ASSERT_TRUE(r["entities"].isArray());
	ASSERT_EQ(r["entities"].size(), 1u);
	EXPECT_FALSE(r["entities"][0]["enabled"].asBool());
}

// ═══════════════════════════════════════════════════════════════════════════
// remove_entity
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(SceneEditorNewActionsTest, RemoveEntity_ValidEntity_ReturnsSuccess)
{
	LoadSceneWithEntity("enemy");

	Json::Value data;
	data["entityId"] = "enemy";
	Json::Value r = Execute("scene_editor.remove_entity", data);

	EXPECT_TRUE(r["success"].asBool());
}

TEST_F(SceneEditorNewActionsTest, RemoveEntity_NonexistentEntity_ReturnsError)
{
	LoadScene(MakeMinimalScene());

	Json::Value data;
	data["entityId"] = "ghost";
	Json::Value r = Execute("scene_editor.remove_entity", data);

	EXPECT_FALSE(r.get("success", false).asBool());
	EXPECT_TRUE(r.isMember("error"));
}

TEST_F(SceneEditorNewActionsTest, RemoveEntity_NoSceneLoaded_ReturnsError)
{
	// No scene loaded — plugin starts empty.
	Json::Value data;
	data["entityId"] = "x";
	Json::Value r = Execute("scene_editor.remove_entity", data);

	EXPECT_FALSE(r.get("success", false).asBool());
}

TEST_F(SceneEditorNewActionsTest, RemoveEntity_EntityGoneAfterRemoval)
{
	LoadSceneWithEntity("target");

	Json::Value removeData;
	removeData["entityId"] = "target";
	Json::Value removeResult = Execute("scene_editor.remove_entity", removeData);
	ASSERT_TRUE(removeResult["success"].asBool());

	// get_entities should now return an empty list.
	Json::Value listResult = Execute("scene_editor.get_entities");
	EXPECT_TRUE(listResult["success"].asBool());
	ASSERT_TRUE(listResult["entities"].isArray());
	EXPECT_EQ(listResult["entities"].size(), 0u);
}

// ═══════════════════════════════════════════════════════════════════════════
// place_entity
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(SceneEditorNewActionsTest, PlaceEntity_ValidTemplate_ReturnsSuccessAndEntityId)
{
	LoadScene(MakeMinimalScene());

	Json::Value data;
	data["templateId"] = "bp_hero";
	Json::Value r = Execute("scene_editor.place_entity", data);

	EXPECT_TRUE(r["success"].asBool());
	EXPECT_TRUE(r.isMember("entityId"));
	EXPECT_FALSE(r["entityId"].asString().empty());
}

TEST_F(SceneEditorNewActionsTest, PlaceEntity_NoSceneLoaded_ReturnsError)
{
	// No SetTestScene — no scene in plugin.
	Json::Value data;
	data["templateId"] = "bp_hero";
	Json::Value r = Execute("scene_editor.place_entity", data);

	EXPECT_FALSE(r.get("success", false).asBool());
}

TEST_F(SceneEditorNewActionsTest, PlaceEntity_ExplicitId_EntityIdMatchesProvided)
{
	LoadScene(MakeMinimalScene());

	Json::Value data;
	data["templateId"] = "bp_hero";
	data["id"]         = "hero_001";
	Json::Value r = Execute("scene_editor.place_entity", data);

	EXPECT_TRUE(r["success"].asBool());
	EXPECT_STREQ(r["entityId"].asCString(), "hero_001");
}

TEST_F(SceneEditorNewActionsTest, PlaceEntity_EntityAppearsInGetEntities)
{
	LoadScene(MakeMinimalScene());

	Json::Value placeData;
	placeData["templateId"] = "bp_hero";
	placeData["id"]         = "new_hero";
	Json::Value placeResult = Execute("scene_editor.place_entity", placeData);
	ASSERT_TRUE(placeResult["success"].asBool());

	Json::Value listResult = Execute("scene_editor.get_entities");
	EXPECT_TRUE(listResult["success"].asBool());
	ASSERT_TRUE(listResult["entities"].isArray());
	ASSERT_EQ(listResult["entities"].size(), 1u);
	EXPECT_STREQ(listResult["entities"][0]["id"].asCString(), "new_hero");
}

TEST_F(SceneEditorNewActionsTest, PlaceEntity_MissingTemplate_ReturnsError)
{
	LoadScene(MakeMinimalScene());

	// No templateId in data — HandleAddItem will reject it.
	Json::Value data(Json::objectValue);
	Json::Value r = Execute("scene_editor.place_entity", data);

	EXPECT_FALSE(r.get("success", false).asBool());
}

// ═══════════════════════════════════════════════════════════════════════════
// create_scene — registration smoke test
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(SceneEditorNewActionsTest, CreateScene_HandlerRegistered_NotUnknownAction)
{
	// No asset_catalogue plugin is loaded so the bridge call returns null,
	// which the handler propagates. What matters here is that the action IS
	// registered — ExecuteAction for an unregistered name returns
	// {"success":false,"reason":"unknown_action"}, which we must not see.
	Json::Value data;
	data["id"]          = "s1";
	data["source_path"] = "scenes/s1.diascene";

	Json::Value r = Execute("scene_editor.create_scene", data);

	// If the action were unregistered ExecuteAction sets reason="unknown_action".
	EXPECT_NE(r.get("reason", "").asString(), "unknown_action");
}

// ═══════════════════════════════════════════════════════════════════════════
// create_asset — registration smoke test
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(SceneEditorNewActionsTest, CreateAsset_HandlerRegistered_NotUnknownAction)
{
	// Same situation as create_scene — no catalogue plugin loaded.
	Json::Value data;
	data["assetType"]   = "entity_template";
	data["id"]          = "et1";
	data["source_path"] = "et/et1.diablueprint";

	Json::Value r = Execute("scene_editor.create_asset", data);

	EXPECT_NE(r.get("reason", "").asString(), "unknown_action");
}
