#define _CRT_SECURE_NO_WARNINGS
#include <gtest/gtest.h>
#include <DiaGame/DiaGameManifest.h>
#include <DiaGame/DiaGameManifestLoader.h>
#include <DiaGame/GameFileComposer.h>
#include <DiaGame/GameLoader.h>
#include <DiaApplication/Manifest/ManifestComposer.h>
#include <DiaApplication/Manifest/ApplicationManifest.h>
#include <DiaApplication/Manifest/ManifestValidator.h>
#include <DiaApplication/TypeRegistry/ApplicationTypeRegistry.h>
#include <DiaApplication/ApplicationProcessingUnit.h>
#include <DiaApplication/ApplicationPhase.h>
#include <DiaApplication/ApplicationModule.h>
#include <DiaCore/CRC/StringCRC.h>
#include <cstdio>
#include <cstring>
#include <direct.h>

using namespace Dia::Application;
using namespace Dia::Game;
using namespace Dia::Core;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void WriteFile(const char* path, const char* content)
{
	FILE* f = fopen(path, "w");
	if (f) { fputs(content, f); fclose(f); }
}

static void DeleteFile(const char* path) { remove(path); }

// ---------------------------------------------------------------------------
// DiaGameManifestLoader — .diagame parsing
// ---------------------------------------------------------------------------

TEST(DiaGameFormat, LoadGameFile_ValidFile_ReturnsSuccess)
{
	WriteFile("test_game.diagame",
		"{ \"name\": \"TestGame\", \"version\": \"1.0\","
		"  \"imports\": [{ \"path\": \"main.diaapp\", \"type\": \"manifest\" }],"
		"  \"config\": { \"asset_root\": \"Data/Assets\" } }");

	DiaGameManifest manifest;
	ManifestValidationResult result = DiaGameManifestLoader::LoadGameFile("test_game.diagame", manifest);

	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
	EXPECT_STREQ(manifest.name.AsCStr(), "TestGame");
	EXPECT_STREQ(manifest.version.AsCStr(), "1.0");
	EXPECT_EQ(manifest.imports.Size(), 1u);
	EXPECT_STREQ(manifest.imports[0].path.AsCStr(), "main.diaapp");
	EXPECT_EQ(manifest.imports[0].type, TypedImport::ImportType::kManifest);
	EXPECT_STREQ(manifest.config.assetRoot.AsCStr(), "Data/Assets");

	DeleteFile("test_game.diagame");
}

TEST(DiaGameFormat, LoadGameFile_MultipleTypedImports_ParsedCorrectly)
{
	WriteFile("test_game_multi.diagame",
		"{ \"name\": \"TestGame\", \"version\": \"1.0\","
		"  \"imports\": ["
		"    { \"path\": \"main.diaapp\", \"type\": \"manifest\" },"
		"    { \"path\": \"stages/level1.diastage\", \"type\": \"stage\" }"
		"  ] }");

	DiaGameManifest manifest;
	ManifestValidationResult result = DiaGameManifestLoader::LoadGameFile("test_game_multi.diagame", manifest);

	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
	ASSERT_EQ(manifest.imports.Size(), 2u);
	EXPECT_EQ(manifest.imports[0].type, TypedImport::ImportType::kManifest);
	EXPECT_EQ(manifest.imports[1].type, TypedImport::ImportType::kStage);
	EXPECT_STREQ(manifest.imports[1].path.AsCStr(), "stages/level1.diastage");

	DeleteFile("test_game_multi.diagame");
}

TEST(DiaGameFormat, LoadGameFile_MissingFile_ReturnsImportNotFound)
{
	DiaGameManifest manifest;
	ManifestValidationResult result = DiaGameManifestLoader::LoadGameFile("nonexistent.diagame", manifest);
	EXPECT_EQ(result, ManifestValidationResult::kImportNotFound);
}

TEST(DiaGameFormat, LoadGameFile_InvalidJSON_ReturnsInvalidJSON)
{
	WriteFile("test_bad_game.diagame", "{ not valid json");

	DiaGameManifest manifest;
	ManifestValidationResult result = DiaGameManifestLoader::LoadGameFile("test_bad_game.diagame", manifest);
	EXPECT_EQ(result, ManifestValidationResult::kInvalidJSON);

	DeleteFile("test_bad_game.diagame");
}

