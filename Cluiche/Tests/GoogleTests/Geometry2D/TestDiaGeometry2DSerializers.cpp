// =============================================================================
// TestDiaGeometry2DSerializers.cpp
// Tests for serialize() free functions defined in DiaGeometry2DSerializers.h.
//
// Covers: Point, Circle, AARect, OORect, Line, Ray, Triangle, Capsule,
//         Arc, Sector, ConvexPolygon
// Archives: JSON round-trip, binary round-trip, missing-field tolerance.
// =============================================================================

#include <gtest/gtest.h>
#include "DiaGeometry2D/DiaGeometry2DSerializers.h"
#include "DiaCore/Reflect/JsonArchive.h"
#include "DiaCore/Reflect/BinaryArchive.h"
#include "DiaCore/Json/external/json/json.h"

using namespace Dia::Reflect;
using namespace Dia::Geometry2D;
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
// Point
// =============================================================================

TEST(DiaGeometry2DSerializer_Point, JsonRoundTrip) {
    Point src(Vector2D(3.0f, 4.0f));
    Json::Value root = WriteToJson(src);

    Point dst;
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.GetPosition().x, 3.0f);
    EXPECT_FLOAT_EQ(dst.GetPosition().y, 4.0f);
}

TEST(DiaGeometry2DSerializer_Point, BinaryRoundTrip) {
    Point src(Vector2D(1.0f, 2.0f));
    Point dst;
    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.GetPosition().x, 1.0f);
    EXPECT_FLOAT_EQ(dst.GetPosition().y, 2.0f);
}

// =============================================================================
// Circle
// =============================================================================

TEST(DiaGeometry2DSerializer_Circle, JsonRoundTrip) {
    Circle src(5.0f, Vector2D(1.0f, 2.0f));
    Json::Value root = WriteToJson(src);

    Circle dst;
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.GetRadius(), 5.0f);
    EXPECT_FLOAT_EQ(dst.GetCenter().x, 1.0f);
    EXPECT_FLOAT_EQ(dst.GetCenter().y, 2.0f);
}

TEST(DiaGeometry2DSerializer_Circle, BinaryRoundTrip) {
    Circle src(3.5f, Vector2D(-1.0f, 7.0f));
    Circle dst;
    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.GetRadius(), 3.5f);
    EXPECT_FLOAT_EQ(dst.GetCenter().x, -1.0f);
    EXPECT_FLOAT_EQ(dst.GetCenter().y, 7.0f);
}

TEST(DiaGeometry2DSerializer_Circle, MissingFieldKeepsDefault) {
    Json::Value partial;
    partial["mRadius"] = 2.0f;
    // mCenter absent

    Circle dst(10.0f, Vector2D(99.0f, 99.0f));
    ReadFromJson(partial, dst);

    EXPECT_FLOAT_EQ(dst.GetRadius(), 2.0f);
    EXPECT_FLOAT_EQ(dst.GetCenter().x, 99.0f);
    EXPECT_FLOAT_EQ(dst.GetCenter().y, 99.0f);
}

// =============================================================================
// AARect
// =============================================================================

TEST(DiaGeometry2DSerializer_AARect, JsonRoundTrip) {
    AARect src(Vector2D(0.0f, 0.0f), Vector2D(10.0f, 5.0f));
    Json::Value root = WriteToJson(src);

    AARect dst;
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.GetBottomLeft().x, 0.0f);
    EXPECT_FLOAT_EQ(dst.GetTopRight().x, 10.0f);
    EXPECT_FLOAT_EQ(dst.GetTopRight().y, 5.0f);
}

TEST(DiaGeometry2DSerializer_AARect, BinaryRoundTrip) {
    AARect src(Vector2D(-5.0f, -3.0f), Vector2D(5.0f, 3.0f));
    AARect dst;
    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.GetBottomLeft().x, -5.0f);
    EXPECT_FLOAT_EQ(dst.GetTopRight().y, 3.0f);
}

// =============================================================================
// OORect
// =============================================================================

TEST(DiaGeometry2DSerializer_OORect, JsonRoundTrip) {
    OORect src(Vector2D(0,0), Vector2D(1,0), Vector2D(1,1), Vector2D(0,1));
    Json::Value root = WriteToJson(src);

    OORect dst;
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.GetPt(0).x, 0.0f);
    EXPECT_FLOAT_EQ(dst.GetPt(1).x, 1.0f);
    EXPECT_FLOAT_EQ(dst.GetPt(2).y, 1.0f);
    EXPECT_FLOAT_EQ(dst.GetPt(3).x, 0.0f);
}

