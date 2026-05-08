#include <gtest/gtest.h>

#include <DiaCore/Type/TypeDeclarationMacros.h>
#include <DiaCore/Type/TypeDefinitionMacros.h>
#include <DiaCore/Type/TypeFacade.h>
#include <DiaCore/Type/TypeVariableAttributes.h>
#include <DiaCore/Type/TypeInstance.h>
#include <DiaCore/Containers/Strings/StringWriter.h>
#include <DiaCore/Containers/Strings/StringReader.h>
#include <DiaCore/CRC/CRC.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/FilePath/FilePath.h>
#include <DiaCore/FilePath/PathStore.h>
#include <DiaCore/Json/external/json/json.h>

#include <DiaAssetCatalogue/LoadResult.h>
#include <DiaAssetCatalogue/JsonDefinitionLoader.h>
#include <DiaAssetCatalogue/AssetTypeDescriptor.h>
#include <DiaAssetCatalogue/AssetTypeRegistry.h>
#include <DiaAssetCatalogue/BuiltInAssetTypes.h>
#include <DiaAssetCatalogue/AssetRecord.h>
#include <DiaAssetCatalogue/RelationshipTypes.h>
#include <DiaAssetCatalogue/AssetRegistry.h>
#include <DiaAssetCatalogue/RelationshipIndex.h>
#include <DiaAssetCatalogue/CatalogueManifestSerializer.h>
#include <DiaAssetCatalogue/ContentHasher.h>
#include <DiaAssetCatalogue/ScopeComputer.h>
#include <DiaAssetCatalogue/RelationshipInferrer.h>
#include <DiaAssetCatalogue/CatalogueRulesEngine.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string.h>

// ===========================================================================
// Anonymous namespace — test helpers and type stubs
// ===========================================================================
namespace
{
	// Nested struct for loader tests
	struct NestedInner
	{
		DIA_TYPE_DECLARATION;
		int mInnerValue = 0;
	};

	struct NestedOuter
	{
		DIA_TYPE_DECLARATION;
		int mOuterValue = 0;
		NestedInner mInner;
	};

	// Struct with an asset reference field for RelationshipInferrer tests
	struct AssetRefStruct
	{
		DIA_TYPE_DECLARATION;
		char mReferencedAsset[64];

		AssetRefStruct() { mReferencedAsset[0] = '\0'; }
	};

	DIA_TYPE_DEFINITION(NestedInner)
		DIA_TYPE_ADD_VARIABLE("mInnerValue", mInnerValue)
			DIA_TYPE_ADD_VARIABLE_ATTRIBUTE(Dia::Core::Types::TypeVariableAttributeRequired)
	DIA_TYPE_DEFINITION_END()

	DIA_TYPE_DEFINITION(NestedOuter)
		DIA_TYPE_ADD_VARIABLE("mOuterValue", mOuterValue)
			DIA_TYPE_ADD_VARIABLE_ATTRIBUTE(Dia::Core::Types::TypeVariableAttributeRequired)
	DIA_TYPE_DEFINITION_END()

	DIA_TYPE_DEFINITION(AssetRefStruct)
		DIA_TYPE_ADD_VARIABLE_ARRAY("mReferencedAsset", mReferencedAsset, 64)
			DIA_TYPE_ADD_VARIABLE_ATTRIBUTE_PARAM_1(Dia::Core::Types::TypeVariableAttributeAssetReference, Dia::Core::StringCRC("texture"))
	DIA_TYPE_DEFINITION_END()

	// Helper: register a type if not already present
	template<typename T>
	void EnsureTypeRegistered(Dia::Core::Types::TypeRegistry& reg)
	{
		Dia::Core::Types::TypeDefinition* def = T::GetTypeStatic();
		Dia::Core::CRC crc(def->GetUniqueCRC());
		if (!reg.ContainsType(crc))
		{
			reg.Add(def);
		}
	}

	// Helper: create AssetRecord
	Dia::AssetCatalogue::AssetRecord MakeTestRecord(const char* id, const char* typeId, const char* sourcePath = "Raw/test.json")
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

	bool EnsureTestTempDir()
	{
		CreateDirectoryW(L"C:\\Temp", NULL);
		DWORD attr = GetFileAttributesW(L"C:\\Temp");
		return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
	}

	bool WriteTestFileHelper(const char* path, const char* content, size_t len)
	{
		FILE* f = nullptr;
		fopen_s(&f, path, "wb");
		if (!f) return false;
		fwrite(content, 1, len, f);
		fclose(f);
		return true;
	}

	bool WriteTestFileHelper(const char* path, const char* content)
	{
		return WriteTestFileHelper(path, content, strlen(content));
	}
}

// ===========================================================================
// FEATURE 1 — JSON Definition Loader Exhaustive
// ===========================================================================
class JsonDefinitionLoaderExhaustive : public ::testing::Test
{
protected:
	static void SetUpTestSuite()
	{
		Dia::Core::Types::TypeRegistry& reg = Dia::Core::Types::GetTypeFacade().Registry();
		EnsureTypeRegistered<NestedInner>(reg);
		EnsureTypeRegistered<NestedOuter>(reg);
		EnsureTypeRegistered<AssetRefStruct>(reg);
	}
};

// LoadResult HasErrors false on success
TEST_F(JsonDefinitionLoaderExhaustive, LoadResult_NoErrors_HasErrorsFalse)
{
	Dia::AssetCatalogue::LoadResult<int> result;
	result.mSuccess = true;
	// No errors added
	EXPECT_FALSE(result.HasErrors());
	EXPECT_EQ(result.mErrors.Size(), 0u);
}

// LoadResult HasErrors true after adding error
TEST_F(JsonDefinitionLoaderExhaustive, LoadResult_WithError_HasErrorsTrue)
{
	Dia::AssetCatalogue::LoadResult<int> result;
	Dia::AssetCatalogue::LoadError err;
	err.mKind = Dia::AssetCatalogue::LoadErrorKind::FileNotFound;
	err.mMessage = Dia::Core::Containers::String64("test error");
	result.mErrors.Add(err);

	EXPECT_TRUE(result.HasErrors());
	EXPECT_EQ(result.mErrors.Size(), 1u);
}

// LoadResult GetFirstError returns first in sequence
TEST_F(JsonDefinitionLoaderExhaustive, LoadResult_MultipleErrors_GetFirstReturnsFirst)
{
	Dia::AssetCatalogue::LoadResult<int> result;

	Dia::AssetCatalogue::LoadError err1;
	err1.mKind = Dia::AssetCatalogue::LoadErrorKind::FileNotFound;
	err1.mMessage = Dia::Core::Containers::String64("first");
	result.mErrors.Add(err1);

	Dia::AssetCatalogue::LoadError err2;
	err2.mKind = Dia::AssetCatalogue::LoadErrorKind::JsonParseError;
	err2.mMessage = Dia::Core::Containers::String64("second");
	result.mErrors.Add(err2);

	EXPECT_EQ(result.GetFirstError().mKind, Dia::AssetCatalogue::LoadErrorKind::FileNotFound);
}

// LoadResult void specialization
TEST_F(JsonDefinitionLoaderExhaustive, LoadResultVoid_HasErrors_WorksCorrectly)
{
	Dia::AssetCatalogue::LoadResult<void> result;
	EXPECT_FALSE(result.HasErrors());
	EXPECT_FALSE(result.mSuccess);

	Dia::AssetCatalogue::LoadError err;
	err.mKind = Dia::AssetCatalogue::LoadErrorKind::MissingRequiredField;
	result.mErrors.Add(err);

	EXPECT_TRUE(result.HasErrors());
	EXPECT_EQ(result.GetFirstError().mKind, Dia::AssetCatalogue::LoadErrorKind::MissingRequiredField);
}

// LoadResult multiple errors accumulate up to capacity
TEST_F(JsonDefinitionLoaderExhaustive, LoadResult_ErrorsAccumulate_UpToCapacity16)
{
	Dia::AssetCatalogue::LoadResult<int> result;
	for (int i = 0; i < 16; ++i)
	{
		Dia::AssetCatalogue::LoadError err;
		err.mKind = Dia::AssetCatalogue::LoadErrorKind::TypeMismatch;
		result.mErrors.Add(err);
	}
	EXPECT_EQ(result.mErrors.Size(), 16u);
}