// ---------------------------------------------------------------------------
// DiaGameManifestLoader — .diastage parsing
// ---------------------------------------------------------------------------

TEST(DiaGameFormat, LoadStageFile_ValidFile_ReturnsSuccess)
{
	WriteFile("test_stage.diastage",
		"{ \"name\": \"Level1\", \"manifest\": \"level1.diaapp\" }");

	DiaStageManifest manifest;
	ManifestValidationResult result = DiaGameManifestLoader::LoadStageFile("test_stage.diastage", manifest);

	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
	EXPECT_STREQ(manifest.name.AsCStr(), "Level1");
	EXPECT_STREQ(manifest.manifestPath.AsCStr(), "level1.diaapp");

	DeleteFile("test_stage.diastage");
}

TEST(DiaGameFormat, LoadStageFile_MissingName_ReturnsMissingField)
{
	WriteFile("test_bad_stage.diastage", "{ \"manifest\": \"level1.diaapp\" }");

	DiaStageManifest manifest;
	ManifestValidationResult result = DiaGameManifestLoader::LoadStageFile("test_bad_stage.diastage", manifest);
	EXPECT_EQ(result, ManifestValidationResult::kMissingRequiredField);

	DeleteFile("test_bad_stage.diastage");
}

TEST(DiaGameFormat, LoadStageFile_MissingManifest_ReturnsMissingField)
{
	WriteFile("test_bad_stage2.diastage", "{ \"name\": \"Level1\" }");

	DiaStageManifest manifest;
	ManifestValidationResult result = DiaGameManifestLoader::LoadStageFile("test_bad_stage2.diastage", manifest);
	EXPECT_EQ(result, ManifestValidationResult::kMissingRequiredField);

	DeleteFile("test_bad_stage2.diastage");
}

// ---------------------------------------------------------------------------
// ComposeFromGameFile — manifest import resolution
// ---------------------------------------------------------------------------

static const char* kSimplePU =
	"{ \"version\": 1, \"processing_units\": [{"
	"  \"type_id\": \"TestPU\", \"instance_id\": \"MainPU\","
	"  \"root\": true, \"frequency_hz\": 30.0, \"dedicated_thread\": false,"
	"  \"initial_phase\": \"BootPhase\","
	"  \"phases\": [{ \"type_id\": \"TestPhase\", \"instance_id\": \"BootPhase\", \"config\": {} }],"
	"  \"modules\": [], \"transitions\": [], \"config\": {}"
	"}] }";

TEST(DiaGameFormat, ComposeFromGameFile_ManifestImport_ResolvesPUs)
{
	WriteFile("test_compose_main.diaapp", kSimplePU);
	WriteFile("test_compose.diagame",
		"{ \"name\": \"TestGame\", \"version\": \"1.0\","
		"  \"imports\": [{ \"path\": \"test_compose_main.diaapp\", \"type\": \"manifest\" }] }");

	GameFileComposer composer;
	ApplicationManifest manifest;
	DiaGameManifest gameManifest;
	ManifestValidationResult result = composer.ComposeFromGameFile("test_compose.diagame", manifest, gameManifest);

	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
	EXPECT_EQ(manifest.processingUnits.Size(), 1u);
	EXPECT_EQ(manifest.processingUnits[0].instanceId, StringCRC("MainPU"));

	DeleteFile("test_compose.diagame");
	DeleteFile("test_compose_main.diaapp");
}

TEST(DiaGameFormat, ComposeFromGameFile_MissingImport_ReturnsError)
{
	WriteFile("test_compose_missing.diagame",
		"{ \"name\": \"TestGame\", \"version\": \"1.0\","
		"  \"imports\": [{ \"path\": \"nonexistent.diaapp\", \"type\": \"manifest\" }] }");

	GameFileComposer composer;
	ApplicationManifest manifest;
	DiaGameManifest gameManifest;
	ManifestValidationResult result = composer.ComposeFromGameFile("test_compose_missing.diagame", manifest, gameManifest);

	EXPECT_NE(result, ManifestValidationResult::kSuccess);

	DeleteFile("test_compose_missing.diagame");
}

