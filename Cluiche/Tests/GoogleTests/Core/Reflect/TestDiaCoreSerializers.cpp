// =============================================================================
// TestDiaCoreSerializers.cpp
// Tests for the DiaCore String types and PathStoreConfig serialization.
//
// Covers: String8/32/64/128 JSON round-trips, binary round-trips,
//         empty string, capacity truncation, String-in-struct (DIA_SERIALIZE),
//         PathStoreConfig types, and missing-field default preservation.
// Archives: JsonWriteArchive/JsonReadArchive, BinaryWriteArchive/BinaryReadArchive
// =============================================================================

#include <gtest/gtest.h>
#include "DiaCore/Reflect/Reflect.h"
#include "DiaCore/Reflect/JsonArchive.h"
#include "DiaCore/Reflect/BinaryArchive.h"
#include "DiaCore/Reflect/DiaCoreSerializers.h"
#include "DiaCore/Json/external/json/json.h"

using namespace Dia::Reflect;
using namespace Dia::Core::Containers;

// =============================================================================
// Helpers — mirrors the pattern from TestDiaMathsSerializers
// =============================================================================

template<typename T>
static Json::Value WriteToJson(T& obj) {
    JsonWriteArchive ar;
    serialize(ar, obj, 0u);
    return ar.GetRoot();
}

template<typename T>
static void ReadFromJson(const Json::Value& root, T& obj) {
    JsonReadArchive ar(root);
    serialize(ar, obj, 0u);
}

template<typename T>
static BinaryReadArchive WriteAndReadBinary(const T& src, T& dst) {
    static BinaryWriteArchive wAr;
    wAr = BinaryWriteArchive{};
    wAr.WriteVersion(1u);
    T& mutableSrc = const_cast<T&>(src);
    serialize(wAr, mutableSrc, 1u);

    BinaryReadArchive rAr(wAr.GetData(), wAr.GetSize());
    rAr.ReadVersion();
    serialize(rAr, dst, 1u);
    return rAr;
}

// =============================================================================
// Minimal wrapper structs for round-tripping bare string values through
// the archive (archives operate on named fields inside an object, so we wrap).
// =============================================================================

struct WrapString8 {
    String8 mValue;
};

template<class Archive>
void serialize(Archive& ar, WrapString8& obj, unsigned = 1u) {
    ar & Dia::Reflect::named("mValue", obj.mValue);
}

struct WrapString32 {
    String32 mValue;
};

template<class Archive>
void serialize(Archive& ar, WrapString32& obj, unsigned = 1u) {
    ar & Dia::Reflect::named("mValue", obj.mValue);
}

struct WrapString64 {
    String64 mValue;
};

template<class Archive>
void serialize(Archive& ar, WrapString64& obj, unsigned = 1u) {
    ar & Dia::Reflect::named("mValue", obj.mValue);
}

struct WrapString128 {
    String128 mValue;
};

template<class Archive>
void serialize(Archive& ar, WrapString128& obj, unsigned = 1u) {
    ar & Dia::Reflect::named("mValue", obj.mValue);
}

// =============================================================================
// A) String8 JSON round-trip
// =============================================================================

TEST(DiaCoreSerializer_String8, JsonRoundTrip) {
    WrapString8 src;
    src.mValue = String8("hello");

    Json::Value root = WriteToJson(src);

    WrapString8 dst;
    ReadFromJson(root, dst);

    EXPECT_STREQ(dst.mValue.AsCStr(), "hello");
}

// =============================================================================
// B) String32 JSON round-trip
// =============================================================================

TEST(DiaCoreSerializer_String32, JsonRoundTrip) {
    WrapString32 src;
    src.mValue = String32("test string");

    Json::Value root = WriteToJson(src);

    WrapString32 dst;
    ReadFromJson(root, dst);

    EXPECT_STREQ(dst.mValue.AsCStr(), "test string");
}

// =============================================================================
// C) String64 JSON round-trip
// =============================================================================

TEST(DiaCoreSerializer_String64, JsonRoundTrip) {
    WrapString64 src;
    src.mValue = String64("hello from String64");

    Json::Value root = WriteToJson(src);

    WrapString64 dst;
    ReadFromJson(root, dst);

    EXPECT_STREQ(dst.mValue.AsCStr(), "hello from String64");
}

// =============================================================================
// D) String128 JSON round-trip
// =============================================================================

