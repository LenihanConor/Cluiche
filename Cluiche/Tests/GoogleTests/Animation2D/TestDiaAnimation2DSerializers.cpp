// =============================================================================
// TestDiaAnimation2DSerializers.cpp
// Tests for serialize() free functions defined in DiaAnimation2DSerializers.h.
//
// Covers: SpringNodeDef, SpringChainDef, BoneMask, Keyframe, KeyframeTrack,
//         AnimClipDef (nested tracks -> keyframes)
// Archives: JSON round-trip, binary round-trip, missing-field tolerance.
// =============================================================================

#include <gtest/gtest.h>
#include "DiaAnimation2D/DiaAnimation2DSerializers.h"
#include "DiaCore/Reflect/JsonArchive.h"
#include "DiaCore/Reflect/BinaryArchive.h"
#include "DiaCore/Json/external/json/json.h"

using namespace Dia::Reflect;
using namespace Dia::Animation2D;
using namespace Dia::Maths;

// =============================================================================
// Helpers
// =============================================================================

template<typename T>
static Json::Value WriteToJson(T& obj) {
    JsonWriteArchive ar;
    serialize(ar, obj);
    return ar.GetRoot();
}

template<typename T>
static void ReadFromJson(const Json::Value& root, T& obj) {
    JsonReadArchive ar(root);
    serialize(ar, obj);
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
// SpringNodeDef
// =============================================================================

TEST(DiaAnimation2DSerializer_SpringNodeDef, JsonRoundTrip) {
    SpringNodeDef src;
    src.stiffness          = 100.0f;
    src.damping            = 8.0f;
    src.maxAngularVelocity = 30.0f;

    Json::Value root = WriteToJson(src);

    SpringNodeDef dst;
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.stiffness,          100.0f);
    EXPECT_FLOAT_EQ(dst.damping,            8.0f);
    EXPECT_FLOAT_EQ(dst.maxAngularVelocity, 30.0f);
}

TEST(DiaAnimation2DSerializer_SpringNodeDef, BinaryRoundTrip) {
    SpringNodeDef src;
    src.stiffness = 75.0f;
    src.damping   = 4.5f;

    SpringNodeDef dst;
    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.stiffness, 75.0f);
    EXPECT_FLOAT_EQ(dst.damping,   4.5f);
}

TEST(DiaAnimation2DSerializer_SpringNodeDef, MissingFieldKeepsDefault) {
    Json::Value partial;
    partial["stiffness"] = 200.0f;

    SpringNodeDef dst;
    ReadFromJson(partial, dst);

    EXPECT_FLOAT_EQ(dst.stiffness, 200.0f);
    EXPECT_FLOAT_EQ(dst.damping,   5.0f);   // default preserved
}

// =============================================================================
// SpringChainDef
// =============================================================================

TEST(DiaAnimation2DSerializer_SpringChainDef, JsonRoundTrip) {
    SpringChainDef src;
    src.id         = Dia::Core::StringCRC("tail");
    src.rootBoneId = Dia::Core::StringCRC("tail_root");
    src.boneIds.Add(Dia::Core::StringCRC("tail_01"));
    src.boneIds.Add(Dia::Core::StringCRC("tail_02"));
    src.defaultNode.stiffness = 60.0f;
    src.gravityDirection  = Vector2D(0.0f, -1.0f);
    src.gravityStrength   = 9.8f;

    Json::Value root = WriteToJson(src);

    SpringChainDef dst;
    ReadFromJson(root, dst);

    EXPECT_STREQ(static_cast<const char*>(dst.id),         "tail");
    EXPECT_STREQ(static_cast<const char*>(dst.rootBoneId), "tail_root");
    EXPECT_EQ(dst.boneIds.Size(), 2u);
    EXPECT_STREQ(static_cast<const char*>(dst.boneIds[0]), "tail_01");
    EXPECT_STREQ(static_cast<const char*>(dst.boneIds[1]), "tail_02");
    EXPECT_FLOAT_EQ(dst.defaultNode.stiffness, 60.0f);
    EXPECT_FLOAT_EQ(dst.gravityDirection.y,    -1.0f);
    EXPECT_FLOAT_EQ(dst.gravityStrength,        9.8f);
}

