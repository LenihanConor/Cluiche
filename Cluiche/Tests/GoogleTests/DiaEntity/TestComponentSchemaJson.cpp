#include <gtest/gtest.h>
#include <cstring>
#include <DiaEntity/ComponentSchemaJson.h>
#include <DiaEntity/Testing/TestDescribedComponent.h>

using namespace Dia::Entity;
using DiaEntityTesting::TestDescribedComponent;

namespace {

    const Json::Value* FindField(const Json::Value& schema, const char* name) {
        const Json::Value& fields = schema["fields"];
        for (Json::ArrayIndex i = 0; i < fields.size(); ++i) {
            if (fields[i]["name"].asString() == name) {
                return &fields[i];
            }
        }
        return nullptr;
    }

    void SaveMarker(const IComponent*, Json::Value& out) {
        out["marker"] = 0;
    }

    const FieldDesc kHandFields[] = {
        { "a", 0u, 0u, FieldKind::Primitive, false },
        { "b", 0u, 4u, FieldKind::StringId,  false },
        { "c", 0u, 8u, FieldKind::Container, false },
    };

    ComponentTypeDesc MakeHandDesc(SaveToJsonFn save) {
        return ComponentTypeDesc {
            Dia::Core::StringCRC("hand-schema"), "HandSchema", 16u, 4u, 1u, 0u,
            kHandFields, 3u, nullptr, 0u, nullptr, 0u, nullptr, save
        };
    }

} // namespace

TEST(DiaEntityComponentSchemaJson, KeepsLegacyComponentKeys) {
    Json::Value schema = BuildComponentSchemaJson(TestDescribedComponent::GetDesc());
    EXPECT_EQ(schema["type_id"].asString(), "test-described-component");
    EXPECT_TRUE(schema["debug_name"].isString());
    EXPECT_TRUE(schema["description"].isString());
    EXPECT_TRUE(schema["fields"].isArray());
    EXPECT_TRUE(schema["default_values"].isObject());
}

TEST(DiaEntityComponentSchemaJson, EveryFieldKeepsNameAndKindStrings) {
    Json::Value schema = BuildComponentSchemaJson(TestDescribedComponent::GetDesc());
    ASSERT_EQ(schema["fields"].size(), 6u);
    for (Json::ArrayIndex i = 0; i < schema["fields"].size(); ++i) {
        EXPECT_TRUE(schema["fields"][i]["name"].isString());
        EXPECT_TRUE(schema["fields"][i]["kind"].isString());
    }
}

TEST(DiaEntityComponentSchemaJson, DescribedRangedFieldCarriesAllKeys) {
    Json::Value schema = BuildComponentSchemaJson(TestDescribedComponent::GetDesc());
    const Json::Value* f = FindField(schema, "speed");
    ASSERT_NE(f, nullptr);
    EXPECT_EQ((*f)["kind"].asString(), "primitive");
    EXPECT_EQ((*f)["label"].asString(), "Speed");
    EXPECT_EQ((*f)["unit"].asString(), "m/s");
    EXPECT_EQ((*f)["description"].asString(), "Movement speed");
    EXPECT_DOUBLE_EQ((*f)["min"].asDouble(), 0.0);
    EXPECT_DOUBLE_EQ((*f)["max"].asDouble(), 20.0);
    EXPECT_FLOAT_EQ((*f)["default"].asFloat(), 3.5f);
}

TEST(DiaEntityComponentSchemaJson, UndescribedUnrangedFieldHasOnlyNameKindDefault) {
    Json::Value schema = BuildComponentSchemaJson(TestDescribedComponent::GetDesc());
    const Json::Value* f = FindField(schema, "active");
    ASSERT_NE(f, nullptr);
    EXPECT_TRUE(f->isMember("name"));
    EXPECT_TRUE(f->isMember("kind"));
    EXPECT_TRUE(f->isMember("default"));
    EXPECT_TRUE((*f)["default"].asBool());
    EXPECT_FALSE(f->isMember("label"));
    EXPECT_FALSE(f->isMember("description"));
    EXPECT_FALSE(f->isMember("unit"));
    EXPECT_FALSE(f->isMember("hint"));
    EXPECT_FALSE(f->isMember("min"));
    EXPECT_FALSE(f->isMember("max"));
}

