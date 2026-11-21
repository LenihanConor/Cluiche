#include <gtest/gtest.h>
#include <DiaValueSchema/ComponentFieldCollector.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaEntity/Testing/TestDescribedComponent.h>
#include <DiaEntity/ComponentTypeDesc.h>
#include <string>

using namespace Dia::ValueSchema;
using Dia::Core::StringCRC;
using DiaEntityTesting::TestDescribedComponent;

namespace
{
	const char* kType = "test-described-component";

	const ValueInfo* FindField(const ValueSchemaRegistry& r, const char* field)
	{
		return r.Find(StringCRC(kType), StringCRC(field));
	}

	ComponentFieldReport CollectFixture(ValueSchemaRegistry& reg)
	{
		const Dia::Entity::ComponentTypeDesc* descs[1] = { &TestDescribedComponent::GetDesc() };
		return ComponentFieldCollector().Collect(descs, 1, reg);
	}
}

TEST(DiaValueSchemaComponentField, KeyForKindAndFullKey)
{
	ValueSchemaRegistry reg;
	ComponentFieldReport rep = CollectFixture(reg);
	const ValueInfo* v = FindField(reg, "speed");
	ASSERT_NE(v, nullptr);
	EXPECT_STREQ(v->mFor.AsCStr(), kType);
	EXPECT_STREQ(v->mKey.AsCStr(), "speed");
	EXPECT_EQ(v->mKind, ValueKind::ComponentField);
	EXPECT_STREQ(v->mFullKey.AsCStr(), "test-described-component/speed");
	EXPECT_EQ(reg.Find(StringCRC("test-described-component/speed")), v);
	EXPECT_EQ(rep.mCollected, 6u);
	EXPECT_EQ(rep.mRejectedCount, 0u);
}

TEST(DiaValueSchemaComponentField, LabelDescriptionUnitHint)
{
	ValueSchemaRegistry reg;
	CollectFixture(reg);
	const ValueInfo* speed = FindField(reg, "speed");
	ASSERT_NE(speed, nullptr);
	EXPECT_STREQ(speed->mLabel.AsCStr(), "Speed");
	EXPECT_STREQ(speed->mDescription.AsCStr(), "Movement speed");
	EXPECT_STREQ(speed->mUnit.AsCStr(), "m/s");
	EXPECT_STREQ(speed->mHint.AsCStr(), "");

	const ValueInfo* hp = FindField(reg, "hitPoints");
	ASSERT_NE(hp, nullptr);
	EXPECT_STREQ(hp->mLabel.AsCStr(), "Hit Points");
	EXPECT_STREQ(hp->mHint.AsCStr(), "Whole numbers only");
	EXPECT_STREQ(hp->mUnit.AsCStr(), "");
}

TEST(DiaValueSchemaComponentField, RangeAndDefaultAndTypeFloat)
{
	ValueSchemaRegistry reg;
	CollectFixture(reg);
	const ValueInfo* v = FindField(reg, "speed");
	ASSERT_NE(v, nullptr);
	EXPECT_TRUE(v->mHasMin);
	EXPECT_TRUE(v->mHasMax);
	EXPECT_DOUBLE_EQ(v->mMin, 0.0);
	EXPECT_DOUBLE_EQ(v->mMax, 20.0);
	EXPECT_TRUE(v->mHasDefault);
	EXPECT_DOUBLE_EQ(v->mDefaultNumber, 3.5);
	EXPECT_EQ(v->mType, ValueType::Float);
}

TEST(DiaValueSchemaComponentField, IntFieldHasIntType)
{
	ValueSchemaRegistry reg;
	CollectFixture(reg);
	const ValueInfo* v = FindField(reg, "hitPoints");
	ASSERT_NE(v, nullptr);
	EXPECT_EQ(v->mType, ValueType::Int);
	EXPECT_DOUBLE_EQ(v->mDefaultNumber, 7.0);
	EXPECT_DOUBLE_EQ(v->mMin, 1.0);
	EXPECT_DOUBLE_EQ(v->mMax, 100.0);
}

TEST(DiaValueSchemaComponentField, BoolFieldHasBoolType)
{
	ValueSchemaRegistry reg;
	CollectFixture(reg);
	const ValueInfo* v = FindField(reg, "active");
	ASSERT_NE(v, nullptr);
	EXPECT_EQ(v->mType, ValueType::Bool);
	EXPECT_TRUE(v->mHasDefault);
	EXPECT_DOUBLE_EQ(v->mDefaultNumber, 1.0);
	EXPECT_FALSE(v->mHasMin);
	EXPECT_FALSE(v->mHasMax);
}