// ---------------------------------------------------------------------------
// ComposeFromGameFile — stage import resolution
// ---------------------------------------------------------------------------

TEST(DiaGameFormat, ComposeFromGameFile_StageImport_MergesPhasesIntoTargetPU)
{
	WriteFile("test_stage_main.diaapp",
		"{ \"version\": 1, \"processing_units\": [{"
		"  \"type_id\": \"TestPU\", \"instance_id\": \"MainPU\","
		"  \"root\": true, \"frequency_hz\": 30.0, \"dedicated_thread\": false,"
		"  \"initial_phase\": \"BootPhase\","
		"  \"phases\": [{ \"type_id\": \"TestPhase\", \"instance_id\": \"BootPhase\", \"config\": {} }],"
		"  \"modules\": [], \"transitions\": [], \"config\": {}"
		"}] }");

	WriteFile("test_stage_level.diaapp",
		"{ \"version\": 1,"
		"  \"metadata\": { \"type\": \"stage\", \"name\": \"TestStage\","
		"    \"requires_phases\": [\"BootPhase\"] },"
		"  \"stage_phases\": ["
		"    { \"type_id\": \"LevelPhase\", \"instance_id\": \"LevelLoad\", \"target_processing_unit\": \"MainPU\" }"
		"  ],"
		"  \"stage_transitions\": ["
		"    { \"from\": \"BootPhase\", \"to\": \"LevelLoad\", \"target_processing_unit\": \"MainPU\" }"
		"  ],"
		"  \"stage_modules\": [],"
		"  \"processing_units\": [] }");

	WriteFile("test_stage_level.diastage",
		"{ \"name\": \"TestStage\", \"manifest\": \"test_stage_level.diaapp\" }");

	WriteFile("test_stage_game.diagame",
		"{ \"name\": \"TestGame\", \"version\": \"1.0\","
		"  \"imports\": ["
		"    { \"path\": \"test_stage_main.diaapp\", \"type\": \"manifest\" },"
		"    { \"path\": \"test_stage_level.diastage\", \"type\": \"stage\" }"
		"  ] }");

	GameFileComposer composer;
	ApplicationManifest manifest;
	DiaGameManifest gameManifest;
	ManifestValidationResult result = composer.ComposeFromGameFile("test_stage_game.diagame", manifest, gameManifest);

	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
	ASSERT_EQ(manifest.processingUnits.Size(), 1u);
	// MainPU should have BootPhase + LevelLoad = 2 phases
	EXPECT_EQ(manifest.processingUnits[0].phases.Size(), 2u);
	// And 1 transition (BootPhase -> LevelLoad)
	EXPECT_EQ(manifest.processingUnits[0].transitions.Size(), 1u);

	DeleteFile("test_stage_game.diagame");
	DeleteFile("test_stage_main.diaapp");
	DeleteFile("test_stage_level.diaapp");
	DeleteFile("test_stage_level.diastage");
}

TEST(DiaGameFormat, ComposeFromGameFile_StageRequiresPhase_MissingPhase_ReturnsError)
{
	WriteFile("test_reqphase_main.diaapp",
		"{ \"version\": 1, \"processing_units\": [{"
		"  \"type_id\": \"TestPU\", \"instance_id\": \"MainPU\","
		"  \"root\": true, \"frequency_hz\": 30.0, \"dedicated_thread\": false,"
		"  \"initial_phase\": \"BootPhase\","
		"  \"phases\": [{ \"type_id\": \"TestPhase\", \"instance_id\": \"BootPhase\", \"config\": {} }],"
		"  \"modules\": [], \"transitions\": [], \"config\": {}"
		"}] }");

	WriteFile("test_reqphase_stage.diaapp",
		"{ \"version\": 1,"
		"  \"metadata\": { \"type\": \"stage\", \"name\": \"BadStage\","
		"    \"requires_phases\": [\"NonExistentPhase\"] },"
		"  \"stage_phases\": [],"
		"  \"stage_transitions\": [],"
		"  \"stage_modules\": [],"
		"  \"processing_units\": [] }");

	WriteFile("test_reqphase_stage.diastage",
		"{ \"name\": \"BadStage\", \"manifest\": \"test_reqphase_stage.diaapp\" }");

	WriteFile("test_reqphase_game.diagame",
		"{ \"name\": \"TestGame\", \"version\": \"1.0\","
		"  \"imports\": ["
		"    { \"path\": \"test_reqphase_main.diaapp\", \"type\": \"manifest\" },"
		"    { \"path\": \"test_reqphase_stage.diastage\", \"type\": \"stage\" }"
		"  ] }");

	GameFileComposer composer;
	ApplicationManifest manifest;
	DiaGameManifest gameManifest;
	ManifestValidationResult result = composer.ComposeFromGameFile("test_reqphase_game.diagame", manifest, gameManifest);

	EXPECT_NE(result, ManifestValidationResult::kSuccess);

	DeleteFile("test_reqphase_game.diagame");
	DeleteFile("test_reqphase_main.diaapp");
	DeleteFile("test_reqphase_stage.diaapp");
	DeleteFile("test_reqphase_stage.diastage");
}

