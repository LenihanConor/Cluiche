#include <gtest/gtest.h>
#include <DiaGame/JsonDiaStageSerializer.h>
#include <DiaGame/DiaGameManifest.h>
#include <cstring>
#include <string>

using namespace Dia::Game;

namespace
{
    const char* kBase = R"("name":"S","manifest":"s.diastage.json")";

    std::string MakeStageJson(const std::string& extra)
    {
        return std::string("{") + kBase + (extra.empty() ? "" : "," + extra) + "}";
    }

    void ExpectFailNamed(const std::string& extra, const char* field)
    {
        JsonDiaStageSerializer s;
        DiaStageManifest m;
        auto r = s.Load(MakeStageJson(extra).c_str(), m);
        EXPECT_FALSE(r.ok) << extra;
        ASSERT_NE(r.error, nullptr);
        EXPECT_NE(std::strstr(r.error, field), nullptr) << r.error;
    }
}

TEST(JsonDiaStageSerializer, Load_AbsentFieldsAreEmpty)
{
    JsonDiaStageSerializer s;
    DiaStageManifest m;
    EXPECT_TRUE(s.Load(MakeStageJson("").c_str(), m).ok);
    EXPECT_EQ(m.tags.Size(), 0u);
    EXPECT_EQ(m.description.Length(), 0u);
}

TEST(JsonDiaStageSerializer, Load_TagsAndDescription)
{
    JsonDiaStageSerializer s;
    DiaStageManifest m;
    EXPECT_TRUE(s.Load(MakeStageJson(R"("tags":["tool","a%s"],"description":"hello 100%d")").c_str(), m).ok);
    ASSERT_EQ(m.tags.Size(), 2u);
    EXPECT_STREQ(m.tags[0].AsCStr(), "tool");
    EXPECT_STREQ(m.tags[1].AsCStr(), "a%s");
    EXPECT_STREQ(m.description.AsCStr(), "hello 100%d");
}

TEST(JsonDiaStageSerializer, Load_LimitsAreAccepted)
{
    JsonDiaStageSerializer s;
    DiaStageManifest m;
    std::string tags = "\"tags\":[";
    for (int i = 0; i < 16; ++i) tags += std::string(i ? "," : "") + "\"" + std::string(63, 'a') + "\"";
    tags += "],\"description\":\"" + std::string(120, 'd') + "\"";
    EXPECT_TRUE(s.Load(MakeStageJson(tags).c_str(), m).ok);
    EXPECT_EQ(m.tags.Size(), 16u);
    EXPECT_EQ(m.description.Length(), 120u);
}

TEST(JsonDiaStageSerializer, Load_TagsNotArrayFails)
{
    ExpectFailNamed(R"("tags":"tool")", "tags");
}

TEST(JsonDiaStageSerializer, Load_TagElementNotStringFails)
{
    ExpectFailNamed(R"("tags":["tool",3])", "tags");
}

TEST(JsonDiaStageSerializer, Load_DescriptionNotStringFails)
{
    ExpectFailNamed(R"("description":5)", "description");
}

TEST(JsonDiaStageSerializer, Load_TagTooLongFails)
{
    ExpectFailNamed("\"tags\":[\"" + std::string(64, 'a') + "\"]", "tags");
}

TEST(JsonDiaStageSerializer, Load_TooManyTagsFails)
{
    std::string tags = "\"tags\":[";
    for (int i = 0; i < 17; ++i) tags += std::string(i ? "," : "") + "\"t\"";
    tags += "]";
    ExpectFailNamed(tags, "tags");
}

TEST(JsonDiaStageSerializer, Load_DescriptionTooLongFails)
{
    ExpectFailNamed("\"description\":\"" + std::string(121, 'd') + "\"", "description");
}

TEST(JsonDiaStageSerializer, Save_OldShapeHasNoNewKeys)
{
    JsonDiaStageSerializer s;
    DiaStageManifest m;
    m.name = Dia::Core::Containers::String256("%s", "S");
    m.manifestPath = Dia::Core::Containers::String256("%s", "s.diastage.json");
    char buf[1024];
    ASSERT_TRUE(s.Save(m, buf, sizeof(buf)).ok);
    EXPECT_EQ(std::strstr(buf, "tags"), nullptr);
    EXPECT_EQ(std::strstr(buf, "description"), nullptr);
}

TEST(JsonDiaStageSerializer, RoundTrip_WithNewFields)
{
    JsonDiaStageSerializer s;
    DiaStageManifest m;
    m.name = Dia::Core::Containers::String256("%s", "S");
    m.manifestPath = Dia::Core::Containers::String256("%s", "s.diastage.json");
    m.scenePath = Dia::Core::Containers::String256("%s", "x.diascene");
    m.tags.Add(Dia::Core::Containers::String64("tool"));
    m.tags.Add(Dia::Core::Containers::String64("physics"));
    m.description = Dia::Core::Containers::String128("%s", "A description");
    char buf[1024];
    ASSERT_TRUE(s.Save(m, buf, sizeof(buf)).ok);

    DiaStageManifest out;
    ASSERT_TRUE(s.Load(buf, out).ok);
    EXPECT_STREQ(out.name.AsCStr(), "S");
    EXPECT_STREQ(out.scenePath.AsCStr(), "x.diascene");
    ASSERT_EQ(out.tags.Size(), 2u);
    EXPECT_STREQ(out.tags[0].AsCStr(), "tool");
    EXPECT_STREQ(out.tags[1].AsCStr(), "physics");
    EXPECT_STREQ(out.description.AsCStr(), "A description");
}