// LoadFromBuffer with struct missing required field (serialize then strip)
TEST_F(JsonDefinitionLoaderExhaustive, LoadFromBuffer_NestedMissingRequiredField)
{
	Dia::Core::Types::TypeRegistry& reg = Dia::Core::Types::GetTypeFacade().Registry();

	// Serialize a complete NestedOuter so we get valid JSON
	NestedOuter source;
	source.mOuterValue = 7;

	char bufferMem[8 * 1024];
	Dia::Core::Containers::StringWriter writer(bufferMem, 8 * 1024);
	Dia::Core::Types::GetTypeFacade().JsonSerializer().Serialize(source, writer);

	// Parse and strip the required field
	Json::Value root;
	Json::Reader jsonReader;
	jsonReader.parse(bufferMem, root, false);
	root.removeMember("mOuterValue");

	Json::FastWriter fastWriter;
	std::string modifiedJson = fastWriter.write(root);

	Dia::Core::Containers::StringReader reader(modifiedJson.c_str());
	Dia::AssetCatalogue::JsonDefinitionLoader loader(reg);

	auto result = loader.LoadFromBuffer<NestedOuter>(reader);

	EXPECT_FALSE(result.mSuccess);
	EXPECT_TRUE(result.HasErrors());
	EXPECT_EQ(result.GetFirstError().mKind, Dia::AssetCatalogue::LoadErrorKind::MissingRequiredField);
}

// LoadFromBuffer with correct required field — success
TEST_F(JsonDefinitionLoaderExhaustive, LoadFromBuffer_RequiredFieldPresent_Success)
{
	Dia::Core::Types::TypeRegistry& reg = Dia::Core::Types::GetTypeFacade().Registry();

	// Serialize a NestedOuter with all fields
	NestedOuter source;
	source.mOuterValue = 99;

	char bufferMem[8 * 1024];
	Dia::Core::Containers::StringWriter writer(bufferMem, 8 * 1024);
	Dia::Core::Types::GetTypeFacade().JsonSerializer().Serialize(source, writer);

	Dia::Core::Containers::StringReader reader(bufferMem);
	Dia::AssetCatalogue::JsonDefinitionLoader loader(reg);

	auto result = loader.LoadFromBuffer<NestedOuter>(reader);

	EXPECT_TRUE(result.mSuccess);
	EXPECT_FALSE(result.HasErrors());
	EXPECT_EQ(result.mValue.mOuterValue, 99);
}

// Load from file with valid temp file
TEST_F(JsonDefinitionLoaderExhaustive, Load_ValidTempFile_Success)
{
	if (!EnsureTestTempDir()) GTEST_SKIP() << "C:\\Temp not accessible";

	Dia::Core::Types::TypeRegistry& reg = Dia::Core::Types::GetTypeFacade().Registry();

	// Serialize NestedOuter to a temp file
	NestedOuter source;
	source.mOuterValue = 42;

	char bufferMem[8 * 1024];
	Dia::Core::Containers::StringWriter writer(bufferMem, 8 * 1024);
	Dia::Core::Types::GetTypeFacade().JsonSerializer().Serialize(source, writer);

	const char* filePath = "C:\\Temp\\test_loader_exhaustive.json";
	ASSERT_TRUE(WriteTestFileHelper(filePath, bufferMem));

	// Register a path alias for temp
	if (!Dia::Core::PathStore::IsPathAliasRegistered(Dia::Core::Path::Alias("temptest")))
	{
		Dia::Core::PathStore::RegisterToStore(
			Dia::Core::Path::Alias("temptest"),
			Dia::Core::Path::String("C:/Temp"));
	}

	Dia::Core::FilePath fp(
		Dia::Core::Path::Alias("temptest"),
		Dia::Core::FilePath::FileName("test_loader_exhaustive.json"));

	Dia::AssetCatalogue::JsonDefinitionLoader loader(reg);
	auto result = loader.Load<NestedOuter>(fp);

	EXPECT_TRUE(result.mSuccess);
	EXPECT_EQ(result.mValue.mOuterValue, 42);

	remove(filePath);
}

// Load with empty path returns FileNotFound
TEST_F(JsonDefinitionLoaderExhaustive, Load_EmptyPath_ReturnsFileNotFound)
{
	Dia::Core::Types::TypeRegistry& reg = Dia::Core::Types::GetTypeFacade().Registry();

	Dia::Core::FilePath emptyPath;
	Dia::AssetCatalogue::JsonDefinitionLoader loader(reg);
	auto result = loader.Load<NestedOuter>(emptyPath);

	EXPECT_FALSE(result.mSuccess);
	EXPECT_TRUE(result.HasErrors());
	EXPECT_EQ(result.GetFirstError().mKind, Dia::AssetCatalogue::LoadErrorKind::FileNotFound);
}

// ===========================================================================
// FEATURE 2 — Asset Type Framework Exhaustive
// ===========================================================================
class AssetTypeFrameworkExhaustive : public ::testing::Test
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

	static Dia::Core::FilePath MakePath(const char* filename)
	{
		return Dia::Core::FilePath(
			Dia::Core::Path::Alias("assets"),
			Dia::Core::FilePath::FileName(filename));
	}
};

// All 8 built-in types present after RegisterBuiltInAssetTypes
TEST_F(AssetTypeFrameworkExhaustive, BuiltIn_All8TypesRegistered)
{
	Dia::AssetCatalogue::AssetTypeRegistry registry;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(registry);

	EXPECT_EQ(registry.GetCount(), 8u);
}

// Verify each built-in type ID string
TEST_F(AssetTypeFrameworkExhaustive, BuiltIn_EachTypeId_Exists)
{
	Dia::AssetCatalogue::AssetTypeRegistry registry;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(registry);

	const char* expectedIds[] = { "texture", "sprite", "audio", "config", "entity", "stage", "ui", "folder" };
	for (int i = 0; i < 8; ++i)
	{
		const Dia::AssetCatalogue::AssetTypeDescriptor* desc =
			registry.FindByTypeId(Dia::Core::StringCRC(expectedIds[i]));
		EXPECT_NE(desc, nullptr) << "Missing type: " << expectedIds[i];
	}
}

// All 8 type IDs are distinct (CRC values different)
TEST_F(AssetTypeFrameworkExhaustive, BuiltIn_TypeIds_AreDistinct)
{
	Dia::AssetCatalogue::AssetTypeRegistry registry;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(registry);

	const char* ids[] = { "texture", "sprite", "audio", "config", "entity", "stage", "ui", "folder" };
	for (int i = 0; i < 8; ++i)
	{
		for (int j = i + 1; j < 8; ++j)
		{
			Dia::Core::StringCRC a(ids[i]);
			Dia::Core::StringCRC b(ids[j]);
			EXPECT_NE(a.Value(), b.Value())
				<< ids[i] << " and " << ids[j] << " have same CRC";
		}
	}
}

// FindByFilePath with each built-in file extension pattern
TEST_F(AssetTypeFrameworkExhaustive, BuiltIn_FindByFilePath_SpriteJson)
{
	Dia::AssetCatalogue::AssetTypeRegistry registry;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(registry);

	auto path = MakePath("hero.sprite.json");
	const Dia::AssetCatalogue::AssetTypeDescriptor* found = registry.FindByFilePath(path);
	ASSERT_NE(found, nullptr);
	EXPECT_EQ(found->mTypeId, Dia::Core::StringCRC("sprite"));
}

TEST_F(AssetTypeFrameworkExhaustive, BuiltIn_FindByFilePath_AudioWav)
{
	Dia::AssetCatalogue::AssetTypeRegistry registry;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(registry);

	auto path = MakePath("explosion.audio.wav");
	const Dia::AssetCatalogue::AssetTypeDescriptor* found = registry.FindByFilePath(path);
	ASSERT_NE(found, nullptr);
	EXPECT_EQ(found->mTypeId, Dia::Core::StringCRC("audio"));
}

TEST_F(AssetTypeFrameworkExhaustive, BuiltIn_FindByFilePath_StageJson)
{
	Dia::AssetCatalogue::AssetTypeRegistry registry;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(registry);

	auto path = MakePath("level1.stage.json");
	const Dia::AssetCatalogue::AssetTypeDescriptor* found = registry.FindByFilePath(path);
	ASSERT_NE(found, nullptr);
	EXPECT_EQ(found->mTypeId, Dia::Core::StringCRC("stage"));
}

TEST_F(AssetTypeFrameworkExhaustive, BuiltIn_FindByFilePath_UiJson)
{
	Dia::AssetCatalogue::AssetTypeRegistry registry;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(registry);

	auto path = MakePath("hud.ui.json");
	const Dia::AssetCatalogue::AssetTypeDescriptor* found = registry.FindByFilePath(path);
	ASSERT_NE(found, nullptr);
	EXPECT_EQ(found->mTypeId, Dia::Core::StringCRC("ui"));
}

