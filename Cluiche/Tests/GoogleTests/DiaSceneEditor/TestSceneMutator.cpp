#include <gtest/gtest.h>
#include <DiaSceneEditor/SceneMutator.h>
#include <DiaCore/Json/external/json/json.h>
#include <cstring>

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
		cam["id"]        = MakeWrappedId(id);
		cam["blueprint"] = MakeWrappedId(blueprint);
		cam["active"]    = active;
		cam["enabled"]   = true;
		cam["instance_data"] = Json::Value(Json::objectValue);
		root["scene2d"]["cameras"].append(cam);
	}

	void AddLight(Json::Value& root, const char* id, const char* blueprint = "light_bp")
	{
		Json::Value light(Json::objectValue);
		light["id"]        = MakeWrappedId(id);
		light["blueprint"] = MakeWrappedId(blueprint);
		light["enabled"]   = true;
		light["affects_layers"] = Json::Value(Json::arrayValue);
		light["instance_data"]  = Json::Value(Json::objectValue);
		root["scene2d"]["lights"].append(light);
	}

	void AddLayer(Json::Value& root, const char* id, int sortOrder = 0)
	{
		Json::Value layer(Json::objectValue);
		layer["id"]          = MakeWrappedId(id);
		layer["sort_order"]  = sortOrder;
		layer["parallax"]    = Json::Value(Json::arrayValue);
		layer["parallax"].append(1.0f);
		layer["parallax"].append(1.0f);
		layer["sort_policy"] = MakeWrappedId("insertion");
		layer["enabled"]     = true;
		root["scene2d"]["layers"].append(layer);
	}
}

// ═══════════════════════════════════════════════════════════════════════════
// AddItem
// ═══════════════════════════════════════════════════════════════════════════

TEST(SceneMutator_AddItem, Entity_AppendsWithAutoId)
{
	Json::Value scene = MakeMinimalScene();
	char err[256] = {};
	EXPECT_TRUE(SceneMutator::AddItem(scene, "entity", "player", err, sizeof(err)));
	EXPECT_EQ(scene["scene2d"]["entities"].size(), 1u);
	EXPECT_EQ(scene["scene2d"]["entities"][0]["id"]["value"].asString(), "player_0");
	EXPECT_EQ(scene["scene2d"]["entities"][0]["blueprint"]["value"].asString(), "player");
	EXPECT_TRUE(scene["scene2d"]["entities"][0]["enabled"].asBool());
}

TEST(SceneMutator_AddItem, Camera_FirstIsActive)
{
	Json::Value scene = MakeMinimalScene();
	char err[256] = {};
	EXPECT_TRUE(SceneMutator::AddItem(scene, "camera", "cam_follow", err, sizeof(err)));
	EXPECT_TRUE(scene["scene2d"]["cameras"][0]["active"].asBool());
}

TEST(SceneMutator_AddItem, Camera_SecondIsInactive)
{
	Json::Value scene = MakeMinimalScene();
	char err[256] = {};
	SceneMutator::AddItem(scene, "camera", "cam1", err, sizeof(err));
	SceneMutator::AddItem(scene, "camera", "cam2", err, sizeof(err));
	EXPECT_TRUE(scene["scene2d"]["cameras"][0]["active"].asBool());
	EXPECT_FALSE(scene["scene2d"]["cameras"][1]["active"].asBool());
}

TEST(SceneMutator_AddItem, UnknownType_ReturnsFalse)
{
	Json::Value scene = MakeMinimalScene();
	char err[256] = {};
	EXPECT_FALSE(SceneMutator::AddItem(scene, "bogus", "x", err, sizeof(err)));
	EXPECT_STRNE(err, "");
}

TEST(SceneMutator_AddItem, CollisionAvoidance_SkipsExistingIds)
{
	Json::Value scene = MakeMinimalScene();
	AddEntity(scene, "myblueprint_0");
	char err[256] = {};
	EXPECT_TRUE(SceneMutator::AddItem(scene, "entity", "myblueprint", err, sizeof(err)));
	EXPECT_EQ(scene["scene2d"]["entities"][1]["id"]["value"].asString(), "myblueprint_1");
}

