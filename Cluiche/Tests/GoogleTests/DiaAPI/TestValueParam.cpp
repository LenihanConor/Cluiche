#include <gtest/gtest.h>
#include <DiaAPI/CommandRegistry/ValueParam.h>
#include <DiaEditor/EditorAPI/EditorActionManifest.h>
#include <DiaCore/Json/external/json/json.h>
#include <string>
#include <type_traits>

using namespace Dia::Editor;

namespace
{
    std::string Compact(const Json::Value& v)
    {
        Json::StreamWriterBuilder b;
        b["indentation"] = "";
        return Json::writeString(b, v);
    }
}

TEST(ValueParam, ManifestJson_OldFourFields_Unchanged)
{
    EditorActionParam p;
    p.name = "kind"; p.type = "enum"; p.required = true; p.description = "desc";
    EXPECT_EQ(Compact(EditorActionParamToJson(p)),
        "{\"description\":\"desc\",\"name\":\"kind\",\"required\":true,\"type\":\"enum\"}");
}

static_assert(std::is_same_v<EditorActionParam, Dia::API::ValueParam>, "EditorActionParam must alias ValueParam");

TEST(ValueParam, OldAggregateForms_StillCompile)
{
    Dia::API::ValueParam a = {"kind", "enum", true, "desc"};
    EXPECT_STREQ(a.name, "kind");
    EXPECT_STREQ(a.type, "enum");
    EXPECT_TRUE(a.required);
    EXPECT_STREQ(a.description, "desc");
    EXPECT_EQ(a.unit, nullptr);
    EXPECT_FALSE(a.hasRange);
    EXPECT_EQ(a.min, 0.0f);
    EXPECT_EQ(a.max, 0.0f);
    EXPECT_EQ(a.enumValues, nullptr);

    EditorActionParam b = {"n", "int"};
    EXPECT_FALSE(b.required);
    EXPECT_EQ(b.description, nullptr);

    EditorActionParam c;
    c.name = "x"; c.type = "float"; c.required = false; c.description = "d";
    EXPECT_STREQ(c.name, "x");

    EditorActionParamSchema schema;
    schema.params.Add(a);
    EXPECT_EQ(schema.params.Size(), 1u);
}

TEST(ValueParam, ManifestJson_NewFields_Emitted)
{
    EditorActionParam p = {"layer", "enum", true, "desc"};
    p.unit = "m"; p.hasRange = true; p.min = 1.0f; p.max = 5.0f; p.enumValues = "weather|ecology";
    const Json::Value j = EditorActionParamToJson(p);
    EXPECT_STREQ(j["unit"].asCString(), "m");
    EXPECT_FLOAT_EQ(j["min"].asFloat(), 1.0f);
    EXPECT_FLOAT_EQ(j["max"].asFloat(), 5.0f);
    EXPECT_STREQ(j["values"].asCString(), "weather|ecology");
}

TEST(ValueParam, ManifestJson_RangeOnlyWhenHasRange)
{
    EditorActionParam p = {"n", "int", false, "d"};
    p.min = 3.0f; p.max = 9.0f;
    const Json::Value j = EditorActionParamToJson(p);
    EXPECT_FALSE(j.isMember("min"));
    EXPECT_FALSE(j.isMember("max"));
    EXPECT_FALSE(j.isMember("unit"));
    EXPECT_FALSE(j.isMember("values"));
}
