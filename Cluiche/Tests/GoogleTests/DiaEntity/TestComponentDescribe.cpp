#include <gtest/gtest.h>
#include <cstring>
#include <DiaEntity/Testing/TestDescribedComponent.h>
#include <DiaEntity/ComponentDescribe.h>

using namespace Dia::Entity;
using DiaEntityTesting::TestDescribedComponent;

namespace {

constexpr unsigned kMaxFields = 16u;

const ComponentFieldInfo* Find(const ComponentFieldInfo* infos, unsigned n, const char* name) {
    for (unsigned i = 0; i < n; ++i) {
        if (std::strcmp(infos[i].name, name) == 0) return &infos[i];
    }
    return nullptr;
}

} // namespace

TEST(ComponentDescribe, ReturnsOneItemPerField) {
    ComponentFieldInfo infos[kMaxFields];
    const ComponentTypeDesc& desc = TestDescribedComponent::GetDesc();
    EXPECT_EQ(DescribeComponentFields(desc, infos, kMaxFields), static_cast<unsigned>(desc.fieldCount));
}

TEST(ComponentDescribe, TypeIdMatchesComponentTypeIdString) {
    ComponentFieldInfo infos[kMaxFields];
    unsigned n = DescribeComponentFields(TestDescribedComponent::GetDesc(), infos, kMaxFields);
    ASSERT_GT(n, 0u);
    for (unsigned i = 0; i < n; ++i) {
        EXPECT_STREQ(infos[i].typeId, "test-described-component");
    }
}

TEST(ComponentDescribe, DescribedFieldReportsLabelUnitDescription) {
    ComponentFieldInfo infos[kMaxFields];
    unsigned n = DescribeComponentFields(TestDescribedComponent::GetDesc(), infos, kMaxFields);
    const ComponentFieldInfo* speed = Find(infos, n, "speed");
    ASSERT_NE(speed, nullptr);
    EXPECT_TRUE(speed->described);
    EXPECT_STREQ(speed->label, "Speed");
    EXPECT_STREQ(speed->unit, "m/s");
    EXPECT_STREQ(speed->description, "Movement speed");
    EXPECT_EQ(speed->hint, nullptr);
}

TEST(ComponentDescribe, HintIsReported) {
    ComponentFieldInfo infos[kMaxFields];
    unsigned n = DescribeComponentFields(TestDescribedComponent::GetDesc(), infos, kMaxFields);
    const ComponentFieldInfo* hp = Find(infos, n, "hitPoints");
    ASSERT_NE(hp, nullptr);
    EXPECT_TRUE(hp->described);
    EXPECT_STREQ(hp->label, "Hit Points");
    EXPECT_STREQ(hp->hint, "Whole numbers only");
}

TEST(ComponentDescribe, UndescribedFieldFallsBackToMemberNameAndNullStrings) {
    ComponentFieldInfo infos[kMaxFields];
    unsigned n = DescribeComponentFields(TestDescribedComponent::GetDesc(), infos, kMaxFields);
    const ComponentFieldInfo* active = Find(infos, n, "active");
    ASSERT_NE(active, nullptr);
    EXPECT_FALSE(active->described);
    EXPECT_STREQ(active->label, "active");
    EXPECT_EQ(active->description, nullptr);
    EXPECT_EQ(active->unit, nullptr);
    EXPECT_EQ(active->hint, nullptr);
}

TEST(ComponentDescribe, RangeReportedFromAttribute) {
    ComponentFieldInfo infos[kMaxFields];
    unsigned n = DescribeComponentFields(TestDescribedComponent::GetDesc(), infos, kMaxFields);
    const ComponentFieldInfo* speed = Find(infos, n, "speed");
    ASSERT_NE(speed, nullptr);
    EXPECT_TRUE(speed->hasRange);
    EXPECT_DOUBLE_EQ(speed->min, 0.0);
    EXPECT_DOUBLE_EQ(speed->max, 20.0);
    const ComponentFieldInfo* hp = Find(infos, n, "hitPoints");
    ASSERT_NE(hp, nullptr);
    EXPECT_TRUE(hp->hasRange);
    EXPECT_DOUBLE_EQ(hp->min, 1.0);
    EXPECT_DOUBLE_EQ(hp->max, 100.0);
}

TEST(ComponentDescribe, RangedButUndescribedField) {
    ComponentFieldInfo infos[kMaxFields];
    unsigned n = DescribeComponentFields(TestDescribedComponent::GetDesc(), infos, kMaxFields);
    const ComponentFieldInfo* armor = Find(infos, n, "armor");
    ASSERT_NE(armor, nullptr);
    EXPECT_FALSE(armor->described);
    EXPECT_STREQ(armor->label, "armor");
    EXPECT_TRUE(armor->hasRange);
    EXPECT_DOUBLE_EQ(armor->min, -1.0);
    EXPECT_DOUBLE_EQ(armor->max, 5.0);
}

