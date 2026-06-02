#include <gtest/gtest.h>
#include <DiaBlueprintEditor/BlueprintListController.h>
#include <DiaAssetCatalogue/AssetRegistry.h>
#include <DiaAssetCatalogue/AssetRecord.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::BlueprintEditor;
using namespace Dia::AssetCatalogue;
using namespace Dia::Core;

// ===========================================================================
// IsBlueprintType
// ===========================================================================

TEST(BlueprintListController, IsBlueprintType_DiaEntity_ReturnsTrue)
{
	EXPECT_TRUE(BlueprintListController::IsBlueprintType(StringCRC("diaentity")));
}

TEST(BlueprintListController, IsBlueprintType_DiaCamera_ReturnsTrue)
{
	EXPECT_TRUE(BlueprintListController::IsBlueprintType(StringCRC("diacamera")));
}

TEST(BlueprintListController, IsBlueprintType_DiaLight_ReturnsTrue)
{
	EXPECT_TRUE(BlueprintListController::IsBlueprintType(StringCRC("dialight")));
}

TEST(BlueprintListController, IsBlueprintType_Texture_ReturnsFalse)
{
	EXPECT_FALSE(BlueprintListController::IsBlueprintType(StringCRC("texture")));
}

TEST(BlueprintListController, IsBlueprintType_Empty_ReturnsFalse)
{
	EXPECT_FALSE(BlueprintListController::IsBlueprintType(StringCRC("")));
}

// ===========================================================================
// BuildListJson — empty registry
// ===========================================================================

TEST(BlueprintListController, BuildListJson_EmptyRegistry_ReturnsSuccessWithNoGroups)
{
	AssetRegistry registry;
	BlueprintListController ctrl;

	Json::Value result = ctrl.BuildListJson(registry);

	EXPECT_TRUE(result["success"].asBool());
	EXPECT_EQ(result["groups"].size(), 0u);
}

// ===========================================================================
// BuildListJson — single type
// ===========================================================================

TEST(BlueprintListController, BuildListJson_OneEntityAsset_ReturnsSingleEntityGroup)
{
	AssetRegistry registry;
	AssetRecord rec;
	rec.mId          = StringCRC("diaentity.player");
	rec.mAssetTypeId = StringCRC("diaentity");
	rec.mSourcePath  = "Assets/player.diaentity";
	registry.Register(rec);

	BlueprintListController ctrl;
	Json::Value result = ctrl.BuildListJson(registry);

	EXPECT_TRUE(result["success"].asBool());
	ASSERT_EQ(result["groups"].size(), 1u);
	EXPECT_EQ(result["groups"][0]["label"].asString(), "Entity");
	ASSERT_EQ(result["groups"][0]["items"].size(), 1u);
	EXPECT_EQ(result["groups"][0]["items"][0]["id"].asString(), "diaentity.player");
}

// ===========================================================================
// BuildListJson — multiple types, ordering preserved
// ===========================================================================

TEST(BlueprintListController, BuildListJson_AllThreeTypes_GroupsInEntityCameraLightOrder)
{
	AssetRegistry registry;

	AssetRecord light;
	light.mId          = StringCRC("dialight.warm");
	light.mAssetTypeId = StringCRC("dialight");
	light.mSourcePath  = "Assets/warm.dialight";
	registry.Register(light);

	AssetRecord cam;
	cam.mId          = StringCRC("diacamera.follow");
	cam.mAssetTypeId = StringCRC("diacamera");
	cam.mSourcePath  = "Assets/follow.diacamera";
	registry.Register(cam);

	AssetRecord entity;
	entity.mId          = StringCRC("diaentity.enemy");
	entity.mAssetTypeId = StringCRC("diaentity");
	entity.mSourcePath  = "Assets/enemy.diaentity";
	registry.Register(entity);

	BlueprintListController ctrl;
	Json::Value result = ctrl.BuildListJson(registry);

	EXPECT_TRUE(result["success"].asBool());
	ASSERT_EQ(result["groups"].size(), 3u);
	EXPECT_EQ(result["groups"][0]["label"].asString(), "Entity");
	EXPECT_EQ(result["groups"][1]["label"].asString(), "Camera");
	EXPECT_EQ(result["groups"][2]["label"].asString(), "Light");
}

TEST(BlueprintListController, BuildListJson_NonBlueprintAssetsIgnored)
{
	AssetRegistry registry;

	AssetRecord tex;
	tex.mId          = StringCRC("texture.player");
	tex.mAssetTypeId = StringCRC("texture");
	tex.mSourcePath  = "Assets/player.png";
	registry.Register(tex);

	AssetRecord entity;
	entity.mId          = StringCRC("diaentity.player");
	entity.mAssetTypeId = StringCRC("diaentity");
	entity.mSourcePath  = "Assets/player.diaentity";
	registry.Register(entity);

	BlueprintListController ctrl;
	Json::Value result = ctrl.BuildListJson(registry);

	EXPECT_TRUE(result["success"].asBool());
	ASSERT_EQ(result["groups"].size(), 1u);
	EXPECT_EQ(result["groups"][0]["label"].asString(), "Entity");
}
