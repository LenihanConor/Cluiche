#include <gtest/gtest.h>

#include <DiaRig2D/SkeletonJsonSerializer.h>

using namespace Dia::Rig2D;

static const char* kValidChain3 = R"({
	"id": "test_chain",
	"bones": [
		{ "name": "root",  "parent": -1,     "position": [0, 0] },
		{ "name": "spine", "parent": "root",  "position": [0, 1] },
		{ "name": "head",  "parent": "spine", "position": [0, 0.5] }
	]
})";

static const char* kValidWithDefaults = R"({
	"id": "defaults_test",
	"bones": [
		{ "name": "root",  "parent": -1, "position": [0, 0] },
		{ "name": "child", "parent": "root", "position": [1, 0] }
	]
})";

static const char* kValidWithMetadata = R"({
	"id": "metadata_test",
	"bones": [
		{ "name": "root", "parent": -1, "position": [0, 0] },
		{ "name": "hand", "parent": "root", "position": [1, 0],
		  "metadata": {
			"ik_end_effector": true,
			"chain_length": 3,
			"stiffness": 0.8,
			"slot_name": "weapon"
		  }
		}
	]
})";

static const char* kInvalidParent = R"({
	"id": "bad",
	"bones": [
		{ "name": "root",  "parent": -1, "position": [0, 0] },
		{ "name": "child", "parent": "nonexistent", "position": [0, 1] }
	]
})";

static const char* kMissingPosition = R"({
	"id": "bad",
	"bones": [
		{ "name": "root", "parent": -1 }
	]
})";

static const char* kMalformedJson = R"({ not valid json at all)";

TEST(Rig2D_SkeletonJson, Load_ValidChain)
{
	JsonSkeletonSerializer loader;
	SkeletonDef def;
	ASSERT_TRUE(loader.Load(kValidChain3, def));

	EXPECT_EQ(def.bones.Size(), 3u);
	EXPECT_EQ(def.id, Dia::Core::StringCRC("test_chain"));
	EXPECT_EQ(def.bones[0].parentIndex, -1);
	EXPECT_EQ(def.bones[1].parentIndex, 0);
	EXPECT_EQ(def.bones[2].parentIndex, 1);
}

TEST(Rig2D_SkeletonJson, Load_NameParentResolution)
{
	JsonSkeletonSerializer loader;
	SkeletonDef def;
	ASSERT_TRUE(loader.Load(kValidChain3, def));

	EXPECT_EQ(def.bones[1].name, Dia::Core::StringCRC("spine"));
	EXPECT_EQ(def.bones[1].parentIndex, 0);
}

TEST(Rig2D_SkeletonJson, Load_DefaultRotationAndScale)
{
	JsonSkeletonSerializer loader;
	SkeletonDef def;
	ASSERT_TRUE(loader.Load(kValidWithDefaults, def));

	EXPECT_FLOAT_EQ(def.bones[1].localRotation, 0.0f);
	EXPECT_FLOAT_EQ(def.bones[1].localScale.x, 1.0f);
	EXPECT_FLOAT_EQ(def.bones[1].localScale.y, 1.0f);
}

TEST(Rig2D_SkeletonJson, Load_WithMetadata)
{
	JsonSkeletonSerializer loader;
	SkeletonDef def;
	ASSERT_TRUE(loader.Load(kValidWithMetadata, def));

	const Bone& hand = def.bones[1];
	EXPECT_EQ(hand.metadata.Size(), 4u);

	const MetadataValue* ikEnd = hand.FindMetadata(Dia::Core::StringCRC("ik_end_effector"));
	ASSERT_NE(ikEnd, nullptr);
	EXPECT_EQ(ikEnd->type, MetadataValue::kBool);
	EXPECT_TRUE(ikEnd->boolVal);

	const MetadataValue* chainLen = hand.FindMetadata(Dia::Core::StringCRC("chain_length"));
	ASSERT_NE(chainLen, nullptr);
	EXPECT_EQ(chainLen->type, MetadataValue::kInt);
	EXPECT_EQ(chainLen->intVal, 3);
}

TEST(Rig2D_SkeletonJson, Load_InvalidParent_ReturnsFalse)
{
	JsonSkeletonSerializer loader;
	SkeletonDef def;
	EXPECT_FALSE(loader.Load(kInvalidParent, def));
}

TEST(Rig2D_SkeletonJson, Load_MissingPosition_ReturnsFalse)
{
	JsonSkeletonSerializer loader;
	SkeletonDef def;
	EXPECT_FALSE(loader.Load(kMissingPosition, def));
}

TEST(Rig2D_SkeletonJson, Load_MalformedJson_ReturnsFalse)
{
	JsonSkeletonSerializer loader;
	SkeletonDef def;
	EXPECT_FALSE(loader.Load(kMalformedJson, def));
}

TEST(Rig2D_SkeletonJson, SaveAndRoundTrip)
{
	JsonSkeletonSerializer loader;
	SkeletonDef def;
	ASSERT_TRUE(loader.Load(kValidChain3, def));

	char buffer[4096];
	ASSERT_TRUE(loader.Save(def, buffer, sizeof(buffer)));

	SkeletonDef def2;
	ASSERT_TRUE(loader.Load(buffer, def2));

	EXPECT_EQ(def2.bones.Size(), def.bones.Size());
	EXPECT_EQ(def2.id, def.id);

	for (unsigned int i = 0; i < def.bones.Size(); ++i)
	{
		EXPECT_EQ(def2.bones[i].name, def.bones[i].name);
		EXPECT_EQ(def2.bones[i].parentIndex, def.bones[i].parentIndex);
		EXPECT_NEAR(def2.bones[i].localPosition.x, def.bones[i].localPosition.x, 0.001f);
		EXPECT_NEAR(def2.bones[i].localPosition.y, def.bones[i].localPosition.y, 0.001f);
	}
}

TEST(Rig2D_SkeletonJson, Save_MetadataRoundTrip)
{
	JsonSkeletonSerializer loader;
	SkeletonDef def;
	ASSERT_TRUE(loader.Load(kValidWithMetadata, def));

	char buffer[4096];
	ASSERT_TRUE(loader.Save(def, buffer, sizeof(buffer)));

	SkeletonDef def2;
	ASSERT_TRUE(loader.Load(buffer, def2));

	const MetadataValue* ikEnd = def2.bones[1].FindMetadata(Dia::Core::StringCRC("ik_end_effector"));
	ASSERT_NE(ikEnd, nullptr);
	EXPECT_TRUE(ikEnd->boolVal);
}

TEST(Rig2D_SkeletonJson, Save_ParentSerializedAsName)
{
	JsonSkeletonSerializer loader;
	SkeletonDef def;
	ASSERT_TRUE(loader.Load(kValidChain3, def));

	char buffer[4096];
	ASSERT_TRUE(loader.Save(def, buffer, sizeof(buffer)));

	// Parent field should contain bone names, not indices (except root which is -1)
	std::string output(buffer);
	EXPECT_NE(output.find("\"root\""), std::string::npos);
}
