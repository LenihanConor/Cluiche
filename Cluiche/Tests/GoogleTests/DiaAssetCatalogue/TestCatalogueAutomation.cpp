#include <gtest/gtest.h>

#include <DiaAssetCatalogue/ContentHasher.h>
#include <DiaAssetCatalogue/ScopeComputer.h>
#include <DiaAssetCatalogue/RelationshipInferrer.h>
#include <DiaAssetCatalogue/CatalogueRulesEngine.h>
#include <DiaAssetCatalogue/AssetRecord.h>
#include <DiaAssetCatalogue/AssetRegistry.h>
#include <DiaAssetCatalogue/AssetTypeRegistry.h>
#include <DiaAssetCatalogue/BuiltInAssetTypes.h>
#include <DiaAssetCatalogue/RelationshipIndex.h>
#include <DiaAssetCatalogue/RelationshipTypes.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include <stdio.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace
{
	Dia::AssetCatalogue::AssetRecord MakeRecord(const char* id, const char* typeId, const char* sourcePath = "Raw/test.json")
	{
		Dia::AssetCatalogue::AssetRecord r;
		r.mId = Dia::Core::StringCRC(id);
		r.mAssetTypeId = Dia::Core::StringCRC(typeId);
		r.mSourcePath = Dia::Core::Containers::String256(sourcePath);
		r.mContentHash = 0;
		r.mStatus = Dia::AssetCatalogue::AssetStatus::Active;
		r.mScope = Dia::AssetCatalogue::AssetScope::kGlobal;
		return r;
	}

	bool EnsureTempDir()
	{
		CreateDirectoryW(L"C:\\Temp", NULL);
		// Verify it exists
		DWORD attr = GetFileAttributesW(L"C:\\Temp");
		return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
	}

	bool WriteTestFile(const char* path, const char* content, size_t len)
	{
		FILE* f = nullptr;
#if defined(_MSC_VER)
		fopen_s(&f, path, "wb");
#else
		f = fopen(path, "wb");
#endif
		if (!f) return false;
		fwrite(content, 1, len, f);
		fclose(f);
		return true;
	}

	bool WriteTestFile(const char* path, const char* content)
	{
		return WriteTestFile(path, content, strlen(content));
	}
}

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------
class CatalogueAutomation : public ::testing::Test
{
protected:
	void SetUp() override
	{
		EnsureTempDir();
	}
};

// ---------------------------------------------------------------------------
// Test 1: ContentHasher_ComputeHash_KnownFile_NonZero
// ---------------------------------------------------------------------------
TEST_F(CatalogueAutomation, ContentHasher_ComputeHash_KnownFile_NonZero)
{
	const char* path = "C:\\Temp\\test_hash_known.txt";
	const char* content = "Hello, DiaAssetCatalogue!";
	ASSERT_TRUE(WriteTestFile(path, content));

	Dia::AssetCatalogue::ContentHasher hasher;
	unsigned int hash = hasher.ComputeHash(path);

	EXPECT_NE(hash, 0u);

	remove(path);
}

// ---------------------------------------------------------------------------
// Test 2: ContentHasher_ComputeHash_SameContent_SameHash
// ---------------------------------------------------------------------------
TEST_F(CatalogueAutomation, ContentHasher_ComputeHash_SameContent_SameHash)
{
	const char* path1 = "C:\\Temp\\test_hash_same1.txt";
	const char* path2 = "C:\\Temp\\test_hash_same2.txt";
	const char* content = "identical content for hashing";
	ASSERT_TRUE(WriteTestFile(path1, content));
	ASSERT_TRUE(WriteTestFile(path2, content));

	Dia::AssetCatalogue::ContentHasher hasher;
	unsigned int hash1 = hasher.ComputeHash(path1);
	unsigned int hash2 = hasher.ComputeHash(path2);

	EXPECT_EQ(hash1, hash2);
	EXPECT_NE(hash1, 0u);

	remove(path1);
	remove(path2);
}

// ---------------------------------------------------------------------------
// Test 3: ContentHasher_ComputeHash_DifferentContent_DifferentHash
// ---------------------------------------------------------------------------
TEST_F(CatalogueAutomation, ContentHasher_ComputeHash_DifferentContent_DifferentHash)
{
	const char* path1 = "C:\\Temp\\test_hash_diff1.txt";
	const char* path2 = "C:\\Temp\\test_hash_diff2.txt";
	ASSERT_TRUE(WriteTestFile(path1, "content A"));
	ASSERT_TRUE(WriteTestFile(path2, "content B"));

	Dia::AssetCatalogue::ContentHasher hasher;
	unsigned int hash1 = hasher.ComputeHash(path1);
	unsigned int hash2 = hasher.ComputeHash(path2);

	EXPECT_NE(hash1, hash2);

	remove(path1);
	remove(path2);
}

