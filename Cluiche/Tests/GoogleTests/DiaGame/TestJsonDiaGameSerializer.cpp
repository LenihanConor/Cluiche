#include <gtest/gtest.h>
#include <DiaGame/JsonDiaGameSerializer.h>
#include <DiaGame/DiaGameManifest.h>
#include <DiaCore/Json/external/json/json.h>
#include <cstring>
#include <fstream>
#include <cstdio>

using namespace Dia::Game;

// ---------------------------------------------------------------------------
// Load — name / version fields
// ---------------------------------------------------------------------------

TEST(JsonDiaGameSerializer, Load_NameAndVersion)
{
    JsonDiaGameSerializer s;
    DiaGameManifest m;
    const char* json = R"({"name":"MyGame","version":"2.0","imports":[],"config":{}})";
    EXPECT_TRUE(s.Load(json, m).ok);
    EXPECT_STREQ(m.name.AsCStr(), "MyGame");
    EXPECT_STREQ(m.version.AsCStr(), "2.0");
}

TEST(JsonDiaGameSerializer, Load_MissingNameDefaultsToEmpty)
{
    JsonDiaGameSerializer s;
    DiaGameManifest m;
    EXPECT_TRUE(s.Load(R"({"version":"1.0","imports":[],"config":{}})", m).ok);
    EXPECT_EQ(m.name.Length(), 0u);
}

// ---------------------------------------------------------------------------
// Load — imports array
// ---------------------------------------------------------------------------

TEST(JsonDiaGameSerializer, Load_ImportsManifestType)
{
    JsonDiaGameSerializer s;
    DiaGameManifest m;
    const char* json = R"({
        "name":"G","version":"1","imports":[
            {"path":"app/main.diaapp","type":"manifest"}
        ],"config":{}})";
    ASSERT_TRUE(s.Load(json, m).ok);
    ASSERT_EQ(m.imports.Size(), 1u);
    EXPECT_STREQ(m.imports[0].path.AsCStr(), "app/main.diaapp");
    EXPECT_EQ(m.imports[0].type, Dia::Application::TypedImport::ImportType::kManifest);
}

TEST(JsonDiaGameSerializer, Load_ImportsStageType)
{
    JsonDiaGameSerializer s;
    DiaGameManifest m;
    const char* json = R"({
        "name":"G","version":"1","imports":[
            {"path":"stages/intro.diastage","type":"stage"}
        ],"config":{}})";
    ASSERT_TRUE(s.Load(json, m).ok);
    ASSERT_EQ(m.imports.Size(), 1u);
    EXPECT_EQ(m.imports[0].type, Dia::Application::TypedImport::ImportType::kStage);
}

TEST(JsonDiaGameSerializer, Load_EmptyImportsArray)
{
    JsonDiaGameSerializer s;
    DiaGameManifest m;
    ASSERT_TRUE(s.Load(R"({"name":"G","version":"1","imports":[],"config":{}})", m).ok);
    EXPECT_EQ(m.imports.Size(), 0u);
}

// ---------------------------------------------------------------------------
// Load — config block: asset_root
// ---------------------------------------------------------------------------

TEST(JsonDiaGameSerializer, Load_Config_AssetRoot)
{
    JsonDiaGameSerializer s;
    DiaGameManifest m;
    ASSERT_TRUE(s.Load(R"({"name":"G","version":"1","imports":[],"config":{"asset_root":"data/assets"}})", m).ok);
    EXPECT_STREQ(m.config.assetRoot.AsCStr(), "data/assets");
}

TEST(JsonDiaGameSerializer, Load_Config_MissingAssetRoot_DefaultsToEmpty)
{
    JsonDiaGameSerializer s;
    DiaGameManifest m;
    ASSERT_TRUE(s.Load(R"({"name":"G","version":"1","imports":[],"config":{}})", m).ok);
    EXPECT_EQ(m.config.assetRoot.Length(), 0u);
}

// ---------------------------------------------------------------------------
// Load — config block: asset_catalogue (new field)
// ---------------------------------------------------------------------------

TEST(JsonDiaGameSerializer, Load_Config_AssetCatalogue)
{
    JsonDiaGameSerializer s;
    DiaGameManifest m;
    ASSERT_TRUE(s.Load(R"({"name":"G","version":"1","imports":[],"config":{"asset_catalogue":"data/catalogue.json"}})", m).ok);
    EXPECT_STREQ(m.config.assetCatalogue.AsCStr(), "data/catalogue.json");
}

TEST(JsonDiaGameSerializer, Load_Config_MissingAssetCatalogue_DefaultsToEmpty)
{
    JsonDiaGameSerializer s;
    DiaGameManifest m;
    ASSERT_TRUE(s.Load(R"({"name":"G","version":"1","imports":[],"config":{}})", m).ok);
    EXPECT_EQ(m.config.assetCatalogue.Length(), 0u);
}

TEST(JsonDiaGameSerializer, Load_Config_BothAssetRootAndCatalogue)
{
    JsonDiaGameSerializer s;
    DiaGameManifest m;
    const char* json = R"({"name":"G","version":"1","imports":[],"config":{
        "asset_root":"data/assets",
        "asset_catalogue":"data/catalogue.json"
    }})";
    ASSERT_TRUE(s.Load(json, m).ok);
    EXPECT_STREQ(m.config.assetRoot.AsCStr(), "data/assets");
    EXPECT_STREQ(m.config.assetCatalogue.AsCStr(), "data/catalogue.json");
}

// ---------------------------------------------------------------------------
// Load — rawConfig passthrough
// ---------------------------------------------------------------------------

