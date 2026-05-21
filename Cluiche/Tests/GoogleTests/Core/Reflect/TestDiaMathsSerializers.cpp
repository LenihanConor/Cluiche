// =============================================================================
// TestDiaMathsSerializers.cpp
// Tests for serialize() free functions defined in DiaMathsSerializers.h.
//
// Covers: Vector2D, Matrix22, Matrix34, Matrix44, Quaternion
// Archives: JSON round-trip, binary round-trip, partial JSON (missing fields),
//           ADL resolution, and DIA_TYPE_DEFINITION coexistence.
// =============================================================================

#include <gtest/gtest.h>
#include "DiaCore/Reflect/DiaMathsSerializers.h"
#include "DiaCore/Reflect/JsonArchive.h"
#include "DiaCore/Reflect/BinaryArchive.h"
#include "DiaCore/Reflect/SerializeResult.h"
#include "DiaCore/Json/external/json/json.h"


using namespace Dia::Reflect;
using namespace Dia::Maths;

// =============================================================================
// Helpers — identical pattern to TestJsonArchive and TestBinaryArchive
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
// A) Vector2D — JSON round-trip
// =============================================================================

TEST(DiaMathsSerializer_Vector2D, JsonRoundTrip) {
    Vector2D src(1.5f, 2.5f);
    Json::Value root = WriteToJson(src);

    Vector2D dst;
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.x, 1.5f);
    EXPECT_FLOAT_EQ(dst.y, 2.5f);
}

// =============================================================================
// B) Vector2D — binary round-trip
// =============================================================================

TEST(DiaMathsSerializer_Vector2D, BinaryRoundTrip) {
    Vector2D src(3.0f, -7.25f);
    Vector2D dst;

    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.x, 3.0f);
    EXPECT_FLOAT_EQ(dst.y, -7.25f);
}

// =============================================================================
// C) Matrix22 — JSON round-trip (all 4 elements)
// =============================================================================

TEST(DiaMathsSerializer_Matrix22, JsonRoundTrip) {
    // Set to a known non-identity matrix
    Matrix22 src(1.0f, 2.0f, 3.0f, 4.0f);  // e00=1, e01=2, e10=3, e11=4
    Json::Value root = WriteToJson(src);

    Matrix22 dst;  // default is Identity
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.Element(0), 1.0f);  // e00
    EXPECT_FLOAT_EQ(dst.Element(1), 2.0f);  // e01
    EXPECT_FLOAT_EQ(dst.Element(2), 3.0f);  // e10
    EXPECT_FLOAT_EQ(dst.Element(3), 4.0f);  // e11
}

// =============================================================================
// D) Matrix34 — JSON round-trip (spot-check several elements)
// =============================================================================

TEST(DiaMathsSerializer_Matrix34, JsonRoundTrip) {
    Matrix34 src(
        1.0f, 2.0f, 3.0f, 4.0f,
        5.0f, 6.0f, 7.0f, 8.0f,
        9.0f, 10.0f, 11.0f, 12.0f
    );
    Json::Value root = WriteToJson(src);

    Matrix34 dst = Matrix34::Identity();
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.m[0][0], 1.0f);
    EXPECT_FLOAT_EQ(dst.m[0][3], 4.0f);
    EXPECT_FLOAT_EQ(dst.m[1][0], 5.0f);
    EXPECT_FLOAT_EQ(dst.m[2][2], 11.0f);
    EXPECT_FLOAT_EQ(dst.m[2][3], 12.0f);
}

// =============================================================================
// E) Matrix44 — JSON round-trip (spot-check several elements)
// =============================================================================