TEST_F(AssetTypeFrameworkExhaustive, BuiltIn_FindByFilePath_TexturePng)
{
	Dia::AssetCatalogue::AssetTypeRegistry registry;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(registry);

	auto path = MakePath("player.texture.png");
	const Dia::AssetCatalogue::AssetTypeDescriptor* found = registry.FindByFilePath(path);
	ASSERT_NE(found, nullptr);
	EXPECT_EQ(found->mTypeId, Dia::Core::StringCRC("texture"));
}

TEST_F(AssetTypeFrameworkExhaustive, BuiltIn_FindByFilePath_ConfigJson)
{
	Dia::AssetCatalogue::AssetTypeRegistry registry;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(registry);

	auto path = MakePath("game.config.json");
	const Dia::AssetCatalogue::AssetTypeDescriptor* found = registry.FindByFilePath(path);
	ASSERT_NE(found, nullptr);
	EXPECT_EQ(found->mTypeId, Dia::Core::StringCRC("config"));
}

TEST_F(AssetTypeFrameworkExhaustive, BuiltIn_FindByFilePath_Folder)
{
	Dia::AssetCatalogue::AssetTypeRegistry registry;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(registry);

	auto path = MakePath("sprites.folder");
	const Dia::AssetCatalogue::AssetTypeDescriptor* found = registry.FindByFilePath(path);
	ASSERT_NE(found, nullptr);
	EXPECT_EQ(found->mTypeId, Dia::Core::StringCRC("folder"));
}

// Pattern matching is suffix-based (case sensitive)
TEST_F(AssetTypeFrameworkExhaustive, FindByFilePath_CaseSensitive_NoMatchUppercase)
{
	Dia::AssetCatalogue::AssetTypeRegistry registry;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(registry);

	// Uppercase ".CONFIG.JSON" should NOT match "*.config.json"
	auto path = MakePath("game.CONFIG.JSON");
	const Dia::AssetCatalogue::AssetTypeDescriptor* found = registry.FindByFilePath(path);
	EXPECT_EQ(found, nullptr);
}

// ===========================================================================
// FEATURE 3 — Identity & Relationship Backbone Exhaustive
// ===========================================================================
class IdentityRelationshipExhaustive : public ::testing::Test
{
protected:
	void SetUp() override
	{
		EnsureTestTempDir();
	}
};

// AssetRecord HasContentHash false when mContentHash==0
TEST_F(IdentityRelationshipExhaustive, AssetRecord_HasContentHash_FalseWhenZero)
{
	Dia::AssetCatalogue::AssetRecord r;
	r.mContentHash = 0;
	EXPECT_FALSE(r.HasContentHash());
}

// AssetRecord HasContentHash true when non-zero
TEST_F(IdentityRelationshipExhaustive, AssetRecord_HasContentHash_TrueWhenNonZero)
{
	Dia::AssetCatalogue::AssetRecord r;
	r.mContentHash = 42;
	EXPECT_TRUE(r.HasContentHash());
}

// AssetStatus enum values exist
TEST_F(IdentityRelationshipExhaustive, AssetStatus_AllValuesExist)
{
	Dia::AssetCatalogue::AssetStatus active = Dia::AssetCatalogue::AssetStatus::Active;
	Dia::AssetCatalogue::AssetStatus draft = Dia::AssetCatalogue::AssetStatus::Draft;
	Dia::AssetCatalogue::AssetStatus deprecated = Dia::AssetCatalogue::AssetStatus::Deprecated;

	EXPECT_NE(static_cast<int>(active), static_cast<int>(draft));
	EXPECT_NE(static_cast<int>(active), static_cast<int>(deprecated));
	EXPECT_NE(static_cast<int>(draft), static_cast<int>(deprecated));
}

// AssetScope enum values exist
TEST_F(IdentityRelationshipExhaustive, AssetScope_AllValuesExist)
{
	Dia::AssetCatalogue::AssetScope global = Dia::AssetCatalogue::AssetScope::kGlobal;
	Dia::AssetCatalogue::AssetScope stage = Dia::AssetCatalogue::AssetScope::kStage;

	EXPECT_NE(static_cast<int>(global), static_cast<int>(stage));
}

// Register invalid ID format — missing dot
TEST_F(IdentityRelationshipExhaustive, AssetRegistry_Register_MissingDot_ReturnsFalse)
{
	Dia::AssetCatalogue::AssetRegistry registry;

	Dia::AssetCatalogue::AssetRecord r;
	r.mId = Dia::Core::StringCRC("nodothere");
	r.mAssetTypeId = Dia::Core::StringCRC("texture");
	r.mSourcePath = Dia::Core::Containers::String256("test.png");

	EXPECT_FALSE(registry.Register(r));
	EXPECT_EQ(registry.GetCount(), 0u);
}

// Register invalid ID format — uppercase character
TEST_F(IdentityRelationshipExhaustive, AssetRegistry_Register_UppercaseChar_ReturnsFalse)
{
	Dia::AssetCatalogue::AssetRegistry registry;

	Dia::AssetCatalogue::AssetRecord r;
	r.mId = Dia::Core::StringCRC("texture.Player");
	r.mAssetTypeId = Dia::Core::StringCRC("texture");
	r.mSourcePath = Dia::Core::Containers::String256("test.png");

	EXPECT_FALSE(registry.Register(r));
	EXPECT_EQ(registry.GetCount(), 0u);
}

// Register invalid ID format — empty name part (ends with dot)
TEST_F(IdentityRelationshipExhaustive, AssetRegistry_Register_EmptyNamePart_ReturnsFalse)
{
	Dia::AssetCatalogue::AssetRegistry registry;

	Dia::AssetCatalogue::AssetRecord r;
	r.mId = Dia::Core::StringCRC("texture.");
	r.mAssetTypeId = Dia::Core::StringCRC("texture");
	r.mSourcePath = Dia::Core::Containers::String256("test.png");

	EXPECT_FALSE(registry.Register(r));
}

// QueryByTag returns correct subset
TEST_F(IdentityRelationshipExhaustive, AssetRegistry_QueryByTag_ReturnsCorrectSubset)
{
	Dia::AssetCatalogue::AssetRegistry registry;

	auto r1 = MakeTestRecord("texture.hero", "texture");
	r1.mTags.Add(Dia::Core::StringCRC("visual"));
	registry.Register(r1);

	auto r2 = MakeTestRecord("config.game", "config");
	r2.mTags.Add(Dia::Core::StringCRC("settings"));
	registry.Register(r2);

	auto r3 = MakeTestRecord("texture.enemy", "texture");
	r3.mTags.Add(Dia::Core::StringCRC("visual"));
	registry.Register(r3);

	Dia::Core::Containers::DynamicArrayC<const Dia::AssetCatalogue::AssetRecord*, 64> results;
	registry.QueryByTag(Dia::Core::StringCRC("visual"), results);

	EXPECT_EQ(results.Size(), 2u);
}

// Remove decrements count
TEST_F(IdentityRelationshipExhaustive, AssetRegistry_Remove_DecrementsCount)
{
	Dia::AssetCatalogue::AssetRegistry registry;
	registry.Register(MakeTestRecord("texture.hero", "texture"));
	registry.Register(MakeTestRecord("texture.enemy", "texture"));
	EXPECT_EQ(registry.GetCount(), 2u);

	bool removed = registry.Remove(Dia::Core::StringCRC("texture.hero"));
	EXPECT_TRUE(removed);
	EXPECT_EQ(registry.GetCount(), 1u);
}

// FindById after Remove returns null
TEST_F(IdentityRelationshipExhaustive, AssetRegistry_FindById_AfterRemove_ReturnsNull)
{
	Dia::AssetCatalogue::AssetRegistry registry;
	registry.Register(MakeTestRecord("texture.hero", "texture"));

	registry.Remove(Dia::Core::StringCRC("texture.hero"));
	const Dia::AssetCatalogue::AssetRecord* found =
		registry.FindById(Dia::Core::StringCRC("texture.hero"));

	EXPECT_EQ(found, nullptr);
}

// Non-const FindById allows mutation
TEST_F(IdentityRelationshipExhaustive, AssetRegistry_FindById_NonConst_AllowsMutation)
{
	Dia::AssetCatalogue::AssetRegistry registry;
	registry.Register(MakeTestRecord("texture.hero", "texture"));

	Dia::AssetCatalogue::AssetRecord* mutable_record =
		registry.FindById(Dia::Core::StringCRC("texture.hero"));
	ASSERT_NE(mutable_record, nullptr);

	mutable_record->mContentHash = 99999;

	const Dia::AssetCatalogue::AssetRecord* found =
		registry.FindById(Dia::Core::StringCRC("texture.hero"));
	EXPECT_EQ(found->mContentHash, 99999u);
}

