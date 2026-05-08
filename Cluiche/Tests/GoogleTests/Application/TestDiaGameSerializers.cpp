#define _CRT_SECURE_NO_WARNINGS
#include <gtest/gtest.h>
#include <DiaGame/DiaGameManifest.h>
#include <DiaGame/JsonDiaGameSerializer.h>
#include <DiaGame/JsonDiaStageSerializer.h>
#include <DiaGame/GameLoader.h>
#include <DiaApplication/Manifest/ManifestValidator.h>
#include <DiaApplication/TypeRegistry/ApplicationTypeRegistry.h>
#include <DiaCore/CRC/StringCRC.h>
#include <cstdio>
#include <cstring>

using namespace Dia::Game;
using namespace Dia::Application;
using namespace Dia::Core;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void WriteFile(const char* path, const char* content)
{
	FILE* f = fopen(path, "w");
	if (f) { fputs(content, f); fclose(f); }
}

static void DeleteTestFile(const char* path) { remove(path); }

// ---------------------------------------------------------------------------
// JsonDiaGameSerializer — Load
// ---------------------------------------------------------------------------

TEST(DiaGameSerializer, Load_ValidJson_ParsesAllFields)
{
	const char* json =
		"{ \"name\": \"TestGame\", \"version\": \"2.0\","
		"  \"imports\": ["
		"    { \"path\": \"main.diaapp\", \"type\": \"manifest\" },"
		"    { \"path\": \"level1.diastage\", \"type\": \"stage\" }"
		"  ],"
		"  \"config\": { \"asset_root\": \"Data/Built\" } }";

	JsonDiaGameSerializer serializer;
	DiaGameManifest manifest;
	auto result = serializer.Load(json, manifest);

	EXPECT_TRUE(result.ok);
	EXPECT_STREQ(manifest.name.AsCStr(), "TestGame");
	EXPECT_STREQ(manifest.version.AsCStr(), "2.0");
	ASSERT_EQ(manifest.imports.Size(), 2u);
	EXPECT_EQ(manifest.imports[0].type, TypedImport::ImportType::kManifest);
	EXPECT_STREQ(manifest.imports[0].path.AsCStr(), "main.diaapp");
	EXPECT_EQ(manifest.imports[1].type, TypedImport::ImportType::kStage);
	EXPECT_STREQ(manifest.imports[1].path.AsCStr(), "level1.diastage");
	EXPECT_STREQ(manifest.config.assetRoot.AsCStr(), "Data/Built");
	EXPECT_NE(manifest.rawConfig, nullptr);
}

TEST(DiaGameSerializer, Load_InvalidJson_ReturnsFailure)
{
	JsonDiaGameSerializer serializer;
	DiaGameManifest manifest;
	auto result = serializer.Load("{ not valid json", manifest);

	EXPECT_FALSE(result.ok);
	EXPECT_NE(result.error, nullptr);
}

TEST(DiaGameSerializer, Load_EmptyObject_SucceedsWithDefaults)
{
	JsonDiaGameSerializer serializer;
	DiaGameManifest manifest;
	auto result = serializer.Load("{}", manifest);

	EXPECT_TRUE(result.ok);
	EXPECT_EQ(manifest.imports.Size(), 0u);
	EXPECT_EQ(manifest.rawConfig, nullptr);
}

TEST(DiaGameSerializer, Load_DefaultImportType_IsManifest)
{
	const char* json =
		"{ \"imports\": [{ \"path\": \"no_type.diaapp\" }] }";

	JsonDiaGameSerializer serializer;
	DiaGameManifest manifest;
	auto result = serializer.Load(json, manifest);

	EXPECT_TRUE(result.ok);
	ASSERT_EQ(manifest.imports.Size(), 1u);
	EXPECT_EQ(manifest.imports[0].type, TypedImport::ImportType::kManifest);
}

// ---------------------------------------------------------------------------
// JsonDiaGameSerializer — Save
// ---------------------------------------------------------------------------

TEST(DiaGameSerializer, Save_BasicManifest_ProducesValidJson)
{
	DiaGameManifest manifest;
	manifest.name = "MyGame";
	manifest.version = "1.5";

	TypedImport import("core.diaapp", TypedImport::ImportType::kManifest);
	manifest.imports.Add(import);

	JsonDiaGameSerializer serializer;
	char buffer[4096];
	auto result = serializer.Save(manifest, buffer, sizeof(buffer));

	EXPECT_TRUE(result.ok);
	EXPECT_NE(strstr(buffer, "\"MyGame\""), nullptr);
	EXPECT_NE(strstr(buffer, "\"1.5\""), nullptr);
	EXPECT_NE(strstr(buffer, "\"core.diaapp\""), nullptr);
	EXPECT_NE(strstr(buffer, "\"manifest\""), nullptr);
}