TEST(DiaCoreSerializer_String128, JsonRoundTrip) {
    WrapString128 src;
    src.mValue = String128("a longer string value for String128");

    Json::Value root = WriteToJson(src);

    WrapString128 dst;
    ReadFromJson(root, dst);

    EXPECT_STREQ(dst.mValue.AsCStr(), "a longer string value for String128");
}

// =============================================================================
// E) String8 binary round-trip
// =============================================================================

TEST(DiaCoreSerializer_String8, BinaryRoundTrip) {
    WrapString8 src;
    src.mValue = String8("world");
    WrapString8 dst;

    WriteAndReadBinary(src, dst);

    EXPECT_STREQ(dst.mValue.AsCStr(), "world");
}

// =============================================================================
// F) String64 binary round-trip
// =============================================================================

TEST(DiaCoreSerializer_String64, BinaryRoundTrip) {
    WrapString64 src;
    src.mValue = String64("binary test");
    WrapString64 dst;

    WriteAndReadBinary(src, dst);

    EXPECT_STREQ(dst.mValue.AsCStr(), "binary test");
}

// =============================================================================
// G) Empty string round-trip (JSON)
// =============================================================================

TEST(DiaCoreSerializer_String32, EmptyStringJsonRoundTrip) {
    WrapString32 src;
    src.mValue = String32("");

    Json::Value root = WriteToJson(src);

    WrapString32 dst;
    dst.mValue = String32("non-empty");  // set to non-empty to confirm overwrite
    ReadFromJson(root, dst);

    EXPECT_STREQ(dst.mValue.AsCStr(), "");
}

// =============================================================================
// H) String at capacity — String8 holds 7 usable chars (capacity=8, null=1)
//    Writing a 7-char string should survive a round-trip intact.
//    If source JSON has a longer value the String constructor will truncate.
// =============================================================================

TEST(DiaCoreSerializer_String8, AtCapacityRoundTrip) {
    WrapString8 src;
    src.mValue = String8("1234567");  // 7 chars, fits in String8 (capacity 8)

    Json::Value root = WriteToJson(src);
    ASSERT_TRUE(root["mValue"].isString());
    EXPECT_STREQ(root["mValue"].asCString(), "1234567");

    WrapString8 dst;
    ReadFromJson(root, dst);
    EXPECT_STREQ(dst.mValue.AsCStr(), "1234567");
}

// =============================================================================
// I) String used as field in a struct with DIA_SERIALIZE
// =============================================================================

namespace DiaCoreSerializerTest {
struct Named {
    String64 mName;
    int mId = 0;
};

template<class Archive>
void serialize(Archive& ar, Named& obj, unsigned = 1u) {
    ar & Dia::Reflect::named("mName", obj.mName);
    ar & Dia::Reflect::named("mId",   obj.mId);
}
}  // namespace DiaCoreSerializerTest

TEST(DiaCoreSerializer_InStruct, JsonRoundTrip) {
    DiaCoreSerializerTest::Named src;
    src.mName = String64("PlayerOne");
    src.mId = 42;

    Json::Value root = WriteToJson(src);

    DiaCoreSerializerTest::Named dst;
    ReadFromJson(root, dst);

    EXPECT_STREQ(dst.mName.AsCStr(), "PlayerOne");
    EXPECT_EQ(dst.mId, 42);
}

TEST(DiaCoreSerializer_InStruct, BinaryRoundTrip) {
    DiaCoreSerializerTest::Named src;
    src.mName = String64("PlayerTwo");
    src.mId = 99;
    DiaCoreSerializerTest::Named dst;

    WriteAndReadBinary(src, dst);

    EXPECT_STREQ(dst.mName.AsCStr(), "PlayerTwo");
    EXPECT_EQ(dst.mId, 99);
}

// =============================================================================
// J) PathStoreConfig types — JSON round-trip
// =============================================================================

TEST(DiaCoreSerializer_AliasPathConfigTuple, JsonRoundTrip) {
    Dia::Core::AliasPathConfigTuple src;
    // Use serialize to write fields — need a write-capable path
    // The serialize function writes mAlias and mPath; test via full round-trip.

    // Build via JSON directly to set values (fields are private; use write path)
    JsonWriteArchive wAr;
    // Inject test data by constructing a JSON object manually
    Json::Value manualRoot;
    manualRoot["mAlias"] = "data";
    manualRoot["mPath"]  = "C:/Game/Assets";

    Dia::Core::AliasPathConfigTuple dst;
    JsonReadArchive rAr(manualRoot);
    serialize(rAr, dst, 1u);

    EXPECT_STREQ(dst.GetAlias().AsCStr(), "data");
    EXPECT_STREQ(dst.GetPath().AsCStr(), "C:/Game/Assets");

    // Now write it back and verify the JSON produced matches
    Json::Value outRoot = WriteToJson(dst);
    EXPECT_STREQ(outRoot["mAlias"].asCString(), "data");
    EXPECT_STREQ(outRoot["mPath"].asCString(), "C:/Game/Assets");
}