// ═══════════════════════════════════════════════════════════════════════════
// DuplicateItem
// ═══════════════════════════════════════════════════════════════════════════

TEST(SceneMutator_DuplicateItem, CopySuffix)
{
	Json::Value scene = MakeMinimalScene();
	AddEntity(scene, "player");
	char err[256] = {};
	EXPECT_TRUE(SceneMutator::DuplicateItem(scene, "entity", "player", err, sizeof(err)));
	EXPECT_EQ(scene["scene2d"]["entities"].size(), 2u);
	EXPECT_EQ(scene["scene2d"]["entities"][1]["id"]["value"].asString(), "player_copy");
}

TEST(SceneMutator_DuplicateItem, PositionOffset)
{
	Json::Value scene = MakeMinimalScene();
	AddEntity(scene, "obj");
	scene["scene2d"]["entities"][0]["instance_data"]["Transform2D.position"] = Json::Value(Json::arrayValue);
	scene["scene2d"]["entities"][0]["instance_data"]["Transform2D.position"].append(100.0f);
	scene["scene2d"]["entities"][0]["instance_data"]["Transform2D.position"].append(200.0f);

	char err[256] = {};
	EXPECT_TRUE(SceneMutator::DuplicateItem(scene, "entity", "obj", err, sizeof(err)));
	Json::Value& pos = scene["scene2d"]["entities"][1]["instance_data"]["Transform2D.position"];
	EXPECT_NEAR(pos[0].asFloat(), 150.0f, 0.01f);
	EXPECT_NEAR(pos[1].asFloat(), 250.0f, 0.01f);
}

TEST(SceneMutator_DuplicateItem, NotFound_ReturnsFalse)
{
	Json::Value scene = MakeMinimalScene();
	char err[256] = {};
	EXPECT_FALSE(SceneMutator::DuplicateItem(scene, "entity", "ghost", err, sizeof(err)));
}

TEST(SceneMutator_DuplicateItem, CopySuffixCollision)
{
	Json::Value scene = MakeMinimalScene();
	AddEntity(scene, "player");
	AddEntity(scene, "player_copy");
	char err[256] = {};
	EXPECT_TRUE(SceneMutator::DuplicateItem(scene, "entity", "player", err, sizeof(err)));
	EXPECT_EQ(scene["scene2d"]["entities"][2]["id"]["value"].asString(), "player_copy2");
}

// ═══════════════════════════════════════════════════════════════════════════
// DeleteItem
// ═══════════════════════════════════════════════════════════════════════════

TEST(SceneMutator_DeleteItem, RemovesFromArray)
{
	Json::Value scene = MakeMinimalScene();
	AddEntity(scene, "target");
	AddEntity(scene, "keep");
	char err[256] = {};
	EXPECT_TRUE(SceneMutator::DeleteItem(scene, "entity", "target", err, sizeof(err)));
	EXPECT_EQ(scene["scene2d"]["entities"].size(), 1u);
	EXPECT_EQ(scene["scene2d"]["entities"][0]["id"]["value"].asString(), "keep");
}

TEST(SceneMutator_DeleteItem, NotFound_ReturnsFalse)
{
	Json::Value scene = MakeMinimalScene();
	char err[256] = {};
	EXPECT_FALSE(SceneMutator::DeleteItem(scene, "entity", "ghost", err, sizeof(err)));
}

// ═══════════════════════════════════════════════════════════════════════════
// SetEnabled
// ═══════════════════════════════════════════════════════════════════════════

TEST(SceneMutator_SetEnabled, TogglesFlag)
{
	Json::Value scene = MakeMinimalScene();
	AddEntity(scene, "obj");
	char err[256] = {};
	EXPECT_TRUE(SceneMutator::SetEnabled(scene, "entity", "obj", false, err, sizeof(err)));
	EXPECT_FALSE(scene["scene2d"]["entities"][0]["enabled"].asBool());
	EXPECT_TRUE(SceneMutator::SetEnabled(scene, "entity", "obj", true, err, sizeof(err)));
	EXPECT_TRUE(scene["scene2d"]["entities"][0]["enabled"].asBool());
}

