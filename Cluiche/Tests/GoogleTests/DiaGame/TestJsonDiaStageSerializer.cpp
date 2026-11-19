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

// ---- presets ----

TEST(JsonDiaStageSerializer, Presets_AbsentIsEmptyAndResetsStale)
{
    JsonDiaStageSerializer s;
    DiaStageManifest m;
    m.presets.Add(DiaStagePreset());
    ASSERT_TRUE(s.Load(MakeStageJson("").c_str(), m).ok);
    EXPECT_EQ(m.presets.Size(), 0u);
}

TEST(JsonDiaStageSerializer, Presets_RoundTripNestedOverrides)
{
    JsonDiaStageSerializer s;
    DiaStageManifest m;
    ASSERT_TRUE(s.Load(MakeStageJson(
        R"("presets":[{"name":"alpine42","description":"Alpine, seed 42","modules":{"WorldGen":{"preset":"alpine","seed":42,"nested":{"a":[1,2,{"b":true}],"f":1.5}}}},{"name":"bare","modules":{}}])").c_str(), m).ok);
    ASSERT_EQ(m.presets.Size(), 2u);
    EXPECT_STREQ(m.presets[0].name.AsCStr(), "alpine42");
    EXPECT_STREQ(m.presets[0].description.AsCStr(), "Alpine, seed 42");
    ASSERT_EQ(m.presets[0].modules.Size(), 1u);
    EXPECT_STREQ(m.presets[0].modules[0].instanceId.AsCStr(), "WorldGen");
    EXPECT_STREQ(m.presets[1].name.AsCStr(), "bare");
    EXPECT_EQ(m.presets[1].modules.Size(), 0u);

    std::string buf(16384, '\0');
    ASSERT_TRUE(s.Save(m, &buf[0], static_cast<unsigned int>(buf.size())).ok);
    EXPECT_NE(std::strstr(buf.c_str(), "presets"), nullptr);

    DiaStageManifest out;
    ASSERT_TRUE(s.Load(buf.c_str(), out).ok);
    ASSERT_EQ(out.presets.Size(), 2u);
    ASSERT_EQ(out.presets[0].modules.Size(), 1u);
    EXPECT_STREQ(out.presets[0].description.AsCStr(), "Alpine, seed 42");
    EXPECT_STREQ(out.presets[0].modules[0].overridesJson, m.presets[0].modules[0].overridesJson);
    EXPECT_NE(std::strstr(out.presets[0].modules[0].overridesJson, "\"seed\":42"), nullptr);
    EXPECT_NE(std::strstr(out.presets[0].modules[0].overridesJson, "\"b\":true"), nullptr);
    EXPECT_NE(std::strstr(out.presets[0].modules[0].overridesJson, "1.5"), nullptr);
}

TEST(JsonDiaStageSerializer, Presets_EmptyOmittedOnSave)
{
    JsonDiaStageSerializer s;
    DiaStageManifest m;
    m.name = Dia::Core::Containers::String256("%s", "S");
    m.manifestPath = Dia::Core::Containers::String256("%s", "s.diastage.json");
    char buf[1024];
    ASSERT_TRUE(s.Save(m, buf, sizeof(buf)).ok);
    EXPECT_EQ(std::strstr(buf, "presets"), nullptr);
}

TEST(JsonDiaStageSerializer, Presets_NotArrayFails)
{
    ExpectFailNamed(R"("presets":{"name":"a"})", "presets");
}

TEST(JsonDiaStageSerializer, Presets_EntryNotObjectFails)
{
    ExpectFailNamed(R"("presets":["a"])", "presets");
}

TEST(JsonDiaStageSerializer, Presets_MissingNameFails)
{
    ExpectFailNamed(R"("presets":[{"modules":{}}])", "presets");
}

TEST(JsonDiaStageSerializer, Presets_SeventeenFails)
{
    std::string p = "\"presets\":[";
    for (int i = 0; i < 17; ++i)
        p += std::string(i ? "," : "") + "{\"name\":\"p" + std::to_string(i) + "\",\"modules\":{}}";
    p += "]";
    ExpectFailNamed(p, "presets");
}

TEST(JsonDiaStageSerializer, Presets_SixteenAccepted)
{
    JsonDiaStageSerializer s;
    DiaStageManifest m;
    std::string p = "\"presets\":[";
    for (int i = 0; i < 16; ++i)
        p += std::string(i ? "," : "") + "{\"name\":\"p" + std::to_string(i) + "\",\"modules\":{}}";
    p += "]";
    ASSERT_TRUE(s.Load(MakeStageJson(p).c_str(), m).ok);
    EXPECT_EQ(m.presets.Size(), 16u);
}

TEST(JsonDiaStageSerializer, Presets_NameTooLongFails)
{
    ExpectFailNamed("\"presets\":[{\"name\":\"" + std::string(32, 'a') + "\",\"modules\":{}}]", "name");
}

TEST(JsonDiaStageSerializer, Presets_NameAtLimitAccepted)
{
    JsonDiaStageSerializer s;
    DiaStageManifest m;
    ASSERT_TRUE(s.Load(MakeStageJson("\"presets\":[{\"name\":\"" + std::string(31, 'a') + "\",\"modules\":{}}]").c_str(), m).ok);
    EXPECT_EQ(m.presets.Size(), 1u);
}

TEST(JsonDiaStageSerializer, Presets_DescriptionTooLongFails)
{
    ExpectFailNamed("\"presets\":[{\"name\":\"a\",\"description\":\"" + std::string(121, 'd') + "\",\"modules\":{}}]", "description");
}

TEST(JsonDiaStageSerializer, Presets_OverrideOver2KbFails)
{
    ExpectFailNamed("\"presets\":[{\"name\":\"a\",\"modules\":{\"M\":{\"k\":\"" + std::string(2100, 'x') + "\"}}}]", "presets");
}

TEST(JsonDiaStageSerializer, Presets_ModuleValueNotObjectFails)
{
    ExpectFailNamed(R"("presets":[{"name":"a","modules":{"M":5}}])", "presets");
}

TEST(JsonDiaStageSerializer, Presets_ModulesNotObjectFails)
{
    ExpectFailNamed(R"("presets":[{"name":"a","modules":[1]}])", "presets");
}

TEST(JsonDiaStageSerializer, Presets_TooManyModulesFails)
{
    std::string mods;
    for (int i = 0; i < 9; ++i)
        mods += std::string(i ? "," : "") + "\"M" + std::to_string(i) + "\":{}";
    ExpectFailNamed("\"presets\":[{\"name\":\"a\",\"modules\":{" + mods + "}}]", "presets");
}
