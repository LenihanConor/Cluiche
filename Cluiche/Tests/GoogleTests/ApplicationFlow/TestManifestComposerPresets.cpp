////////////////////////////////////////////////////////////////////////////////
// Filename: TestManifestComposerPresets.cpp
// GoogleTest suite - ManifestComposerV2 reads launch presets from .diastage
// (launch-presets spec AC 4).
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaApplicationFlow/Manifest/ManifestComposerV2.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;

namespace {

const char* kBaseApp = R"({
  "version": 3, "initial_stage": "Boot",
  "stages": [ { "name": "Boot" }, { "name": "S1" }, { "name": "S2" } ],
  "processing_units": [ { "instance_id": "MainPU", "frequency_hz": 30.0, "modules": [] } ]
})";

const char* kStageApp = R"({
  "version": 3, "initial_stage": "Boot", "stages": [],
  "processing_units": [ { "instance_id": "MainPU", "frequency_hz": 30.0, "modules": [] } ]
})";

class ComposerPresets : public ::testing::Test
{
protected:
    std::filesystem::path mDir;

    void SetUp() override
    {
        mDir = std::filesystem::temp_directory_path() /
               (std::string("ComposerPresets_") + ::testing::UnitTest::GetInstance()->current_test_info()->name());
        std::filesystem::remove_all(mDir);
        std::filesystem::create_directories(mDir);
        Write("base.diaapp", kBaseApp);
        Write("stage.diaapp", kStageApp);
    }
    void TearDown() override { std::filesystem::remove_all(mDir); }

    void Write(const char* name, const std::string& text)
    {
        std::ofstream f((mDir / name).string(), std::ios::binary);
        f << text;
    }

    // Compose stages S1 (presetsField) and S2 (s2Presets). Empty string = no "presets" field.
    ComposeResult Compose(const std::string& presetsField, ApplicationManifestV3& out, const std::string& s2Presets = "")
    {
        Write("s1.diastage", "{ \"name\": \"S1\", \"manifest\": \"stage.diaapp\"" +
                             (presetsField.empty() ? std::string() : ", \"presets\": " + presetsField) + " }");
        Write("s2.diastage", "{ \"name\": \"S2\", \"manifest\": \"stage.diaapp\"" +
                             (s2Presets.empty() ? std::string() : ", \"presets\": " + s2Presets) + " }");
        Write("t.diagame",
              "{ \"imports\": [ {\"type\":\"manifest\",\"path\":\"base.diaapp\"},"
              "{\"type\":\"stage\",\"path\":\"s1.diastage\"},{\"type\":\"stage\",\"path\":\"s2.diastage\"} ] }");
        return ManifestComposerV2::Compose((mDir / "t.diagame").string().c_str(), out);
    }
};

const StageDeclaration* StageByName(const ApplicationManifestV3& m, const char* n)
{
    for (unsigned int i = 0; i < m.stages.Size(); ++i)
        if (m.stages[i].name == StringCRC(n)) return &m.stages[i];
    return nullptr;
}

} // namespace

TEST(ComposerPresetsSize, ManifestSizeReport)
{
    const size_t presetStorage = sizeof(Dia::Core::Containers::DynamicArrayC<PresetDeclaration, kMaxManifestPresets>)
                               + kPresetOverridePoolBytes + 32 * sizeof(unsigned int) * 2;
    std::printf("sizeof(ApplicationManifestV3) = %zu, sizeof(PresetDeclaration) = %zu, preset storage growth = %zu\n",
                sizeof(ApplicationManifestV3), sizeof(PresetDeclaration), presetStorage);
    // Growth budget for launch presets across the whole manifest: well under 100 KB.
    EXPECT_LT(presetStorage, 100u * 1024u);
}

TEST_F(ComposerPresets, AbsentPresetsIsEmpty)
{
    auto m = std::make_unique<ApplicationManifestV3>();
    ASSERT_EQ(Compose("", *m), ComposeResult::kSuccess);
    const StageDeclaration* s = StageByName(*m, "S1");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->presetCount, 0u);
    EXPECT_FALSE(m->HasPreset(StringCRC("S1"), StringCRC("alpine42")));
}