TEST(DiaGameSerializer, Save_BufferTooSmall_ReturnsFailure)
{
	DiaGameManifest manifest;
	manifest.name = "TestGame";
	manifest.version = "1.0";

	JsonDiaGameSerializer serializer;
	char tinyBuffer[10];
	auto result = serializer.Save(manifest, tinyBuffer, sizeof(tinyBuffer));

	EXPECT_FALSE(result.ok);
}

// ---------------------------------------------------------------------------
// JsonDiaGameSerializer — Round-trip
// ---------------------------------------------------------------------------

TEST(DiaGameSerializer, RoundTrip_PreservesAllFields)
{
	const char* original =
		"{ \"name\": \"RoundTrip\", \"version\": \"3.0\","
		"  \"imports\": ["
		"    { \"path\": \"a.diaapp\", \"type\": \"manifest\" },"
		"    { \"path\": \"b.diastage\", \"type\": \"stage\" }"
		"  ],"
		"  \"config\": { \"asset_root\": \"Out/Assets\" } }";

	JsonDiaGameSerializer serializer;

	DiaGameManifest manifest;
	auto loadResult = serializer.Load(original, manifest);
	ASSERT_TRUE(loadResult.ok);

	char saveBuffer[4096];
	auto saveResult = serializer.Save(manifest, saveBuffer, sizeof(saveBuffer));
	ASSERT_TRUE(saveResult.ok);

	DiaGameManifest reloaded;
	auto reloadResult = serializer.Load(saveBuffer, reloaded);
	ASSERT_TRUE(reloadResult.ok);

	EXPECT_STREQ(reloaded.name.AsCStr(), "RoundTrip");
	EXPECT_STREQ(reloaded.version.AsCStr(), "3.0");
	ASSERT_EQ(reloaded.imports.Size(), 2u);
	EXPECT_STREQ(reloaded.imports[0].path.AsCStr(), "a.diaapp");
	EXPECT_EQ(reloaded.imports[0].type, TypedImport::ImportType::kManifest);
	EXPECT_STREQ(reloaded.imports[1].path.AsCStr(), "b.diastage");
	EXPECT_EQ(reloaded.imports[1].type, TypedImport::ImportType::kStage);
	EXPECT_STREQ(reloaded.config.assetRoot.AsCStr(), "Out/Assets");
}

TEST(DiaGameSerializer, RoundTrip_PreservesRawConfigUnknownFields)
{
	const char* original =
		"{ \"name\": \"Test\", \"version\": \"1.0\","
		"  \"config\": { \"asset_root\": \"X\", \"custom_field\": 42, \"nested\": { \"key\": \"val\" } } }";

	JsonDiaGameSerializer serializer;

	DiaGameManifest manifest;
	serializer.Load(original, manifest);

	char saveBuffer[4096];
	serializer.Save(manifest, saveBuffer, sizeof(saveBuffer));

	EXPECT_NE(strstr(saveBuffer, "\"custom_field\""), nullptr);
	EXPECT_NE(strstr(saveBuffer, "42"), nullptr);
	EXPECT_NE(strstr(saveBuffer, "\"nested\""), nullptr);
	EXPECT_NE(strstr(saveBuffer, "\"key\""), nullptr);
}

TEST(DiaGameSerializer, LoadFromFile_ValidFile_Succeeds)
{
	WriteFile("test_serializer_game.diagame",
		"{ \"name\": \"FileTest\", \"version\": \"1.0\","
		"  \"imports\": [{ \"path\": \"x.diaapp\" }] }");

	JsonDiaGameSerializer serializer;
	DiaGameManifest manifest;
	auto result = serializer.LoadFromFile("test_serializer_game.diagame", manifest);

	EXPECT_TRUE(result.ok);
	EXPECT_STREQ(manifest.name.AsCStr(), "FileTest");

	DeleteTestFile("test_serializer_game.diagame");
}

TEST(DiaGameSerializer, LoadFromFile_MissingFile_ReturnsFailure)
{
	JsonDiaGameSerializer serializer;
	DiaGameManifest manifest;
	auto result = serializer.LoadFromFile("nonexistent_serializer_test.diagame", manifest);

	EXPECT_FALSE(result.ok);
}

TEST(DiaGameSerializer, SaveToFile_WritesAndReloads)
{
	DiaGameManifest manifest;
	manifest.name = "SaveTest";
	manifest.version = "1.0";

	JsonDiaGameSerializer serializer;
	auto writeResult = serializer.SaveToFile("test_serializer_save.diagame", manifest);
	ASSERT_TRUE(writeResult.ok);

	DiaGameManifest reloaded;
	auto readResult = serializer.LoadFromFile("test_serializer_save.diagame", reloaded);
	EXPECT_TRUE(readResult.ok);
	EXPECT_STREQ(reloaded.name.AsCStr(), "SaveTest");

	DeleteTestFile("test_serializer_save.diagame");
}