TEST(DiaGeometry2DSerializer_OORect, BinaryRoundTrip) {
    OORect src(Vector2D(2,0), Vector2D(4,0), Vector2D(4,2), Vector2D(2,2));
    OORect dst;
    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.GetPt(0).x, 2.0f);
    EXPECT_FLOAT_EQ(dst.GetPt(2).x, 4.0f);
}

// =============================================================================
// Line
// =============================================================================

TEST(DiaGeometry2DSerializer_Line, JsonRoundTrip) {
    Line src(Vector2D(1.0f, 2.0f), Vector2D(3.0f, 4.0f));
    Json::Value root = WriteToJson(src);

    Line dst;
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.GetPt1().x, 1.0f);
    EXPECT_FLOAT_EQ(dst.GetPt2().x, 3.0f);
    EXPECT_FLOAT_EQ(dst.GetPt2().y, 4.0f);
}

TEST(DiaGeometry2DSerializer_Line, BinaryRoundTrip) {
    Line src(Vector2D(-1.0f, 0.0f), Vector2D(1.0f, 0.0f));
    Line dst;
    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.GetPt1().x, -1.0f);
    EXPECT_FLOAT_EQ(dst.GetPt2().x, 1.0f);
}

// =============================================================================
// Ray
// =============================================================================

TEST(DiaGeometry2DSerializer_Ray, JsonRoundTrip) {
    Ray src(Vector2D(0.0f, 0.0f), Vector2D(1.0f, 0.0f));
    Json::Value root = WriteToJson(src);

    Ray dst;
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.GetOrigin().x, 0.0f);
    EXPECT_FLOAT_EQ(dst.GetDirection().x, 1.0f);
    EXPECT_FLOAT_EQ(dst.GetDirection().y, 0.0f);
}

TEST(DiaGeometry2DSerializer_Ray, BinaryRoundTrip) {
    Ray src(Vector2D(5.0f, 3.0f), Vector2D(0.0f, 1.0f));
    Ray dst;
    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.GetOrigin().x, 5.0f);
    EXPECT_FLOAT_EQ(dst.GetOrigin().y, 3.0f);
    EXPECT_FLOAT_EQ(dst.GetDirection().y, 1.0f);
}

// =============================================================================
// Triangle
// =============================================================================

TEST(DiaGeometry2DSerializer_Triangle, JsonRoundTrip) {
    Triangle src(Vector2D(0,0), Vector2D(1,0), Vector2D(0,1));
    Json::Value root = WriteToJson(src);

    Triangle dst;
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.GetPt(0).x, 0.0f);
    EXPECT_FLOAT_EQ(dst.GetPt(1).x, 1.0f);
    EXPECT_FLOAT_EQ(dst.GetPt(2).y, 1.0f);
}

TEST(DiaGeometry2DSerializer_Triangle, BinaryRoundTrip) {
    Triangle src(Vector2D(0,0), Vector2D(2,0), Vector2D(1,2));
    Triangle dst;
    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.GetPt(1).x, 2.0f);
    EXPECT_FLOAT_EQ(dst.GetPt(2).y, 2.0f);
}

// =============================================================================
// Capsule
// =============================================================================

TEST(DiaGeometry2DSerializer_Capsule, JsonRoundTrip) {
    Capsule src(1.5f, Vector2D(0.0f, -2.0f), Vector2D(0.0f, 2.0f));
    Json::Value root = WriteToJson(src);

    Capsule dst;
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.GetRadius(), 1.5f);
    EXPECT_FLOAT_EQ(dst.GetPoint1().y, -2.0f);
    EXPECT_FLOAT_EQ(dst.GetPoint2().y, 2.0f);
}

TEST(DiaGeometry2DSerializer_Capsule, BinaryRoundTrip) {
    Capsule src(0.5f, Vector2D(-1.0f, 0.0f), Vector2D(1.0f, 0.0f));
    Capsule dst;
    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.GetRadius(), 0.5f);
    EXPECT_FLOAT_EQ(dst.GetPoint1().x, -1.0f);
}

// =============================================================================
// Arc
// =============================================================================

