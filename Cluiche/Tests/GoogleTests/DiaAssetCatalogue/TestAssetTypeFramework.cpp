#include <gtest/gtest.h>

#include <DiaAssetCatalogue/AssetTypeDescriptor.h>
#include <DiaAssetCatalogue/AssetTypeRegistry.h>
#include <DiaAssetCatalogue/BuiltInAssetTypes.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/FilePath/FilePath.h>
#include <DiaCore/FilePath/PathStore.h>
#include <DiaCore/Type/TypeVariableAttributes.h>

// ---------------------------------------------------------------------------
// Test fixture — registers a path alias so FilePath::Create doesn't assert
// ---------------------------------------------------------------------------
class AssetTypeFramework : public ::testing::Test
{
protected:
	static void SetUpTestSuite()
	{
		if (!Dia::Core::PathStore::IsPathAliasRegistered(Dia::Core::Path::Alias("assets")))
		{
			Dia::Core::PathStore::RegisterToStore(
				Dia::Core::Path::Alias("assets"),
				Dia::Core::Path::String("C:/Assets"));
		}
	}

	// Convenience: create a FilePath with just a filename using the "assets" alias.
	static Dia::Core::FilePath MakePath(const char* filename)
	{
		return Dia::Core::FilePath(
			Dia::Core::Path::Alias("assets"),
			Dia::Core::FilePath::FileName(filename));
	}
};

// ---------------------------------------------------------------------------
// Test 1: Register a single descriptor — GetCount() returns 1
// ---------------------------------------------------------------------------
TEST_F(AssetTypeFramework, Register_SingleDescriptor_CountIsOne)
{
	Dia::AssetCatalogue::AssetTypeRegistry registry;

	Dia::AssetCatalogue::AssetTypeDescriptor desc;
	desc.mTypeId         = Dia::Core::StringCRC("weapon");
	desc.mName           = Dia::Core::Containers::String64("Weapon Definition");
	desc.mFilePattern    = Dia::Core::Containers::String64("*.weapon.json");
	desc.mTypeDefinition = nullptr;

	bool registered = registry.Register(desc);

	EXPECT_TRUE(registered);
	EXPECT_EQ(registry.GetCount(), 1u);
}

// ---------------------------------------------------------------------------
// Test 2: Duplicate type ID returns false and count unchanged
// ---------------------------------------------------------------------------
TEST_F(AssetTypeFramework, Register_DuplicateTypeId_ReturnsFalseCountUnchanged)
{
	Dia::AssetCatalogue::AssetTypeRegistry registry;

	Dia::AssetCatalogue::AssetTypeDescriptor desc;
	desc.mTypeId         = Dia::Core::StringCRC("config");
	desc.mName           = Dia::Core::Containers::String64("Config");
	desc.mFilePattern    = Dia::Core::Containers::String64("*.config.json");
	desc.mTypeDefinition = nullptr;

	bool first  = registry.Register(desc);
	EXPECT_TRUE(first);
	EXPECT_EQ(registry.GetCount(), 1u);

	bool second = registry.Register(desc);
	EXPECT_FALSE(second);
	EXPECT_EQ(registry.GetCount(), 1u); // unchanged
}

// ---------------------------------------------------------------------------
// Test 3a: FindByTypeId — found
// ---------------------------------------------------------------------------
TEST_F(AssetTypeFramework, FindByTypeId_Found_ReturnsDescriptor)
{
	Dia::AssetCatalogue::AssetTypeRegistry registry;

	Dia::AssetCatalogue::AssetTypeDescriptor desc;
	desc.mTypeId         = Dia::Core::StringCRC("entity");
	desc.mName           = Dia::Core::Containers::String64("Entity Definition");
	desc.mFilePattern    = Dia::Core::Containers::String64("*.entity.json");
	desc.mTypeDefinition = nullptr;

	registry.Register(desc);

	const Dia::AssetCatalogue::AssetTypeDescriptor* found =
		registry.FindByTypeId(Dia::Core::StringCRC("entity"));

	ASSERT_NE(found, nullptr);
	EXPECT_EQ(found->mTypeId, Dia::Core::StringCRC("entity"));
}

