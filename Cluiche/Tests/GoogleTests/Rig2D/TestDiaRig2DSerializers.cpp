// =============================================================================
// TestDiaRig2DSerializers.cpp
// Tests for serialize() free functions defined in DiaRig2DSerializers.h.
//
// Covers: BoneTransform, Bone, SkeletonDef (array nesting)
// Archives: JSON round-trip, binary round-trip, missing-field tolerance.
// =============================================================================

#include <gtest/gtest.h>
#include "DiaRig2D/DiaRig2DSerializers.h"
#include "DiaCore/Reflect/JsonArchive.h"
#include "DiaCore/Reflect/BinaryArchive.h"
#include "DiaCore/Json/external/json/json.h"

using namespace Dia::Reflect;
using namespace Dia::Rig2D;
using namespace Dia::Maths;

// =============================================================================
// Helpers
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
static void WriteAndReadBinary(const T& src, T& dst) {
    BinaryWriteArchive wAr;
    wAr.WriteVersion(1u);
    T& mutableSrc = const_cast<T&>(src);
    serialize(wAr, mutableSrc, 1u);

    BinaryReadArchive rAr(wAr.GetData(), wAr.GetSize());
    rAr.ReadVersion();
    serialize(rAr, dst, 1u);
}

// =============================================================================
// BoneTransform
// =============================================================================

TEST(DiaRig2DSerializer_BoneTransform, JsonRoundTrip) {
    BoneTransform src;
    src.position = Vector2D(1.0f, 2.0f);
    src.rotation = 0.5f;
    src.scale    = Vector2D(2.0f, 3.0f);

    Json::Value root = WriteToJson(src);

    BoneTransform dst;
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.position.x, 1.0f);
    EXPECT_FLOAT_EQ(dst.position.y, 2.0f);
    EXPECT_FLOAT_EQ(dst.rotation,   0.5f);
    EXPECT_FLOAT_EQ(dst.scale.x,    2.0f);
    EXPECT_FLOAT_EQ(dst.scale.y,    3.0f);
}

TEST(DiaRig2DSerializer_BoneTransform, BinaryRoundTrip) {
    BoneTransform src;
    src.position = Vector2D(-3.0f, 4.0f);
    src.rotation = 1.57f;
    src.scale    = Vector2D(1.0f, 1.0f);

    BoneTransform dst;
    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.position.x, -3.0f);
    EXPECT_FLOAT_EQ(dst.rotation,    1.57f);
}

// =============================================================================
// Bone
// =============================================================================

TEST(DiaRig2DSerializer_Bone, JsonRoundTrip) {
    Bone src;
    src.name          = Dia::Core::StringCRC("spine");
    src.parentIndex   = 2;
    src.localPosition = Vector2D(0.0f, 1.5f);
    src.localRotation = 0.25f;
    src.localScale    = Vector2D(1.0f, 1.0f);
    src.length        = 3.0f;

    Json::Value root = WriteToJson(src);

    Bone dst;
    ReadFromJson(root, dst);

    EXPECT_STREQ(static_cast<const char*>(dst.name), "spine");
    EXPECT_EQ(dst.parentIndex, 2);
    EXPECT_FLOAT_EQ(dst.localPosition.y, 1.5f);
    EXPECT_FLOAT_EQ(dst.localRotation,   0.25f);
    EXPECT_FLOAT_EQ(dst.length,          3.0f);
}

TEST(DiaRig2DSerializer_Bone, BinaryRoundTrip) {
    Bone src;
    src.name        = Dia::Core::StringCRC("hip");
    src.parentIndex = -1;
    src.length      = 1.0f;

    Bone dst;
    WriteAndReadBinary(src, dst);

    EXPECT_STREQ(static_cast<const char*>(dst.name), "hip");
    EXPECT_EQ(dst.parentIndex, -1);
    EXPECT_FLOAT_EQ(dst.length, 1.0f);
}

TEST(DiaRig2DSerializer_Bone, MissingFieldKeepsDefault) {
    Json::Value partial;
    partial["parentIndex"] = 5;

    Bone dst;
    dst.length = 99.0f;
    ReadFromJson(partial, dst);

    EXPECT_EQ(dst.parentIndex, 5);
    EXPECT_FLOAT_EQ(dst.length, 99.0f);  // untouched
}

// =============================================================================
// SkeletonDef — nesting test (def -> bones array -> individual bones)
// =============================================================================

TEST(DiaRig2DSerializer_SkeletonDef, JsonRoundTrip) {
    SkeletonDef src;
    src.id = Dia::Core::StringCRC("humanoid");

    Bone root;
    root.name        = Dia::Core::StringCRC("root");
    root.parentIndex = -1;
    root.length      = 1.0f;
    src.bones.Add(root);

    Bone spine;
    spine.name        = Dia::Core::StringCRC("spine");
    spine.parentIndex = 0;
    spine.length      = 2.0f;
    src.bones.Add(spine);

    Json::Value json = WriteToJson(src);

    SkeletonDef dst;
    ReadFromJson(json, dst);

    EXPECT_STREQ(static_cast<const char*>(dst.id), "humanoid");
    EXPECT_EQ(dst.bones.Size(), 2u);
    EXPECT_STREQ(static_cast<const char*>(dst.bones[0].name), "root");
    EXPECT_EQ(dst.bones[0].parentIndex, -1);
    EXPECT_FLOAT_EQ(dst.bones[0].length, 1.0f);
    EXPECT_STREQ(static_cast<const char*>(dst.bones[1].name), "spine");
    EXPECT_EQ(dst.bones[1].parentIndex, 0);
    EXPECT_FLOAT_EQ(dst.bones[1].length, 2.0f);
}

TEST(DiaRig2DSerializer_SkeletonDef, BinaryRoundTrip) {
    SkeletonDef src;
    src.id = Dia::Core::StringCRC("biped");

    Bone b;
    b.name        = Dia::Core::StringCRC("pelvis");
    b.parentIndex = -1;
    b.length      = 0.5f;
    src.bones.Add(b);

    SkeletonDef dst;
    WriteAndReadBinary(src, dst);

    EXPECT_STREQ(static_cast<const char*>(dst.id), "biped");
    EXPECT_EQ(dst.bones.Size(), 1u);
    EXPECT_STREQ(static_cast<const char*>(dst.bones[0].name), "pelvis");
    EXPECT_FLOAT_EQ(dst.bones[0].length, 0.5f);
}