// RelationshipTypes all 4 constants have distinct StringCRC values
TEST_F(IdentityRelationshipExhaustive, RelationshipTypes_AllFourDistinct)
{
	using namespace Dia::AssetCatalogue::RelationshipTypes;
	EXPECT_NE(kUses.Value(), kContains.Value());
	EXPECT_NE(kUses.Value(), kConfiguredBy.Value());
	EXPECT_NE(kUses.Value(), kDependsOn.Value());
	EXPECT_NE(kContains.Value(), kConfiguredBy.Value());
	EXPECT_NE(kContains.Value(), kDependsOn.Value());
	EXPECT_NE(kConfiguredBy.Value(), kDependsOn.Value());
}

// Forward refs of asset with no references returns empty
TEST_F(IdentityRelationshipExhaustive, RelationshipIndex_ForwardRefs_NoRefs_ReturnsEmpty)
{
	Dia::AssetCatalogue::AssetRegistry registry;
	registry.Register(MakeTestRecord("texture.hero", "texture"));

	Dia::Core::Containers::DynamicArrayC<Dia::AssetCatalogue::RelationshipEdge, 16> results;
	registry.GetRelationshipIndex().GetForwardRefs(
		Dia::Core::StringCRC("texture.hero"), registry, results);

	EXPECT_EQ(results.Size(), 0u);
}

// Reverse refs of unconnected asset returns empty
TEST_F(IdentityRelationshipExhaustive, RelationshipIndex_ReverseRefs_Unconnected_ReturnsEmpty)
{
	Dia::AssetCatalogue::AssetRegistry registry;
	registry.Register(MakeTestRecord("texture.hero", "texture"));
	registry.Register(MakeTestRecord("texture.enemy", "texture"));

	Dia::Core::Containers::DynamicArrayC<Dia::AssetCatalogue::RelationshipEdge, 16> results;
	registry.GetRelationshipIndex().GetReverseRefs(
		Dia::Core::StringCRC("texture.hero"), registry, results);

	EXPECT_EQ(results.Size(), 0u);
}

// GetReverseRefsByType filters correctly by relationship type
TEST_F(IdentityRelationshipExhaustive, RelationshipIndex_ReverseRefsByType_FiltersCorrectly)
{
	Dia::AssetCatalogue::AssetRegistry registry;

	// entity uses texture via kUses, stage contains texture via kContains
	auto entity = MakeTestRecord("entity.hero", "entity");
	entity.mReferences.Add(Dia::AssetCatalogue::RelationshipEdge(
		Dia::AssetCatalogue::RelationshipTypes::kUses,
		Dia::Core::StringCRC("texture.hero")));
	registry.Register(entity);

	auto stage = MakeTestRecord("stage.level1", "stage");
	stage.mReferences.Add(Dia::AssetCatalogue::RelationshipEdge(
		Dia::AssetCatalogue::RelationshipTypes::kContains,
		Dia::Core::StringCRC("texture.hero")));
	registry.Register(stage);

	registry.Register(MakeTestRecord("texture.hero", "texture"));

	// Get only "uses" reverse refs for texture.hero
	Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16> usesResults;
	registry.GetRelationshipIndex().GetReverseRefsByType(
		Dia::Core::StringCRC("texture.hero"),
		Dia::AssetCatalogue::RelationshipTypes::kUses,
		registry,
		usesResults);
	EXPECT_EQ(usesResults.Size(), 1u);
	EXPECT_EQ(usesResults[0], Dia::Core::StringCRC("entity.hero"));

	// Get only "contains" reverse refs for texture.hero
	Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16> containsResults;
	registry.GetRelationshipIndex().GetReverseRefsByType(
		Dia::Core::StringCRC("texture.hero"),
		Dia::AssetCatalogue::RelationshipTypes::kContains,
		registry,
		containsResults);
	EXPECT_EQ(containsResults.Size(), 1u);
	EXPECT_EQ(containsResults[0], Dia::Core::StringCRC("stage.level1"));
}

// CatalogueManifestSerializer save+load preserves tags, scope, stage name, references
TEST_F(IdentityRelationshipExhaustive, ManifestSerializer_RoundTrip_PreservesAllFields)
{
	if (!EnsureTestTempDir()) GTEST_SKIP() << "C:\\Temp not accessible";

	const char* testPath = "C:\\Temp\\test_exhaustive_roundtrip.catalogue.json";

	Dia::AssetCatalogue::AssetRegistry original;
	{
		auto r = MakeTestRecord("config.settings", "config");
		r.mContentHash = 555555;
		r.mStatus = Dia::AssetCatalogue::AssetStatus::Draft;
		r.mScope = Dia::AssetCatalogue::AssetScope::kStage;
		r.mScopeStageName = Dia::Core::StringCRC("level1");
		r.mTags.Add(Dia::Core::StringCRC("important"));
		r.mTags.Add(Dia::Core::StringCRC("settings"));
		r.mReferences.Add(Dia::AssetCatalogue::RelationshipEdge(
			Dia::AssetCatalogue::RelationshipTypes::kUses,
			Dia::Core::StringCRC("texture.bg")));
		original.Register(r);
	}

	Dia::AssetCatalogue::CatalogueManifestSerializer serializer;
	ASSERT_TRUE(serializer.SaveManifest(original, testPath));

	auto loaded = serializer.LoadManifest(testPath);
	ASSERT_TRUE(loaded.mSuccess);
	EXPECT_EQ(loaded.mValue.GetCount(), 1u);

	const Dia::AssetCatalogue::AssetRecord* rec =
		loaded.mValue.FindById(Dia::Core::StringCRC("config.settings"));
	ASSERT_NE(rec, nullptr);
	EXPECT_EQ(rec->mContentHash, 555555u);
	EXPECT_EQ(rec->mScope, Dia::AssetCatalogue::AssetScope::kStage);
	EXPECT_EQ(rec->mScopeStageName, Dia::Core::StringCRC("level1"));
	EXPECT_EQ(rec->mTags.Size(), 2u);
	EXPECT_EQ(rec->mReferences.Size(), 1u);
	EXPECT_EQ(rec->mReferences[0].mRelationshipType, Dia::AssetCatalogue::RelationshipTypes::kUses);
	EXPECT_EQ(rec->mReferences[0].mTargetAssetId, Dia::Core::StringCRC("texture.bg"));

	remove(testPath);
}

// Load non-existent manifest file returns FileNotFound error
TEST_F(IdentityRelationshipExhaustive, ManifestSerializer_LoadNonExistent_ReturnsFileNotFound)
{
	Dia::AssetCatalogue::CatalogueManifestSerializer serializer;
	auto result = serializer.LoadManifest("C:\\Temp\\nonexistent_manifest_7777.json");

	EXPECT_FALSE(result.mSuccess);
	EXPECT_TRUE(result.HasErrors());
	EXPECT_EQ(result.GetFirstError().mKind, Dia::AssetCatalogue::LoadErrorKind::FileNotFound);
}

// Sub-manifest with "includes" field returns error
TEST_F(IdentityRelationshipExhaustive, ManifestSerializer_SubManifestWithIncludes_ReturnsError)
{
	if (!EnsureTestTempDir()) GTEST_SKIP() << "C:\\Temp not accessible";

	const char* subPath = "C:\\Temp\\test_sub_nested_include.json";
	const char* masterPath = "C:\\Temp\\test_master_nested_include.json";

	// Sub-manifest has its own "includes" field — forbidden
	const char* subJson =
		"{\n"
		"  \"includes\": [\"something.json\"],\n"
		"  \"assets\": []\n"
		"}";

	const char* masterJson =
		"{\n"
		"  \"includes\": [\"test_sub_nested_include.json\"],\n"
		"  \"assets\": []\n"
		"}";

	ASSERT_TRUE(WriteTestFileHelper(subPath, subJson));
	ASSERT_TRUE(WriteTestFileHelper(masterPath, masterJson));

	Dia::AssetCatalogue::CatalogueManifestSerializer serializer;
	auto result = serializer.LoadManifest(masterPath);

	EXPECT_FALSE(result.mSuccess);
	EXPECT_TRUE(result.HasErrors());

	remove(subPath);
	remove(masterPath);
}