TEST_F(ComposerPresets, ValidPresetsParsedAndOverrideRoundTripsVerbatim)
{
    auto m = std::make_unique<ApplicationManifestV3>();
    ASSERT_EQ(Compose(R"([
      {"name":"alpine42","description":"Alpine, seed 42","modules":{"WorldGen":{"preset":"alpine","seed":42,"size":"Small"},"Other":{"x":1}}},
      {"name":"empty","modules":{}}
    ])", *m), ComposeResult::kSuccess);

    const StageDeclaration* s = StageByName(*m, "S1");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->presetCount, 2u);

    const PresetDeclaration* p = m->FindPreset(*s, StringCRC("alpine42"));
    ASSERT_NE(p, nullptr);
    EXPECT_STREQ(p->name.AsChar(), "alpine42");
    EXPECT_STREQ(p->description, "Alpine, seed 42");
    EXPECT_EQ(p->moduleCount, 2u);

    EXPECT_STREQ(m->GetPresetOverrideJson(*p, StringCRC("WorldGen")), "{\"preset\":\"alpine\",\"seed\":42,\"size\":\"Small\"}");
    EXPECT_STREQ(m->GetPresetOverrideJson(*p, StringCRC("Other")), "{\"x\":1}");
    EXPECT_STREQ(m->GetPresetOverrideJson(*p, StringCRC("NoSuchModule")), "");

    const PresetDeclaration* e = m->FindPreset(StringCRC("S1"), StringCRC("empty"));
    ASSERT_NE(e, nullptr);
    EXPECT_STREQ(e->description, "");
    EXPECT_EQ(e->moduleCount, 0u);
    EXPECT_STREQ(m->GetPresetOverrideJson(*e, StringCRC("WorldGen")), "");
    EXPECT_TRUE(m->HasPreset(StringCRC("S1"), StringCRC("empty")));
    EXPECT_EQ(m->FindPreset(*s, StringCRC("missing")), nullptr);
    EXPECT_FALSE(m->HasPreset(StringCRC("Nope"), StringCRC("empty")));
}

TEST_F(ComposerPresets, PresetsAreScopedPerStage)
{
    auto m = std::make_unique<ApplicationManifestV3>();
    ASSERT_EQ(Compose(R"([{"name":"a","modules":{"M":{"v":1}}}])", *m,
                      R"([{"name":"a","modules":{"M":{"v":2}}},{"name":"b","modules":{}}])"), ComposeResult::kSuccess);
    EXPECT_EQ(StageByName(*m, "S1")->presetCount, 1u);
    EXPECT_EQ(StageByName(*m, "S2")->presetCount, 2u);
    EXPECT_STREQ(m->GetPresetOverrideJson(*m->FindPreset(StringCRC("S1"), StringCRC("a")), StringCRC("M")), "{\"v\":1}");
    EXPECT_STREQ(m->GetPresetOverrideJson(*m->FindPreset(StringCRC("S2"), StringCRC("a")), StringCRC("M")), "{\"v\":2}");
    EXPECT_FALSE(m->HasPreset(StringCRC("S1"), StringCRC("b")));
}

TEST_F(ComposerPresets, MalformedPresetsFieldIgnoredStageStillLoads)
{
    const char* bad[] = { "\"notanarray\"", "42", "{\"a\":1}", "[1,2]", "[{\"description\":\"no name\"}]",
                          "[{\"name\":5,\"modules\":{}}]", "[{\"name\":\"x\",\"modules\":[]}]",
                          "[{\"name\":\"x\",\"modules\":{\"M\":3}}]",
                          "[{\"name\":\"x\",\"description\":7,\"modules\":{}}]" };
    for (const char* b : bad)
    {
        auto m = std::make_unique<ApplicationManifestV3>();
        EXPECT_EQ(Compose(b, *m), ComposeResult::kSuccess) << b;
        const StageDeclaration* s = StageByName(*m, "S1");
        ASSERT_NE(s, nullptr) << b;
        EXPECT_EQ(s->presetCount, 0u) << b;
    }
}

TEST_F(ComposerPresets, MalformedPresetDoesNotDropValidSiblings)
{
    auto m = std::make_unique<ApplicationManifestV3>();
    ASSERT_EQ(Compose(R"([{"name":"good","modules":{"M":{"a":1}}},{"name":"bad","modules":[]},{"name":"good2","modules":{}}])", *m),
              ComposeResult::kSuccess);
    EXPECT_TRUE(m->HasPreset(StringCRC("S1"), StringCRC("good")));
    EXPECT_TRUE(m->HasPreset(StringCRC("S1"), StringCRC("good2")));
    EXPECT_FALSE(m->HasPreset(StringCRC("S1"), StringCRC("bad")));
}

TEST_F(ComposerPresets, DuplicateNameKeepsFirstDropsSecond)
{
    auto m = std::make_unique<ApplicationManifestV3>();
    ASSERT_EQ(Compose(R"([{"name":"dup","modules":{"M":{"v":1}}},{"name":"dup","modules":{"M":{"v":2}}}])", *m), ComposeResult::kSuccess);
    EXPECT_EQ(StageByName(*m, "S1")->presetCount, 1u);
    EXPECT_STREQ(m->GetPresetOverrideJson(*m->FindPreset(StringCRC("S1"), StringCRC("dup")), StringCRC("M")), "{\"v\":1}");
}