TEST(DiaGeometry2DSerializer_Arc, JsonRoundTrip) {
    Arc src(3.0f, Angle::FromDegrees(90.0f), Vector2D(1.0f, 2.0f), Vector2D(1.0f, 0.0f));
    Json::Value root = WriteToJson(src);

    Arc dst;
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.GetRadius(), 3.0f);
    EXPECT_FLOAT_EQ(dst.GetAngle().AsDegrees(), 90.0f);
    EXPECT_FLOAT_EQ(dst.GetFocal().x, 1.0f);
    EXPECT_FLOAT_EQ(dst.GetFocal().y, 2.0f);
    EXPECT_FLOAT_EQ(dst.GetAxis().x, 1.0f);
}

TEST(DiaGeometry2DSerializer_Arc, BinaryRoundTrip) {
    Arc src(2.0f, Angle::FromDegrees(45.0f), Vector2D(0.0f, 0.0f), Vector2D(0.0f, 1.0f));
    Arc dst;
    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.GetRadius(), 2.0f);
    EXPECT_FLOAT_EQ(dst.GetAngle().AsDegrees(), 45.0f);
}

// =============================================================================
// Sector
// =============================================================================

TEST(DiaGeometry2DSerializer_Sector, JsonRoundTrip) {
    Sector src(Vector2D(0.0f, 0.0f), 4.0f, Vector2D(1.0f, 0.0f), Angle::FromDegrees(30.0f));
    Json::Value root = WriteToJson(src);

    Sector dst;
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.GetRadius(), 4.0f);
    EXPECT_FLOAT_EQ(dst.GetCenter().x, 0.0f);
    EXPECT_FLOAT_EQ(dst.GetAxis().x, 1.0f);
    EXPECT_FLOAT_EQ(dst.GetHalfAngle().AsDegrees(), 30.0f);
}

TEST(DiaGeometry2DSerializer_Sector, BinaryRoundTrip) {
    Sector src(Vector2D(1.0f, 1.0f), 2.0f, Vector2D(0.0f, 1.0f), Angle::FromDegrees(60.0f));
    Sector dst;
    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.GetRadius(), 2.0f);
    EXPECT_FLOAT_EQ(dst.GetHalfAngle().AsDegrees(), 60.0f);
}

// =============================================================================
// ConvexPolygon — only live slice serialized
// =============================================================================

TEST(DiaGeometry2DSerializer_ConvexPolygon, JsonRoundTrip) {
    Vector2D verts[4] = {
        Vector2D(0,0), Vector2D(2,0), Vector2D(2,2), Vector2D(0,2)
    };
    ConvexPolygon src(verts, 4);
    Json::Value root = WriteToJson(src);

    ConvexPolygon dst;
    ReadFromJson(root, dst);

    EXPECT_EQ(dst.GetVertexCount(), 4);
    EXPECT_FLOAT_EQ(dst.GetVertex(0).x, 0.0f);
    EXPECT_FLOAT_EQ(dst.GetVertex(1).x, 2.0f);
    EXPECT_FLOAT_EQ(dst.GetVertex(2).y, 2.0f);
    EXPECT_FLOAT_EQ(dst.GetVertex(3).x, 0.0f);
}

TEST(DiaGeometry2DSerializer_ConvexPolygon, BinaryRoundTrip) {
    Vector2D verts[3] = { Vector2D(0,0), Vector2D(1,0), Vector2D(0,1) };
    ConvexPolygon src(verts, 3);
    ConvexPolygon dst;
    WriteAndReadBinary(src, dst);

    EXPECT_EQ(dst.GetVertexCount(), 3);
    EXPECT_FLOAT_EQ(dst.GetVertex(1).x, 1.0f);
    EXPECT_FLOAT_EQ(dst.GetVertex(2).y, 1.0f);
}

TEST(DiaGeometry2DSerializer_ConvexPolygon, OnlyLiveSliceWritten) {
    Vector2D verts[2] = { Vector2D(5,5), Vector2D(10,10) };
    ConvexPolygon src(verts, 2);
    Json::Value root = WriteToJson(src);

    // Only v00 and v01 keys should be present (index 0 and 1)
    EXPECT_TRUE(root.isMember("v00"));
    EXPECT_TRUE(root.isMember("v01"));
    EXPECT_FALSE(root.isMember("v02"));
}
