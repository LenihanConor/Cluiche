#include <gtest/gtest.h>
#include "DiaCore/Reflect/ReflectMacros.h"
#include "DiaCore/Reflect/JsonArchive.h"
#include "DiaCore/Reflect/SerializeResult.h"
#include "DiaCore/Json/external/json/json.h"

using namespace Dia::Reflect;

// =============================================================================
// Test structs — must be at file scope (DIA_SERIALIZE defines a free function template)
// =============================================================================

struct Vec2Json {
    float mX = 0.0f;
    float mY = 0.0f;
};

DIA_SERIALIZE(Vec2Json, 1)
    DIA_FIELD(mX)
    DIA_FIELD(mY)
DIA_SERIALIZE_END

struct ParticleJson {
    Vec2Json mPos{1.0f, 2.0f};
    float    mMass   = 1.0f;
    bool     mActive = true;
    int      mId     = 42;
};

DIA_SERIALIZE(ParticleJson, 1)
    DIA_FIELD(mPos)
    DIA_FIELD(mMass)
    DIA_FIELD(mActive)
    DIA_FIELD(mId)
DIA_SERIALIZE_END

struct WithRequiredJson {
    int   mId    = 0;
    float mValue = 0.0f;
};

DIA_SERIALIZE(WithRequiredJson, 1)
    DIA_FIELD_REQUIRED(mId)
    DIA_FIELD(mValue)
DIA_SERIALIZE_END

struct AllPrimitivesJson {
    bool          mBool         = false;
    int           mInt          = 0;
    unsigned int  mUInt         = 0u;
    float         mFloat        = 0.0f;
    double        mDouble       = 0.0;
    char          mChar         = 0;
    unsigned char mUChar        = 0;
    short         mShort        = 0;
    unsigned short mUShort      = 0;
    int64_t       mInt64        = 0;
    uint64_t      mUInt64       = 0u;
};

DIA_SERIALIZE(AllPrimitivesJson, 1)
    DIA_FIELD(mBool)
    DIA_FIELD(mInt)
    DIA_FIELD(mUInt)
    DIA_FIELD(mFloat)
    DIA_FIELD(mDouble)
    DIA_FIELD(mChar)
    DIA_FIELD(mUChar)
    DIA_FIELD(mShort)
    DIA_FIELD(mUShort)
    DIA_FIELD(mInt64)
    DIA_FIELD(mUInt64)
DIA_SERIALIZE_END

// =============================================================================
// Helper: write a struct to JSON root, then produce a string
// =============================================================================
template<typename T>
Json::Value WriteToJson(T& obj) {
    JsonWriteArchive ar;
    serialize(ar, obj, 0u);
    return ar.GetRoot();
}

template<typename T>
void ReadFromJson(const Json::Value& root, T& obj) {
    JsonReadArchive ar(root);
    serialize(ar, obj, 0u);
}

// =============================================================================
// A) Round-trip primitives
// =============================================================================

TEST(JsonArchive_RoundTrip, Bool) {
    AllPrimitivesJson src;
    src.mBool = true;
    Json::Value root = WriteToJson(src);

    AllPrimitivesJson dst;
    dst.mBool = false;
    ReadFromJson(root, dst);

    EXPECT_EQ(dst.mBool, true);
}

TEST(JsonArchive_RoundTrip, Int) {
    AllPrimitivesJson src;
    src.mInt = -12345;
    Json::Value root = WriteToJson(src);

    AllPrimitivesJson dst;
    dst.mInt = 0;
    ReadFromJson(root, dst);

    EXPECT_EQ(dst.mInt, -12345);
}

TEST(JsonArchive_RoundTrip, UnsignedInt) {
    AllPrimitivesJson src;
    src.mUInt = 99999u;
    Json::Value root = WriteToJson(src);

    AllPrimitivesJson dst;
    dst.mUInt = 0u;
    ReadFromJson(root, dst);

    EXPECT_EQ(dst.mUInt, 99999u);
}

TEST(JsonArchive_RoundTrip, Float) {
    AllPrimitivesJson src;
    src.mFloat = 1.5f;
    Json::Value root = WriteToJson(src);

    AllPrimitivesJson dst;
    dst.mFloat = 0.0f;
    ReadFromJson(root, dst);

    EXPECT_NEAR(dst.mFloat, 1.5f, 0.0001f);
}

TEST(JsonArchive_RoundTrip, Double) {
    AllPrimitivesJson src;
    src.mDouble = 2.718281828;
    Json::Value root = WriteToJson(src);

    AllPrimitivesJson dst;
    dst.mDouble = 0.0;
    ReadFromJson(root, dst);

    EXPECT_NEAR(dst.mDouble, 2.718281828, 1e-9);
}