// ---------------------------------------------------------------------------
// Test 4: ScopeComputer_SingleStageRef_ReturnsStageScope
// ---------------------------------------------------------------------------
TEST_F(CatalogueAutomation, ScopeComputer_SingleStageRef_ReturnsStageScope)
{
	Dia::AssetCatalogue::AssetRegistry registry;

	// Create a stage record that "contains" a texture
	Dia::AssetCatalogue::AssetRecord stage = MakeRecord("stage.gameplay", "stage");
	stage.mReferences.Add(Dia::AssetCatalogue::RelationshipEdge(
		Dia::AssetCatalogue::RelationshipTypes::kContains,
		Dia::Core::StringCRC("texture.player")));
	registry.Register(stage);

	// Create the texture record
	registry.Register(MakeRecord("texture.player", "texture"));

	Dia::AssetCatalogue::ScopeComputer computer;
	Dia::Core::StringCRC outStageName;
	Dia::AssetCatalogue::AssetScope scope = computer.ComputeScope(
		Dia::Core::StringCRC("texture.player"),
		registry,
		registry.GetRelationshipIndex(),
		outStageName);

	EXPECT_EQ(scope, Dia::AssetCatalogue::AssetScope::kStage);
	EXPECT_EQ(outStageName, Dia::Core::StringCRC("stage.gameplay"));
}

// ---------------------------------------------------------------------------
// Test 5: ScopeComputer_MultipleStageRefs_ReturnsGlobalScope
// ---------------------------------------------------------------------------
TEST_F(CatalogueAutomation, ScopeComputer_MultipleStageRefs_ReturnsGlobalScope)
{
	Dia::AssetCatalogue::AssetRegistry registry;

	// Two stages both contain the same texture
	Dia::AssetCatalogue::AssetRecord stage1 = MakeRecord("stage.gameplay", "stage");
	stage1.mReferences.Add(Dia::AssetCatalogue::RelationshipEdge(
		Dia::AssetCatalogue::RelationshipTypes::kContains,
		Dia::Core::StringCRC("texture.shared")));
	registry.Register(stage1);

	Dia::AssetCatalogue::AssetRecord stage2 = MakeRecord("stage.menu", "stage");
	stage2.mReferences.Add(Dia::AssetCatalogue::RelationshipEdge(
		Dia::AssetCatalogue::RelationshipTypes::kContains,
		Dia::Core::StringCRC("texture.shared")));
	registry.Register(stage2);

	registry.Register(MakeRecord("texture.shared", "texture"));

	Dia::AssetCatalogue::ScopeComputer computer;
	Dia::Core::StringCRC outStageName;
	Dia::AssetCatalogue::AssetScope scope = computer.ComputeScope(
		Dia::Core::StringCRC("texture.shared"),
		registry,
		registry.GetRelationshipIndex(),
		outStageName);

	EXPECT_EQ(scope, Dia::AssetCatalogue::AssetScope::kGlobal);
}

// ---------------------------------------------------------------------------
// Test 6: ScopeComputer_NoStageRef_ReturnsGlobal
// ---------------------------------------------------------------------------
TEST_F(CatalogueAutomation, ScopeComputer_NoStageRef_ReturnsGlobal)
{
	Dia::AssetCatalogue::AssetRegistry registry;

	// Texture with no stage containing it
	registry.Register(MakeRecord("texture.orphan", "texture"));

	Dia::AssetCatalogue::ScopeComputer computer;
	Dia::Core::StringCRC outStageName;
	Dia::AssetCatalogue::AssetScope scope = computer.ComputeScope(
		Dia::Core::StringCRC("texture.orphan"),
		registry,
		registry.GetRelationshipIndex(),
		outStageName);

	EXPECT_EQ(scope, Dia::AssetCatalogue::AssetScope::kGlobal);
}

