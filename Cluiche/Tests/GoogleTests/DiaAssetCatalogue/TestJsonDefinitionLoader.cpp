#include <gtest/gtest.h>

#include <DiaCore/Reflect/ReflectMacros.h>
#include <DiaCore/Reflect/JsonArchive.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/Containers/Strings/StringWriter.h>
#include <DiaCore/Containers/Strings/StringReader.h>

#include <DiaAssetCatalogue/LoadResult.h>
#include <DiaAssetCatalogue/JsonDefinitionLoader.h>

// ---------------------------------------------------------------------------
// Test structs
// ---------------------------------------------------------------------------
namespace JsonDefLoaderTests
{
	struct SimpleAsset
	{
		int mHealth = 0;
		float mSpeed = 0.0f;
	};

	struct RequiredFieldAsset
	{
		int mId = 0;
		float mValue = 0.0f;
	};
}

namespace JsonDefLoaderTests
{
	DIA_SERIALIZE(SimpleAsset, 1)
		DIA_FIELD(mHealth)
		DIA_FIELD(mSpeed)
	DIA_SERIALIZE_END

	DIA_SERIALIZE(RequiredFieldAsset, 1)
		DIA_FIELD_REQUIRED(mId)
		DIA_FIELD(mValue)
	DIA_SERIALIZE_END
}

using namespace JsonDefLoaderTests;

// Helper: serialize an object to JSON string via DiaReflect
template<typename T>
static std::string SerializeToJson(const T& obj)
{
	Dia::Reflect::JsonWriteArchive ar;
	T& mutableObj = const_cast<T&>(obj);
	serialize(ar, mutableObj, 0u);
	Json::FastWriter writer;
	return writer.write(ar.GetRoot());
}

// ---------------------------------------------------------------------------
// Test 1: Successful load from buffer
// ---------------------------------------------------------------------------
TEST(JsonDefinitionLoader, LoadFromBuffer_Success_SimpleStruct)
{
	SimpleAsset source;
	source.mHealth = 42;
	source.mSpeed = 3.14f;

	std::string json = SerializeToJson(source);

	Dia::Core::Containers::StringReader reader(json.c_str());
	Dia::AssetCatalogue::JsonDefinitionLoader loader;

	auto result = loader.LoadFromBuffer<SimpleAsset>(reader);

	EXPECT_TRUE(result.mSuccess);
	EXPECT_FALSE(result.HasErrors());
	EXPECT_EQ(result.mValue.mHealth, 42);
	EXPECT_FLOAT_EQ(result.mValue.mSpeed, 3.14f);
}

// ---------------------------------------------------------------------------
// Test 2: Round-trip preserves values
// ---------------------------------------------------------------------------
TEST(JsonDefinitionLoader, LoadFromBuffer_RoundTrip_PreservesValues)
{
	SimpleAsset source;
	source.mHealth = 100;
	source.mSpeed = 7.5f;

	std::string json = SerializeToJson(source);

	Dia::Core::Containers::StringReader reader(json.c_str());
	Dia::AssetCatalogue::JsonDefinitionLoader loader;

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
	const char* badJson = "{ this is not valid json !!!";
	Dia::Core::Containers::StringReader reader(badJson);
	Dia::AssetCatalogue::JsonDefinitionLoader loader;

	auto result = loader.LoadFromBuffer<SimpleAsset>(reader);

	EXPECT_FALSE(result.mSuccess);
	EXPECT_TRUE(result.HasErrors());
	EXPECT_EQ(result.GetFirstError().mKind, Dia::AssetCatalogue::LoadErrorKind::JsonParseError);
}

// ---------------------------------------------------------------------------
// Test 4: Missing required field returns MissingRequiredField
// ---------------------------------------------------------------------------
TEST(JsonDefinitionLoader, LoadFromBuffer_MissingRequiredField_ReturnsError)
{
	// Build JSON missing the required "mId" field
	Json::Value root;
	root["mValue"] = 1.0f;
	Json::FastWriter fastWriter;
	std::string json = fastWriter.write(root);

	Dia::Core::Containers::StringReader reader(json.c_str());
	Dia::AssetCatalogue::JsonDefinitionLoader loader;

	auto result = loader.LoadFromBuffer<RequiredFieldAsset>(reader);

	EXPECT_FALSE(result.mSuccess);
	EXPECT_TRUE(result.HasErrors());
	EXPECT_EQ(result.GetFirstError().mKind, Dia::AssetCatalogue::LoadErrorKind::MissingRequiredField);
}

// ---------------------------------------------------------------------------
// Test 5: Missing optional field uses default value
// ---------------------------------------------------------------------------
TEST(JsonDefinitionLoader, LoadFromBuffer_MissingOptionalField_UsesDefault)
{
	// Only provide mHealth, omit mSpeed
	Json::Value root;
	root["mHealth"] = 77;
	Json::FastWriter fastWriter;
	std::string json = fastWriter.write(root);

	Dia::Core::Containers::StringReader reader(json.c_str());
	Dia::AssetCatalogue::JsonDefinitionLoader loader;

	auto result = loader.LoadFromBuffer<SimpleAsset>(reader);

	EXPECT_TRUE(result.mSuccess);
	EXPECT_EQ(result.mValue.mHealth, 77);
	EXPECT_FLOAT_EQ(result.mValue.mSpeed, 0.0f);  // default
}

// ---------------------------------------------------------------------------
// Test 6: Missing file returns FileNotFound
// ---------------------------------------------------------------------------
TEST(JsonDefinitionLoader, Load_FileNotFound_ReturnsError)
{
	Dia::Core::FilePath nonExistentPath;
	Dia::AssetCatalogue::JsonDefinitionLoader loader;
	auto result = loader.Load<SimpleAsset>(nonExistentPath);

	EXPECT_FALSE(result.mSuccess);
	EXPECT_TRUE(result.HasErrors());
	EXPECT_EQ(result.GetFirstError().mKind, Dia::AssetCatalogue::LoadErrorKind::FileNotFound);
}