// Duplicate ID across includes returns error
TEST_F(IdentityRelationshipExhaustive, ManifestSerializer_DuplicateIdAcrossIncludes_ReturnsError)
{
	if (!EnsureTestTempDir()) GTEST_SKIP() << "C:\\Temp not accessible";

	const char* subPath = "C:\\Temp\\test_sub_dup_id.json";
	const char* masterPath = "C:\\Temp\\test_master_dup_id.json";

	// Sub-manifest has a record with the same ID as master
	const char* subJson =
		"{\n"
		"  \"assets\": [\n"
		"    { \"id\": \"texture.shared\", \"type\": \"texture\", \"source_path\": \"a.png\", "
		"\"content_hash\": 1, \"status\": \"active\", \"scope\": \"global\", \"stage_name\": \"\", "
		"\"tags\": [], \"references\": [] }\n"
		"  ]\n"
		"}";

	const char* masterJson =
		"{\n"
		"  \"includes\": [\"test_sub_dup_id.json\"],\n"
		"  \"assets\": [\n"
		"    { \"id\": \"texture.shared\", \"type\": \"texture\", \"source_path\": \"b.png\", "
		"\"content_hash\": 2, \"status\": \"active\", \"scope\": \"global\", \"stage_name\": \"\", "
		"\"tags\": [], \"references\": [] }\n"
		"  ]\n"
		"}";

	ASSERT_TRUE(WriteTestFileHelper(subPath, subJson));
	ASSERT_TRUE(WriteTestFileHelper(masterPath, masterJson));

	Dia::AssetCatalogue::CatalogueManifestSerializer serializer;
	auto result = serializer.LoadManifest(masterPath);

	EXPECT_FALSE(result.mSuccess);
	EXPECT_TRUE(result.HasErrors());

	remove(subPath);
	remove(masterPath);
}

// Empty registry save produces valid JSON that loads back to count==0
TEST_F(IdentityRelationshipExhaustive, ManifestSerializer_EmptyRegistry_SaveAndLoadBack)
{
	if (!EnsureTestTempDir()) GTEST_SKIP() << "C:\\Temp not accessible";

	const char* testPath = "C:\\Temp\\test_empty_registry.catalogue.json";

	Dia::AssetCatalogue::AssetRegistry emptyReg;
	Dia::AssetCatalogue::CatalogueManifestSerializer serializer;
	ASSERT_TRUE(serializer.SaveManifest(emptyReg, testPath));

	auto loaded = serializer.LoadManifest(testPath);
	EXPECT_TRUE(loaded.mSuccess);
	EXPECT_EQ(loaded.mValue.GetCount(), 0u);

	remove(testPath);
}

// ===========================================================================
// FEATURE 4 — Catalogue Automation Exhaustive
// ===========================================================================
class CatalogueAutomationExhaustive : public ::testing::Test
{
protected:
	void SetUp() override
	{
		EnsureTestTempDir();
	}
};

// ContentHasher — hash of empty file returns 0 (fileSize <= 0 path)
TEST_F(CatalogueAutomationExhaustive, ContentHasher_EmptyFile_ReturnsZero)
{
	const char* path = "C:\\Temp\\test_empty_file_hash.txt";
	ASSERT_TRUE(WriteTestFileHelper(path, "", 0));

	Dia::AssetCatalogue::ContentHasher hasher;
	unsigned int hash = hasher.ComputeHash(path);
	EXPECT_EQ(hash, 0u);

	remove(path);
}

// ContentHasher — same file hashed twice returns same value (determinism)
TEST_F(CatalogueAutomationExhaustive, ContentHasher_Determinism_SameFileTwice)
{
	const char* path = "C:\\Temp\\test_determinism_hash.txt";
	ASSERT_TRUE(WriteTestFileHelper(path, "determinism check content"));

	Dia::AssetCatalogue::ContentHasher hasher;
	unsigned int hash1 = hasher.ComputeHash(path);
	unsigned int hash2 = hasher.ComputeHash(path);

	EXPECT_EQ(hash1, hash2);
	EXPECT_NE(hash1, 0u);

	remove(path);
}

// ContentHasher — modifying file content changes hash
TEST_F(CatalogueAutomationExhaustive, ContentHasher_ModifiedContent_ChangesHash)
{
	const char* path = "C:\\Temp\\test_modified_hash.txt";
	ASSERT_TRUE(WriteTestFileHelper(path, "original content"));

	Dia::AssetCatalogue::ContentHasher hasher;
	unsigned int hash1 = hasher.ComputeHash(path);

	ASSERT_TRUE(WriteTestFileHelper(path, "modified content"));
	unsigned int hash2 = hasher.ComputeHash(path);

	EXPECT_NE(hash1, hash2);

	remove(path);
}

// ScopeComputer — stage record itself returns kGlobal (skipped)
TEST_F(CatalogueAutomationExhaustive, ScopeComputer_StageRecordItself_ReturnsGlobal)
{
	Dia::AssetCatalogue::AssetRegistry registry;

	auto stage = MakeTestRecord("stage.gameplay", "stage");
	registry.Register(stage);

	Dia::AssetCatalogue::ScopeComputer computer;
	Dia::Core::StringCRC outStageName;
	Dia::AssetCatalogue::AssetScope scope = computer.ComputeScope(
		Dia::Core::StringCRC("stage.gameplay"),
		registry,
		registry.GetRelationshipIndex(),
		outStageName);

	EXPECT_EQ(scope, Dia::AssetCatalogue::AssetScope::kGlobal);
}

// ScopeComputer — single stage ref returns stage name
TEST_F(CatalogueAutomationExhaustive, ScopeComputer_SingleStage_StageName)
{
	Dia::AssetCatalogue::AssetRegistry registry;

	auto stage = MakeTestRecord("stage.level-two", "stage");
	stage.mReferences.Add(Dia::AssetCatalogue::RelationshipEdge(
		Dia::AssetCatalogue::RelationshipTypes::kContains,
		Dia::Core::StringCRC("texture.bg")));
	registry.Register(stage);
	registry.Register(MakeTestRecord("texture.bg", "texture"));

	Dia::AssetCatalogue::ScopeComputer computer;
	Dia::Core::StringCRC outStageName;
	Dia::AssetCatalogue::AssetScope scope = computer.ComputeScope(
		Dia::Core::StringCRC("texture.bg"),
		registry,
		registry.GetRelationshipIndex(),
		outStageName);

	EXPECT_EQ(scope, Dia::AssetCatalogue::AssetScope::kStage);
	EXPECT_EQ(outStageName, Dia::Core::StringCRC("stage.level-two"));
}

// CatalogueRulesEngine — empty rules file loads successfully
TEST_F(CatalogueAutomationExhaustive, RulesEngine_EmptyRulesArray_LoadsSuccessfully)
{
	const char* rulesPath = "C:\\Temp\\test_empty_rules.json";
	const char* rulesJson = "{ \"rules\": [] }";
	ASSERT_TRUE(WriteTestFileHelper(rulesPath, rulesJson));

	Dia::AssetCatalogue::AssetTypeRegistry typeRegistry;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(typeRegistry);

	Dia::AssetCatalogue::CatalogueRulesEngine engine;
	auto result = engine.LoadRules(rulesPath, typeRegistry);

	EXPECT_TRUE(result.mSuccess);
	EXPECT_EQ(engine.GetRuleCount(), 0u);

	remove(rulesPath);
}

// CatalogueRulesEngine — dry-run on empty registry returns empty changeset
TEST_F(CatalogueAutomationExhaustive, RulesEngine_DryRun_EmptyRegistry_EmptyChangeset)
{
	const char* rulesPath = "C:\\Temp\\test_dryrun_empty_reg.json";
	const char* rulesJson =
		"{ \"rules\": [ { \"name\": \"tag-all\", \"match\": { \"all\": true }, \"action\": \"assign_tag\", \"tag\": \"auto\" } ] }";
	ASSERT_TRUE(WriteTestFileHelper(rulesPath, rulesJson));

	Dia::AssetCatalogue::AssetTypeRegistry typeRegistry;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(typeRegistry);

	Dia::AssetCatalogue::CatalogueRulesEngine engine;
	engine.LoadRules(rulesPath, typeRegistry);

	Dia::AssetCatalogue::AssetRegistry registry;  // empty
	Dia::AssetCatalogue::RuleChangeset changeset =
		engine.EvaluateDryRun(registry, registry.GetRelationshipIndex());

	EXPECT_EQ(changeset.mChanges.Size(), 0u);

	remove(rulesPath);
}

// CatalogueRulesEngine — assign_tag without "tag" field returns error
TEST_F(CatalogueAutomationExhaustive, RulesEngine_AssignTag_MissingTagField_ReturnsError)
{
	const char* rulesPath = "C:\\Temp\\test_missing_tag.json";
	const char* rulesJson =
		"{ \"rules\": [ { \"name\": \"bad-tag\", \"match\": { \"all\": true }, \"action\": \"assign_tag\" } ] }";
	ASSERT_TRUE(WriteTestFileHelper(rulesPath, rulesJson));

	Dia::AssetCatalogue::AssetTypeRegistry typeRegistry;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(typeRegistry);

	Dia::AssetCatalogue::CatalogueRulesEngine engine;
	auto result = engine.LoadRules(rulesPath, typeRegistry);

	EXPECT_FALSE(result.mSuccess);
	EXPECT_TRUE(result.HasErrors());
	EXPECT_EQ(engine.GetRuleCount(), 0u);

	remove(rulesPath);
}