TEST(DiaGameFormat, ComposeFromGameFile_StageTargetsMissingPU_ReturnsError)
{
	WriteFile("test_badpu_main.diaapp",
		"{ \"version\": 1, \"processing_units\": [{"
		"  \"type_id\": \"TestPU\", \"instance_id\": \"MainPU\","
		"  \"root\": true, \"frequency_hz\": 30.0, \"dedicated_thread\": false,"
		"  \"initial_phase\": \"BootPhase\","
		"  \"phases\": [{ \"type_id\": \"TestPhase\", \"instance_id\": \"BootPhase\", \"config\": {} }],"
		"  \"modules\": [], \"transitions\": [], \"config\": {}"
		"}] }");

	WriteFile("test_badpu_stage.diaapp",
		"{ \"version\": 1,"
		"  \"metadata\": { \"type\": \"stage\", \"name\": \"BadPUStage\" },"
		"  \"stage_phases\": ["
		"    { \"type_id\": \"LevelPhase\", \"instance_id\": \"LevelLoad\", \"target_processing_unit\": \"NonExistentPU\" }"
		"  ],"
		"  \"stage_transitions\": [],"
		"  \"stage_modules\": [],"
		"  \"processing_units\": [] }");

	WriteFile("test_badpu_stage.diastage",
		"{ \"name\": \"BadPUStage\", \"manifest\": \"test_badpu_stage.diaapp\" }");

	WriteFile("test_badpu_game.diagame",
		"{ \"name\": \"TestGame\", \"version\": \"1.0\","
		"  \"imports\": ["
		"    { \"path\": \"test_badpu_main.diaapp\", \"type\": \"manifest\" },"
		"    { \"path\": \"test_badpu_stage.diastage\", \"type\": \"stage\" }"
		"  ] }");

	GameFileComposer composer;
	ApplicationManifest manifest;
	DiaGameManifest gameManifest;
	ManifestValidationResult result = composer.ComposeFromGameFile("test_badpu_game.diagame", manifest, gameManifest);

	EXPECT_NE(result, ManifestValidationResult::kSuccess);

	DeleteFile("test_badpu_game.diagame");
	DeleteFile("test_badpu_main.diaapp");
	DeleteFile("test_badpu_stage.diaapp");
	DeleteFile("test_badpu_stage.diastage");
}

// ---------------------------------------------------------------------------
// Backward compatibility — flat string imports still work
// ---------------------------------------------------------------------------

TEST(DiaGameFormat, FlatStringImports_BackwardCompat)
{
	WriteFile("test_compat_imported.diaapp", kSimplePU);
	WriteFile("test_compat_root.diaapp",
		"{ \"version\": 1, \"imports\": [\"test_compat_imported.diaapp\"],"
		"  \"processing_units\": [] }");

	ManifestComposer composer;
	ApplicationManifest manifest;
	ManifestValidationResult result = composer.ComposeSingleManifest("test_compat_root.diaapp", manifest);

	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
	EXPECT_EQ(manifest.processingUnits.Size(), 1u);

	DeleteFile("test_compat_root.diaapp");
	DeleteFile("test_compat_imported.diaapp");
}