// ---------------------------------------------------------------------------
// JsonDiaStageSerializer — Load
// ---------------------------------------------------------------------------

TEST(DiaStageSerializer, Load_ValidJson_ParsesFields)
{
	const char* json = "{ \"name\": \"Level1\", \"manifest\": \"levels/level1.diaapp\" }";

	JsonDiaStageSerializer serializer;
	DiaStageManifest manifest;
	auto result = serializer.Load(json, manifest);

	EXPECT_TRUE(result.ok);
	EXPECT_STREQ(manifest.name.AsCStr(), "Level1");
	EXPECT_STREQ(manifest.manifestPath.AsCStr(), "levels/level1.diaapp");
}

TEST(DiaStageSerializer, Load_MissingName_ReturnsFailure)
{
	JsonDiaStageSerializer serializer;
	DiaStageManifest manifest;
	auto result = serializer.Load("{ \"manifest\": \"x.diaapp\" }", manifest);

	EXPECT_FALSE(result.ok);
	EXPECT_NE(strstr(result.error, "name"), nullptr);
}

TEST(DiaStageSerializer, Load_MissingManifest_ReturnsFailure)
{
	JsonDiaStageSerializer serializer;
	DiaStageManifest manifest;
	auto result = serializer.Load("{ \"name\": \"Test\" }", manifest);

	EXPECT_FALSE(result.ok);
	EXPECT_NE(strstr(result.error, "manifest"), nullptr);
}

TEST(DiaStageSerializer, Load_InvalidJson_ReturnsFailure)
{
	JsonDiaStageSerializer serializer;
	DiaStageManifest manifest;
	auto result = serializer.Load("not json at all", manifest);

	EXPECT_FALSE(result.ok);
}

// ---------------------------------------------------------------------------
// JsonDiaStageSerializer — Save
// ---------------------------------------------------------------------------

TEST(DiaStageSerializer, Save_ProducesValidJson)
{
	DiaStageManifest manifest;
	manifest.name = "BossArena";
	manifest.manifestPath = "stages/boss.diaapp";

	JsonDiaStageSerializer serializer;
	char buffer[2048];
	auto result = serializer.Save(manifest, buffer, sizeof(buffer));

	EXPECT_TRUE(result.ok);
	EXPECT_NE(strstr(buffer, "\"BossArena\""), nullptr);
	EXPECT_NE(strstr(buffer, "\"stages/boss.diaapp\""), nullptr);
}

TEST(DiaStageSerializer, Save_BufferTooSmall_ReturnsFailure)
{
	DiaStageManifest manifest;
	manifest.name = "TestStage";
	manifest.manifestPath = "stages/test.diaapp";

	JsonDiaStageSerializer serializer;
	char tinyBuffer[5];
	auto result = serializer.Save(manifest, tinyBuffer, sizeof(tinyBuffer));

	EXPECT_FALSE(result.ok);
}

// ---------------------------------------------------------------------------
// JsonDiaStageSerializer — Round-trip
// ---------------------------------------------------------------------------

TEST(DiaStageSerializer, RoundTrip_PreservesAllFields)
{
	const char* original = "{ \"name\": \"Arena\", \"manifest\": \"arena/arena.diaapp\" }";

	JsonDiaStageSerializer serializer;

	DiaStageManifest manifest;
	auto loadResult = serializer.Load(original, manifest);
	ASSERT_TRUE(loadResult.ok);

	char saveBuffer[2048];
	auto saveResult = serializer.Save(manifest, saveBuffer, sizeof(saveBuffer));
	ASSERT_TRUE(saveResult.ok);

	DiaStageManifest reloaded;
	auto reloadResult = serializer.Load(saveBuffer, reloaded);
	ASSERT_TRUE(reloadResult.ok);

	EXPECT_STREQ(reloaded.name.AsCStr(), "Arena");
	EXPECT_STREQ(reloaded.manifestPath.AsCStr(), "arena/arena.diaapp");
}

TEST(DiaStageSerializer, LoadFromFile_ValidFile_Succeeds)
{
	WriteFile("test_serializer_stage.diastage",
		"{ \"name\": \"TestStage\", \"manifest\": \"test.diaapp\" }");

	JsonDiaStageSerializer serializer;
	DiaStageManifest manifest;
	auto result = serializer.LoadFromFile("test_serializer_stage.diastage", manifest);

	EXPECT_TRUE(result.ok);
	EXPECT_STREQ(manifest.name.AsCStr(), "TestStage");

	DeleteTestFile("test_serializer_stage.diastage");
}