TEST(DiaEntityComponentSchemaJson, RangedUndescribedFieldHasRangeButNoLabel) {
    Json::Value schema = BuildComponentSchemaJson(TestDescribedComponent::GetDesc());
    const Json::Value* f = FindField(schema, "armor");
    ASSERT_NE(f, nullptr);
    EXPECT_DOUBLE_EQ((*f)["min"].asDouble(), -1.0);
    EXPECT_DOUBLE_EQ((*f)["max"].asDouble(), 5.0);
    EXPECT_FALSE(f->isMember("label"));
    EXPECT_FALSE(f->isMember("description"));
}

TEST(DiaEntityComponentSchemaJson, HintFieldCarriesHint) {
    Json::Value schema = BuildComponentSchemaJson(TestDescribedComponent::GetDesc());
    const Json::Value* f = FindField(schema, "hitPoints");
    ASSERT_NE(f, nullptr);
    EXPECT_EQ((*f)["hint"].asString(), "Whole numbers only");
    EXPECT_EQ((*f)["label"].asString(), "Hit Points");
}

TEST(DiaEntityComponentSchemaJson, NestedFieldHasDescriptionAndDefaultObject) {
    Json::Value schema = BuildComponentSchemaJson(TestDescribedComponent::GetDesc());
    const Json::Value* f = FindField(schema, "nested");
    ASSERT_NE(f, nullptr);
    EXPECT_EQ((*f)["kind"].asString(), "nested");
    EXPECT_EQ((*f)["description"].asString(), "Nested value struct");
    ASSERT_TRUE((*f)["default"].isObject());
    EXPECT_FLOAT_EQ((*f)["default"]["gain"].asFloat(), 2.0f);
    EXPECT_EQ((*f)["default"]["count"].asInt(), 4);
    EXPECT_FALSE(f->isMember("min"));
}

TEST(DiaEntityComponentSchemaJson, DefaultValuesComeFromConstructedDefault) {
    Json::Value schema = BuildComponentSchemaJson(TestDescribedComponent::GetDesc());
    const Json::Value& dv = schema["default_values"];
    EXPECT_FLOAT_EQ(dv["speed"].asFloat(), 3.5f);
    EXPECT_EQ(dv["hitPoints"].asInt(), 7);
    EXPECT_TRUE(dv["active"].asBool());
    EXPECT_FLOAT_EQ(dv["armor"].asFloat(), 1.5f);
    EXPECT_EQ(dv["nested"]["count"].asInt(), 4);
}

TEST(DiaEntityComponentSchemaJson, WithoutConstructDefaultUsesZeroFilledSave) {
    ComponentTypeDesc desc = MakeHandDesc(&SaveMarker);
    Json::Value schema = BuildComponentSchemaJson(desc);
    EXPECT_EQ(schema["type_id"].asString(), "hand-schema");
    EXPECT_EQ(schema["debug_name"].asString(), "HandSchema");
    ASSERT_TRUE(schema["default_values"].isMember("marker"));
    EXPECT_EQ(schema["default_values"]["marker"].asInt(), 0);
    EXPECT_FALSE(schema["fields"][0u].isMember("default"));
    EXPECT_FALSE(schema["fields"][0u].isMember("label"));
}

TEST(DiaEntityComponentSchemaJson, WithNeitherFallsBackToPerKindZeros) {
    ComponentTypeDesc desc = MakeHandDesc(nullptr);
    Json::Value schema = BuildComponentSchemaJson(desc);
    const Json::Value& dv = schema["default_values"];
    EXPECT_EQ(dv["a"].asInt(), 0);
    EXPECT_TRUE(dv["b"].isString());
    EXPECT_TRUE(dv["c"].isArray());
    EXPECT_STREQ(FieldKindToString(FieldKind::Container), "container");
    EXPECT_EQ(schema["fields"][1u]["kind"].asString(), "string_id");
}