// ---------------------------------------------------------------------------
// CluicheTest data files — integration test
// ---------------------------------------------------------------------------

TEST(DiaGameFormat, CluicheTestDataFiles_DiagameLoads)
{
	DiaGameManifest manifest;
	ManifestValidationResult result = DiaGameManifestLoader::LoadGameFile(
		"Data/Manifests/cluichetest.diagame", manifest);

	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
	EXPECT_STREQ(manifest.name.AsCStr(), "CluicheTest");
	ASSERT_GE(manifest.imports.Size(), 2u);
}

TEST(DiaGameFormat, CluicheTestDataFiles_DiastageLoads)
{
	DiaStageManifest manifest;
	ManifestValidationResult result = DiaGameManifestLoader::LoadStageFile(
		"Data/stages/DummyStage/dummy_stage.diastage", manifest);

	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
	EXPECT_STREQ(manifest.name.AsCStr(), "DummyStage");
	EXPECT_STREQ(manifest.manifestPath.AsCStr(), "misc/ApplicationFlow/dummy_stage.diaapp");
}

TEST(DiaGameFormat, CluicheTestDataFiles_ComposeFromGameFile_Succeeds)
{
	GameFileComposer composer;
	ApplicationManifest manifest;
	DiaGameManifest gameManifest;
	ManifestValidationResult result = composer.ComposeFromGameFile(
		"Data/Manifests/cluichetest.diagame", manifest, gameManifest);

	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
	EXPECT_GE(manifest.processingUnits.Size(), 1u);

	// MainProcessingUnit should have the DummyStage phases merged in
	bool foundMainPU = false;
	for (unsigned int i = 0; i < manifest.processingUnits.Size(); ++i)
	{
		if (manifest.processingUnits[i].instanceId == StringCRC("MainProcessingUnit"))
		{
			foundMainPU = true;
			// Should have original phases + DummyStage phases
			EXPECT_GE(manifest.processingUnits[i].phases.Size(), 4u);
			break;
		}
	}
	EXPECT_TRUE(foundMainPU);
}

// ---------------------------------------------------------------------------
// AC6: Entry/exit transitions derived from stage .diaapp
// ---------------------------------------------------------------------------