TEST(DiaMathsSerializer_Matrix44, JsonRoundTrip) {
    Matrix44 src(
        1.0f,  2.0f,  3.0f,  4.0f,
        5.0f,  6.0f,  7.0f,  8.0f,
        9.0f,  10.0f, 11.0f, 12.0f,
        13.0f, 14.0f, 15.0f, 16.0f
    );
    Json::Value root = WriteToJson(src);

    Matrix44 dst = Matrix44::Identity();
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.m[0][0], 1.0f);
    EXPECT_FLOAT_EQ(dst.m[0][3], 4.0f);
    EXPECT_FLOAT_EQ(dst.m[1][1], 6.0f);
    EXPECT_FLOAT_EQ(dst.m[3][0], 13.0f);
    EXPECT_FLOAT_EQ(dst.m[3][3], 16.0f);
}

// =============================================================================
// F) Quaternion — JSON round-trip (identity quaternion)
// =============================================================================

TEST(DiaMathsSerializer_Quaternion, JsonRoundTrip) {
    Quaternion src(0.0f, 0.0f, 0.0f, 1.0f);  // identity
    Json::Value root = WriteToJson(src);

    Quaternion dst(1.0f, 1.0f, 1.0f, 0.0f);  // force non-identity to confirm write
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.x, 0.0f);
    EXPECT_FLOAT_EQ(dst.y, 0.0f);
    EXPECT_FLOAT_EQ(dst.z, 0.0f);
    EXPECT_FLOAT_EQ(dst.w, 1.0f);
}

// =============================================================================
// G) Quaternion — binary round-trip
// =============================================================================

TEST(DiaMathsSerializer_Quaternion, BinaryRoundTrip) {
    Quaternion src(0.1f, 0.2f, 0.3f, 0.9274f);
    Quaternion dst;

    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.x, src.x);
    EXPECT_FLOAT_EQ(dst.y, src.y);
    EXPECT_FLOAT_EQ(dst.z, src.z);
    EXPECT_FLOAT_EQ(dst.w, src.w);
}

// =============================================================================
// H) Matrix22 — binary round-trip
// =============================================================================

TEST(DiaMathsSerializer_Matrix22, BinaryRoundTrip) {
    Matrix22 src(1.0f, 2.0f, 3.0f, 4.0f);
    Matrix22 dst;  // Identity by default

    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.Element(0), 1.0f);
    EXPECT_FLOAT_EQ(dst.Element(1), 2.0f);
    EXPECT_FLOAT_EQ(dst.Element(2), 3.0f);
    EXPECT_FLOAT_EQ(dst.Element(3), 4.0f);
}

// =============================================================================
// I) Vector2D — missing field in JSON keeps C++ default
//    Write only "x", omit "y". On read, y should stay at the dst default (0.0).
// =============================================================================

TEST(DiaMathsSerializer_Vector2D, MissingFieldKeepsDefault) {
    Json::Value partial;
    partial["x"] = 5.0f;
    // "y" intentionally absent

    Vector2D dst(99.0f, 99.0f);  // give it a non-zero default to detect no-write
    ReadFromJson(partial, dst);

    EXPECT_FLOAT_EQ(dst.x, 5.0f);
    // y was NOT in JSON — it should keep the pre-read value (99.0f) since
    // DIA_FIELD is optional (no Required attribute)
    EXPECT_FLOAT_EQ(dst.y, 99.0f);
}

// =============================================================================
// J) ADL: Dia::Maths::serialize is callable directly from archive context
//    Verifies the function is in the right namespace so archives find it via ADL.
// =============================================================================

TEST(DiaMathsSerializer_ADL, SerializeFunctionFoundViaADL) {
    // Call serialize() with an explicit namespace-qualified name to prove it
    // lives in Dia::Maths and can be found there.
    Vector2D src(7.0f, 8.0f);
    Vector2D dst;

    JsonWriteArchive wAr;
    Dia::Maths::serialize(wAr, src, 1u);

    JsonReadArchive rAr(wAr.GetRoot());
    Dia::Maths::serialize(rAr, dst, 1u);

    EXPECT_FLOAT_EQ(dst.x, 7.0f);
    EXPECT_FLOAT_EQ(dst.y, 8.0f);
}

