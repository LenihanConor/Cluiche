// =============================================================================
// TestDiaMathsSerializers2.cpp
// Tests for serialize() free functions added in Phase 4a of DiaReflect type
// coverage: Vector3D, Vector4D, Angle, Matrix33, Transform2D, Transform3D.
//
// Archives: JSON round-trip, binary round-trip, missing-field tolerance.
// =============================================================================

#include <gtest/gtest.h>
#include "DiaMaths/DiaMathsSerializers.h"
#include "DiaCore/Reflect/JsonArchive.h"
#include "DiaCore/Reflect/BinaryArchive.h"
#include "DiaCore/Json/external/json/json.h"

using namespace Dia::Reflect;
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
// Vector3D
// =============================================================================

TEST(DiaMathsSerializer_Vector3D, JsonRoundTrip) {
    Vector3D src(1.0f, 2.0f, 3.0f);
    Json::Value root = WriteToJson(src);

    Vector3D dst;
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.x, 1.0f);
    EXPECT_FLOAT_EQ(dst.y, 2.0f);
    EXPECT_FLOAT_EQ(dst.z, 3.0f);
}

TEST(DiaMathsSerializer_Vector3D, BinaryRoundTrip) {
    Vector3D src(4.0f, -5.5f, 6.25f);
    Vector3D dst;
    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.x, src.x);
    EXPECT_FLOAT_EQ(dst.y, src.y);
    EXPECT_FLOAT_EQ(dst.z, src.z);
}

TEST(DiaMathsSerializer_Vector3D, MissingFieldKeepsDefault) {
    Json::Value partial;
    partial["x"] = 10.0f;
    // y and z absent

    Vector3D dst(99.0f, 99.0f, 99.0f);
    ReadFromJson(partial, dst);

    EXPECT_FLOAT_EQ(dst.x, 10.0f);
    EXPECT_FLOAT_EQ(dst.y, 99.0f);
    EXPECT_FLOAT_EQ(dst.z, 99.0f);
}

// =============================================================================
// Vector4D
// =============================================================================

TEST(DiaMathsSerializer_Vector4D, JsonRoundTrip) {
    Vector4D src(1.0f, 2.0f, 3.0f, 4.0f);
    Json::Value root = WriteToJson(src);

    Vector4D dst;
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.x, 1.0f);
    EXPECT_FLOAT_EQ(dst.y, 2.0f);
    EXPECT_FLOAT_EQ(dst.z, 3.0f);
    EXPECT_FLOAT_EQ(dst.w, 4.0f);
}

TEST(DiaMathsSerializer_Vector4D, BinaryRoundTrip) {
    Vector4D src(0.1f, 0.2f, 0.3f, 0.4f);
    Vector4D dst;
    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.x, src.x);
    EXPECT_FLOAT_EQ(dst.y, src.y);
    EXPECT_FLOAT_EQ(dst.z, src.z);
    EXPECT_FLOAT_EQ(dst.w, src.w);
}

// =============================================================================
// Angle
// =============================================================================

TEST(DiaMathsSerializer_Angle, JsonRoundTrip) {
    Angle src = Angle::FromDegrees(90.0f);
    Json::Value root = WriteToJson(src);

    Angle dst = Angle::Deg0;
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.AsRadians(), src.AsRadians());
}

TEST(DiaMathsSerializer_Angle, BinaryRoundTrip) {
    Angle src = Angle::FromDegrees(45.0f);
    Angle dst = Angle::Deg0;
    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.AsRadians(), src.AsRadians());
}

TEST(DiaMathsSerializer_Angle, MissingFieldKeepsDefault) {
    Json::Value empty;
    Angle dst = Angle::Deg180;
    ReadFromJson(empty, dst);

    // No field written — dst keeps its pre-read value
    EXPECT_FLOAT_EQ(dst.AsRadians(), Angle::Deg180.AsRadians());
}

// =============================================================================
// Matrix33
// =============================================================================

TEST(DiaMathsSerializer_Matrix33, JsonRoundTrip) {
    Matrix33 src(
        1.0f, 2.0f, 3.0f,
        4.0f, 5.0f, 6.0f,
        7.0f, 8.0f, 9.0f
    );
    Json::Value root = WriteToJson(src);

    Matrix33 dst = Matrix33::Identity();
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst(0,0), 1.0f);
    EXPECT_FLOAT_EQ(dst(0,2), 3.0f);
    EXPECT_FLOAT_EQ(dst(1,1), 5.0f);
    EXPECT_FLOAT_EQ(dst(2,0), 7.0f);
    EXPECT_FLOAT_EQ(dst(2,2), 9.0f);
}