TEST(SceneMutator_SetEnabled, NotFound_ReturnsFalse)
{
	Json::Value scene = MakeMinimalScene();
	char err[256] = {};
	EXPECT_FALSE(SceneMutator::SetEnabled(scene, "entity", "ghost", false, err, sizeof(err)));
}

// ═══════════════════════════════════════════════════════════════════════════
// RenameItem
// ═══════════════════════════════════════════════════════════════════════════

TEST(SceneMutator_RenameItem, UpdatesId)
{
	Json::Value scene = MakeMinimalScene();
	AddEntity(scene, "old_name");
	char err[256] = {};
	EXPECT_TRUE(SceneMutator::RenameItem(scene, "entity", "old_name", "new_name", err, sizeof(err)));
	EXPECT_EQ(scene["scene2d"]["entities"][0]["id"]["value"].asString(), "new_name");
}

TEST(SceneMutator_RenameItem, InvalidChars_ReturnsFalse)
{
	Json::Value scene = MakeMinimalScene();
	AddEntity(scene, "obj");
	char err[256] = {};
	EXPECT_FALSE(SceneMutator::RenameItem(scene, "entity", "obj", "has space", err, sizeof(err)));
	EXPECT_FALSE(SceneMutator::RenameItem(scene, "entity", "obj", "has.dot", err, sizeof(err)));
}

TEST(SceneMutator_RenameItem, Duplicate_ReturnsFalse)
{
	Json::Value scene = MakeMinimalScene();
	AddEntity(scene, "alpha");
	AddEntity(scene, "beta");
	char err[256] = {};
	EXPECT_FALSE(SceneMutator::RenameItem(scene, "entity", "alpha", "beta", err, sizeof(err)));
}

TEST(SceneMutator_RenameItem, Empty_ReturnsFalse)
{
	Json::Value scene = MakeMinimalScene();
	AddEntity(scene, "obj");
	char err[256] = {};
	EXPECT_FALSE(SceneMutator::RenameItem(scene, "entity", "obj", "", err, sizeof(err)));
}

// ═══════════════════════════════════════════════════════════════════════════
// AnalyseChangeBlueprintJson + ChangeBlueprint
// ═══════════════════════════════════════════════════════════════════════════

TEST(SceneMutator_ChangeBlueprint, AnalyseTransferAndOrphan)
{
	Json::Value scene = MakeMinimalScene();
	AddEntity(scene, "player", "old_bp");
	scene["scene2d"]["entities"][0]["instance_data"]["Health.max_hp"] = 100;
	scene["scene2d"]["entities"][0]["instance_data"]["Mana.max_mp"]   = 50;

	// New blueprint only has Health component
	Json::Value newComponents(Json::arrayValue);
	Json::Value comp;
	comp["type"] = "Health";
	comp["fields"]["max_hp"] = 80;
	comp["fields"]["current_hp"] = 80;
	newComponents.append(comp);

	Json::Value analysis = SceneMutator::AnalyseChangeBlueprintJson(
		scene, "entity", "player", newComponents);

	EXPECT_EQ(analysis["transferCount"].asInt(), 1);
	EXPECT_EQ(analysis["orphanCount"].asInt(), 1);
	EXPECT_EQ(analysis["transferred"][0].asString(), "Health.max_hp");
	EXPECT_EQ(analysis["orphaned"][0].asString(), "Mana.max_mp");
}