TEST(JsonDiaStageSerializer, Proves_AbsentIsEmpty)
{
    JsonDiaStageSerializer s;
    DiaStageManifest m;
    m.proves.Add(Dia::Core::Containers::String256("stale"));
    ASSERT_TRUE(s.Load(MakeStageJson("").c_str(), m).ok);
    EXPECT_EQ(m.proves.Size(), 0u);
}

TEST(JsonDiaStageSerializer, Proves_RoundTrip)
{
    JsonDiaStageSerializer s;
    DiaStageManifest m;
    m.name = Dia::Core::Containers::String256("%s", "S");
    m.manifestPath = Dia::Core::Containers::String256("%s", "s.diastage.json");
    m.proves.Add(Dia::Core::Containers::String256("%s", "docs/specs/a/b%s.md"));
    m.proves.Add(Dia::Core::Containers::String256("%s", "docs/specs/c.md"));
    char buf[1024];
    ASSERT_TRUE(s.Save(m, buf, sizeof(buf)).ok);
    EXPECT_NE(std::strstr(buf, "proves"), nullptr);

    DiaStageManifest out;
    ASSERT_TRUE(s.Load(buf, out).ok);
    ASSERT_EQ(out.proves.Size(), 2u);
    EXPECT_STREQ(out.proves[0].AsCStr(), "docs/specs/a/b%s.md");
    EXPECT_STREQ(out.proves[1].AsCStr(), "docs/specs/c.md");
}

TEST(JsonDiaStageSerializer, Proves_EmptyOmittedOnSave)
{
    JsonDiaStageSerializer s;
    DiaStageManifest m;
    m.name = Dia::Core::Containers::String256("%s", "S");
    m.manifestPath = Dia::Core::Containers::String256("%s", "s.diastage.json");
    char buf[1024];
    ASSERT_TRUE(s.Save(m, buf, sizeof(buf)).ok);
    EXPECT_EQ(std::strstr(buf, "proves"), nullptr);
}

TEST(JsonDiaStageSerializer, Proves_NotArrayFails)
{
    ExpectFailNamed(R"("proves":"docs/specs/a.md")", "proves");
}

TEST(JsonDiaStageSerializer, Proves_ElementNotStringFails)
{
    ExpectFailNamed(R"("proves":["docs/specs/a.md",3])", "proves");
}

TEST(JsonDiaStageSerializer, Proves_TooManyFails)
{
    std::string p = "\"proves\":[";
    for (int i = 0; i < 17; ++i) p += std::string(i ? "," : "") + "\"docs/specs/a.md\"";
    p += "]";
    ExpectFailNamed(p, "proves");
}

TEST(JsonDiaStageSerializer, Proves_SixteenAccepted)
{
    JsonDiaStageSerializer s;
    DiaStageManifest m;
    std::string p = "\"proves\":[";
    for (int i = 0; i < 16; ++i) p += std::string(i ? "," : "") + "\"docs/specs/a.md\"";
    p += "]";
    ASSERT_TRUE(s.Load(MakeStageJson(p).c_str(), m).ok);
    EXPECT_EQ(m.proves.Size(), 16u);
}

TEST(JsonDiaStageSerializer, Proves_TooLongFails)
{
    ExpectFailNamed("\"proves\":[\"" + std::string(256, 'a') + "\"]", "proves");
}

TEST(JsonDiaStageSerializer, Proves_MaxLengthAccepted)
{
    JsonDiaStageSerializer s;
    DiaStageManifest m;
    ASSERT_TRUE(s.Load(MakeStageJson("\"proves\":[\"" + std::string(255, 'a') + "\"]").c_str(), m).ok);
    ASSERT_EQ(m.proves.Size(), 1u);
    EXPECT_EQ(m.proves[0].Length(), 255u);
}

TEST(JsonDiaStageSerializer, RoundTrip_WithoutNewFields)
{
    JsonDiaStageSerializer s;
    DiaStageManifest out;
    ASSERT_TRUE(s.Load(MakeStageJson(R"("scene":"x.diascene")").c_str(), out).ok);
    char buf[1024];
    ASSERT_TRUE(s.Save(out, buf, sizeof(buf)).ok);
    DiaStageManifest again;
    ASSERT_TRUE(s.Load(buf, again).ok);
    EXPECT_STREQ(again.scenePath.AsCStr(), "x.diascene");
    EXPECT_EQ(again.tags.Size(), 0u);
    EXPECT_EQ(again.description.Length(), 0u);
}
