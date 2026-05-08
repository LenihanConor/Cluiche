#include <gtest/gtest.h>

#include <DiaAssetCatalogue/AssetRecord.h>
#include <DiaAssetCatalogue/AssetRegistry.h>
#include <DiaAssetCatalogue/RelationshipIndex.h>
#include <DiaAssetCatalogue/RelationshipTypes.h>
#include <DiaAssetCatalogue/CatalogueManifestSerializer.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace
{
	// Build a minimal valid AssetRecord for testing.
	Dia::AssetCatalogue::AssetRecord MakeRecord(const char* id, const char* typeId)
	{
		Dia::AssetCatalogue::AssetRecord r;
		r.mId          = Dia::Core::StringCRC(id);
		r.mAssetTypeId = Dia::Core::StringCRC(typeId);
		r.mSourcePath  = Dia::Core::Containers::String256("Raw/test.json");
		r.mContentHash = 12345678;
		r.mStatus      = Dia::AssetCatalogue::AssetStatus::Active;
		r.mScope       = Dia::AssetCatalogue::AssetScope::kGlobal;
		return r;
	}

	// Write a string to a file (for manifest tests).
	bool WriteFile(const char* path, const char* content)
	{
		FILE* f = nullptr;
#if defined(_MSC_VER)
		fopen_s(&f, path, "wb");
#else
		f = fopen(path, "wb");
#endif
		if (!f) return false;
		fwrite(content, 1, strlen(content), f);
		fclose(f);
		return true;
	}

	// Ensure C:\Temp exists (or try to create it).
	bool EnsureTempDir()
	{
#if defined(_WIN32)
		// CreateDirectory returns non-zero on success or if directory already exists
		// Use _mkdir from <direct.h> instead to avoid windows.h
		// Use a simpler approach: try to write a probe file
		FILE* f = nullptr;
		fopen_s(&f, "C:\\Temp\\_probe.tmp", "wb");
		if (f)
		{
			fclose(f);
			remove("C:\\Temp\\_probe.tmp");
			return true;
		}
		return false;
#else
		return true;
#endif
	}
}

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------
class IdentityRelationshipBackbone : public ::testing::Test
{
protected:
	void SetUp() override
	{
		// Nothing needed per test — registry is a local in each test.
	}
};

// ---------------------------------------------------------------------------
// Test 1: Register valid record — count incremented
// ---------------------------------------------------------------------------
TEST_F(IdentityRelationshipBackbone, Register_ValidRecord_CountIncremented)
{
	Dia::AssetCatalogue::AssetRegistry registry;

	Dia::AssetCatalogue::AssetRecord r = MakeRecord("texture.player_ship", "texture");
	bool result = registry.Register(r);

	EXPECT_TRUE(result);
	EXPECT_EQ(registry.GetCount(), 1u);
}

// ---------------------------------------------------------------------------
// Test 2: Register duplicate ID returns false and count unchanged
// ---------------------------------------------------------------------------
TEST_F(IdentityRelationshipBackbone, Register_DuplicateId_ReturnsFalse)
{
	Dia::AssetCatalogue::AssetRegistry registry;

	Dia::AssetCatalogue::AssetRecord r = MakeRecord("config.ship_stats", "config");
	EXPECT_TRUE(registry.Register(r));
	EXPECT_EQ(registry.GetCount(), 1u);

	bool second = registry.Register(r);
	EXPECT_FALSE(second);
	EXPECT_EQ(registry.GetCount(), 1u); // unchanged
}

// ---------------------------------------------------------------------------
// Test 3: FindById registered record returns correct pointer
// ---------------------------------------------------------------------------
TEST_F(IdentityRelationshipBackbone, FindById_Registered_ReturnsRecord)
{
	Dia::AssetCatalogue::AssetRegistry registry;

	Dia::AssetCatalogue::AssetRecord r = MakeRecord("entity.hero", "entity");
	registry.Register(r);

	const Dia::AssetCatalogue::AssetRecord* found =
		registry.FindById(Dia::Core::StringCRC("entity.hero"));

	ASSERT_NE(found, nullptr);
	EXPECT_EQ(found->mId, Dia::Core::StringCRC("entity.hero"));
	EXPECT_EQ(found->mAssetTypeId, Dia::Core::StringCRC("entity"));
}

// ---------------------------------------------------------------------------
// Test 4: QueryByType with two texture records returns both
// ---------------------------------------------------------------------------
TEST_F(IdentityRelationshipBackbone, QueryByType_TwoTextureRecords_ReturnsBoth)
{
	Dia::AssetCatalogue::AssetRegistry registry;

	registry.Register(MakeRecord("texture.player_ship", "texture"));
	registry.Register(MakeRecord("texture.enemy_ship", "texture"));
	registry.Register(MakeRecord("config.ship_stats", "config"));

	Dia::Core::Containers::DynamicArrayC<const Dia::AssetCatalogue::AssetRecord*, 64> results;
	registry.QueryByType(Dia::Core::StringCRC("texture"), results);

	EXPECT_EQ(results.Size(), 2u);
}

