#include <gtest/gtest.h>
#include <DiaSceneEditor/SceneValidator.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::SceneEditor;

namespace
{
	Json::Value MakeWrappedId(const char* id)
	{
		Json::Value v(Json::objectValue);
		v["value"] = id;
		return v;
	}

	Json::Value MakeValidScene()
	{
		Json::Value root;
		root["scene2d"]["layers"] = Json::Value(Json::arrayValue);
		root["scene2d"]["cameras"] = Json::Value(Json::arrayValue);
		root["scene2d"]["lights"] = Json::Value(Json::arrayValue);
		root["scene2d"]["entities"] = Json::Value(Json::arrayValue);

		// One layer
		Json::Value layer;
		layer["id"] = MakeWrappedId("default");
		layer["sort_order"] = 0;
		layer["enabled"] = true;
		root["scene2d"]["layers"].append(layer);

		// One active camera
		Json::Value cam;
		cam["id"] = MakeWrappedId("main_cam");
		cam["active"] = true;
		root["scene2d"]["cameras"].append(cam);

		// One entity
		Json::Value entity;
		entity["id"] = MakeWrappedId("player");
		root["scene2d"]["entities"].append(entity);

		return root;
	}
}

// ═══════════════════════════════════════════════════════════════════════════
// Valid scene
// ═══════════════════════════════════════════════════════════════════════════

TEST(SceneValidator, ValidScene_NoIssues)
{
	SceneValidator validator;
	Json::Value result = validator.Validate(MakeValidScene());
	EXPECT_TRUE(result["valid"].asBool());
	EXPECT_EQ(result["errors"].size(), 0u);
	EXPECT_EQ(result["warnings"].size(), 0u);
}

// ═══════════════════════════════════════════════════════════════════════════
// Camera checks
// ═══════════════════════════════════════════════════════════════════════════

TEST(SceneValidator, NoActiveCamera_Error)
{
	Json::Value scene = MakeValidScene();
	scene["scene2d"]["cameras"][0]["active"] = false;

	SceneValidator validator;
	Json::Value result = validator.Validate(scene);
	EXPECT_FALSE(result["valid"].asBool());
	EXPECT_GE(result["errors"].size(), 1u);
	EXPECT_EQ(result["errors"][0]["code"].asString(), "NO_ACTIVE_CAMERA");
}

TEST(SceneValidator, MultipleActiveCameras_Error)
{
	Json::Value scene = MakeValidScene();
	Json::Value cam2;
	cam2["id"] = MakeWrappedId("cam2");
	cam2["active"] = true;
	scene["scene2d"]["cameras"].append(cam2);

	SceneValidator validator;
	Json::Value result = validator.Validate(scene);
	EXPECT_FALSE(result["valid"].asBool());

	bool found = false;
	for (unsigned int i = 0; i < result["errors"].size(); ++i)
		if (result["errors"][i]["code"].asString() == "MULTIPLE_ACTIVE_CAMERAS") found = true;
	EXPECT_TRUE(found);
}

TEST(SceneValidator, SingleActiveCamera_Valid)
{
	SceneValidator validator;
	Json::Value result = validator.Validate(MakeValidScene());
	for (unsigned int i = 0; i < result["errors"].size(); ++i)
	{
		EXPECT_NE(result["errors"][i]["code"].asString(), "NO_ACTIVE_CAMERA");
		EXPECT_NE(result["errors"][i]["code"].asString(), "MULTIPLE_ACTIVE_CAMERAS");
	}
}

// ═══════════════════════════════════════════════════════════════════════════
// Layer checks
// ═══════════════════════════════════════════════════════════════════════════

TEST(SceneValidator, NoLayers_Warning)
{
	Json::Value scene = MakeValidScene();
	scene["scene2d"]["layers"] = Json::Value(Json::arrayValue);

	SceneValidator validator;
	Json::Value result = validator.Validate(scene);
	EXPECT_GE(result["warnings"].size(), 1u);
	EXPECT_EQ(result["warnings"][0]["code"].asString(), "NO_LAYERS");
}

// ═══════════════════════════════════════════════════════════════════════════
// ID checks
// ═══════════════════════════════════════════════════════════════════════════

TEST(SceneValidator, DuplicateId_Error)
{
	Json::Value scene = MakeValidScene();
	Json::Value entity2;
	entity2["id"] = MakeWrappedId("player");  // duplicate of existing entity
	scene["scene2d"]["entities"].append(entity2);

	SceneValidator validator;
	Json::Value result = validator.Validate(scene);
	EXPECT_FALSE(result["valid"].asBool());

	bool found = false;
	for (unsigned int i = 0; i < result["errors"].size(); ++i)
		if (result["errors"][i]["code"].asString() == "DUPLICATE_ID") found = true;
	EXPECT_TRUE(found);
}

TEST(SceneValidator, EmptyId_Error)
{
	Json::Value scene = MakeValidScene();
	Json::Value entity;
	entity["id"] = MakeWrappedId("");
	scene["scene2d"]["entities"].append(entity);

	SceneValidator validator;
	Json::Value result = validator.Validate(scene);
	EXPECT_FALSE(result["valid"].asBool());

	bool found = false;
	for (unsigned int i = 0; i < result["errors"].size(); ++i)
		if (result["errors"][i]["code"].asString() == "EMPTY_ID") found = true;
	EXPECT_TRUE(found);
}

// ═══════════════════════════════════════════════════════════════════════════
// Layer reference checks
// ═══════════════════════════════════════════════════════════════════════════

TEST(SceneValidator, UnknownLayerRef_Warning)
{
	Json::Value scene = MakeValidScene();
	Json::Value light;
	light["id"] = MakeWrappedId("torch");
	light["affects_layers"] = Json::Value(Json::arrayValue);
	light["affects_layers"].append(MakeWrappedId("nonexistent_layer"));
	scene["scene2d"]["lights"].append(light);

	SceneValidator validator;
	Json::Value result = validator.Validate(scene);

	bool found = false;
	for (unsigned int i = 0; i < result["warnings"].size(); ++i)
		if (result["warnings"][i]["code"].asString() == "UNKNOWN_LAYER_REF") found = true;
	EXPECT_TRUE(found);
}

TEST(SceneValidator, MissingScene2d_Error)
{
	Json::Value root(Json::objectValue);
	SceneValidator validator;
	Json::Value result = validator.Validate(root);
	EXPECT_FALSE(result["valid"].asBool());
	EXPECT_EQ(result["errors"][0]["code"].asString(), "MISSING_SCENE2D");
}