TEST(SceneMutator_ChangeBlueprint, TransfersOverridesDropsOrphans)
{
	Json::Value scene = MakeMinimalScene();
	AddEntity(scene, "player", "old_bp");
	scene["scene2d"]["entities"][0]["instance_data"]["Health.max_hp"] = 100;
	scene["scene2d"]["entities"][0]["instance_data"]["Mana.max_mp"]   = 50;

	Json::Value newComponents(Json::arrayValue);
	Json::Value comp;
	comp["type"] = "Health";
	comp["fields"]["max_hp"] = 80;
	newComponents.append(comp);

	char err[256] = {};
	EXPECT_TRUE(SceneMutator::ChangeBlueprint(scene, "entity", "player", "new_bp", newComponents, err, sizeof(err)));

	EXPECT_EQ(scene["scene2d"]["entities"][0]["blueprint"]["value"].asString(), "new_bp");
	EXPECT_TRUE(scene["scene2d"]["entities"][0]["instance_data"].isMember("Health.max_hp"));
	EXPECT_FALSE(scene["scene2d"]["entities"][0]["instance_data"].isMember("Mana.max_mp"));
}

TEST(SceneMutator_ChangeBlueprint, NotFound_ReturnsFalse)
{
	Json::Value scene = MakeMinimalScene();
	Json::Value newComponents(Json::arrayValue);
	char err[256] = {};
	EXPECT_FALSE(SceneMutator::ChangeBlueprint(scene, "entity", "ghost", "new_bp", newComponents, err, sizeof(err)));
}

// ═══════════════════════════════════════════════════════════════════════════
// Layer CRUD
// ═══════════════════════════════════════════════════════════════════════════

TEST(SceneMutator_AddLayer, HappyPath)
{
	Json::Value scene = MakeMinimalScene();
	char err[256] = {};
	EXPECT_TRUE(SceneMutator::AddLayer(scene, "foreground", err, sizeof(err)));
	EXPECT_EQ(scene["scene2d"]["layers"].size(), 1u);
	EXPECT_EQ(scene["scene2d"]["layers"][0]["id"]["value"].asString(), "foreground");
}

TEST(SceneMutator_AddLayer, Duplicate_ReturnsFalse)
{
	Json::Value scene = MakeMinimalScene();
	AddLayer(scene, "bg");
	char err[256] = {};
	EXPECT_FALSE(SceneMutator::AddLayer(scene, "bg", err, sizeof(err)));
}

TEST(SceneMutator_DeleteLayer, RemovesAndCleansLightRefs)
{
	Json::Value scene = MakeMinimalScene();
	AddLayer(scene, "bg", -10);
	AddLayer(scene, "fg", 10);
	AddLight(scene, "sun");
	scene["scene2d"]["lights"][0]["affects_layers"].append(MakeWrappedId("bg"));
	scene["scene2d"]["lights"][0]["affects_layers"].append(MakeWrappedId("fg"));

	char err[256] = {};
	EXPECT_TRUE(SceneMutator::DeleteLayer(scene, "bg", err, sizeof(err)));
	EXPECT_EQ(scene["scene2d"]["layers"].size(), 1u);
	EXPECT_EQ(scene["scene2d"]["lights"][0]["affects_layers"].size(), 1u);
	EXPECT_EQ(scene["scene2d"]["lights"][0]["affects_layers"][0]["value"].asString(), "fg");
}

TEST(SceneMutator_DeleteLayer, LastLayer_ReturnsFalse)
{
	Json::Value scene = MakeMinimalScene();
	AddLayer(scene, "only_one");
	char err[256] = {};
	EXPECT_FALSE(SceneMutator::DeleteLayer(scene, "only_one", err, sizeof(err)));
}

TEST(SceneMutator_ReorderLayer, MovesToNewIndex)
{
	Json::Value scene = MakeMinimalScene();
	AddLayer(scene, "A", 0);
	AddLayer(scene, "B", 10);
	AddLayer(scene, "C", 20);

	char err[256] = {};
	EXPECT_TRUE(SceneMutator::ReorderLayer(scene, "C", 0, err, sizeof(err)));
	EXPECT_EQ(scene["scene2d"]["layers"][0]["id"]["value"].asString(), "C");
	EXPECT_EQ(scene["scene2d"]["layers"][1]["id"]["value"].asString(), "A");
	EXPECT_EQ(scene["scene2d"]["layers"][2]["id"]["value"].asString(), "B");
}