// ---------------------------------------------------------------------------
// Test 5: RelationshipIndex forward refs match record references
// ---------------------------------------------------------------------------
TEST_F(IdentityRelationshipBackbone, RelationshipIndex_ForwardRefs_MatchRecordReferences)
{
	Dia::AssetCatalogue::AssetRegistry registry;

	// entity.hero uses texture.player_ship and config.ship_stats
	Dia::AssetCatalogue::AssetRecord hero = MakeRecord("entity.hero", "entity");
	hero.mReferences.Add(Dia::AssetCatalogue::RelationshipEdge(
		Dia::AssetCatalogue::RelationshipTypes::kUses,
		Dia::Core::StringCRC("texture.player_ship")));
	hero.mReferences.Add(Dia::AssetCatalogue::RelationshipEdge(
		Dia::AssetCatalogue::RelationshipTypes::kUses,
		Dia::Core::StringCRC("config.ship_stats")));

	registry.Register(hero);
	registry.Register(MakeRecord("texture.player_ship", "texture"));
	registry.Register(MakeRecord("config.ship_stats", "config"));

	Dia::Core::Containers::DynamicArrayC<Dia::AssetCatalogue::RelationshipEdge, 16> fwdRefs;
	registry.GetRelationshipIndex().GetForwardRefs(
		Dia::Core::StringCRC("entity.hero"), registry, fwdRefs);

	EXPECT_EQ(fwdRefs.Size(), 2u);
	EXPECT_EQ(fwdRefs[0].mRelationshipType, Dia::AssetCatalogue::RelationshipTypes::kUses);
	EXPECT_EQ(fwdRefs[0].mTargetAssetId, Dia::Core::StringCRC("texture.player_ship"));
	EXPECT_EQ(fwdRefs[1].mTargetAssetId, Dia::Core::StringCRC("config.ship_stats"));
}

// ---------------------------------------------------------------------------
// Test 6: Reverse refs — lazy build and invalidation
// Register A->uses->B. Get reverse refs of B: A must appear.
// Then register C->uses->B, invalidate, get reverse refs of B: A and C must both appear.
// ---------------------------------------------------------------------------
TEST_F(IdentityRelationshipBackbone, RelationshipIndex_ReverseRefs_LazyBuildAndInvalidation)
{
	Dia::AssetCatalogue::AssetRegistry registry;

	// A uses B
	Dia::AssetCatalogue::AssetRecord a = MakeRecord("entity.a", "entity");
	a.mReferences.Add(Dia::AssetCatalogue::RelationshipEdge(
		Dia::AssetCatalogue::RelationshipTypes::kUses,
		Dia::Core::StringCRC("texture.b")));
	registry.Register(a);
	registry.Register(MakeRecord("texture.b", "texture"));

	// First reverse query for B
	Dia::Core::Containers::DynamicArrayC<Dia::AssetCatalogue::RelationshipEdge, 16> revRefs1;
	registry.GetRelationshipIndex().GetReverseRefs(
		Dia::Core::StringCRC("texture.b"), registry, revRefs1);

	EXPECT_EQ(revRefs1.Size(), 1u);
	EXPECT_EQ(revRefs1[0].mTargetAssetId, Dia::Core::StringCRC("entity.a")); // fromId in edge

	// Register C uses B — this invalidates the reverse cache
	Dia::AssetCatalogue::AssetRecord c = MakeRecord("entity.c", "entity");
	c.mReferences.Add(Dia::AssetCatalogue::RelationshipEdge(
		Dia::AssetCatalogue::RelationshipTypes::kUses,
		Dia::Core::StringCRC("texture.b")));
	registry.Register(c);

	// Second reverse query — cache was invalidated by Register, must rebuild
	Dia::Core::Containers::DynamicArrayC<Dia::AssetCatalogue::RelationshipEdge, 16> revRefs2;
	registry.GetRelationshipIndex().GetReverseRefs(
		Dia::Core::StringCRC("texture.b"), registry, revRefs2);

	EXPECT_EQ(revRefs2.Size(), 2u);
}