TEST(JsonArchive_RoundTrip, Char) {
    AllPrimitivesJson src;
    src.mChar = 'Z';
    Json::Value root = WriteToJson(src);

    AllPrimitivesJson dst;
    dst.mChar = 0;
    ReadFromJson(root, dst);

    EXPECT_EQ(dst.mChar, 'Z');
}

TEST(JsonArchive_RoundTrip, UnsignedChar) {
    AllPrimitivesJson src;
    src.mUChar = 200;
    Json::Value root = WriteToJson(src);

    AllPrimitivesJson dst;
    dst.mUChar = 0;
    ReadFromJson(root, dst);

    EXPECT_EQ(dst.mUChar, 200u);
}

TEST(JsonArchive_RoundTrip, Short) {
    AllPrimitivesJson src;
    src.mShort = -300;
    Json::Value root = WriteToJson(src);

    AllPrimitivesJson dst;
    dst.mShort = 0;
    ReadFromJson(root, dst);

    EXPECT_EQ(dst.mShort, -300);
}

TEST(JsonArchive_RoundTrip, UnsignedShort) {
    AllPrimitivesJson src;
    src.mUShort = 500;
    Json::Value root = WriteToJson(src);

    AllPrimitivesJson dst;
    dst.mUShort = 0;
    ReadFromJson(root, dst);

    EXPECT_EQ(dst.mUShort, 500u);
}

// =============================================================================
// B) Round-trip nested struct (Vec2Json inside ParticleJson)
// =============================================================================

TEST(JsonArchive_RoundTrip, NestedStruct_ProducesSubObject) {
    ParticleJson src;
    src.mPos.mX = 3.0f;
    src.mPos.mY = 4.0f;
    Json::Value root = WriteToJson(src);

    // JSON should have "mPos" as a nested object with "mX" and "mY"
    ASSERT_TRUE(root.isMember("mPos"));
    EXPECT_TRUE(root["mPos"].isObject());
    EXPECT_TRUE(root["mPos"].isMember("mX"));
    EXPECT_TRUE(root["mPos"].isMember("mY"));
}

TEST(JsonArchive_RoundTrip, NestedStruct_ValuesRoundTrip) {
    ParticleJson src;
    src.mPos.mX = 3.0f;
    src.mPos.mY = 4.0f;
    src.mMass   = 2.5f;
    src.mActive = false;
    src.mId     = 7;
    Json::Value root = WriteToJson(src);

    ParticleJson dst;
    dst.mPos.mX = 0.0f;
    dst.mPos.mY = 0.0f;
    dst.mMass   = 0.0f;
    dst.mActive = true;
    dst.mId     = 0;
    ReadFromJson(root, dst);

    EXPECT_NEAR(dst.mPos.mX, 3.0f, 0.0001f);
    EXPECT_NEAR(dst.mPos.mY, 4.0f, 0.0001f);
    EXPECT_NEAR(dst.mMass,   2.5f, 0.0001f);
    EXPECT_EQ(dst.mActive, false);
    EXPECT_EQ(dst.mId, 7);
}

// =============================================================================
// C) Missing optional field keeps C++ default
// =============================================================================

TEST(JsonArchive_OptionalField, MissingFieldKeepsDefault) {
    // JSON that only has mPos, mActive, mId — no mMass
    Json::Value partial(Json::objectValue);
    partial["mPos"] = Json::Value(Json::objectValue);
    partial["mPos"]["mX"] = 0.0f;
    partial["mPos"]["mY"] = 0.0f;
    partial["mActive"] = true;
    partial["mId"] = 42;
    // mMass is intentionally absent

    ParticleJson dst;
    dst.mMass = 1.0f; // C++ default from member initializer
    ReadFromJson(partial, dst);

    EXPECT_NEAR(dst.mMass, 1.0f, 0.0001f); // unchanged
}

// =============================================================================
// D) Required field present — no error
// =============================================================================

TEST(JsonArchive_RequiredField, PresentProducesNoError) {
    Json::Value root(Json::objectValue);
    root["mId"]    = 99;
    root["mValue"] = 3.14f;

    WithRequiredJson obj;
    JsonReadArchive ar(root);
    serialize(ar, obj, 0u);

    EXPECT_TRUE(ar.GetResult().IsOk());
    EXPECT_EQ(obj.mId, 99);
}

// =============================================================================
// E) Required field missing — SerializeResult has 1 error
// =============================================================================

TEST(JsonArchive_RequiredField, MissingProducesError) {
    Json::Value root(Json::objectValue);
    root["mValue"] = 3.14f;
    // mId is intentionally absent

    WithRequiredJson obj;
    JsonReadArchive ar(root);
    serialize(ar, obj, 0u);

    EXPECT_FALSE(ar.GetResult().IsOk());
    EXPECT_EQ(ar.GetResult().ErrorCount(), 1u);
    EXPECT_EQ(ar.GetResult().GetError(0).kind, SerializeErrorKind::RequiredFieldMissing);
    EXPECT_EQ(ar.GetResult().GetError(0).fieldName, Dia::Core::StringCRC("mId"));
}

