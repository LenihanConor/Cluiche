#include <gtest/gtest.h>
#include <DiaEntityTemplateEditor/BlueprintListController.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::EntityTemplateEditor;
using namespace Dia::Core;

// Helper: build a records array as returned by asset_catalogue.query_by_type
static Json::Value MakeRecords(const char* const* ids, const char* const* paths, unsigned int count)
{
	Json::Value arr(Json::arrayValue);
	for (unsigned int i = 0; i < count; ++i)
	{
		Json::Value rec;
		rec["id"]          = ids[i];
		rec["source_path"] = paths[i];
		arr.append(rec);
	}
	return arr;
}

static const Json::Value kEmpty(Json::arrayValue);

// ===========================================================================
// IsBlueprintType
// ===========================================================================

TEST(BlueprintListController, IsBlueprintType_DiaEntity_ReturnsTrue)
{
	EXPECT_TRUE(BlueprintListController::IsBlueprintType(StringCRC("diaentitytemplate")));
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
// BuildListJson — all-empty
// ===========================================================================

TEST(BlueprintListController, BuildListJson_AllEmpty_ReturnsSuccessWithNoGroups)
{
	BlueprintListController ctrl;
	Json::Value result = ctrl.BuildListJson(kEmpty, kEmpty, kEmpty);
	EXPECT_TRUE(result["success"].asBool());
	EXPECT_EQ(result["groups"].size(), 0u);
}

// ===========================================================================
// BuildListJson — single type
// ===========================================================================

TEST(BlueprintListController, BuildListJson_OneEntityAsset_ReturnsSingleEntityGroup)
{
	const char* ids[]   = { "diaentitytemplate.player" };
	const char* paths[] = { "Assets/player.diaentitytemplate" };
	Json::Value entities = MakeRecords(ids, paths, 1);

	BlueprintListController ctrl;
	Json::Value result = ctrl.BuildListJson(entities, kEmpty, kEmpty);

	EXPECT_TRUE(result["success"].asBool());
	ASSERT_EQ(result["groups"].size(), 1u);
	EXPECT_EQ(result["groups"][0]["label"].asString(), "Entity");
	ASSERT_EQ(result["groups"][0]["items"].size(), 1u);
	EXPECT_EQ(result["groups"][0]["items"][0]["id"].asString(), "diaentitytemplate.player");
}

// ===========================================================================
// BuildListJson — ordering Entity → Camera → Light preserved
// ===========================================================================

TEST(BlueprintListController, BuildListJson_AllThreeTypes_GroupsInEntityCameraLightOrder)
{
	const char* eIds[]  = { "diaentitytemplate.enemy" };
	const char* ePaths[] = { "Assets/enemy.diaentitytemplate" };
	const char* cIds[]  = { "diacamera.follow" };
	const char* cPaths[] = { "Assets/follow.diacamera" };
	const char* lIds[]  = { "dialight.warm" };
	const char* lPaths[] = { "Assets/warm.dialight" };

	BlueprintListController ctrl;
	Json::Value result = ctrl.BuildListJson(
		MakeRecords(eIds, ePaths, 1),
		MakeRecords(cIds, cPaths, 1),
		MakeRecords(lIds, lPaths, 1));

	EXPECT_TRUE(result["success"].asBool());
	ASSERT_EQ(result["groups"].size(), 3u);
	EXPECT_EQ(result["groups"][0]["label"].asString(), "Entity");
	EXPECT_EQ(result["groups"][1]["label"].asString(), "Camera");
	EXPECT_EQ(result["groups"][2]["label"].asString(), "Light");
}

TEST(BlueprintListController, BuildListJson_MultipleItemsInGroup_AllPresent)
{
	const char* ids[]   = { "diaentitytemplate.a", "diaentitytemplate.b", "diaentitytemplate.c" };
	const char* paths[] = { "a.diaentitytemplate", "b.diaentitytemplate", "c.diaentitytemplate" };
	Json::Value entities = MakeRecords(ids, paths, 3);

	BlueprintListController ctrl;
	Json::Value result = ctrl.BuildListJson(entities, kEmpty, kEmpty);

	ASSERT_EQ(result["groups"].size(), 1u);
	EXPECT_EQ(result["groups"][0]["items"].size(), 3u);
}

TEST(BlueprintListController, BuildListJson_ItemHasCorrectPath)
{
	const char* ids[]   = { "diacamera.follow" };
	const char* paths[] = { "Assets/Cameras/follow.diacamera" };
	Json::Value cameras = MakeRecords(ids, paths, 1);

	BlueprintListController ctrl;
	Json::Value result = ctrl.BuildListJson(kEmpty, cameras, kEmpty);

	ASSERT_EQ(result["groups"].size(), 1u);
	EXPECT_EQ(result["groups"][0]["items"][0]["path"].asString(), "Assets/Cameras/follow.diacamera");
}

TEST(BlueprintListController, BuildListJson_GroupHasCorrectTypeId)
{
	const char* ids[]   = { "dialight.warm" };
	const char* paths[] = { "Assets/warm.dialight" };
	Json::Value lights = MakeRecords(ids, paths, 1);

	BlueprintListController ctrl;
	Json::Value result = ctrl.BuildListJson(kEmpty, kEmpty, lights);

	ASSERT_EQ(result["groups"].size(), 1u);
	EXPECT_EQ(result["groups"][0]["typeId"].asString(), "dialight");
}

// Empty group is omitted when only other types are present
TEST(BlueprintListController, BuildListJson_OnlyCameraAndLight_NoEntityGroup)
{
	const char* cIds[]  = { "diacamera.c" };
	const char* cPaths[] = { "c.diacamera" };
	const char* lIds[]  = { "dialight.l" };
	const char* lPaths[] = { "l.dialight" };

	BlueprintListController ctrl;
	Json::Value result = ctrl.BuildListJson(
		kEmpty,
		MakeRecords(cIds, cPaths, 1),
		MakeRecords(lIds, lPaths, 1));

	ASSERT_EQ(result["groups"].size(), 2u);
	EXPECT_EQ(result["groups"][0]["label"].asString(), "Camera");
	EXPECT_EQ(result["groups"][1]["label"].asString(), "Light");
}