TEST(SceneMutator_UpdateLayer, PatchesFields)
{
	Json::Value scene = MakeMinimalScene();
	AddLayer(scene, "bg", 0);

	Json::Value fields;
	fields["sort_order"] = 99;
	fields["enabled"] = false;

	char err[256] = {};
	EXPECT_TRUE(SceneMutator::UpdateLayer(scene, "bg", fields, err, sizeof(err)));
	EXPECT_EQ(scene["scene2d"]["layers"][0]["sort_order"].asInt(), 99);
	EXPECT_FALSE(scene["scene2d"]["layers"][0]["enabled"].asBool());
}

// ═══════════════════════════════════════════════════════════════════════════
// Camera / Light
// ═══════════════════════════════════════════════════════════════════════════

TEST(SceneMutator_SetCameraActive, ExclusiveSwitch)
{
	Json::Value scene = MakeMinimalScene();
	AddCamera(scene, "cam_a", true);
	AddCamera(scene, "cam_b", false);

	char err[256] = {};
	EXPECT_TRUE(SceneMutator::SetCameraActive(scene, "cam_b", err, sizeof(err)));
	EXPECT_FALSE(scene["scene2d"]["cameras"][0]["active"].asBool());
	EXPECT_TRUE(scene["scene2d"]["cameras"][1]["active"].asBool());
}

TEST(SceneMutator_SetLightAffectsLayers, WrapsIds)
{
	Json::Value scene = MakeMinimalScene();
	AddLight(scene, "torch");

	Json::Value layerIds(Json::arrayValue);
	layerIds.append("bg");
	layerIds.append("fg");

	char err[256] = {};
	EXPECT_TRUE(SceneMutator::SetLightAffectsLayers(scene, "torch", layerIds, err, sizeof(err)));
	const Json::Value& al = scene["scene2d"]["lights"][0]["affects_layers"];
	EXPECT_EQ(al.size(), 2u);
	EXPECT_EQ(al[0]["value"].asString(), "bg");
	EXPECT_EQ(al[1]["value"].asString(), "fg");
}

// ═══════════════════════════════════════════════════════════════════════════
// Override Management
// ═══════════════════════════════════════════════════════════════════════════

TEST(SceneMutator_AddOverride, InsertsKey)
{
	Json::Value scene = MakeMinimalScene();
	AddEntity(scene, "obj");
	char err[256] = {};
	EXPECT_TRUE(SceneMutator::AddOverride(scene, "entity", "obj", "Health.max_hp", Json::Value(100), err, sizeof(err)));
	EXPECT_EQ(scene["scene2d"]["entities"][0]["instance_data"]["Health.max_hp"].asInt(), 100);
}

TEST(SceneMutator_RemoveOverride, DeletesKey)
{
	Json::Value scene = MakeMinimalScene();
	AddEntity(scene, "obj");
	scene["scene2d"]["entities"][0]["instance_data"]["Health.max_hp"] = 50;
	char err[256] = {};
	EXPECT_TRUE(SceneMutator::RemoveOverride(scene, "entity", "obj", "Health.max_hp", err, sizeof(err)));
	EXPECT_FALSE(scene["scene2d"]["entities"][0]["instance_data"].isMember("Health.max_hp"));
}

TEST(SceneMutator_UpdateOverride, ChangesValue)
{
	Json::Value scene = MakeMinimalScene();
	AddEntity(scene, "obj");
	scene["scene2d"]["entities"][0]["instance_data"]["Health.max_hp"] = 50;
	char err[256] = {};
	EXPECT_TRUE(SceneMutator::UpdateOverride(scene, "entity", "obj", "Health.max_hp", Json::Value(999), err, sizeof(err)));
	EXPECT_EQ(scene["scene2d"]["entities"][0]["instance_data"]["Health.max_hp"].asInt(), 999);
}
