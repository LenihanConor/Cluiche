#include <gtest/gtest.h>
#include "DiaCore/Reflect/Reflect.h"
#include "DiaCore/CRC/StringCRC.h"

using namespace Dia::Reflect;
using namespace Dia::Core;

// =============================================================================
// Test types — polymorphic hierarchy
// =============================================================================

struct IShape {
    virtual ~IShape() = default;
};

struct CircleShape : IShape {
    float mRadius = 0.0f;
};

struct BoxShape : IShape {
    float mWidth = 0.0f;
    float mHeight = 0.0f;
};

// Register CircleShape
DIA_SERIALIZE_POLYMORPHIC(CircleShape, IShape, 1)
    DIA_FIELD(mRadius)
DIA_SERIALIZE_END

// Register BoxShape
DIA_SERIALIZE_POLYMORPHIC(BoxShape, IShape, 1)
    DIA_FIELD(mWidth)
    DIA_FIELD(mHeight)
DIA_SERIALIZE_END

// A struct that owns a polymorphic pointer (uses CircleShape as the concrete type)
struct PhysicsBodyCircle {
    IShape* mShape = nullptr;
    float mMass = 1.0f;

    ~PhysicsBodyCircle() { delete mShape; }
};

DIA_SERIALIZE(PhysicsBodyCircle, 1)
    DIA_FIELD_POLY_OWNED_PTR(mShape, CircleShape)
    DIA_FIELD(mMass)
DIA_SERIALIZE_END

// A struct that owns a polymorphic pointer (uses BoxShape as the concrete type)
struct PhysicsBodyBox {
    IShape* mShape = nullptr;
    float mMass = 1.0f;

    ~PhysicsBodyBox() { delete mShape; }
};

DIA_SERIALIZE(PhysicsBodyBox, 1)
    DIA_FIELD_POLY_OWNED_PTR(mShape, BoxShape)
    DIA_FIELD(mMass)
DIA_SERIALIZE_END

// A non-polymorphic struct to verify no interference
struct SimpleVec {
    float mX = 0.0f;
    float mY = 0.0f;
};

DIA_SERIALIZE(SimpleVec, 1)
    DIA_FIELD(mX)
    DIA_FIELD(mY)
DIA_SERIALIZE_END

// =============================================================================
// Helper: write to JSON, return root
// =============================================================================
template<typename T>
Json::Value WriteToJson(T& obj) {
    JsonWriteArchive ar;
    serialize(ar, obj, 0u);
    return ar.GetRoot();
}

// =============================================================================
// Tests
// =============================================================================

// --- A: Registry lookup by CircleShape CRC ---
TEST(PolymorphicRegistry, FindCircleShapeByCrc) {
    auto* entry = PolymorphicRegistry::Instance().Find(StringCRC("CircleShape").Value());
    ASSERT_NE(entry, nullptr);
    EXPECT_STREQ(entry->typeName, "CircleShape");
}

// --- B: Registry lookup by BoxShape CRC ---
TEST(PolymorphicRegistry, FindBoxShapeByCrc) {
    auto* entry = PolymorphicRegistry::Instance().Find(StringCRC("BoxShape").Value());
    ASSERT_NE(entry, nullptr);
    EXPECT_STREQ(entry->typeName, "BoxShape");
}

// --- C: Unknown CRC returns nullptr ---
TEST(PolymorphicRegistry, FindUnknownCrcReturnsNull) {
    auto* entry = PolymorphicRegistry::Instance().Find(StringCRC("NonExistent").Value());
    EXPECT_EQ(entry, nullptr);
}

// --- D: Factory creates CircleShape with default values ---
TEST(PolymorphicRegistry, FactoryCreatesCircleShape) {
    auto* entry = PolymorphicRegistry::Instance().Find(StringCRC("CircleShape").Value());
    ASSERT_NE(entry, nullptr);
    void* raw = entry->factory();
    ASSERT_NE(raw, nullptr);
    CircleShape* circle = static_cast<CircleShape*>(raw);
    EXPECT_FLOAT_EQ(circle->mRadius, 0.0f);
    delete circle;
}

// --- E: JSON round-trip PhysicsBodyCircle with CircleShape ---
TEST(PolymorphicRegistry, JsonRoundTripCircle) {
    PhysicsBodyCircle src;
    src.mShape = new CircleShape();
    static_cast<CircleShape*>(src.mShape)->mRadius = 3.5f;
    src.mMass = 2.0f;

    // Write
    Json::Value root = WriteToJson(src);

    // Read into a new instance
    PhysicsBodyCircle dst;
    JsonReadArchive readAr(root);
    serialize(readAr, dst, 0u);

    ASSERT_NE(dst.mShape, nullptr);
    CircleShape* dstCircle = dynamic_cast<CircleShape*>(dst.mShape);
    ASSERT_NE(dstCircle, nullptr);
    EXPECT_FLOAT_EQ(dstCircle->mRadius, 3.5f);
    EXPECT_FLOAT_EQ(dst.mMass, 2.0f);
}

