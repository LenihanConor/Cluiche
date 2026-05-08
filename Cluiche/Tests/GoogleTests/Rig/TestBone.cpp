#include <gtest/gtest.h>

#include <DiaRig2D/Bone.h>

using namespace Dia::Rig2D;

TEST(Rig2D_Bone, DefaultConstruction_CorrectDefaults)
{
	Bone bone;
	EXPECT_EQ(bone.parentIndex, -1);
	EXPECT_FLOAT_EQ(bone.localPosition.x, 0.0f);
	EXPECT_FLOAT_EQ(bone.localPosition.y, 0.0f);
	EXPECT_FLOAT_EQ(bone.localRotation, 0.0f);
	EXPECT_FLOAT_EQ(bone.localScale.x, 1.0f);
	EXPECT_FLOAT_EQ(bone.localScale.y, 1.0f);
	EXPECT_FLOAT_EQ(bone.length, 0.0f);
	EXPECT_EQ(bone.metadata.Size(), 0u);
}

TEST(Rig2D_Bone, Metadata_SetAndFind)
{
	Bone bone;
	bone.SetMetadata(Dia::Core::StringCRC("ik_end"), MetadataValue::FromBool(true));
	bone.SetMetadata(Dia::Core::StringCRC("chain_len"), MetadataValue::FromInt(3));
	bone.SetMetadata(Dia::Core::StringCRC("stiffness"), MetadataValue::FromFloat(0.5f));
	bone.SetMetadata(Dia::Core::StringCRC("slot"), MetadataValue::FromString("sword"));

	const MetadataValue* ikEnd = bone.FindMetadata(Dia::Core::StringCRC("ik_end"));
	ASSERT_NE(ikEnd, nullptr);
	EXPECT_EQ(ikEnd->type, MetadataValue::kBool);
	EXPECT_TRUE(ikEnd->boolVal);

	const MetadataValue* chainLen = bone.FindMetadata(Dia::Core::StringCRC("chain_len"));
	ASSERT_NE(chainLen, nullptr);
	EXPECT_EQ(chainLen->type, MetadataValue::kInt);
	EXPECT_EQ(chainLen->intVal, 3);

	const MetadataValue* stiffness = bone.FindMetadata(Dia::Core::StringCRC("stiffness"));
	ASSERT_NE(stiffness, nullptr);
	EXPECT_EQ(stiffness->type, MetadataValue::kFloat);
	EXPECT_FLOAT_EQ(stiffness->floatVal, 0.5f);

	const MetadataValue* slot = bone.FindMetadata(Dia::Core::StringCRC("slot"));
	ASSERT_NE(slot, nullptr);
	EXPECT_EQ(slot->type, MetadataValue::kString);
}

TEST(Rig2D_Bone, Metadata_UpdateExistingKey)
{
	Bone bone;
	bone.SetMetadata(Dia::Core::StringCRC("val"), MetadataValue::FromInt(1));
	bone.SetMetadata(Dia::Core::StringCRC("val"), MetadataValue::FromInt(42));

	EXPECT_EQ(bone.metadata.Size(), 1u);
	const MetadataValue* val = bone.FindMetadata(Dia::Core::StringCRC("val"));
	ASSERT_NE(val, nullptr);
	EXPECT_EQ(val->intVal, 42);
}

TEST(Rig2D_Bone, Metadata_FindMissing_ReturnsNull)
{
	Bone bone;
	EXPECT_EQ(bone.FindMetadata(Dia::Core::StringCRC("missing")), nullptr);
}