TEST(DiaValueSchemaComponentField, StringIdFieldHasStringType)
{
	ValueSchemaRegistry reg;
	CollectFixture(reg);
	const ValueInfo* v = FindField(reg, "tag");
	ASSERT_NE(v, nullptr);
	EXPECT_EQ(v->mType, ValueType::String);
}

TEST(DiaValueSchemaComponentField, NestedFieldIsStringWithUiNested)
{
	ValueSchemaRegistry reg;
	CollectFixture(reg);
	const ValueInfo* v = FindField(reg, "nested");
	ASSERT_NE(v, nullptr);
	EXPECT_EQ(v->mType, ValueType::String);
	EXPECT_TRUE(v->mUi["nested"].asBool());
	EXPECT_STREQ(v->mLabel.AsCStr(), "Nested");
}

TEST(DiaValueSchemaComponentField, RangedUndescribedFieldRegisteredWithMemberLabelAndListed)
{
	ValueSchemaRegistry reg;
	ComponentFieldReport rep = CollectFixture(reg);
	const ValueInfo* v = FindField(reg, "armor");
	ASSERT_NE(v, nullptr);
	EXPECT_STREQ(v->mLabel.AsCStr(), "armor");
	EXPECT_STREQ(v->mDescription.AsCStr(), "");
	EXPECT_DOUBLE_EQ(v->mMin, -1.0);
	EXPECT_DOUBLE_EQ(v->mMax, 5.0);

	ASSERT_EQ(rep.mUndescribedFields.Size(), 1u);
	EXPECT_STREQ(rep.mUndescribedFields[0].mName.AsCStr(), "test-described-component/armor");
	EXPECT_STREQ(rep.mUndescribedFields[0].mOwner.AsCStr(), kType);
	EXPECT_EQ(rep.mUndescribed, 1u);
}

TEST(DiaValueSchemaComponentField, FieldWithNoAttributesIsNotReportedUndescribed)
{
	ValueSchemaRegistry reg;
	ComponentFieldReport rep = CollectFixture(reg);
	ASSERT_NE(FindField(reg, "active"), nullptr);
	for (unsigned int i = 0; i < rep.mUndescribedFields.Size(); ++i)
	{
		EXPECT_STRNE(rep.mUndescribedFields[i].mName.AsCStr(), "test-described-component/active");
	}
	// speed, hitPoints, nested are described.
	EXPECT_EQ(rep.mDescribed, 3u);
}

TEST(DiaValueSchemaComponentField, QueryableThroughService)
{
	ValueSchemaRegistry reg;
	CollectFixture(reg);
	reg.Publish();
	ValueSchemaService svc(reg);
	const ValueInfo* v = svc.Find(StringCRC("test-described-component/speed"));
	ASSERT_NE(v, nullptr);
	EXPECT_EQ(v->mKind, ValueKind::ComponentField);
	EXPECT_EQ(svc.Find(StringCRC(kType), StringCRC("speed")), v);
}

TEST(DiaValueSchemaComponentField, ComponentWithNoFieldsRegistersNothing)
{
	Dia::Entity::ComponentTypeDesc desc {
		StringCRC("empty-component"), "EmptyComponent", 0u, 0u, 1u, 0u,
		nullptr, 0u, nullptr, 0u, nullptr, 0u, nullptr, nullptr
	};
	const Dia::Entity::ComponentTypeDesc* descs[1] = { &desc };
	ValueSchemaRegistry reg;
	ComponentFieldReport rep = ComponentFieldCollector().Collect(descs, 1, reg);
	EXPECT_EQ(rep.mCollected, 0u);
	EXPECT_EQ(reg.Count(), 0u);
}

TEST(DiaValueSchemaComponentField, DuplicateCollectionCountsDuplicatesNotRejections)
{
	ValueSchemaRegistry reg;
	CollectFixture(reg);
	ComponentFieldReport second = CollectFixture(reg);
	EXPECT_EQ(second.mCollected, 0u);
	EXPECT_EQ(second.mDuplicateCount, 6u);
	EXPECT_EQ(second.mRejectedCount, 0u);
	EXPECT_EQ(reg.Count(), 6u);
}

TEST(DiaValueSchemaComponentField, GlobalRegistryOverloadIncludesFixture)
{
	ValueSchemaRegistry reg;
	ComponentFieldReport rep = ComponentFieldCollector().Collect(reg);
	EXPECT_GE(rep.mCollected, 6u);
	EXPECT_NE(FindField(reg, "speed"), nullptr);
}