TEST_F(ComposerPresets, NameAndDescriptionCaps)
{
    auto m = std::make_unique<ApplicationManifestV3>();
    const std::string n31(31, 'a'), n32(32, 'b'), d120(120, 'x'), d121(121, 'y');
    ASSERT_EQ(Compose("[{\"name\":\"" + n31 + "\",\"description\":\"" + d120 + "\",\"modules\":{}},"
                      "{\"name\":\"" + n32 + "\",\"modules\":{}},"
                      "{\"name\":\"okname\",\"description\":\"" + d121 + "\",\"modules\":{}}]", *m), ComposeResult::kSuccess);
    EXPECT_EQ(StageByName(*m, "S1")->presetCount, 1u);
    const PresetDeclaration* p = m->FindPreset(StringCRC("S1"), StringCRC(n31.c_str()));
    ASSERT_NE(p, nullptr);
    EXPECT_STREQ(p->description, d120.c_str());
}

TEST_F(ComposerPresets, SeventeenPresetsKeepsFirstSixteen)
{
    std::string arr = "[";
    for (int i = 0; i < 17; ++i)
        arr += std::string(i ? "," : "") + "{\"name\":\"p" + std::to_string(i) + "\",\"modules\":{}}";
    arr += "]";
    auto m = std::make_unique<ApplicationManifestV3>();
    ASSERT_EQ(Compose(arr, *m), ComposeResult::kSuccess);
    EXPECT_EQ(StageByName(*m, "S1")->presetCount, 16u);
    EXPECT_TRUE(m->HasPreset(StringCRC("S1"), StringCRC("p15")));
    EXPECT_FALSE(m->HasPreset(StringCRC("S1"), StringCRC("p16")));
}

TEST_F(ComposerPresets, OverTwoKbPresetIgnored)
{
    const std::string big(2100, 'z');
    auto m = std::make_unique<ApplicationManifestV3>();
    ASSERT_EQ(Compose("[{\"name\":\"big\",\"modules\":{\"M\":{\"k\":\"" + big + "\"}}},{\"name\":\"small\",\"modules\":{\"M\":{\"k\":1}}}]", *m),
              ComposeResult::kSuccess);
    EXPECT_FALSE(m->HasPreset(StringCRC("S1"), StringCRC("big")));
    EXPECT_TRUE(m->HasPreset(StringCRC("S1"), StringCRC("small")));
}

TEST_F(ComposerPresets, MoreThanFourModulesPresetIgnored)
{
    auto m = std::make_unique<ApplicationManifestV3>();
    ASSERT_EQ(Compose(R"([{"name":"five","modules":{"A":{},"B":{},"C":{},"D":{},"E":{}}},{"name":"four","modules":{"A":{},"B":{},"C":{},"D":{}}}])", *m),
              ComposeResult::kSuccess);
    EXPECT_FALSE(m->HasPreset(StringCRC("S1"), StringCRC("five")));
    const PresetDeclaration* p = m->FindPreset(StringCRC("S1"), StringCRC("four"));
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->moduleCount, 4u);
}

TEST_F(ComposerPresets, SharedPoolExhaustionIgnoresPresetsThatDoNotFit)
{
    // ~1.5 KB each; 16 per stage x 2 stages = 32 presets ~ 48 KB > 32 KB pool.
    const std::string chunk(1500, 'q');
    std::string arr = "[";
    for (int i = 0; i < 16; ++i)
        arr += std::string(i ? "," : "") + "{\"name\":\"p" + std::to_string(i) + "\",\"modules\":{\"M\":{\"k\":\"" + chunk + "\"}}}";
    arr += "]";
    auto m = std::make_unique<ApplicationManifestV3>();
    ASSERT_EQ(Compose(arr, *m, arr), ComposeResult::kSuccess);
    EXPECT_LE(m->presetOverridePoolUsed, kPresetOverridePoolBytes);
    EXPECT_GT(StageByName(*m, "S1")->presetCount, 0u);
    EXPECT_LT(StageByName(*m, "S1")->presetCount + StageByName(*m, "S2")->presetCount, 32u);
    const PresetDeclaration* p = m->FindPreset(StringCRC("S1"), StringCRC("p0"));
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(std::string(m->GetPresetOverrideJson(*p, StringCRC("M"))), "{\"k\":\"" + chunk + "\"}");
}

TEST_F(ComposerPresets, MovedManifestKeepsPresets)
{
    auto m = std::make_unique<ApplicationManifestV3>();
    ASSERT_EQ(Compose(R"([{"name":"a","modules":{"M":{"v":1}}}])", *m), ComposeResult::kSuccess);
    auto m2 = std::make_unique<ApplicationManifestV3>(std::move(*m));
    const PresetDeclaration* p = m2->FindPreset(StringCRC("S1"), StringCRC("a"));
    ASSERT_NE(p, nullptr);
    EXPECT_STREQ(m2->GetPresetOverrideJson(*p, StringCRC("M")), "{\"v\":1}");
}