TEST(DiaAnimation2DSerializer_SpringChainDef, BinaryRoundTrip) {
    SpringChainDef src;
    src.id           = Dia::Core::StringCRC("hair");
    src.gravityStrength = 5.0f;

    Dia::Core::StringCRC bone("hair_01");
    src.boneIds.Add(bone);

    SpringChainDef dst;
    WriteAndReadBinary(src, dst);

    EXPECT_STREQ(static_cast<const char*>(dst.id), "hair");
    EXPECT_FLOAT_EQ(dst.gravityStrength, 5.0f);
    EXPECT_EQ(dst.boneIds.Size(), 1u);
}

// =============================================================================
// BoneMask
// =============================================================================

TEST(DiaAnimation2DSerializer_BoneMask, JsonRoundTrip) {
    BoneMask src;
    src.id = Dia::Core::StringCRC("upper_body");
    src.boneIds.Add(Dia::Core::StringCRC("spine"));
    src.boneIds.Add(Dia::Core::StringCRC("shoulder_l"));
    src.boneIds.Add(Dia::Core::StringCRC("shoulder_r"));

    Json::Value root = WriteToJson(src);

    BoneMask dst;
    ReadFromJson(root, dst);

    EXPECT_STREQ(static_cast<const char*>(dst.id), "upper_body");
    EXPECT_EQ(dst.boneIds.Size(), 3u);
    EXPECT_STREQ(static_cast<const char*>(dst.boneIds[0]), "spine");
    EXPECT_STREQ(static_cast<const char*>(dst.boneIds[1]), "shoulder_l");
    EXPECT_STREQ(static_cast<const char*>(dst.boneIds[2]), "shoulder_r");
}

TEST(DiaAnimation2DSerializer_BoneMask, BinaryRoundTrip) {
    BoneMask src;
    src.id = Dia::Core::StringCRC("lower_body");
    src.boneIds.Add(Dia::Core::StringCRC("hip"));

    BoneMask dst;
    WriteAndReadBinary(src, dst);

    EXPECT_STREQ(static_cast<const char*>(dst.id), "lower_body");
    EXPECT_EQ(dst.boneIds.Size(), 1u);
    EXPECT_STREQ(static_cast<const char*>(dst.boneIds[0]), "hip");
}

// =============================================================================
// Keyframe
// =============================================================================

TEST(DiaAnimation2DSerializer_Keyframe, JsonRoundTrip) {
    Keyframe src;
    src.time     = 0.5f;
    src.rotation = 1.2f;
    src.position = Vector2D(3.0f, 4.0f);
    src.scale    = Vector2D(2.0f, 2.0f);

    Json::Value root = WriteToJson(src);

    Keyframe dst;
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.time,       0.5f);
    EXPECT_FLOAT_EQ(dst.rotation,   1.2f);
    EXPECT_FLOAT_EQ(dst.position.x, 3.0f);
    EXPECT_FLOAT_EQ(dst.position.y, 4.0f);
    EXPECT_FLOAT_EQ(dst.scale.x,    2.0f);
    EXPECT_FLOAT_EQ(dst.scale.y,    2.0f);
}

TEST(DiaAnimation2DSerializer_Keyframe, BinaryRoundTrip) {
    Keyframe src;
    src.time     = 1.0f;
    src.rotation = 3.14f;

    Keyframe dst;
    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.time,     1.0f);
    EXPECT_FLOAT_EQ(dst.rotation, 3.14f);
}

// =============================================================================
// KeyframeTrack
// =============================================================================