// =============================================================================
// F) Write produces correct key names
// =============================================================================

TEST(JsonArchive_Write, KeyNamesMatchMemberNames) {
    Vec2Json v;
    v.mX = 1.0f;
    v.mY = 2.0f;
    Json::Value root = WriteToJson(v);

    // Keys must be "mX" and "mY" (exact string from DIA_FIELD stringification)
    EXPECT_TRUE(root.isMember("mX"));
    EXPECT_TRUE(root.isMember("mY"));
    EXPECT_FALSE(root.isMember("x"));
    EXPECT_FALSE(root.isMember("y"));
}

// =============================================================================
// G) Write then read — perfect round-trip via WriteArchive/ReadArchive
// =============================================================================

TEST(JsonArchive_RoundTrip, WriteAndReadBackVec2) {
    Vec2Json src;
    src.mX = 7.7f;
    src.mY = -3.3f;

    Json::Value root = WriteToJson(src);

    Vec2Json dst;
    dst.mX = 0.0f;
    dst.mY = 0.0f;
    ReadFromJson(root, dst);

    EXPECT_NEAR(dst.mX, 7.7f, 0.0001f);
    EXPECT_NEAR(dst.mY, -3.3f, 0.0001f);
}

// =============================================================================
// H) Float precision round-trip
// =============================================================================

TEST(JsonArchive_RoundTrip, FloatPrecision) {
    AllPrimitivesJson src;
    src.mFloat = 3.14159265f;
    Json::Value root = WriteToJson(src);

    AllPrimitivesJson dst;
    dst.mFloat = 0.0f;
    ReadFromJson(root, dst);

    EXPECT_NEAR(dst.mFloat, 3.14159265f, 0.0001f);
}

// =============================================================================
// I) Unknown key in JSON — silently ignored, no error, no crash
// =============================================================================

TEST(JsonArchive_UnknownKey, SilentlyIgnored) {
    Json::Value root(Json::objectValue);
    root["mX"]          = 1.0f;
    root["mY"]          = 2.0f;
    root["unknownKey"]  = 42;
    root["anotherKey"]  = "hello";

    Vec2Json dst;
    JsonReadArchive ar(root);
    serialize(ar, dst, 0u);

    EXPECT_TRUE(ar.GetResult().IsOk());
    EXPECT_NEAR(dst.mX, 1.0f, 0.0001f);
    EXPECT_NEAR(dst.mY, 2.0f, 0.0001f);
}

// =============================================================================
// J) Read from empty JSON object — all fields keep defaults
// =============================================================================

TEST(JsonArchive_EmptyJson, AllFieldsKeepDefaults) {
    Json::Value emptyRoot(Json::objectValue);

    ParticleJson dst;
    // Keep the C++ defaults
    dst.mPos.mX = 1.0f;  // default from struct definition
    dst.mPos.mY = 2.0f;
    dst.mMass   = 1.0f;
    dst.mActive = true;
    dst.mId     = 42;

    JsonReadArchive ar(emptyRoot);
    serialize(ar, dst, 0u);

    EXPECT_TRUE(ar.GetResult().IsOk());
    EXPECT_NEAR(dst.mPos.mX, 1.0f, 0.0001f);
    EXPECT_NEAR(dst.mPos.mY, 2.0f, 0.0001f);
    EXPECT_NEAR(dst.mMass,   1.0f, 0.0001f);
    EXPECT_EQ(dst.mActive, true);
    EXPECT_EQ(dst.mId, 42);
}

// =============================================================================
// K) Nested struct missing in JSON — parent struct keeps nested struct defaults
// =============================================================================

TEST(JsonArchive_MissingNestedStruct, NestedStructKeepsDefaults) {
    Json::Value root(Json::objectValue);
    root["mMass"]   = 5.0f;
    root["mActive"] = false;
    root["mId"]     = 10;
    // mPos is intentionally absent

    ParticleJson dst;
    dst.mPos.mX = 1.0f;   // defaults
    dst.mPos.mY = 2.0f;

    ReadFromJson(root, dst);

    EXPECT_NEAR(dst.mPos.mX, 1.0f, 0.0001f);
    EXPECT_NEAR(dst.mPos.mY, 2.0f, 0.0001f);
    EXPECT_NEAR(dst.mMass,   5.0f, 0.0001f);
}

// =============================================================================
// L) int64_t and uint64_t fields round-trip
// =============================================================================