TEST(DiaCoreSerializer_AliasAppendPathConfig, JsonRoundTrip) {
    Json::Value manualRoot;
    manualRoot["mAlias"]      = "textures";
    manualRoot["mBaseAlias"]  = "data";
    manualRoot["mPathAppend"] = "Textures";

    Dia::Core::AliasAppendPathConfig dst;
    JsonReadArchive rAr(manualRoot);
    serialize(rAr, dst, 1u);

    EXPECT_STREQ(dst.GetAlias().AsCStr(), "textures");
    EXPECT_STREQ(dst.GetBaseAlias().AsCStr(), "data");
    EXPECT_STREQ(dst.GetPathAppend().AsCStr(), "Textures");

    Json::Value outRoot = WriteToJson(dst);
    EXPECT_STREQ(outRoot["mAlias"].asCString(), "textures");
    EXPECT_STREQ(outRoot["mBaseAlias"].asCString(), "data");
    EXPECT_STREQ(outRoot["mPathAppend"].asCString(), "Textures");
}

TEST(DiaCoreSerializer_PathStoreConfigFragment, JsonRoundTrip) {
    Json::Value manualRoot;
    manualRoot["mBaseAlias"]  = "config";
    manualRoot["mFileName"]   = "paths_extra.json";
    manualRoot["mPathAppend"] = "SubDir";

    Dia::Core::PathStoreConfigFragment dst;
    JsonReadArchive rAr(manualRoot);
    serialize(rAr, dst, 1u);

    EXPECT_STREQ(dst.GetBaseAlias().AsCStr(), "config");
    EXPECT_STREQ(dst.GetFileName().AsCStr(),  "paths_extra.json");
    EXPECT_STREQ(dst.GetPathAppend().AsCStr(), "SubDir");
}

TEST(DiaCoreSerializer_PathStoreConfig, JsonRoundTrip) {
    // Build a PathStoreConfig JSON and deserialize it
    Json::Value tupleEntry;
    tupleEntry["mAlias"] = "assets";
    tupleEntry["mPath"]  = "C:/Assets";

    Json::Value appendEntry;
    appendEntry["mAlias"]      = "audio";
    appendEntry["mBaseAlias"]  = "assets";
    appendEntry["mPathAppend"] = "Audio";

    Json::Value root;
    root["mAliasPathTupleArray"]    = Json::Value(Json::arrayValue);
    root["mAliasPathTupleArray"].append(tupleEntry);
    root["mAliasAppendPathArray"]   = Json::Value(Json::arrayValue);
    root["mAliasAppendPathArray"].append(appendEntry);
    root["mPathStoreConfigFragmentArray"] = Json::Value(Json::arrayValue);

    Dia::Core::PathStoreConfig config;
    JsonReadArchive rAr(root);
    serialize(rAr, config, 1u);

    ASSERT_EQ(config.GetAliasPathTupleArray().Size(), 1u);
    EXPECT_STREQ(config.GetAliasPathTupleArray().At(0).GetAlias().AsCStr(), "assets");
    EXPECT_STREQ(config.GetAliasPathTupleArray().At(0).GetPath().AsCStr(), "C:/Assets");

    ASSERT_EQ(config.GetAliasAppendPathTupleArray().Size(), 1u);
    EXPECT_STREQ(config.GetAliasAppendPathTupleArray().At(0).GetAlias().AsCStr(), "audio");

    EXPECT_EQ(config.GetPathStoreConfigFragmentArray().Size(), 0u);
}

// =============================================================================
// K) Missing string field in JSON keeps default (empty string)
// =============================================================================

TEST(DiaCoreSerializer_String64, MissingFieldKeepsDefault) {
    Json::Value partial;
    // "mValue" intentionally absent

    WrapString64 dst;
    dst.mValue = String64("original");  // pre-set to detect no-overwrite
    ReadFromJson(partial, dst);

    // Field was absent — value should stay at "original" (optional field)
    EXPECT_STREQ(dst.mValue.AsCStr(), "original");
}