// ---------------------------------------------------------------------------
// Test 7: ScopeComputer_RecomputeAllScopes_UpdatesRegistry
// ---------------------------------------------------------------------------
TEST_F(CatalogueAutomation, ScopeComputer_RecomputeAllScopes_UpdatesRegistry)
{
	Dia::AssetCatalogue::AssetRegistry registry;

	// Stage contains texture.a but not texture.b
	Dia::AssetCatalogue::AssetRecord stage = MakeRecord("stage.level1", "stage");
	stage.mReferences.Add(Dia::AssetCatalogue::RelationshipEdge(
		Dia::AssetCatalogue::RelationshipTypes::kContains,
		Dia::Core::StringCRC("texture.a")));
	registry.Register(stage);

	registry.Register(MakeRecord("texture.a", "texture"));
	registry.Register(MakeRecord("texture.b", "texture"));

	Dia::AssetCatalogue::ScopeComputer computer;
	unsigned int updated = computer.RecomputeAllScopes(registry, registry.GetRelationshipIndex());

	// Should update texture.a and texture.b (2 non-stage records)
	EXPECT_EQ(updated, 2u);

	// Verify texture.a is now stage-scoped
	const Dia::AssetCatalogue::AssetRecord* a = registry.FindById(Dia::Core::StringCRC("texture.a"));
	ASSERT_NE(a, nullptr);
	EXPECT_EQ(a->mScope, Dia::AssetCatalogue::AssetScope::kStage);
	EXPECT_EQ(a->mScopeStageName, Dia::Core::StringCRC("stage.level1"));

	// Verify texture.b is global
	const Dia::AssetCatalogue::AssetRecord* b = registry.FindById(Dia::Core::StringCRC("texture.b"));
	ASSERT_NE(b, nullptr);
	EXPECT_EQ(b->mScope, Dia::AssetCatalogue::AssetScope::kGlobal);
}

// ---------------------------------------------------------------------------
// Test 8: CatalogueRulesEngine_LoadRules_ValidFile_LoadsCorrectCount
// ---------------------------------------------------------------------------
TEST_F(CatalogueAutomation, CatalogueRulesEngine_LoadRules_ValidFile_LoadsCorrectCount)
{
	const char* rulesPath = "C:\\Temp\\test_valid_rules.json";
	const char* rulesJson =
		"{\n"
		"  \"rules\": [\n"
		"    { \"name\": \"tag-all\", \"match\": { \"all\": true }, \"action\": \"assign_tag\", \"tag\": \"auto\" },\n"
		"    { \"name\": \"scope-auto\", \"match\": { \"all\": true }, \"action\": \"compute_scope\" }\n"
		"  ]\n"
		"}";
	ASSERT_TRUE(WriteTestFile(rulesPath, rulesJson));

	Dia::AssetCatalogue::AssetTypeRegistry typeRegistry;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(typeRegistry);

	Dia::AssetCatalogue::CatalogueRulesEngine engine;
	Dia::AssetCatalogue::LoadResult<void> result = engine.LoadRules(rulesPath, typeRegistry);

	EXPECT_TRUE(result.mSuccess);
	EXPECT_EQ(engine.GetRuleCount(), 2u);

	remove(rulesPath);
}

// ---------------------------------------------------------------------------
// Test 9: CatalogueRulesEngine_LoadRules_UnknownAction_ReturnsError
// ---------------------------------------------------------------------------
TEST_F(CatalogueAutomation, CatalogueRulesEngine_LoadRules_UnknownAction_ReturnsError)
{
	const char* rulesPath = "C:\\Temp\\test_bad_action_rules.json";
	const char* rulesJson =
		"{\n"
		"  \"rules\": [\n"
		"    { \"name\": \"bad-rule\", \"match\": { \"all\": true }, \"action\": \"explode_everything\" }\n"
		"  ]\n"
		"}";
	ASSERT_TRUE(WriteTestFile(rulesPath, rulesJson));

	Dia::AssetCatalogue::AssetTypeRegistry typeRegistry;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(typeRegistry);

	Dia::AssetCatalogue::CatalogueRulesEngine engine;
	Dia::AssetCatalogue::LoadResult<void> result = engine.LoadRules(rulesPath, typeRegistry);

	EXPECT_FALSE(result.mSuccess);
	EXPECT_TRUE(result.HasErrors());
	EXPECT_EQ(engine.GetRuleCount(), 0u);

	remove(rulesPath);
}

// ---------------------------------------------------------------------------
// Test 10: CatalogueRulesEngine_EvaluateDryRun_AssignTag_NoMutation
// ---------------------------------------------------------------------------
TEST_F(CatalogueAutomation, CatalogueRulesEngine_EvaluateDryRun_AssignTag_NoMutation)
{
	const char* rulesPath = "C:\\Temp\\test_dryrun_rules.json";
	const char* rulesJson =
		"{\n"
		"  \"rules\": [\n"
		"    { \"name\": \"tag-textures\", \"match\": { \"type\": \"texture\" }, \"action\": \"assign_tag\", \"tag\": \"visual\" }\n"
		"  ]\n"
		"}";
	ASSERT_TRUE(WriteTestFile(rulesPath, rulesJson));

	Dia::AssetCatalogue::AssetTypeRegistry typeRegistry;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(typeRegistry);

	Dia::AssetCatalogue::CatalogueRulesEngine engine;
	engine.LoadRules(rulesPath, typeRegistry);

	Dia::AssetCatalogue::AssetRegistry registry;
	registry.Register(MakeRecord("texture.hero", "texture"));
	registry.Register(MakeRecord("config.settings", "config"));

	Dia::AssetCatalogue::RuleChangeset changeset =
		engine.EvaluateDryRun(registry, registry.GetRelationshipIndex());

	// Should propose tagging texture.hero with "visual"
	EXPECT_GE(changeset.mChanges.Size(), 1u);

	// Verify no mutation occurred
	const Dia::AssetCatalogue::AssetRecord* hero = registry.FindById(Dia::Core::StringCRC("texture.hero"));
	ASSERT_NE(hero, nullptr);
	EXPECT_EQ(hero->mTags.Size(), 0u); // still no tags

	remove(rulesPath);
}