TEST(DiaStageSerializer, LoadFromFile_MissingFile_ReturnsFailure)
{
	JsonDiaStageSerializer serializer;
	DiaStageManifest manifest;
	auto result = serializer.LoadFromFile("nonexistent_stage.diastage", manifest);

	EXPECT_FALSE(result.ok);
}

TEST(DiaStageSerializer, SaveToFile_WritesAndReloads)
{
	DiaStageManifest manifest;
	manifest.name = "SaveStage";
	manifest.manifestPath = "save/stage.diaapp";

	JsonDiaStageSerializer serializer;
	auto writeResult = serializer.SaveToFile("test_stage_save.diastage", manifest);
	ASSERT_TRUE(writeResult.ok);

	DiaStageManifest reloaded;
	auto readResult = serializer.LoadFromFile("test_stage_save.diastage", reloaded);
	EXPECT_TRUE(readResult.ok);
	EXPECT_STREQ(reloaded.name.AsCStr(), "SaveStage");

	DeleteTestFile("test_stage_save.diastage");
}

// ---------------------------------------------------------------------------
// JsonDiaGameSerializer — GetVersion
// ---------------------------------------------------------------------------

TEST(DiaGameSerializer, GetVersion_Returns1_0)
{
	JsonDiaGameSerializer serializer;
	EXPECT_STREQ(serializer.GetVersion(), "1.0");
}

TEST(DiaStageSerializer, GetVersion_Returns1_0)
{
	JsonDiaStageSerializer serializer;
	EXPECT_STREQ(serializer.GetVersion(), "1.0");
}

// ---------------------------------------------------------------------------
// GameLoader — unit tests with mock data files
// ---------------------------------------------------------------------------

TEST(GameLoaderTest, LoadGameManifest_ValidFile_Succeeds)
{
	WriteFile("test_gameloader.diagame",
		"{ \"name\": \"LoaderTest\", \"version\": \"1.0\","
		"  \"imports\": [{ \"path\": \"x.diaapp\", \"type\": \"manifest\" }] }");

	DiaGameManifest manifest;
	auto result = GameLoader::LoadGameManifest("test_gameloader.diagame", manifest);

	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
	EXPECT_STREQ(manifest.name.AsCStr(), "LoaderTest");

	DeleteTestFile("test_gameloader.diagame");
}

TEST(GameLoaderTest, LoadGameManifest_MissingFile_ReturnsError)
{
	DiaGameManifest manifest;
	auto result = GameLoader::LoadGameManifest("nonexistent_loader.diagame", manifest);

	EXPECT_EQ(result, ManifestValidationResult::kImportNotFound);
}

TEST(GameLoaderTest, LoadStageManifest_ValidFile_Succeeds)
{
	WriteFile("test_gameloader_stage.diastage",
		"{ \"name\": \"LoaderStage\", \"manifest\": \"stage.diaapp\" }");

	DiaStageManifest manifest;
	auto result = GameLoader::LoadStageManifest("test_gameloader_stage.diastage", manifest);

	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
	EXPECT_STREQ(manifest.name.AsCStr(), "LoaderStage");

	DeleteTestFile("test_gameloader_stage.diastage");
}

TEST(GameLoaderTest, LoadStageManifest_MissingFile_ReturnsError)
{
	DiaStageManifest manifest;
	auto result = GameLoader::LoadStageManifest("nonexistent_loader.diastage", manifest);

	EXPECT_EQ(result, ManifestValidationResult::kImportNotFound);
}

TEST(GameLoaderTest, LoadFromGameFile_NoRegisteredTypes_ReturnsError)
{
	WriteFile("test_loader_main.diaapp",
		"{ \"version\": 1, \"processing_units\": [{"
		"  \"type_id\": \"UnknownPU\", \"instance_id\": \"MainPU\","
		"  \"root\": true, \"frequency_hz\": 30.0, \"dedicated_thread\": false,"
		"  \"initial_phase\": \"Boot\","
		"  \"phases\": [{ \"type_id\": \"UnknownPhase\", \"instance_id\": \"Boot\", \"config\": {} }],"
		"  \"modules\": [], \"transitions\": [], \"config\": {}"
		"}] }");

	WriteFile("test_loader_full.diagame",
		"{ \"name\": \"LoaderFull\", \"version\": \"1.0\","
		"  \"imports\": [{ \"path\": \"test_loader_main.diaapp\", \"type\": \"manifest\" }] }");

	ApplicationTypeRegistry registry;
	ManifestValidationResult result;
	auto* pu = GameLoader::LoadFromGameFile(registry, "test_loader_full.diagame", result);

	EXPECT_EQ(pu, nullptr);
	EXPECT_NE(result, ManifestValidationResult::kSuccess);

	DeleteTestFile("test_loader_full.diagame");
	DeleteTestFile("test_loader_main.diaapp");
}
