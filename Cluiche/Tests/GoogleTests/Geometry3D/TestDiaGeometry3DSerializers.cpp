// =============================================================================
// TestDiaGeometry3DSerializers.cpp
// Tests for serialize() free functions defined in DiaGeometry3DSerializers.h.
//
// Covers: Sphere, AABB, OOBB, Capsule
// Archives: JSON round-trip, binary round-trip, missing-field tolerance.
// =============================================================================

#include <gtest/gtest.h>
#include "DiaGeometry3D/DiaGeometry3DSerializers.h"
#include "DiaCore/Reflect/JsonArchive.h"
#include "DiaCore/Reflect/BinaryArchive.h"
#include "DiaCore/Json/external/json/json.h"

using namespace Dia::Reflect;
using namespace Dia::Geometry3D;
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
// Sphere
// =============================================================================

TEST(DiaGeometry3DSerializer_Sphere, JsonRoundTrip) {
    Sphere src(Vector3D(1.0f, 2.0f, 3.0f), 4.0f);
    Json::Value root = WriteToJson(src);

    Sphere dst;
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.GetCenter().x, 1.0f);
    EXPECT_FLOAT_EQ(dst.GetCenter().y, 2.0f);
    EXPECT_FLOAT_EQ(dst.GetCenter().z, 3.0f);
    EXPECT_FLOAT_EQ(dst.GetRadius(), 4.0f);
}

TEST(DiaGeometry3DSerializer_Sphere, BinaryRoundTrip) {
    Sphere src(Vector3D(-1.0f, 0.0f, 5.0f), 2.5f);
    Sphere dst;
    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.GetCenter().x, -1.0f);
    EXPECT_FLOAT_EQ(dst.GetCenter().z, 5.0f);
    EXPECT_FLOAT_EQ(dst.GetRadius(), 2.5f);
}

TEST(DiaGeometry3DSerializer_Sphere, MissingFieldKeepsDefault) {
    Json::Value partial;
    partial["mRadius"] = 7.0f;
    // mCenter absent

    Sphere dst(Vector3D(99.0f, 99.0f, 99.0f), 0.0f);
    ReadFromJson(partial, dst);

    EXPECT_FLOAT_EQ(dst.GetRadius(), 7.0f);
    EXPECT_FLOAT_EQ(dst.GetCenter().x, 99.0f);
}

// =============================================================================
// AABB
// =============================================================================

TEST(DiaGeometry3DSerializer_AABB, JsonRoundTrip) {
    AABB src(Vector3D(-1.0f, -2.0f, -3.0f), Vector3D(1.0f, 2.0f, 3.0f));
    Json::Value root = WriteToJson(src);

    AABB dst;
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.GetMin().x, -1.0f);
    EXPECT_FLOAT_EQ(dst.GetMin().z, -3.0f);
    EXPECT_FLOAT_EQ(dst.GetMax().y,  2.0f);
    EXPECT_FLOAT_EQ(dst.GetMax().z,  3.0f);
}

TEST(DiaGeometry3DSerializer_AABB, BinaryRoundTrip) {
    AABB src(Vector3D(0.0f, 0.0f, 0.0f), Vector3D(10.0f, 5.0f, 2.0f));
    AABB dst;
    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.GetMax().x, 10.0f);
    EXPECT_FLOAT_EQ(dst.GetMax().y,  5.0f);
    EXPECT_FLOAT_EQ(dst.GetMax().z,  2.0f);
}

// =============================================================================
// OOBB
// =============================================================================

TEST(DiaGeometry3DSerializer_OOBB, JsonRoundTrip) {
    Quaternion orient(0.0f, 0.0f, 0.0f, 1.0f);  // identity
    OOBB src(Vector3D(1.0f, 2.0f, 3.0f), Vector3D(0.5f, 1.0f, 1.5f), orient);
    Json::Value root = WriteToJson(src);

    OOBB dst;
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.GetCenter().x, 1.0f);
    EXPECT_FLOAT_EQ(dst.GetCenter().z, 3.0f);
    EXPECT_FLOAT_EQ(dst.GetHalfExtents().y, 1.0f);
    EXPECT_FLOAT_EQ(dst.GetOrientation().w, 1.0f);
}

TEST(DiaGeometry3DSerializer_OOBB, BinaryRoundTrip) {
    Quaternion orient(0.1f, 0.2f, 0.3f, 0.9274f);
    OOBB src(Vector3D(0.0f, 0.0f, 0.0f), Vector3D(2.0f, 2.0f, 2.0f), orient);
    OOBB dst;
    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.GetHalfExtents().x, 2.0f);
    EXPECT_FLOAT_EQ(dst.GetOrientation().x, 0.1f);
    EXPECT_FLOAT_EQ(dst.GetOrientation().w, 0.9274f);
}

// =============================================================================
// Capsule
// =============================================================================

TEST(DiaGeometry3DSerializer_Capsule, JsonRoundTrip) {
    Capsule src(Vector3D(0.0f, -3.0f, 0.0f), Vector3D(0.0f, 3.0f, 0.0f), 1.0f);
    Json::Value root = WriteToJson(src);

    Capsule dst;
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.GetStartA().y, -3.0f);
    EXPECT_FLOAT_EQ(dst.GetEndB().y,    3.0f);
    EXPECT_FLOAT_EQ(dst.GetRadius(),    1.0f);
}

TEST(DiaGeometry3DSerializer_Capsule, BinaryRoundTrip) {
    Capsule src(Vector3D(-1.0f, 0.0f, 0.0f), Vector3D(1.0f, 0.0f, 0.0f), 0.5f);
    Capsule dst;
    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.GetStartA().x, -1.0f);
    EXPECT_FLOAT_EQ(dst.GetEndB().x,    1.0f);
    EXPECT_FLOAT_EQ(dst.GetRadius(),    0.5f);
}

TEST(DiaGeometry3DSerializer_Capsule, MissingFieldKeepsDefault) {
    Json::Value partial;
    partial["mRadius"] = 3.0f;

    Capsule dst(Vector3D(99.0f, 0.0f, 0.0f), Vector3D(99.0f, 0.0f, 0.0f), 0.0f);
    ReadFromJson(partial, dst);

    EXPECT_FLOAT_EQ(dst.GetRadius(), 3.0f);
    EXPECT_FLOAT_EQ(dst.GetStartA().x, 99.0f);
}