TEST(JsonDiaGameSerializer, Load_RawConfigStored)
{
    JsonDiaGameSerializer s;
    DiaGameManifest m;
    ASSERT_TRUE(s.Load(R"({"name":"G","version":"1","imports":[],"config":{"custom_key":"custom_val"}})", m).ok);
    ASSERT_TRUE(m.rawConfig != nullptr);
    EXPECT_TRUE(m.rawConfig->isMember("custom_key"));
}

TEST(JsonDiaGameSerializer, Load_MissingConfigBlock_RawConfigIsNull)
{
    JsonDiaGameSerializer s;
    DiaGameManifest m;
    ASSERT_TRUE(s.Load(R"({"name":"G","version":"1","imports":[]})", m).ok);
    EXPECT_TRUE(m.rawConfig == nullptr);
}

// ---------------------------------------------------------------------------
// Load — malformed JSON
// ---------------------------------------------------------------------------

TEST(JsonDiaGameSerializer, Load_InvalidJson_ReturnsFalure)
{
    JsonDiaGameSerializer s;
    DiaGameManifest m;
    EXPECT_FALSE(s.Load("not valid json {{{", m).ok);
}

TEST(JsonDiaGameSerializer, Load_EmptyString_ReturnsFalure)
{
    JsonDiaGameSerializer s;
    DiaGameManifest m;
    EXPECT_FALSE(s.Load("", m).ok);
}

// ---------------------------------------------------------------------------
// Save — round-trip: asset_catalogue survives Save→Load
// ---------------------------------------------------------------------------

TEST(JsonDiaGameSerializer, Save_AssetCatalogue_RoundTrip)
{
    JsonDiaGameSerializer s;
    DiaGameManifest m;
    m.name = "RoundTrip";
    m.version = "1.0";
    m.config.assetCatalogue = "path/to/catalogue.json";

    char buf[4096] = {};
    ASSERT_TRUE(s.Save(m, buf, sizeof(buf)).ok);

    DiaGameManifest m2;
    ASSERT_TRUE(s.Load(buf, m2).ok);
    EXPECT_STREQ(m2.config.assetCatalogue.AsCStr(), "path/to/catalogue.json");
}

TEST(JsonDiaGameSerializer, Save_EmptyCatalogue_NotWrittenToJson)
{
    JsonDiaGameSerializer s;
    DiaGameManifest m;
    m.name = "G";
    m.version = "1";

    char buf[4096] = {};
    ASSERT_TRUE(s.Save(m, buf, sizeof(buf)).ok);
    EXPECT_EQ(strstr(buf, "asset_catalogue"), nullptr);
}

TEST(JsonDiaGameSerializer, Save_AssetRoot_RoundTrip)
{
    JsonDiaGameSerializer s;
    DiaGameManifest m;
    m.name = "G";
    m.version = "1";
    m.config.assetRoot = "data/assets";

    char buf[4096] = {};
    ASSERT_TRUE(s.Save(m, buf, sizeof(buf)).ok);

    DiaGameManifest m2;
    ASSERT_TRUE(s.Load(buf, m2).ok);
    EXPECT_STREQ(m2.config.assetRoot.AsCStr(), "data/assets");
}

TEST(JsonDiaGameSerializer, Save_Imports_RoundTrip)
{
    JsonDiaGameSerializer s;
    DiaGameManifest m;
    m.name = "G";
    m.version = "1";

    Dia::Application::TypedImport imp;
    imp.path = "app/main.diaapp";
    imp.type = Dia::Application::TypedImport::ImportType::kManifest;
    m.imports.Add(imp);

    char buf[4096] = {};
    ASSERT_TRUE(s.Save(m, buf, sizeof(buf)).ok);

    DiaGameManifest m2;
    ASSERT_TRUE(s.Load(buf, m2).ok);
    ASSERT_EQ(m2.imports.Size(), 1u);
    EXPECT_STREQ(m2.imports[0].path.AsCStr(), "app/main.diaapp");
}

TEST(JsonDiaGameSerializer, Save_BufferTooSmall_ReturnsFalure)
{
    JsonDiaGameSerializer s;
    DiaGameManifest m;
    m.name = "G";
    m.version = "1";

    char tiny[4] = {};
    EXPECT_FALSE(s.Save(m, tiny, sizeof(tiny)).ok);
}

// ---------------------------------------------------------------------------
// GetVersion
// ---------------------------------------------------------------------------

TEST(JsonDiaGameSerializer, GetVersion_Returns1_0)
{
    JsonDiaGameSerializer s;
    EXPECT_STREQ(s.GetVersion(), "1.0");
}

// ---------------------------------------------------------------------------
// LoadFromFile — round-trip via disk
// ---------------------------------------------------------------------------

TEST(JsonDiaGameSerializer, LoadFromFile_ValidFile_Succeeds)
{
    const char* path = "test_serializer_valid.diagame";
    {
        std::ofstream f(path);
        f << R"({"name":"Disk","version":"1","imports":[],"config":{"asset_catalogue":"cat.json"}})";
    }

    JsonDiaGameSerializer s;
    DiaGameManifest m;
    EXPECT_TRUE(s.LoadFromFile(path, m).ok);
    EXPECT_STREQ(m.name.AsCStr(), "Disk");
    EXPECT_STREQ(m.config.assetCatalogue.AsCStr(), "cat.json");

    std::remove(path);
}

TEST(JsonDiaGameSerializer, LoadFromFile_MissingFile_ReturnsFalure)
{
    JsonDiaGameSerializer s;
    DiaGameManifest m;
    EXPECT_FALSE(s.LoadFromFile("nonexistent_xyz.diagame", m).ok);
}