// --- F: JSON write produces expected structure ---
TEST(PolymorphicRegistry, JsonWriteProducesTypeTag) {
    PhysicsBodyCircle src;
    src.mShape = new CircleShape();
    static_cast<CircleShape*>(src.mShape)->mRadius = 1.5f;
    src.mMass = 2.0f;

    Json::Value root = WriteToJson(src);

    ASSERT_TRUE(root.isMember("mShape"));
    ASSERT_TRUE(root["mShape"].isObject());
    ASSERT_TRUE(root["mShape"].isMember("_type"));
    EXPECT_STREQ(root["mShape"]["_type"].asCString(), "CircleShape");
    EXPECT_FLOAT_EQ(root["mShape"]["mRadius"].asFloat(), 1.5f);
    EXPECT_FLOAT_EQ(root["mMass"].asFloat(), 2.0f);
}

// --- G: JSON read with _type creates correct concrete instance ---
TEST(PolymorphicRegistry, JsonReadCreatesConcreteFromTypeTag) {
    // Build JSON manually
    Json::Value root(Json::objectValue);
    root["mShape"] = Json::Value(Json::objectValue);
    root["mShape"]["_type"] = "CircleShape";
    root["mShape"]["mRadius"] = 7.0f;
    root["mMass"] = 5.0f;

    PhysicsBodyCircle dst;
    JsonReadArchive readAr(root);
    serialize(readAr, dst, 0u);

    EXPECT_TRUE(readAr.GetResult().IsOk());
    ASSERT_NE(dst.mShape, nullptr);
    CircleShape* circle = dynamic_cast<CircleShape*>(dst.mShape);
    ASSERT_NE(circle, nullptr);
    EXPECT_FLOAT_EQ(circle->mRadius, 7.0f);
    EXPECT_FLOAT_EQ(dst.mMass, 5.0f);
}

// --- H: JSON read with unknown _type produces error ---
TEST(PolymorphicRegistry, JsonReadUnknownTypeProducesError) {
    Json::Value root(Json::objectValue);
    root["mShape"] = Json::Value(Json::objectValue);
    root["mShape"]["_type"] = "TriangleShape";
    root["mShape"]["mRadius"] = 1.0f;
    root["mMass"] = 1.0f;

    PhysicsBodyCircle dst;
    JsonReadArchive readAr(root);
    serialize(readAr, dst, 0u);

    EXPECT_TRUE(readAr.GetResult().HasErrors());
    EXPECT_EQ(readAr.GetResult().GetError(0).kind, SerializeErrorKind::UnknownPolymorphicType);
    EXPECT_EQ(dst.mShape, nullptr);
}

// --- I: JSON read with missing _type key produces error ---
TEST(PolymorphicRegistry, JsonReadMissingTypeKeyProducesError) {
    Json::Value root(Json::objectValue);
    root["mShape"] = Json::Value(Json::objectValue);
    root["mShape"]["mRadius"] = 1.0f;
    // No "_type" key
    root["mMass"] = 1.0f;

    PhysicsBodyCircle dst;
    JsonReadArchive readAr(root);
    serialize(readAr, dst, 0u);

    EXPECT_TRUE(readAr.GetResult().HasErrors());
    EXPECT_EQ(readAr.GetResult().GetError(0).kind, SerializeErrorKind::UnknownPolymorphicType);
    EXPECT_EQ(dst.mShape, nullptr);
}

// --- J: Binary round-trip PhysicsBodyCircle with CircleShape ---
TEST(PolymorphicRegistry, BinaryRoundTripCircle) {
    PhysicsBodyCircle src;
    src.mShape = new CircleShape();
    static_cast<CircleShape*>(src.mShape)->mRadius = 4.25f;
    src.mMass = 9.0f;

    // Write
    BinaryWriteArchive writeAr;
    writeAr.WriteVersion(1u);
    serialize(writeAr, src, 0u);

    // Read
    PhysicsBodyCircle dst;
    BinaryReadArchive readAr(writeAr.GetData(), writeAr.GetSize());
    readAr.ReadVersion();
    serialize(readAr, dst, 0u);

    ASSERT_NE(dst.mShape, nullptr);
    CircleShape* dstCircle = dynamic_cast<CircleShape*>(dst.mShape);
    ASSERT_NE(dstCircle, nullptr);
    EXPECT_FLOAT_EQ(dstCircle->mRadius, 4.25f);
    EXPECT_FLOAT_EQ(dst.mMass, 9.0f);
}

// --- K: Polymorphic types don't interfere with non-polymorphic types ---
TEST(PolymorphicRegistry, NoInterferenceWithNonPolymorphic) {
    // Serialize a non-polymorphic type alongside polymorphic types
    SimpleVec vec;
    vec.mX = 10.0f;
    vec.mY = 20.0f;

    Json::Value root = WriteToJson(vec);
    EXPECT_FLOAT_EQ(root["mX"].asFloat(), 10.0f);
    EXPECT_FLOAT_EQ(root["mY"].asFloat(), 20.0f);
    EXPECT_FALSE(root.isMember("_type"));

    // Also verify polymorphic still works in same process
    auto* entry = PolymorphicRegistry::Instance().Find(StringCRC("CircleShape").Value());
    ASSERT_NE(entry, nullptr);
}