TEST(DiaGameFormat, StageTransitions_MergedIntoTargetPU)
{
	WriteFile("test_ac6_main.diaapp",
		"{ \"version\": 1, \"processing_units\": [{"
		"  \"type_id\": \"TestPU\", \"instance_id\": \"MainPU\","
		"  \"root\": true, \"frequency_hz\": 30.0, \"dedicated_thread\": false,"
		"  \"initial_phase\": \"BootPhase\","
		"  \"phases\": [{ \"type_id\": \"TestPhase\", \"instance_id\": \"BootPhase\", \"config\": {} }],"
		"  \"modules\": [], \"transitions\": [], \"config\": {}"
		"}] }");

	WriteFile("test_ac6_stage.diaapp",
		"{ \"version\": 1,"
		"  \"metadata\": { \"type\": \"stage\", \"name\": \"AC6Stage\","
		"    \"requires_phases\": [\"BootPhase\"] },"
		"  \"stage_phases\": ["
		"    { \"type_id\": \"StagePhase\", \"instance_id\": \"StageLoad\", \"target_processing_unit\": \"MainPU\" },"
		"    { \"type_id\": \"StagePhase\", \"instance_id\": \"StageRun\", \"target_processing_unit\": \"MainPU\" }"
		"  ],"
		"  \"stage_transitions\": ["
		"    { \"from\": \"BootPhase\", \"to\": \"StageLoad\", \"target_processing_unit\": \"MainPU\" },"
		"    { \"from\": \"StageLoad\", \"to\": \"StageRun\", \"target_processing_unit\": \"MainPU\" },"
		"    { \"from\": \"StageRun\", \"to\": \"BootPhase\", \"target_processing_unit\": \"MainPU\" }"
		"  ],"
		"  \"stage_modules\": [],"
		"  \"processing_units\": [] }");

	WriteFile("test_ac6_stage.diastage",
		"{ \"name\": \"AC6Stage\", \"manifest\": \"test_ac6_stage.diaapp\" }");

	WriteFile("test_ac6.diagame",
		"{ \"name\": \"TestGame\", \"version\": \"1.0\","
		"  \"imports\": ["
		"    { \"path\": \"test_ac6_main.diaapp\", \"type\": \"manifest\" },"
		"    { \"path\": \"test_ac6_stage.diastage\", \"type\": \"stage\" }"
		"  ] }");

	GameFileComposer composer;
	ApplicationManifest manifest;
	DiaGameManifest gameManifest;
	ManifestValidationResult result = composer.ComposeFromGameFile("test_ac6.diagame", manifest, gameManifest);

	ASSERT_EQ(result, ManifestValidationResult::kSuccess);
	ASSERT_EQ(manifest.processingUnits.Size(), 1u);

	const auto& pu = manifest.processingUnits[0];
	// 3 transitions merged: BootPhase->StageLoad, StageLoad->StageRun, StageRun->BootPhase
	ASSERT_EQ(pu.transitions.Size(), 3u);

	// Verify entry transition: BootPhase -> StageLoad
	bool foundEntry = false;
	bool foundExit = false;
	for (unsigned int i = 0; i < pu.transitions.Size(); ++i)
	{
		if (pu.transitions[i].fromPhase == StringCRC("BootPhase") &&
			pu.transitions[i].toPhase == StringCRC("StageLoad"))
			foundEntry = true;
		if (pu.transitions[i].fromPhase == StringCRC("StageRun") &&
			pu.transitions[i].toPhase == StringCRC("BootPhase"))
			foundExit = true;
	}
	EXPECT_TRUE(foundEntry);
	EXPECT_TRUE(foundExit);

	DeleteFile("test_ac6.diagame");
	DeleteFile("test_ac6_main.diaapp");
	DeleteFile("test_ac6_stage.diaapp");
	DeleteFile("test_ac6_stage.diastage");
}

// ---------------------------------------------------------------------------
// AC11: Existing .diaapp-only workflow still works (no .diagame required)
// ---------------------------------------------------------------------------

TEST(DiaGameFormat, LoadApplication_DiaappOnly_StillWorks)
{
	WriteFile("test_ac11.diaapp",
		"{ \"version\": 1, \"processing_units\": [{"
		"  \"type_id\": \"TestPU\", \"instance_id\": \"AC11PU\","
		"  \"root\": true, \"frequency_hz\": 30.0, \"dedicated_thread\": false,"
		"  \"initial_phase\": \"Phase1\","
		"  \"phases\": [{ \"type_id\": \"TestPhase\", \"instance_id\": \"Phase1\", \"config\": {} }],"
		"  \"modules\": [], \"transitions\": [], \"config\": {}"
		"}] }");

	ManifestComposer composer;
	ApplicationManifest manifest;
	ManifestValidationResult result = composer.ComposeSingleManifest("test_ac11.diaapp", manifest);

	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
	EXPECT_EQ(manifest.processingUnits.Size(), 1u);
	EXPECT_EQ(manifest.processingUnits[0].instanceId, StringCRC("AC11PU"));

	DeleteFile("test_ac11.diaapp");
}

// ---------------------------------------------------------------------------
// AC14: LoadFromGameFile via ApplicationLoader (integration)
// ---------------------------------------------------------------------------

TEST(DiaGameFormat, GameLoader_LoadGameManifest_ParsesFile)
{
	DiaGameManifest manifest;
	ManifestValidationResult result = GameLoader::LoadGameManifest(
		"Data/Manifests/cluichetest.diagame", manifest);

	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
	EXPECT_STREQ(manifest.name.AsCStr(), "CluicheTest");
}

TEST(DiaGameFormat, GameLoader_LoadStageManifest_ParsesFile)
{
	DiaStageManifest manifest;
	ManifestValidationResult result = GameLoader::LoadStageManifest(
		"Data/stages/DummyStage/dummy_stage.diastage", manifest);

	EXPECT_EQ(result, ManifestValidationResult::kSuccess);
	EXPECT_STREQ(manifest.name.AsCStr(), "DummyStage");
}
