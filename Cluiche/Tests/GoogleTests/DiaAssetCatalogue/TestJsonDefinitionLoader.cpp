#include <gtest/gtest.h>

#include <DiaCore/Type/TypeDeclarationMacros.h>
#include <DiaCore/Type/TypeDefinitionMacros.h>
#include <DiaCore/Type/TypeFacade.h>
#include <DiaCore/Type/TypeVariableAttributes.h>
#include <DiaCore/Containers/Strings/StringWriter.h>
#include <DiaCore/Containers/Strings/StringReader.h>
#include <DiaCore/CRC/CRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <DiaAssetCatalogue/LoadResult.h>
#include <DiaAssetCatalogue/JsonDefinitionLoader.h>

// ---------------------------------------------------------------------------
// Test structs — defined in their own namespace to avoid name collisions
// ---------------------------------------------------------------------------
namespace JsonDefLoaderTests
{
	// Simple struct used for round-trip and basic tests
	struct SimpleAsset
	{
		DIA_TYPE_DECLARATION;
		int mHealth = 0;
		float mSpeed = 0.0f;
	};

	// Struct with a required field
	struct RequiredFieldAsset
	{
		DIA_TYPE_DECLARATION;
		int mId = 0;
		float mValue = 0.0f;
	};

	// Type definitions must be in the same namespace
	DIA_TYPE_DEFINITION(SimpleAsset)
		DIA_TYPE_ADD_VARIABLE("mHealth", mHealth)
		DIA_TYPE_ADD_VARIABLE("mSpeed", mSpeed)
	DIA_TYPE_DEFINITION_END()

	DIA_TYPE_DEFINITION(RequiredFieldAsset)
		DIA_TYPE_ADD_VARIABLE("mId", mId)
			DIA_TYPE_ADD_VARIABLE_ATTRIBUTE(Dia::Core::Types::TypeVariableAttributeRequired)
		DIA_TYPE_ADD_VARIABLE("mValue", mValue)
	DIA_TYPE_DEFINITION_END()

} // namespace JsonDefLoaderTests

using namespace JsonDefLoaderTests;

// ---------------------------------------------------------------------------
// Helper: register a type into the global TypeFacade registry (idempotent)
// ---------------------------------------------------------------------------
template<typename T>
static void EnsureRegistered(Dia::Core::Types::TypeRegistry& reg)
{
	Dia::Core::Types::TypeDefinition* def = T::GetTypeStatic();
	Dia::Core::CRC crc(def->GetUniqueCRC());
	if (!reg.ContainsType(crc))
	{
		reg.Add(def);
	}
}

// ---------------------------------------------------------------------------
// Test 1: Successful load from buffer — deserialize a simple struct
// ---------------------------------------------------------------------------
TEST(JsonDefinitionLoader, LoadFromBuffer_Success_SimpleStruct)
{
	Dia::Core::Types::TypeRegistry& reg = Dia::Core::Types::GetTypeFacade().Registry();
	EnsureRegistered<SimpleAsset>(reg);

	// Serialize a known value
	SimpleAsset source;
	source.mHealth = 42;
	source.mSpeed = 3.14f;

	char bufferMemory[8 * 1024];
	Dia::Core::Containers::StringWriter writer(bufferMemory, 8 * 1024);
	Dia::Core::Types::GetTypeFacade().JsonSerializer().Serialize(source, writer);

	// Load it back via JsonDefinitionLoader
	Dia::Core::Containers::StringReader reader(bufferMemory);
	Dia::AssetCatalogue::JsonDefinitionLoader loader(reg);

	auto result = loader.LoadFromBuffer<SimpleAsset>(reader);

	EXPECT_TRUE(result.mSuccess);
	EXPECT_FALSE(result.HasErrors());
	EXPECT_EQ(result.mValue.mHealth, 42);
	EXPECT_FLOAT_EQ(result.mValue.mSpeed, 3.14f);
}

// ---------------------------------------------------------------------------
// Test 2: Load from buffer — basic round-trip (alias of test 1, distinct path)
// ---------------------------------------------------------------------------
TEST(JsonDefinitionLoader, LoadFromBuffer_RoundTrip_PreservesValues)
{
	Dia::Core::Types::TypeRegistry& reg = Dia::Core::Types::GetTypeFacade().Registry();
	EnsureRegistered<SimpleAsset>(reg);

	SimpleAsset source;
	source.mHealth = 100;
	source.mSpeed = 7.5f;

	char bufferMemory[8 * 1024];
	Dia::Core::Containers::StringWriter writer(bufferMemory, 8 * 1024);
	Dia::Core::Types::GetTypeFacade().JsonSerializer().Serialize(source, writer);

	Dia::Core::Containers::StringReader reader(bufferMemory);
	Dia::AssetCatalogue::JsonDefinitionLoader loader(reg);

	auto result = loader.LoadFromBuffer<SimpleAsset>(reader);

	EXPECT_TRUE(result.mSuccess);
	EXPECT_EQ(result.mValue.mHealth, 100);
	EXPECT_FLOAT_EQ(result.mValue.mSpeed, 7.5f);
}

