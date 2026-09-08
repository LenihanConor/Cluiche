// =============================================================================
// TestDiaIK2DSerializers.cpp
// Tests for serialize() free functions defined in DiaIK2DSerializers.h.
//
// Covers: JointLimitDef, PoleVector, IKChainDef (nested jointLimits array)
// Archives: JSON round-trip, binary round-trip, missing-field tolerance.
// =============================================================================

#include <gtest/gtest.h>
#include "DiaIK2D/DiaIK2DSerializers.h"
#include "DiaMaths/DiaMathsSerializers.h"
#include "DiaCore/Reflect/JsonArchive.h"
#include "DiaCore/Reflect/BinaryArchive.h"
#include "DiaCore/Json/external/json/json.h"

using namespace Dia::Reflect;
using namespace Dia::IK2D;
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
// JointLimitDef
// =============================================================================

TEST(DiaIK2DSerializer_JointLimitDef, JsonRoundTrip) {
    JointLimitDef src;
    src.minAngle = -1.57f;
    src.maxAngle =  1.57f;
    src.enabled  = true;

    Json::Value root = WriteToJson(src);

    JointLimitDef dst;
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.minAngle, -1.57f);
    EXPECT_FLOAT_EQ(dst.maxAngle,  1.57f);
    EXPECT_TRUE(dst.enabled);
}

TEST(DiaIK2DSerializer_JointLimitDef, BinaryRoundTrip) {
    JointLimitDef src;
    src.minAngle = -0.5f;
    src.maxAngle =  0.5f;
    src.enabled  = false;

    JointLimitDef dst;
    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.minAngle, -0.5f);
    EXPECT_FLOAT_EQ(dst.maxAngle,  0.5f);
    EXPECT_FALSE(dst.enabled);
}

TEST(DiaIK2DSerializer_JointLimitDef, MissingFieldKeepsDefault) {
    Json::Value partial;
    partial["enabled"] = true;

    JointLimitDef dst;
    ReadFromJson(partial, dst);

    EXPECT_TRUE(dst.enabled);
    EXPECT_FLOAT_EQ(dst.minAngle, -Dia::Maths::PI);  // default preserved
    EXPECT_FLOAT_EQ(dst.maxAngle,  Dia::Maths::PI);
}

// =============================================================================
// PoleVector
// =============================================================================

TEST(DiaIK2DSerializer_PoleVector, JsonRoundTrip) {
    PoleVector src;
    src.direction = Vector2D(0.0f, 1.0f);
    src.weight    = 0.75f;

    Json::Value root = WriteToJson(src);

    PoleVector dst;
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.direction.x, 0.0f);
    EXPECT_FLOAT_EQ(dst.direction.y, 1.0f);
    EXPECT_FLOAT_EQ(dst.weight,      0.75f);
}

TEST(DiaIK2DSerializer_PoleVector, BinaryRoundTrip) {
    PoleVector src;
    src.direction = Vector2D(-1.0f, 0.0f);
    src.weight    = 0.5f;

    PoleVector dst;
    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.direction.x, -1.0f);
    EXPECT_FLOAT_EQ(dst.direction.y,  0.0f);
    EXPECT_FLOAT_EQ(dst.weight,       0.5f);
}

// =============================================================================
// IKChainDef
// =============================================================================

TEST(DiaIK2DSerializer_IKChainDef, JsonRoundTrip) {
    IKChainDef src;
    src.id           = Dia::Core::StringCRC("arm_chain");
    src.startBoneId  = Dia::Core::StringCRC("shoulder");
    src.endBoneId    = Dia::Core::StringCRC("wrist");
    src.reachWeight  = 0.9f;
    src.maxIterations = 15;
    src.tolerance    = 0.005f;

    JointLimitDef limit;
    limit.minAngle = -1.0f;
    limit.maxAngle =  1.0f;
    limit.enabled  = true;
    src.jointLimits.Add(limit);

    Json::Value root = WriteToJson(src);

    IKChainDef dst;
    ReadFromJson(root, dst);

    EXPECT_STREQ(static_cast<const char*>(dst.id),          "arm_chain");
    EXPECT_STREQ(static_cast<const char*>(dst.startBoneId), "shoulder");
    EXPECT_STREQ(static_cast<const char*>(dst.endBoneId),   "wrist");
    EXPECT_FLOAT_EQ(dst.reachWeight,   0.9f);
    EXPECT_EQ(dst.maxIterations,       15);
    EXPECT_FLOAT_EQ(dst.tolerance,     0.005f);
    EXPECT_EQ(dst.jointLimits.Size(),  1u);
    EXPECT_FLOAT_EQ(dst.jointLimits[0].minAngle, -1.0f);
    EXPECT_FLOAT_EQ(dst.jointLimits[0].maxAngle,  1.0f);
    EXPECT_TRUE(dst.jointLimits[0].enabled);
}

TEST(DiaIK2DSerializer_IKChainDef, BinaryRoundTrip) {
    IKChainDef src;
    src.id          = Dia::Core::StringCRC("leg_chain");
    src.startBoneId = Dia::Core::StringCRC("hip");
    src.endBoneId   = Dia::Core::StringCRC("ankle");
    src.maxIterations = 10;

    IKChainDef dst;
    WriteAndReadBinary(src, dst);

    EXPECT_STREQ(static_cast<const char*>(dst.id),          "leg_chain");
    EXPECT_STREQ(static_cast<const char*>(dst.startBoneId), "hip");
    EXPECT_STREQ(static_cast<const char*>(dst.endBoneId),   "ankle");
    EXPECT_EQ(dst.maxIterations, 10);
    EXPECT_EQ(dst.jointLimits.Size(), 0u);
}

TEST(DiaIK2DSerializer_IKChainDef, MultipleJointLimitsRoundTrip) {
    IKChainDef src;
    src.id = Dia::Core::StringCRC("spine_chain");

    for (int i = 0; i < 3; ++i) {
        JointLimitDef lim;
        lim.minAngle = static_cast<float>(-i) - 0.5f;
        lim.maxAngle = static_cast<float>( i) + 0.5f;
        lim.enabled  = (i % 2 == 0);
        src.jointLimits.Add(lim);
    }

    Json::Value root = WriteToJson(src);

    IKChainDef dst;
    ReadFromJson(root, dst);

    EXPECT_EQ(dst.jointLimits.Size(), 3u);
    for (int i = 0; i < 3; ++i) {
        EXPECT_FLOAT_EQ(dst.jointLimits[i].minAngle, static_cast<float>(-i) - 0.5f);
        EXPECT_FLOAT_EQ(dst.jointLimits[i].maxAngle, static_cast<float>( i) + 0.5f);
        EXPECT_EQ(dst.jointLimits[i].enabled, (i % 2 == 0));
    }
}

TEST(DiaIK2DSerializer_IKChainDef, MissingFieldKeepsDefault) {
    Json::Value partial;
    partial["id"]["value"] = "partial_chain";

    IKChainDef dst;
    ReadFromJson(partial, dst);

    EXPECT_STREQ(static_cast<const char*>(dst.id), "partial_chain");
    EXPECT_FLOAT_EQ(dst.reachWeight, 1.0f);       // default preserved
    EXPECT_EQ(dst.maxIterations,     20);          // default preserved
    EXPECT_FLOAT_EQ(dst.tolerance,   0.001f);      // default preserved
    EXPECT_EQ(dst.jointLimits.Size(), 0u);
}