// CatalogueRulesEngine — match "tag" matches only records with that tag
TEST_F(CatalogueAutomationExhaustive, RulesEngine_MatchTag_MatchesOnlyTaggedRecords)
{
	const char* rulesPath = "C:\\Temp\\test_match_tag.json";
	const char* rulesJson =
		"{ \"rules\": [ { \"name\": \"scope-visual\", \"match\": { \"tag\": \"visual\" }, \"action\": \"assign_tag\", \"tag\": \"processed\" } ] }";
	ASSERT_TRUE(WriteTestFileHelper(rulesPath, rulesJson));

	Dia::AssetCatalogue::AssetTypeRegistry typeRegistry;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(typeRegistry);

	Dia::AssetCatalogue::CatalogueRulesEngine engine;
	engine.LoadRules(rulesPath, typeRegistry);

	Dia::AssetCatalogue::AssetRegistry registry;
	auto r1 = MakeTestRecord("texture.hero", "texture");
	r1.mTags.Add(Dia::Core::StringCRC("visual"));
	registry.Register(r1);
	registry.Register(MakeTestRecord("config.game", "config")); // no tag

	auto changeset = engine.EvaluateDryRun(registry, registry.GetRelationshipIndex());

	// Only texture.hero should have a change
	EXPECT_EQ(changeset.mChanges.Size(), 1u);
	EXPECT_EQ(changeset.mChanges[0].mRecordId, Dia::Core::StringCRC("texture.hero"));

	remove(rulesPath);
}

// CatalogueRulesEngine — match "stage_ref_count" with min/max range
TEST_F(CatalogueAutomationExhaustive, RulesEngine_MatchStageRefCount_MinMax)
{
	const char* rulesPath = "C:\\Temp\\test_stage_ref_count.json";
	const char* rulesJson =
		"{ \"rules\": [ { \"name\": \"single-stage\", \"match\": { \"stage_ref_count\": { \"min\": 1, \"max\": 1 } }, \"action\": \"assign_tag\", \"tag\": \"scoped\" } ] }";
	ASSERT_TRUE(WriteTestFileHelper(rulesPath, rulesJson));

	Dia::AssetCatalogue::AssetTypeRegistry typeRegistry;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(typeRegistry);

	Dia::AssetCatalogue::CatalogueRulesEngine engine;
	engine.LoadRules(rulesPath, typeRegistry);

	Dia::AssetCatalogue::AssetRegistry registry;

	// stage.gameplay contains texture.a only
	auto stage = MakeTestRecord("stage.gameplay", "stage");
	stage.mReferences.Add(Dia::AssetCatalogue::RelationshipEdge(
		Dia::AssetCatalogue::RelationshipTypes::kContains,
		Dia::Core::StringCRC("texture.a")));
	registry.Register(stage);

	registry.Register(MakeTestRecord("texture.a", "texture"));
	registry.Register(MakeTestRecord("texture.b", "texture")); // no stage ref

	auto changeset = engine.EvaluateDryRun(registry, registry.GetRelationshipIndex());

	// Only texture.a has exactly 1 stage ref — should match
	bool foundA = false;
	bool foundB = false;
	for (unsigned int i = 0; i < changeset.mChanges.Size(); ++i)
	{
		if (changeset.mChanges[i].mRecordId == Dia::Core::StringCRC("texture.a"))
			foundA = true;
		if (changeset.mChanges[i].mRecordId == Dia::Core::StringCRC("texture.b"))
			foundB = true;
	}
	EXPECT_TRUE(foundA);
	EXPECT_FALSE(foundB);

	remove(rulesPath);
}

// Snapshot semantics — rule B matching on tag added by rule A in same pass does NOT see it
TEST_F(CatalogueAutomationExhaustive, RulesEngine_SnapshotSemantics_RuleBDoesNotSeeRuleATag)
{
	const char* rulesPath = "C:\\Temp\\test_snapshot_semantics.json";
	// Rule A: assign tag "intermediate" to all records
	// Rule B: match tag "intermediate" -> assign tag "final"
	// With snapshot semantics, rule B should NOT match because it uses snapshot (before rule A applied)
	const char* rulesJson =
		"{ \"rules\": [\n"
		"  { \"name\": \"add-intermediate\", \"match\": { \"all\": true }, \"action\": \"assign_tag\", \"tag\": \"intermediate\" },\n"
		"  { \"name\": \"add-final\", \"match\": { \"tag\": \"intermediate\" }, \"action\": \"assign_tag\", \"tag\": \"final\" }\n"
		"] }";
	ASSERT_TRUE(WriteTestFileHelper(rulesPath, rulesJson));

	Dia::AssetCatalogue::AssetTypeRegistry typeRegistry;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(typeRegistry);

	Dia::AssetCatalogue::CatalogueRulesEngine engine;
	engine.LoadRules(rulesPath, typeRegistry);

	Dia::AssetCatalogue::AssetRegistry registry;
	registry.Register(MakeTestRecord("texture.hero", "texture")); // no tags initially

	auto changeset = engine.EvaluateDryRun(registry, registry.GetRelationshipIndex());

	// Should only have "intermediate" tag addition, NOT "final"
	bool foundIntermediate = false;
	bool foundFinal = false;
	for (unsigned int i = 0; i < changeset.mChanges.Size(); ++i)
	{
		if (strcmp(changeset.mChanges[i].mNewValue.AsCStr(), "intermediate") == 0)
			foundIntermediate = true;
		if (strcmp(changeset.mChanges[i].mNewValue.AsCStr(), "final") == 0)
			foundFinal = true;
	}
	EXPECT_TRUE(foundIntermediate);
	EXPECT_FALSE(foundFinal);

	remove(rulesPath);
}

// ===========================================================================
// INTEGRATION TESTS (across features)
// ===========================================================================
class AssetCatalogueIntegrationExhaustive : public ::testing::Test
{
protected:
	void SetUp() override
	{
		EnsureTestTempDir();
	}
};

// Integration: register built-in types, create records, query by type, save, load, verify
TEST_F(AssetCatalogueIntegrationExhaustive, BuiltInTypes_RegisterRecords_SaveLoad_QueryByType)
{
	if (!EnsureTestTempDir()) GTEST_SKIP() << "C:\\Temp not accessible";

	const char* path = "C:\\Temp\\test_integration_types.catalogue.json";

	// Register built-in types
	Dia::AssetCatalogue::AssetTypeRegistry typeReg;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(typeReg);

	// Create records using built-in type IDs
	Dia::AssetCatalogue::AssetRegistry registry;
	registry.Register(MakeTestRecord("texture.hero", "texture"));
	registry.Register(MakeTestRecord("texture.enemy", "texture"));
	registry.Register(MakeTestRecord("sprite.hero-idle", "sprite"));
	registry.Register(MakeTestRecord("config.game-settings", "config"));

	// Query by type
	Dia::Core::Containers::DynamicArrayC<const Dia::AssetCatalogue::AssetRecord*, 64> textures;
	registry.QueryByType(Dia::Core::StringCRC("texture"), textures);
	EXPECT_EQ(textures.Size(), 2u);

	// Save manifest
	Dia::AssetCatalogue::CatalogueManifestSerializer serializer;
	ASSERT_TRUE(serializer.SaveManifest(registry, path));

	// Load manifest back
	auto loaded = serializer.LoadManifest(path);
	ASSERT_TRUE(loaded.mSuccess);
	EXPECT_EQ(loaded.mValue.GetCount(), 4u);

	// Verify record types preserved
	const Dia::AssetCatalogue::AssetRecord* hero =
		loaded.mValue.FindById(Dia::Core::StringCRC("texture.hero"));
	ASSERT_NE(hero, nullptr);
	EXPECT_EQ(hero->mAssetTypeId, Dia::Core::StringCRC("texture"));

	const Dia::AssetCatalogue::AssetRecord* sprite =
		loaded.mValue.FindById(Dia::Core::StringCRC("sprite.hero-idle"));
	ASSERT_NE(sprite, nullptr);
	EXPECT_EQ(sprite->mAssetTypeId, Dia::Core::StringCRC("sprite"));

	remove(path);
}

// Integration: RelationshipInferrer with AssetRecord overload returns empty (placeholder)
TEST_F(AssetCatalogueIntegrationExhaustive, RelationshipInferrer_RecordOverload_ReturnsEmpty)
{
	Dia::AssetCatalogue::AssetTypeRegistry typeReg;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(typeReg);

	auto record = MakeTestRecord("sprite.hero-idle", "sprite");

	Dia::AssetCatalogue::RelationshipInferrer inferrer;
	Dia::Core::Containers::DynamicArrayC<Dia::AssetCatalogue::RelationshipEdge, 16> edges;
	inferrer.InferRelationships(record, typeReg, edges);

	// Record overload is placeholder — returns empty
	EXPECT_EQ(edges.Size(), 0u);
}