TEST(DiaMathsSerializer_Matrix33, BinaryRoundTrip) {
    Matrix33 src(
        1.0f, 0.0f, 100.0f,
        0.0f, 1.0f, 200.0f,
        0.0f, 0.0f,   1.0f
    );
    Matrix33 dst = Matrix33::Identity();
    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst(0,2), 100.0f);
    EXPECT_FLOAT_EQ(dst(1,2), 200.0f);
    EXPECT_FLOAT_EQ(dst(2,2),   1.0f);
}

// =============================================================================
// Transform2D
// =============================================================================

TEST(DiaMathsSerializer_Transform2D, JsonRoundTrip) {
    Transform2D src(Vector2D(10.0f, 20.0f), Angle::FromDegrees(45.0f), Vector2D(2.0f, 3.0f));
    Json::Value root = WriteToJson(src);

    Transform2D dst;
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.GetLocalPosition().x, 10.0f);
    EXPECT_FLOAT_EQ(dst.GetLocalPosition().y, 20.0f);
    EXPECT_FLOAT_EQ(dst.GetLocalRotation().AsDegrees(), 45.0f);
    EXPECT_FLOAT_EQ(dst.GetLocalScale().x, 2.0f);
    EXPECT_FLOAT_EQ(dst.GetLocalScale().y, 3.0f);
}

TEST(DiaMathsSerializer_Transform2D, BinaryRoundTrip) {
    Transform2D src(Vector2D(5.0f, -5.0f), Angle::FromDegrees(180.0f), Vector2D(1.0f, 1.0f));
    Transform2D dst;
    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.GetLocalPosition().x, 5.0f);
    EXPECT_FLOAT_EQ(dst.GetLocalPosition().y, -5.0f);
    EXPECT_FLOAT_EQ(dst.GetLocalRotation().AsDegrees(), 180.0f);
}

TEST(DiaMathsSerializer_Transform2D, ParentNotSerialized) {
    Transform2D parent;
    Transform2D src;
    src.SetParent(&parent);

    Json::Value root = WriteToJson(src);

    // mParent is skipped — "mParent" key must not appear in output
    EXPECT_FALSE(root.isMember("mParent"));
}

// =============================================================================
// Transform3D
// =============================================================================

TEST(DiaMathsSerializer_Transform3D, JsonRoundTrip) {
    Quaternion rot(0.0f, 0.0f, 0.0f, 1.0f);  // identity
    Transform3D src(Vector3D(1.0f, 2.0f, 3.0f), rot, Vector3D(4.0f, 5.0f, 6.0f));
    Json::Value root = WriteToJson(src);

    Transform3D dst;
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.GetLocalPosition().x, 1.0f);
    EXPECT_FLOAT_EQ(dst.GetLocalPosition().y, 2.0f);
    EXPECT_FLOAT_EQ(dst.GetLocalPosition().z, 3.0f);
    EXPECT_FLOAT_EQ(dst.GetLocalScale().x, 4.0f);
    EXPECT_FLOAT_EQ(dst.GetLocalScale().y, 5.0f);
    EXPECT_FLOAT_EQ(dst.GetLocalScale().z, 6.0f);
    EXPECT_FLOAT_EQ(dst.GetLocalRotation().w, 1.0f);
}

TEST(DiaMathsSerializer_Transform3D, BinaryRoundTrip) {
    Quaternion rot(0.1f, 0.2f, 0.3f, 0.9274f);
    Transform3D src(Vector3D(7.0f, 8.0f, 9.0f), rot, Vector3D(1.0f, 1.0f, 1.0f));
    Transform3D dst;
    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.GetLocalPosition().x, 7.0f);
    EXPECT_FLOAT_EQ(dst.GetLocalRotation().x, 0.1f);
    EXPECT_FLOAT_EQ(dst.GetLocalRotation().w, 0.9274f);
}

TEST(DiaMathsSerializer_Transform3D, ParentNotSerialized) {
    Transform3D parent;
    Transform3D src;
    src.SetParent(&parent);

    Json::Value root = WriteToJson(src);

    EXPECT_FALSE(root.isMember("mParent"));
}