// ---------------------------------------------------------------------------
// Test 7: ManifestSerializer round-trip — create registry, save, load, verify
// ---------------------------------------------------------------------------
TEST_F(IdentityRelationshipBackbone, ManifestSerializer_RoundTrip_RecordsPreserved)
{
	if (!EnsureTempDir())
	{
		GTEST_SKIP() << "C:\\Temp not accessible; skipping round-trip test";
	}

	const char* testPath = "C:\\Temp\\test_roundtrip.catalogue.json";

	// Build registry
	Dia::AssetCatalogue::AssetRegistry original;
	{
		Dia::AssetCatalogue::AssetRecord r1 = MakeRecord("texture.hero", "texture");
		r1.mContentHash = 111111;
		r1.mStatus = Dia::AssetCatalogue::AssetStatus::Active;
		r1.mScope  = Dia::AssetCatalogue::AssetScope::kGlobal;
		original.Register(r1);

		Dia::AssetCatalogue::AssetRecord r2 = MakeRecord("config.game_settings", "config");
		r2.mContentHash = 222222;
		r2.mScope  = Dia::AssetCatalogue::AssetScope::kStage;
		r2.mScopeStageName = Dia::Core::StringCRC("Gameplay");
		r2.mTags.Add(Dia::Core::StringCRC("settings"));
		r2.mReferences.Add(Dia::AssetCatalogue::RelationshipEdge(
			Dia::AssetCatalogue::RelationshipTypes::kDependsOn,
			Dia::Core::StringCRC("texture.hero")));
		original.Register(r2);
	}

	// Save
	Dia::AssetCatalogue::CatalogueManifestSerializer serializer;
	bool saved = serializer.SaveManifest(original, testPath);
	ASSERT_TRUE(saved) << "SaveManifest failed";

	// Load
	Dia::AssetCatalogue::LoadResult<Dia::AssetCatalogue::AssetRegistry> loaded =
		serializer.LoadManifest(testPath);

	EXPECT_TRUE(loaded.mSuccess) << "LoadManifest failed";
	EXPECT_EQ(loaded.mValue.GetCount(), 2u);

	const Dia::AssetCatalogue::AssetRecord* hero =
		loaded.mValue.FindById(Dia::Core::StringCRC("texture.hero"));
	ASSERT_NE(hero, nullptr);
	EXPECT_EQ(hero->mContentHash, 111111u);

	const Dia::AssetCatalogue::AssetRecord* cfg =
		loaded.mValue.FindById(Dia::Core::StringCRC("config.game_settings"));
	ASSERT_NE(cfg, nullptr);
	EXPECT_EQ(cfg->mContentHash, 222222u);
	EXPECT_EQ(cfg->mScope, Dia::AssetCatalogue::AssetScope::kStage);
	EXPECT_EQ(cfg->mScopeStageName, Dia::Core::StringCRC("Gameplay"));
	EXPECT_EQ(cfg->mTags.Size(), 1u);
	EXPECT_EQ(cfg->mReferences.Size(), 1u);
	EXPECT_EQ(cfg->mReferences[0].mRelationshipType, Dia::AssetCatalogue::RelationshipTypes::kDependsOn);
	EXPECT_EQ(cfg->mReferences[0].mTargetAssetId, Dia::Core::StringCRC("texture.hero"));

	remove(testPath);
}

// ---------------------------------------------------------------------------
// Test 8: ManifestSerializer include support — master + sub manifest
// ---------------------------------------------------------------------------
TEST_F(IdentityRelationshipBackbone, ManifestSerializer_Include_LoadsSubManifestRecords)
{
	if (!EnsureTempDir())
	{
		GTEST_SKIP() << "C:\\Temp not accessible; skipping include test";
	}

	const char* subPath    = "C:\\Temp\\test_sub.catalogue.json";
	const char* masterPath = "C:\\Temp\\test_master.catalogue.json";

	// Sub-manifest: one record
	const char* subJson =
		"{\n"
		"  \"assets\": [\n"
		"    {\n"
		"      \"id\": \"texture.sub_asset\",\n"
		"      \"type\": \"texture\",\n"
		"      \"source_path\": \"Raw/Textures/sub.png\",\n"
		"      \"content_hash\": 999,\n"
		"      \"status\": \"active\",\n"
		"      \"scope\": \"global\",\n"
		"      \"stage_name\": \"\",\n"
		"      \"tags\": [],\n"
		"      \"references\": []\n"
		"    }\n"
		"  ]\n"
		"}";

	// Master manifest: one record + include of sub
	const char* masterJson =
		"{\n"
		"  \"includes\": [\"test_sub.catalogue.json\"],\n"
		"  \"assets\": [\n"
		"    {\n"
		"      \"id\": \"config.master_config\",\n"
		"      \"type\": \"config\",\n"
		"      \"source_path\": \"Raw/Config/master.json\",\n"
		"      \"content_hash\": 777,\n"
		"      \"status\": \"active\",\n"
		"      \"scope\": \"global\",\n"
		"      \"stage_name\": \"\",\n"
		"      \"tags\": [],\n"
		"      \"references\": []\n"
		"    }\n"
		"  ]\n"
		"}";

	ASSERT_TRUE(WriteFile(subPath, subJson));
	ASSERT_TRUE(WriteFile(masterPath, masterJson));

	Dia::AssetCatalogue::CatalogueManifestSerializer serializer;
	Dia::AssetCatalogue::LoadResult<Dia::AssetCatalogue::AssetRegistry> result =
		serializer.LoadManifest(masterPath);

	EXPECT_TRUE(result.mSuccess) << "LoadManifest failed";
	EXPECT_EQ(result.mValue.GetCount(), 2u);

	EXPECT_NE(result.mValue.FindById(Dia::Core::StringCRC("texture.sub_asset")), nullptr);
	EXPECT_NE(result.mValue.FindById(Dia::Core::StringCRC("config.master_config")), nullptr);

	remove(subPath);
	remove(masterPath);
}