// Integration: RelationshipInferrer with TypeInstance reads asset reference field
TEST_F(AssetCatalogueIntegrationExhaustive, RelationshipInferrer_TypeInstance_InfersUsesEdge)
{
	Dia::Core::Types::TypeRegistry& reg = Dia::Core::Types::GetTypeFacade().Registry();
	EnsureTypeRegistered<AssetRefStruct>(reg);

	// Register AssetRefStruct as a custom type in AssetTypeRegistry
	Dia::AssetCatalogue::AssetTypeRegistry typeReg;

	Dia::AssetCatalogue::AssetTypeDescriptor desc;
	desc.mTypeId = Dia::Core::StringCRC("assetref");
	desc.mName = Dia::Core::Containers::String64("AssetRef Test");
	desc.mFilePattern = Dia::Core::Containers::String64("*.assetref.json");
	desc.mTypeDefinition = AssetRefStruct::GetTypeStatic();
	typeReg.Register(desc);

	// Create an instance with a reference value
	AssetRefStruct instance;
	strcpy_s(instance.mReferencedAsset, sizeof(instance.mReferencedAsset), "texture.hero");

	Dia::Core::Types::TypeInstance typeInst(AssetRefStruct::GetTypeStatic(), &instance);

	Dia::AssetCatalogue::RelationshipInferrer inferrer;
	Dia::Core::Containers::DynamicArrayC<Dia::AssetCatalogue::RelationshipEdge, 16> edges;
	inferrer.InferRelationships(typeInst, Dia::Core::StringCRC("assetref"), typeReg, edges);

	EXPECT_EQ(edges.Size(), 1u);
	EXPECT_EQ(edges[0].mRelationshipType, Dia::AssetCatalogue::RelationshipTypes::kUses);
	EXPECT_EQ(edges[0].mTargetAssetId, Dia::Core::StringCRC("texture.hero"));
}

// Integration: Full pipeline — LoadRules -> EvaluateDryRun -> Apply -> SaveManifest -> LoadManifest
TEST_F(AssetCatalogueIntegrationExhaustive, FullPipeline_Rules_Apply_SaveLoad_Verify)
{
	if (!EnsureTestTempDir()) GTEST_SKIP() << "C:\\Temp not accessible";

	const char* rulesPath = "C:\\Temp\\test_full_pipeline_rules.json";
	const char* manifestPath = "C:\\Temp\\test_full_pipeline.catalogue.json";

	// Create rules: tag all textures with "visual"
	const char* rulesJson =
		"{ \"rules\": [ { \"name\": \"tag-textures\", \"match\": { \"type\": \"texture\" }, \"action\": \"assign_tag\", \"tag\": \"visual\" } ] }";
	ASSERT_TRUE(WriteTestFileHelper(rulesPath, rulesJson));

	Dia::AssetCatalogue::AssetTypeRegistry typeReg;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(typeReg);

	Dia::AssetCatalogue::CatalogueRulesEngine engine;
	auto loadResult = engine.LoadRules(rulesPath, typeReg);
	ASSERT_TRUE(loadResult.mSuccess);

	Dia::AssetCatalogue::AssetRegistry registry;
	registry.Register(MakeTestRecord("texture.hero", "texture"));
	registry.Register(MakeTestRecord("config.game", "config"));

	// Dry-run — no mutation
	auto dryRun = engine.EvaluateDryRun(registry, registry.GetRelationshipIndex());
	EXPECT_GE(dryRun.mChanges.Size(), 1u);
	EXPECT_EQ(registry.FindById(Dia::Core::StringCRC("texture.hero"))->mTags.Size(), 0u);

	// Apply — mutation occurs
	auto applied = engine.Apply(registry, registry.GetRelationshipIndex());
	EXPECT_GE(applied.mChanges.Size(), 1u);
	EXPECT_EQ(registry.FindById(Dia::Core::StringCRC("texture.hero"))->mTags.Size(), 1u);
	EXPECT_EQ(registry.FindById(Dia::Core::StringCRC("texture.hero"))->mTags[0],
		Dia::Core::StringCRC("visual"));

	// config should not be tagged
	EXPECT_EQ(registry.FindById(Dia::Core::StringCRC("config.game"))->mTags.Size(), 0u);

	// Save
	Dia::AssetCatalogue::CatalogueManifestSerializer serializer;
	ASSERT_TRUE(serializer.SaveManifest(registry, manifestPath));

	// Load back
	auto loaded = serializer.LoadManifest(manifestPath);
	ASSERT_TRUE(loaded.mSuccess);
	EXPECT_EQ(loaded.mValue.GetCount(), 2u);

	const Dia::AssetCatalogue::AssetRecord* heroLoaded =
		loaded.mValue.FindById(Dia::Core::StringCRC("texture.hero"));
	ASSERT_NE(heroLoaded, nullptr);
	EXPECT_EQ(heroLoaded->mTags.Size(), 1u);
	EXPECT_EQ(heroLoaded->mTags[0], Dia::Core::StringCRC("visual"));

	remove(rulesPath);
	remove(manifestPath);
}

// Integration: ScopeComputer + CatalogueRulesEngine compute_scope action
TEST_F(AssetCatalogueIntegrationExhaustive, ScopeComputer_RulesEngine_ComputeScope_SetsStage)
{
	if (!EnsureTestTempDir()) GTEST_SKIP() << "C:\\Temp not accessible";

	const char* rulesPath = "C:\\Temp\\test_compute_scope_rules.json";
	const char* rulesJson =
		"{ \"rules\": [ { \"name\": \"auto-scope\", \"match\": { \"all\": true }, \"action\": \"compute_scope\" } ] }";
	ASSERT_TRUE(WriteTestFileHelper(rulesPath, rulesJson));

	Dia::AssetCatalogue::AssetTypeRegistry typeReg;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(typeReg);

	Dia::AssetCatalogue::CatalogueRulesEngine engine;
	engine.LoadRules(rulesPath, typeReg);

	Dia::AssetCatalogue::AssetRegistry registry;

	// stage.level1 contains texture.hero
	auto stage = MakeTestRecord("stage.level1", "stage");
	stage.mReferences.Add(Dia::AssetCatalogue::RelationshipEdge(
		Dia::AssetCatalogue::RelationshipTypes::kContains,
		Dia::Core::StringCRC("texture.hero")));
	registry.Register(stage);

	registry.Register(MakeTestRecord("texture.hero", "texture"));
	registry.Register(MakeTestRecord("texture.global-bg", "texture")); // not contained by any stage

	// Apply rules
	auto changeset = engine.Apply(registry, registry.GetRelationshipIndex());

	// texture.hero should now be stage-scoped
	const Dia::AssetCatalogue::AssetRecord* hero =
		registry.FindById(Dia::Core::StringCRC("texture.hero"));
	ASSERT_NE(hero, nullptr);
	EXPECT_EQ(hero->mScope, Dia::AssetCatalogue::AssetScope::kStage);

	// texture.global-bg should remain global
	const Dia::AssetCatalogue::AssetRecord* bg =
		registry.FindById(Dia::Core::StringCRC("texture.global-bg"));
	ASSERT_NE(bg, nullptr);
	EXPECT_EQ(bg->mScope, Dia::AssetCatalogue::AssetScope::kGlobal);

	remove(rulesPath);
}

// Integration: ContentHasher preserves through manifest round-trip
TEST_F(AssetCatalogueIntegrationExhaustive, ContentHasher_ManifestRoundTrip_PreservesHash)
{
	if (!EnsureTestTempDir()) GTEST_SKIP() << "C:\\Temp not accessible";

	const char* dataPath = "C:\\Temp\\test_hash_data.txt";
	const char* manifestPath = "C:\\Temp\\test_hash_manifest.catalogue.json";

	ASSERT_TRUE(WriteTestFileHelper(dataPath, "some asset data for hashing"));

	// Compute hash
	Dia::AssetCatalogue::ContentHasher hasher;
	unsigned int computedHash = hasher.ComputeHash(dataPath);
	EXPECT_NE(computedHash, 0u);

	// Create registry with the hash
	Dia::AssetCatalogue::AssetRegistry registry;
	auto record = MakeTestRecord("texture.data", "texture");
	record.mContentHash = computedHash;
	registry.Register(record);

	// Save and load
	Dia::AssetCatalogue::CatalogueManifestSerializer serializer;
	ASSERT_TRUE(serializer.SaveManifest(registry, manifestPath));

	auto loaded = serializer.LoadManifest(manifestPath);
	ASSERT_TRUE(loaded.mSuccess);

	const Dia::AssetCatalogue::AssetRecord* loadedRec =
		loaded.mValue.FindById(Dia::Core::StringCRC("texture.data"));
	ASSERT_NE(loadedRec, nullptr);
	EXPECT_EQ(loadedRec->mContentHash, computedHash);

	remove(dataPath);
	remove(manifestPath);
}