// ---------------------------------------------------------------------------
// Test 11: CatalogueRulesEngine_Apply_AssignTag_MutatesRegistry
// ---------------------------------------------------------------------------
TEST_F(CatalogueAutomation, CatalogueRulesEngine_Apply_AssignTag_MutatesRegistry)
{
	const char* rulesPath = "C:\\Temp\\test_apply_rules.json";
	const char* rulesJson =
		"{\n"
		"  \"rules\": [\n"
		"    { \"name\": \"tag-textures\", \"match\": { \"type\": \"texture\" }, \"action\": \"assign_tag\", \"tag\": \"visual\" }\n"
		"  ]\n"
		"}";
	ASSERT_TRUE(WriteTestFile(rulesPath, rulesJson));

	Dia::AssetCatalogue::AssetTypeRegistry typeRegistry;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(typeRegistry);

	Dia::AssetCatalogue::CatalogueRulesEngine engine;
	engine.LoadRules(rulesPath, typeRegistry);

	Dia::AssetCatalogue::AssetRegistry registry;
	registry.Register(MakeRecord("texture.hero", "texture"));
	registry.Register(MakeRecord("config.settings", "config"));

	Dia::AssetCatalogue::RuleChangeset changeset =
		engine.Apply(registry, registry.GetRelationshipIndex());

	// Verify mutation occurred — texture.hero should now have "visual" tag
	const Dia::AssetCatalogue::AssetRecord* hero = registry.FindById(Dia::Core::StringCRC("texture.hero"));
	ASSERT_NE(hero, nullptr);
	EXPECT_EQ(hero->mTags.Size(), 1u);
	EXPECT_EQ(hero->mTags[0], Dia::Core::StringCRC("visual"));

	// Config should not have tag
	const Dia::AssetCatalogue::AssetRecord* cfg = registry.FindById(Dia::Core::StringCRC("config.settings"));
	ASSERT_NE(cfg, nullptr);
	EXPECT_EQ(cfg->mTags.Size(), 0u);

	remove(rulesPath);
}

// ---------------------------------------------------------------------------
// Test 12: CatalogueRulesEngine_ConflictDetection_TwoRulesSetSameScope
// ---------------------------------------------------------------------------
TEST_F(CatalogueAutomation, CatalogueRulesEngine_ConflictDetection_TwoRulesSetSameScope)
{
	const char* rulesPath = "C:\\Temp\\test_conflict_rules.json";
	// Two assign_stage rules targeting the same record with different stage IDs.
	// Both produce "relationship" field changes with different new values, triggering conflict detection.
	const char* rulesJson =
		"{\n"
		"  \"rules\": [\n"
		"    { \"name\": \"stage-assign-a\", \"match\": { \"type\": \"texture\" }, \"action\": \"assign_stage\", \"stage_id\": \"stage.gameplay\" },\n"
		"    { \"name\": \"stage-assign-b\", \"match\": { \"type\": \"texture\" }, \"action\": \"assign_stage\", \"stage_id\": \"stage.menu\" }\n"
		"  ]\n"
		"}";
	ASSERT_TRUE(WriteTestFile(rulesPath, rulesJson));

	Dia::AssetCatalogue::AssetTypeRegistry typeRegistry;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(typeRegistry);

	Dia::AssetCatalogue::CatalogueRulesEngine engine;
	engine.LoadRules(rulesPath, typeRegistry);

	Dia::AssetCatalogue::AssetRegistry registry;
	registry.Register(MakeRecord("texture.hero", "texture"));

	Dia::AssetCatalogue::RuleChangeset changeset =
		engine.EvaluateDryRun(registry, registry.GetRelationshipIndex());

	// Both rules produce a "relationship" change on the same record with different values.
	// Should detect conflict.
	EXPECT_GT(changeset.mConflictCount, 0u);

	// Verify at least one change is marked as conflict
	bool foundConflict = false;
	for (unsigned int i = 0; i < changeset.mChanges.Size(); ++i)
	{
		if (changeset.mChanges[i].mIsConflict)
		{
			foundConflict = true;
			break;
		}
	}
	EXPECT_TRUE(foundConflict);

	remove(rulesPath);
}