TEST(JsonArchive_RoundTrip, Int64) {
    AllPrimitivesJson src;
    src.mInt64 = -9000000000LL;
    Json::Value root = WriteToJson(src);

    AllPrimitivesJson dst;
    dst.mInt64 = 0;
    ReadFromJson(root, dst);

    EXPECT_EQ(dst.mInt64, -9000000000LL);
}

TEST(JsonArchive_RoundTrip, UInt64) {
    AllPrimitivesJson src;
    src.mUInt64 = 18000000000ULL;
    Json::Value root = WriteToJson(src);

    AllPrimitivesJson dst;
    dst.mUInt64 = 0u;
    ReadFromJson(root, dst);

    EXPECT_EQ(dst.mUInt64, 18000000000ULL);
}

// =============================================================================
// M) Write produces valid JSON parseable by Json::Reader
// =============================================================================

TEST(JsonArchive_Write, ProducesValidJson) {
    ParticleJson src;
    src.mPos.mX = 1.0f;
    src.mPos.mY = 2.0f;
    src.mMass   = 3.0f;
    src.mActive = true;
    src.mId     = 5;

    JsonWriteArchive writeAr;
    serialize(writeAr, src, 0u);

    Json::StyledWriter writer;
    std::string jsonStr = writer.write(writeAr.GetRoot());

    Json::Value parsed;
    Json::Reader reader;
    bool ok = reader.parse(jsonStr, parsed);
    EXPECT_TRUE(ok);

    EXPECT_TRUE(parsed.isMember("mPos"));
    EXPECT_TRUE(parsed.isMember("mMass"));
    EXPECT_TRUE(parsed.isMember("mActive"));
    EXPECT_TRUE(parsed.isMember("mId"));
}

// =============================================================================
// N) Multiple types round-trip independently (no cross-contamination)
// =============================================================================

TEST(JsonArchive_RoundTrip, MultipleTypesIndependent) {
    // Serialize Vec2Json
    Vec2Json v;
    v.mX = 10.0f;
    v.mY = 20.0f;
    Json::Value vRoot = WriteToJson(v);

    // Serialize ParticleJson
    ParticleJson p;
    p.mPos.mX = 5.0f;
    p.mPos.mY = 6.0f;
    p.mMass   = 9.0f;
    p.mActive = false;
    p.mId     = 100;
    Json::Value pRoot = WriteToJson(p);

    // Read back Vec2Json
    Vec2Json vDst;
    vDst.mX = 0.0f;
    vDst.mY = 0.0f;
    ReadFromJson(vRoot, vDst);

    EXPECT_NEAR(vDst.mX, 10.0f, 0.0001f);
    EXPECT_NEAR(vDst.mY, 20.0f, 0.0001f);

    // Read back ParticleJson
    ParticleJson pDst;
    ReadFromJson(pRoot, pDst);

    EXPECT_NEAR(pDst.mPos.mX, 5.0f,  0.0001f);
    EXPECT_NEAR(pDst.mPos.mY, 6.0f,  0.0001f);
    EXPECT_NEAR(pDst.mMass,   9.0f,  0.0001f);
    EXPECT_EQ(pDst.mActive, false);
    EXPECT_EQ(pDst.mId, 100);
}

// =============================================================================
// O) Zero values round-trip (not confused with null/missing)
// =============================================================================

TEST(JsonArchive_RoundTrip, ZeroValues) {
    AllPrimitivesJson src;
    src.mBool   = false;
    src.mInt    = 0;
    src.mUInt   = 0u;
    src.mFloat  = 0.0f;
    src.mDouble = 0.0;

    Json::Value root = WriteToJson(src);

    AllPrimitivesJson dst;
    dst.mBool   = true;
    dst.mInt    = 99;
    dst.mUInt   = 99u;
    dst.mFloat  = 99.0f;
    dst.mDouble = 99.0;

    ReadFromJson(root, dst);

    EXPECT_EQ(dst.mBool, false);
    EXPECT_EQ(dst.mInt, 0);
    EXPECT_EQ(dst.mUInt, 0u);
    EXPECT_NEAR(dst.mFloat,  0.0f, 0.0001f);
    EXPECT_NEAR(dst.mDouble, 0.0,  1e-9);
}

// =============================================================================
// Archive concept: JsonWriteArchive and JsonReadArchive satisfy Archive
// =============================================================================

TEST(JsonArchive_Concept, WriteArchiveSatisfiesConcept) {
    JsonWriteArchive ar;
    EXPECT_FALSE(ar.IsReading());
    EXPECT_TRUE(ar.IsWriting());
}

TEST(JsonArchive_Concept, ReadArchiveSatisfiesConcept) {
    Json::Value root(Json::objectValue);
    JsonReadArchive ar(root);
    EXPECT_TRUE(ar.IsReading());
    EXPECT_FALSE(ar.IsWriting());
}