// ---------------------------------------------------------------------------
// Test 3b: FindByTypeId — not found returns nullptr
// ---------------------------------------------------------------------------
TEST_F(AssetTypeFramework, FindByTypeId_NotFound_ReturnsNullptr)
{
	Dia::AssetCatalogue::AssetTypeRegistry registry;

	const Dia::AssetCatalogue::AssetTypeDescriptor* found =
		registry.FindByTypeId(Dia::Core::StringCRC("nonexistent"));

	EXPECT_EQ(found, nullptr);
}

// ---------------------------------------------------------------------------
// Test 4: FindByFilePath — matches "ship_stats.config.json" against "*.config.json"
// ---------------------------------------------------------------------------
TEST_F(AssetTypeFramework, FindByFilePath_MatchesConfigPattern)
{
	Dia::AssetCatalogue::AssetTypeRegistry registry;

	Dia::AssetCatalogue::AssetTypeDescriptor desc;
	desc.mTypeId         = Dia::Core::StringCRC("config");
	desc.mName           = Dia::Core::Containers::String64("Config");
	desc.mFilePattern    = Dia::Core::Containers::String64("*.config.json");
	desc.mTypeDefinition = nullptr;

	registry.Register(desc);

	Dia::Core::FilePath path = MakePath("ship_stats.config.json");
	const Dia::AssetCatalogue::AssetTypeDescriptor* found = registry.FindByFilePath(path);

	ASSERT_NE(found, nullptr);
	EXPECT_EQ(found->mTypeId, Dia::Core::StringCRC("config"));
}

// ---------------------------------------------------------------------------
// Test 5: FindByFilePath — no match returns nullptr
// ---------------------------------------------------------------------------
TEST_F(AssetTypeFramework, FindByFilePath_NoMatch_ReturnsNullptr)
{
	Dia::AssetCatalogue::AssetTypeRegistry registry;

	Dia::AssetCatalogue::AssetTypeDescriptor desc;
	desc.mTypeId         = Dia::Core::StringCRC("config");
	desc.mName           = Dia::Core::Containers::String64("Config");
	desc.mFilePattern    = Dia::Core::Containers::String64("*.config.json");
	desc.mTypeDefinition = nullptr;

	registry.Register(desc);

	Dia::Core::FilePath path = MakePath("ship_stats.png");
	const Dia::AssetCatalogue::AssetTypeDescriptor* found = registry.FindByFilePath(path);

	EXPECT_EQ(found, nullptr);
}

// ---------------------------------------------------------------------------
// Test 6: RegisterBuiltInAssetTypes registers 8 types; "texture" is found
// ---------------------------------------------------------------------------
TEST_F(AssetTypeFramework, RegisterBuiltInAssetTypes_RegistersEightTypes)
{
	Dia::AssetCatalogue::AssetTypeRegistry registry;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(registry);

	EXPECT_EQ(registry.GetCount(), 8u);

	const Dia::AssetCatalogue::AssetTypeDescriptor* texture =
		registry.FindByTypeId(Dia::Core::StringCRC("texture"));

	ASSERT_NE(texture, nullptr);
	EXPECT_EQ(texture->mTypeId, Dia::Core::StringCRC("texture"));
}

// ---------------------------------------------------------------------------
// Test 7: Built-in pattern matching — a .entity.json file returns entity descriptor
// ---------------------------------------------------------------------------
TEST_F(AssetTypeFramework, BuiltIn_FindByFilePath_EntityJson_ReturnsEntityDescriptor)
{
	Dia::AssetCatalogue::AssetTypeRegistry registry;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(registry);

	Dia::Core::FilePath path = MakePath("hero.entity.json");
	const Dia::AssetCatalogue::AssetTypeDescriptor* found = registry.FindByFilePath(path);

	ASSERT_NE(found, nullptr);
	EXPECT_EQ(found->mTypeId, Dia::Core::StringCRC("entity"));
}