// Integration: source_path_glob match criterion
TEST_F(AssetCatalogueIntegrationExhaustive, RulesEngine_SourcePathGlob_MatchesSubstring)
{
	if (!EnsureTestTempDir()) GTEST_SKIP() << "C:\\Temp not accessible";

	const char* rulesPath = "C:\\Temp\\test_glob_rules.json";
	const char* rulesJson =
		"{ \"rules\": [ { \"name\": \"tag-textures-dir\", \"match\": { \"source_path_glob\": \"**/Textures/**\" }, \"action\": \"assign_tag\", \"tag\": \"in-textures-dir\" } ] }";
	ASSERT_TRUE(WriteTestFileHelper(rulesPath, rulesJson));

	Dia::AssetCatalogue::AssetTypeRegistry typeReg;
	Dia::AssetCatalogue::RegisterBuiltInAssetTypes(typeReg);

	Dia::AssetCatalogue::CatalogueRulesEngine engine;
	engine.LoadRules(rulesPath, typeReg);

	Dia::AssetCatalogue::AssetRegistry registry;
	registry.Register(MakeTestRecord("texture.hero", "texture", "Raw/Textures/hero.png"));
	registry.Register(MakeTestRecord("config.game", "config", "Raw/Config/game.json"));

	auto changeset = engine.Apply(registry, registry.GetRelationshipIndex());

	// texture.hero path contains "Textures" so should match
	const Dia::AssetCatalogue::AssetRecord* hero =
		registry.FindById(Dia::Core::StringCRC("texture.hero"));
	ASSERT_NE(hero, nullptr);
	EXPECT_EQ(hero->mTags.Size(), 1u);

	// config.game should NOT match
	const Dia::AssetCatalogue::AssetRecord* cfg =
		registry.FindById(Dia::Core::StringCRC("config.game"));
	ASSERT_NE(cfg, nullptr);
	EXPECT_EQ(cfg->mTags.Size(), 0u);

	remove(rulesPath);
}

// ---------------------------------------------------------------------------
// rules_path: round-trip through serializer (load manifest with rules_path,
// verify accessor, save, reload, verify preserved)
// ---------------------------------------------------------------------------
TEST_F(AssetCatalogueIntegrationExhaustive, ManifestSerializer_RulesPath_RoundTrip)
{
	if (!EnsureTestTempDir()) GTEST_SKIP() << "C:\\Temp not accessible";

	const char* manifestPath = "C:\\Temp\\test_rulespath.catalogue.json";
	const char* manifestJson =
		"{ \"version\": 1, \"rules_path\": \"my.rules.json\", \"assets\": ["
		"  { \"id\": \"texture.hero\", \"type\": \"texture\", \"source_path\": \"hero.png\" }"
		"]}";
	ASSERT_TRUE(WriteTestFileHelper(manifestPath, manifestJson));

	Dia::AssetCatalogue::CatalogueManifestSerializer serializer;

	// Load
	auto result = serializer.LoadManifest(manifestPath);
	ASSERT_TRUE(result.mSuccess);
	EXPECT_TRUE(serializer.HasRulesPath());
	EXPECT_STREQ(serializer.GetRulesPath(), "my.rules.json");
	EXPECT_EQ(result.mValue.GetCount(), 1u);

	// Save
	const char* outPath = "C:\\Temp\\test_rulespath_out.catalogue.json";
	ASSERT_TRUE(serializer.SaveManifest(result.mValue, outPath));

	// Reload and verify round-trip
	Dia::AssetCatalogue::CatalogueManifestSerializer serializer2;
	auto result2 = serializer2.LoadManifest(outPath);
	ASSERT_TRUE(result2.mSuccess);
	EXPECT_TRUE(serializer2.HasRulesPath());
	EXPECT_STREQ(serializer2.GetRulesPath(), "my.rules.json");
	EXPECT_EQ(result2.mValue.GetCount(), 1u);

	remove(manifestPath);
	remove(outPath);
}

// rules_path: manifest without rules_path has no rules path set
TEST_F(AssetCatalogueIntegrationExhaustive, ManifestSerializer_NoRulesPath_HasRulesPathFalse)
{
	if (!EnsureTestTempDir()) GTEST_SKIP() << "C:\\Temp not accessible";

	const char* manifestPath = "C:\\Temp\\test_norulespath.catalogue.json";
	const char* manifestJson =
		"{ \"version\": 1, \"assets\": ["
		"  { \"id\": \"config.main\", \"type\": \"config\", \"source_path\": \"main.json\" }"
		"]}";
	ASSERT_TRUE(WriteTestFileHelper(manifestPath, manifestJson));

	Dia::AssetCatalogue::CatalogueManifestSerializer serializer;
	auto result = serializer.LoadManifest(manifestPath);
	ASSERT_TRUE(result.mSuccess);
	EXPECT_FALSE(serializer.HasRulesPath());
	EXPECT_STREQ(serializer.GetRulesPath(), "");

	remove(manifestPath);
}

// rules_path: SetRulesPath updates the stored path and is written on save
TEST_F(AssetCatalogueIntegrationExhaustive, ManifestSerializer_SetRulesPath_PersistsOnSave)
{
	if (!EnsureTestTempDir()) GTEST_SKIP() << "C:\\Temp not accessible";

	const char* manifestPath = "C:\\Temp\\test_setrulespath.catalogue.json";
	const char* manifestJson =
		"{ \"version\": 1, \"assets\": ["
		"  { \"id\": \"texture.a\", \"type\": \"texture\", \"source_path\": \"a.png\" }"
		"]}";
	ASSERT_TRUE(WriteTestFileHelper(manifestPath, manifestJson));

	Dia::AssetCatalogue::CatalogueManifestSerializer serializer;
	auto result = serializer.LoadManifest(manifestPath);
	ASSERT_TRUE(result.mSuccess);
	EXPECT_FALSE(serializer.HasRulesPath());

	// User browses and sets a rules path
	serializer.SetRulesPath("custom.rules.json");
	EXPECT_TRUE(serializer.HasRulesPath());
	EXPECT_STREQ(serializer.GetRulesPath(), "custom.rules.json");

	// Save and reload
	const char* outPath = "C:\\Temp\\test_setrulespath_out.catalogue.json";
	ASSERT_TRUE(serializer.SaveManifest(result.mValue, outPath));

	Dia::AssetCatalogue::CatalogueManifestSerializer serializer2;
	auto result2 = serializer2.LoadManifest(outPath);
	ASSERT_TRUE(result2.mSuccess);
	EXPECT_TRUE(serializer2.HasRulesPath());
	EXPECT_STREQ(serializer2.GetRulesPath(), "custom.rules.json");

	remove(manifestPath);
	remove(outPath);
}

// rules_path: manifest with invalid rules_path loads fine (rules loading is caller's concern)
TEST_F(AssetCatalogueIntegrationExhaustive, ManifestSerializer_InvalidRulesPath_LoadSucceeds)
{
	if (!EnsureTestTempDir()) GTEST_SKIP() << "C:\\Temp not accessible";

	const char* manifestPath = "C:\\Temp\\test_badrulespath.catalogue.json";
	const char* manifestJson =
		"{ \"version\": 1, \"rules_path\": \"nonexistent.rules.json\", \"assets\": ["
		"  { \"id\": \"texture.b\", \"type\": \"texture\", \"source_path\": \"b.png\" }"
		"]}";
	ASSERT_TRUE(WriteTestFileHelper(manifestPath, manifestJson));

	Dia::AssetCatalogue::CatalogueManifestSerializer serializer;
	auto result = serializer.LoadManifest(manifestPath);

	// Manifest load itself succeeds — rules_path is just metadata
	ASSERT_TRUE(result.mSuccess);
	EXPECT_TRUE(serializer.HasRulesPath());
	EXPECT_STREQ(serializer.GetRulesPath(), "nonexistent.rules.json");

	// Attempting to load the rules file would fail, but that's the caller's job
	Dia::AssetCatalogue::AssetTypeRegistry typeReg;
	Dia::AssetCatalogue::CatalogueRulesEngine engine;
	auto lr = engine.LoadRules("C:\\Temp\\nonexistent.rules.json", typeReg);
	EXPECT_FALSE(lr.mSuccess);

	remove(manifestPath);
}