// --- L: Multiple concrete types registered independently ---
TEST(PolymorphicRegistry, MultipleConcretesRegisteredIndependently) {
    auto* circle = PolymorphicRegistry::Instance().Find(StringCRC("CircleShape").Value());
    auto* box = PolymorphicRegistry::Instance().Find(StringCRC("BoxShape").Value());

    ASSERT_NE(circle, nullptr);
    ASSERT_NE(box, nullptr);
    EXPECT_NE(circle->typeCrc, box->typeCrc);
    EXPECT_STREQ(circle->typeName, "CircleShape");
    EXPECT_STREQ(box->typeName, "BoxShape");
}

// --- M: Objects created via factory are deleteable via base pointer (virtual destructor) ---
TEST(PolymorphicRegistry, FactoryObjectsDeletableViaBasePointer) {
    auto* entry = PolymorphicRegistry::Instance().Find(StringCRC("BoxShape").Value());
    ASSERT_NE(entry, nullptr);
    void* raw = entry->factory();
    ASSERT_NE(raw, nullptr);
    // Delete via base pointer — safe because IShape has virtual destructor
    IShape* base = static_cast<IShape*>(raw);
    delete base;  // Should not leak or crash
}

// --- Extra: JSON round-trip with BoxShape ---
TEST(PolymorphicRegistry, JsonRoundTripBox) {
    PhysicsBodyBox src;
    src.mShape = new BoxShape();
    static_cast<BoxShape*>(src.mShape)->mWidth = 5.0f;
    static_cast<BoxShape*>(src.mShape)->mHeight = 3.0f;
    src.mMass = 7.0f;

    Json::Value root = WriteToJson(src);

    ASSERT_TRUE(root["mShape"].isMember("_type"));
    EXPECT_STREQ(root["mShape"]["_type"].asCString(), "BoxShape");

    PhysicsBodyBox dst;
    JsonReadArchive readAr(root);
    serialize(readAr, dst, 0u);

    ASSERT_NE(dst.mShape, nullptr);
    BoxShape* dstBox = dynamic_cast<BoxShape*>(dst.mShape);
    ASSERT_NE(dstBox, nullptr);
    EXPECT_FLOAT_EQ(dstBox->mWidth, 5.0f);
    EXPECT_FLOAT_EQ(dstBox->mHeight, 3.0f);
    EXPECT_FLOAT_EQ(dst.mMass, 7.0f);
}

// --- Extra: Binary round-trip with BoxShape ---
TEST(PolymorphicRegistry, BinaryRoundTripBox) {
    PhysicsBodyBox src;
    src.mShape = new BoxShape();
    static_cast<BoxShape*>(src.mShape)->mWidth = 2.5f;
    static_cast<BoxShape*>(src.mShape)->mHeight = 1.5f;
    src.mMass = 6.0f;

    BinaryWriteArchive writeAr;
    writeAr.WriteVersion(1u);
    serialize(writeAr, src, 0u);

    PhysicsBodyBox dst;
    BinaryReadArchive readAr(writeAr.GetData(), writeAr.GetSize());
    readAr.ReadVersion();
    serialize(readAr, dst, 0u);

    ASSERT_NE(dst.mShape, nullptr);
    BoxShape* dstBox = dynamic_cast<BoxShape*>(dst.mShape);
    ASSERT_NE(dstBox, nullptr);
    EXPECT_FLOAT_EQ(dstBox->mWidth, 2.5f);
    EXPECT_FLOAT_EQ(dstBox->mHeight, 1.5f);
    EXPECT_FLOAT_EQ(dst.mMass, 6.0f);
}

// --- Extra: Null pointer write/read round-trip JSON ---
TEST(PolymorphicRegistry, JsonNullPointerRoundTrip) {
    PhysicsBodyCircle src;
    // mShape is nullptr

    Json::Value root = WriteToJson(src);
    EXPECT_TRUE(root["mShape"].isNull());

    PhysicsBodyCircle dst;
    JsonReadArchive readAr(root);
    serialize(readAr, dst, 0u);

    EXPECT_EQ(dst.mShape, nullptr);
    EXPECT_TRUE(readAr.GetResult().IsOk());
}

// --- Extra: Null pointer write/read round-trip Binary ---
TEST(PolymorphicRegistry, BinaryNullPointerRoundTrip) {
    PhysicsBodyCircle src;
    // mShape is nullptr

    BinaryWriteArchive writeAr;
    writeAr.WriteVersion(1u);
    serialize(writeAr, src, 0u);

    PhysicsBodyCircle dst;
    BinaryReadArchive readAr(writeAr.GetData(), writeAr.GetSize());
    readAr.ReadVersion();
    serialize(readAr, dst, 0u);

    EXPECT_EQ(dst.mShape, nullptr);
}