TEST(ComponentDescribe, NoRangeMeansHasRangeFalse) {
    ComponentFieldInfo infos[kMaxFields];
    unsigned n = DescribeComponentFields(TestDescribedComponent::GetDesc(), infos, kMaxFields);
    const ComponentFieldInfo* active = Find(infos, n, "active");
    ASSERT_NE(active, nullptr);
    EXPECT_FALSE(active->hasRange);
}

TEST(ComponentDescribe, DefaultsComeFromFieldMacroNotZero) {
    ComponentFieldInfo infos[kMaxFields];
    unsigned n = DescribeComponentFields(TestDescribedComponent::GetDesc(), infos, kMaxFields);
    const ComponentFieldInfo* speed = Find(infos, n, "speed");
    const ComponentFieldInfo* hp = Find(infos, n, "hitPoints");
    const ComponentFieldInfo* active = Find(infos, n, "active");
    ASSERT_NE(speed, nullptr);
    ASSERT_NE(hp, nullptr);
    ASSERT_NE(active, nullptr);
    EXPECT_TRUE(speed->hasDefault);
    EXPECT_FLOAT_EQ(speed->defaultValue.asFloat(), 3.5f);
    EXPECT_TRUE(hp->hasDefault);
    EXPECT_EQ(hp->defaultValue.asInt(), 7);
    EXPECT_TRUE(active->hasDefault);
    EXPECT_TRUE(active->defaultValue.asBool());
}

TEST(ComponentDescribe, KindIsReported) {
    ComponentFieldInfo infos[kMaxFields];
    unsigned n = DescribeComponentFields(TestDescribedComponent::GetDesc(), infos, kMaxFields);
    const ComponentFieldInfo* speed = Find(infos, n, "speed");
    const ComponentFieldInfo* tag = Find(infos, n, "tag");
    const ComponentFieldInfo* nested = Find(infos, n, "nested");
    ASSERT_NE(speed, nullptr);
    ASSERT_NE(tag, nullptr);
    ASSERT_NE(nested, nullptr);
    EXPECT_EQ(speed->kind, FieldKind::Primitive);
    EXPECT_EQ(tag->kind, FieldKind::StringId);
    EXPECT_EQ(nested->kind, FieldKind::Nested);
}

TEST(ComponentDescribe, NestedFieldIsExactlyOneItemWithObjectDefault) {
    ComponentFieldInfo infos[kMaxFields];
    unsigned n = DescribeComponentFields(TestDescribedComponent::GetDesc(), infos, kMaxFields);
    unsigned nestedCount = 0;
    for (unsigned i = 0; i < n; ++i) {
        if (infos[i].kind == FieldKind::Nested) ++nestedCount;
        // Sub-fields (gain/count) must not appear as their own items.
        EXPECT_STRNE(infos[i].name, "gain");
        EXPECT_STRNE(infos[i].name, "count");
    }
    EXPECT_EQ(nestedCount, 1u);
    const ComponentFieldInfo* nested = Find(infos, n, "nested");
    ASSERT_NE(nested, nullptr);
    EXPECT_FALSE(nested->hasRange);
    // Documented choice: a nested default is reported as its JSON object.
    EXPECT_TRUE(nested->hasDefault);
    EXPECT_TRUE(nested->defaultValue.isObject());
    EXPECT_FLOAT_EQ(nested->defaultValue["gain"].asFloat(), 2.0f);
}

TEST(ComponentDescribe, MaxOutSmallerThanFieldCountIsRespected) {
    ComponentFieldInfo infos[kMaxFields];
    unsigned n = DescribeComponentFields(TestDescribedComponent::GetDesc(), infos, 2u);
    EXPECT_EQ(n, 2u);
    EXPECT_STREQ(infos[0].name, "speed");
    EXPECT_STREQ(infos[1].name, "hitPoints");
}

TEST(ComponentDescribe, HandBuiltDescWithoutConstructDefaultHasNoDefault) {
    const ComponentTypeDesc& real = TestDescribedComponent::GetDesc();
    ComponentTypeDesc desc = real;
    desc.constructDefault = nullptr;
    ComponentFieldInfo infos[kMaxFields];
    unsigned n = DescribeComponentFields(desc, infos, kMaxFields);
    ASSERT_EQ(n, static_cast<unsigned>(real.fieldCount));
    for (unsigned i = 0; i < n; ++i) {
        EXPECT_FALSE(infos[i].hasDefault);
    }
}

TEST(ComponentDescribe, ZeroFieldComponentReturnsZero) {
    ComponentTypeDesc desc {
        Dia::Core::StringCRC("no-fields"), "NoFields", 0u, 0u, 1u, 0u,
        nullptr, 0u, nullptr, 0u, nullptr, 0u, nullptr, nullptr
    };
    ComponentFieldInfo infos[1];
    EXPECT_EQ(DescribeComponentFields(desc, infos, 1u), 0u);
}