TEST(DiaAnimation2DSerializer_KeyframeTrack, JsonRoundTrip) {
    KeyframeTrack src;
    src.boneId       = Dia::Core::StringCRC("arm");
    src.rotationOnly = true;

    Keyframe k0; k0.time = 0.0f; k0.rotation = 0.0f;
    Keyframe k1; k1.time = 1.0f; k1.rotation = 1.57f;
    src.keyframes.Add(k0);
    src.keyframes.Add(k1);

    Json::Value root = WriteToJson(src);

    KeyframeTrack dst;
    ReadFromJson(root, dst);

    EXPECT_STREQ(static_cast<const char*>(dst.boneId), "arm");
    EXPECT_TRUE(dst.rotationOnly);
    EXPECT_EQ(dst.keyframes.Size(), 2u);
    EXPECT_FLOAT_EQ(dst.keyframes[0].time,     0.0f);
    EXPECT_FLOAT_EQ(dst.keyframes[1].time,     1.0f);
    EXPECT_FLOAT_EQ(dst.keyframes[1].rotation, 1.57f);
}

TEST(DiaAnimation2DSerializer_KeyframeTrack, BinaryRoundTrip) {
    KeyframeTrack src;
    src.boneId       = Dia::Core::StringCRC("leg");
    src.rotationOnly = false;

    Keyframe k; k.time = 0.25f; k.position = Vector2D(1.0f, 2.0f);
    src.keyframes.Add(k);

    KeyframeTrack dst;
    WriteAndReadBinary(src, dst);

    EXPECT_STREQ(static_cast<const char*>(dst.boneId), "leg");
    EXPECT_FALSE(dst.rotationOnly);
    EXPECT_EQ(dst.keyframes.Size(), 1u);
    EXPECT_FLOAT_EQ(dst.keyframes[0].time,       0.25f);
    EXPECT_FLOAT_EQ(dst.keyframes[0].position.x, 1.0f);
}

// =============================================================================
// AnimClipDef — full nesting: clip -> tracks -> keyframes
// =============================================================================

TEST(DiaAnimation2DSerializer_AnimClipDef, JsonRoundTrip) {
    AnimClipDef src;
    src.id       = Dia::Core::StringCRC("walk_cycle");
    src.duration = 1.0f;

    KeyframeTrack track;
    track.boneId = Dia::Core::StringCRC("spine");

    Keyframe k0; k0.time = 0.0f; k0.rotation = 0.0f;
    Keyframe k1; k1.time = 0.5f; k1.rotation = 0.3f;
    Keyframe k2; k2.time = 1.0f; k2.rotation = 0.0f;
    track.keyframes.Add(k0);
    track.keyframes.Add(k1);
    track.keyframes.Add(k2);
    src.tracks.Add(track);

    Json::Value root = WriteToJson(src);

    AnimClipDef dst;
    ReadFromJson(root, dst);

    EXPECT_STREQ(static_cast<const char*>(dst.id), "walk_cycle");
    EXPECT_FLOAT_EQ(dst.duration, 1.0f);
    EXPECT_EQ(dst.tracks.Size(), 1u);
    EXPECT_STREQ(static_cast<const char*>(dst.tracks[0].boneId), "spine");
    EXPECT_EQ(dst.tracks[0].keyframes.Size(), 3u);
    EXPECT_FLOAT_EQ(dst.tracks[0].keyframes[1].time,     0.5f);
    EXPECT_FLOAT_EQ(dst.tracks[0].keyframes[1].rotation, 0.3f);
}

TEST(DiaAnimation2DSerializer_AnimClipDef, BinaryRoundTrip) {
    AnimClipDef src;
    src.id       = Dia::Core::StringCRC("run_cycle");
    src.duration = 0.5f;

    AnimClipDef dst;
    WriteAndReadBinary(src, dst);

    EXPECT_STREQ(static_cast<const char*>(dst.id), "run_cycle");
    EXPECT_FLOAT_EQ(dst.duration, 0.5f);
    EXPECT_EQ(dst.tracks.Size(), 0u);
}

TEST(DiaAnimation2DSerializer_AnimClipDef, MissingFieldKeepsDefault) {
    Json::Value partial;
    partial["id"]["value"] = "idle";

    AnimClipDef dst;
    dst.duration = 2.0f;
    ReadFromJson(partial, dst);

    EXPECT_STREQ(static_cast<const char*>(dst.id), "idle");
    EXPECT_FLOAT_EQ(dst.duration, 2.0f);  // untouched
}