// ---------------------------------------------------------------------------
// Test 3: Malformed JSON returns JsonParseError
// ---------------------------------------------------------------------------
TEST(JsonDefinitionLoader, LoadFromBuffer_MalformedJson_ReturnsParseError)
{
	Dia::Core::Types::TypeRegistry& reg = Dia::Core::Types::GetTypeFacade().Registry();
	EnsureRegistered<SimpleAsset>(reg);

	const char* badJson = "{ this is not valid json !!!";
	Dia::Core::Containers::StringReader reader(badJson);
	Dia::AssetCatalogue::JsonDefinitionLoader loader(reg);

	auto result = loader.LoadFromBuffer<SimpleAsset>(reader);

	EXPECT_FALSE(result.mSuccess);
	EXPECT_TRUE(result.HasErrors());
	EXPECT_EQ(result.GetFirstError().mKind, Dia::AssetCatalogue::LoadErrorKind::JsonParseError);
}

// ---------------------------------------------------------------------------
// Test 4: Type not registered returns TypeNotRegistered
// ---------------------------------------------------------------------------
TEST(JsonDefinitionLoader, LoadFromBuffer_TypeNotRegistered_ReturnsError)
{
	// Use a fresh local registry with no types registered
	Dia::Core::Types::TypeRegistry emptyRegistry;

	const char* anyJson = "{}";
	Dia::Core::Containers::StringReader reader(anyJson);
	Dia::AssetCatalogue::JsonDefinitionLoader loader(emptyRegistry);

	auto result = loader.LoadFromBuffer<SimpleAsset>(reader);

	EXPECT_FALSE(result.mSuccess);
	EXPECT_TRUE(result.HasErrors());
	EXPECT_EQ(result.GetFirstError().mKind, Dia::AssetCatalogue::LoadErrorKind::TypeNotRegistered);
}

// ---------------------------------------------------------------------------
// Test 5: Missing required field returns MissingRequiredField
// ---------------------------------------------------------------------------
TEST(JsonDefinitionLoader, LoadFromBuffer_MissingRequiredField_ReturnsError)
{
	Dia::Core::Types::TypeRegistry& reg = Dia::Core::Types::GetTypeFacade().Registry();
	EnsureRegistered<RequiredFieldAsset>(reg);

	// Serialize a complete RequiredFieldAsset so we get valid JSON,
	// then manually strip the required field out.
	RequiredFieldAsset source;
	source.mId = 5;
	source.mValue = 1.0f;

	char bufferMemory[8 * 1024];
	Dia::Core::Containers::StringWriter writer(bufferMemory, 8 * 1024);
	Dia::Core::Types::GetTypeFacade().JsonSerializer().Serialize(source, writer);

	// Parse the serialized JSON and remove the required field
	Json::Value root;
	Json::Reader jsonReader;
	jsonReader.parse(bufferMemory, root, false);
	root.removeMember("mId");

	Json::FastWriter fastWriter;
	std::string modifiedJson = fastWriter.write(root);

	Dia::Core::Containers::StringReader reader(modifiedJson.c_str());
	Dia::AssetCatalogue::JsonDefinitionLoader loader(reg);

	auto result = loader.LoadFromBuffer<RequiredFieldAsset>(reader);

	EXPECT_FALSE(result.mSuccess);
	EXPECT_TRUE(result.HasErrors());
	EXPECT_EQ(result.GetFirstError().mKind, Dia::AssetCatalogue::LoadErrorKind::MissingRequiredField);
}

// ---------------------------------------------------------------------------
// Test 6: Missing file returns FileNotFound
// ---------------------------------------------------------------------------
TEST(JsonDefinitionLoader, Load_FileNotFound_ReturnsError)
{
	Dia::Core::Types::TypeRegistry& reg = Dia::Core::Types::GetTypeFacade().Registry();
	EnsureRegistered<SimpleAsset>(reg);

	// Use a FilePath with an absolute path that doesn't exist
	// FilePath requires alias-based construction; we use an empty alias path approach.
	// Since FilePath::Resolve looks up aliases and we may not have the aliases set up,
	// we pass a path that will never resolve to a real file.
	// The simplest way: use a raw path via the load function by passing a nonexistent alias.
	// However FilePath constructor requires a Path::Alias — let's use Load via the
	// public API with a non-existent file path that we create using Path::Alias.

	// We can't easily inject a direct path without PathStore. Instead, test through Load()
	// by checking that a FilePath that resolves to a non-existent file returns FileNotFound.
	// Since PathStore may not have any aliases registered in test context, Resolve() returns
	// an empty/invalid string, and fopen on that will fail with FileNotFound.
	Dia::Core::FilePath::ResoledFilePath resolvedBuf;
	Dia::Core::FilePath nonExistentPath;
	// The default-constructed FilePath resolves to an empty string, which fopen fails on.

	Dia::AssetCatalogue::JsonDefinitionLoader loader(reg);
	auto result = loader.Load<SimpleAsset>(nonExistentPath);

	EXPECT_FALSE(result.mSuccess);
	EXPECT_TRUE(result.HasErrors());
	EXPECT_EQ(result.GetFirstError().mKind, Dia::AssetCatalogue::LoadErrorKind::FileNotFound);
}
